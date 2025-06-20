#include <chrono>       // 시간 관련 기능
#include <fstream>      // 파일 입출력 기능
#include <iomanip>      // 시간 및 날짜 포맷팅
#include <iostream>
#include <sstream>      // 문자열 스트림 : 문자열에서 원하는 자료형의 데이터 추출
#include <limits.h>     // PATH_MAX 사용을 위해 추가
#include <unistd.h>     // readlink 사용을 위해 추가

#include "QuarantineManager.h" // QuarantineManager 클래스 헤더파일 호출
#include "ScanMalware.h" // DetectionResultRecord 구조체 사용

namespace fs = std::filesystem;

// 실행 파일 위치를 기준으로 상대 경로를 계산하는 유틸 함수
static fs::path getBinaryRelativePath(const fs::path& relativePath)
{
    char result[PATH_MAX];
    // /proc/self/exe : 현재 프로세스를 실행하는 실행 파일을 가리키는 심볼릭 링크
    // readlink : 심볼릭 링크가 실제로 가리키는 대상의 경로를 읽어오는 함수
    // readlink와 /proc/self/exe로 현재 실행하는 프로세스의 실행파일 절대경로를 result에 저장한다. 오류 발생 시 -1을 반환한다.
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    if (count == -1)
    {
        std::cerr << "ERROR: Could not get executable path for relative path calculation." << std::endl;
        return relativePath; // 실패 시 원래 경로 반환 또는 오류 처리
    }
    // 실행 파일 디렉토리의 절대 경로를 binaryPath에 저장
    fs::path binaryPath = fs::path(std::string(result, count)).parent_path();
    // 실행 파일 디렉토리의 절대 경로와 함수 인자로 받은 상대 경로를 결합
    return binaryPath / relativePath;
}


// 생성자 및 소멸자 정의(링커 오류 방지)
QuarantineManager::QuarantineManager()
    : mDb(nullptr) // DB 연결은 Run()에서 수행
{}
QuarantineManager::~QuarantineManager()
{}

// SQLite DB 연결 구현
bool QuarantineManager::openDatabase(const std::string& dbPath)
{
    // 실행파일(바이너리) 상대 경로를 통해, 해당 경로의 SQLite 파일 지정
    fs::path absoluteDbPath = getBinaryRelativePath(dbPath);

    // SQLite DB 파일 열기
    // 파일 열기 성공시 SQLITE_OK(값은 0) 반환 후, mDb 포인터에 DB 연결 핸들 저장
    int rc = sqlite3_open(absoluteDbPath.string().c_str(), &mDb);
    if (rc)     // 연결 실패(rc 값이 0(SQLITE_OK 값)이 아닌 경우)에는 false 반환
    {
        std::cerr << "ERROR: Can't open database: " << sqlite3_errmsg(mDb) << std::endl;
        mDb = nullptr;
        return false;
    }
    else    // 연결 성공 시, true 반환
    {
        std::cout << "DEBUG: Opened database successfully: " << absoluteDbPath << std::endl;
        // DB 테이블이 없는 경우 생성, 테이블이 있다면 IF NOT EXISTS로 건너뜀
        const char* createTableSQL =
            "CREATE TABLE IF NOT EXISTS quarantine_log ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "original_path TEXT NOT NULL,"
            "quarantined_path TEXT NOT NULL,"
            "original_size INTEGER,"
            "quarantine_date TEXT NOT NULL,"
            "quarantine_reason TEXT,"
            "malware_name_or_rule TEXT"
            ");";
        char* errMsg = nullptr; // errMsg : SQLite의 오류 메시지를 메모리에 저장할 수 있도록 하는 포인터 변수
        // sqlite3_exec 함수를 호출하여, DB 연결 및 테이블 생성 명령어 수행
        // 세 번째와 네번째 인자는 콜백함수 및 전달할 사용자 데이터지만, CREATE TABLE명령에서는 필요 없으므로, nullptr
        // sqlite3_exec 함수 수행 결과(성공 : SQLITE_OK(0), 실패 : 0이 아닌 오류 코드)를 rc에 저장
        rc = sqlite3_exec(mDb, createTableSQL, nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK)    // sqlite3_exec의 값이 SQLITE_OK가 아니라면, false 반환 및 DB 연결 종료
        {
            std::cerr << "ERROR: Failed to create DB table: " << errMsg << std::endl;
            sqlite3_free(errMsg);
            closeDatabase();
            return false;
        }
        std::cout << "DEBUG: DB table 'quarantine_log' checked/created." << std::endl;
        return true;
    }
}

// SQLite DB 연결 종료 구현
void QuarantineManager::closeDatabase()
{
    if (mDb)
    {
        sqlite3_close(mDb);  // 연결 종료
        mDb = nullptr;   // 포인터 초기화, nullptr 사용
        std::cout << "DEBUG: Database connection closed." << std::endl;
    }
}

// DB에 격리 메타데이터 기록 구현
bool QuarantineManager::logQuarantineMetadata(const QuarantineMetadata& metadata)
{
    if (!mDb)    // DB 연결이 안되어 있으면, false 반환
    {
        std::cerr << "ERROR: Database not open. Cannot log metadata." << std::endl;
        return false;
    }

    // DB에 메타데이터 기록 SQL문
    const char* sql = "INSERT INTO quarantine_log (original_path, quarantined_path, original_size, quarantine_date, quarantine_reason, malware_name_or_rule) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;

    // SQL 명령문을 DB가 실행 가능한 형태로 컴파일
    int rc = sqlite3_prepare_v2(mDb, sql, -1, &stmt, nullptr);
    // 컴파일 결과(성공 : SQLITE_OK, 실패 : 0이 아닌 값)를 rc에 저장
    if (rc != SQLITE_OK)    // rc가 SQLITE_OK가 아니라면, false 반환
    {
        std::cerr << "ERROR: SQL error preparing statement: " << sqlite3_errmsg(mDb) << std::endl;
        return false;
    }

    // SQL 명령문의 '?' 부분에 실제 데이터 값을 대입
    sqlite3_bind_text(stmt, 1, metadata.OriginalPath.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, metadata.QuarantinedPath.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, metadata.OriginalSize);
    sqlite3_bind_text(stmt, 4, metadata.QuarantineDate.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, metadata.QuarantineReason.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, metadata.MalwareNameOrRule.c_str(), -1, SQLITE_STATIC);

    // SQL 명령문 실행
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)  // 실행 실패 시, false 반환
    {
        std::cerr << "ERROR: SQL error executing statement: " << sqlite3_errmsg(mDb) << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

// XOR 암호화 함수 구현
bool QuarantineManager::applySimpleXOREncryption(const fs::path& filePath)
{
    // 파일을 읽고 쓰기가 가능한 바이너리 모드로 연다.
    std::fstream file(filePath, std::ios::in | std::ios::out | std::ios::binary);
    if (!file)  // 파일 열기 실패 시, false 반환
    {
        std::cerr << "ERROR: Could not open file for encryption: " << filePath << std::endl;
        return false;
    }

    // 파일의 내용을 1 byte씩 읽어서 XOR 연산 후 다시 저장
    char byte;
    while (file.get(byte))
    {
        byte ^= ENCRYPTION_KEY;     // 읽은 byte와 XOR 연산
        file.seekp(-1, std::ios::cur);
        file.put(byte);     // 변경된 byte를 원래 위치에 덮어쓰기
    }

    file.close();   // 파일 닫기
    return true;    // 성공 반환
}

// 파일 격리 처리 구현
bool QuarantineManager::processQuarantine(const DetectionResultRecord& item, const std::string& quarantineDirectory)
{
    fs::path originalFilePath = item.path;   // 원본 파일 경로 객체 생성
    std::string filename = originalFilePath.filename().string();    // 파일 이름만 추출

    // 격리 디렉토리 경로에 바이너리 상대 경로 적용
    fs::path absoluteQuarantineDir = getBinaryRelativePath(quarantineDirectory);

    std::string quarantinedFilename = filename + "_" + getCurrentDateTime();
    fs::path quarantinedFilePath = absoluteQuarantineDir / quarantinedFilename;

    QuarantineMetadata metadata;
    metadata.OriginalPath = item.path;
    metadata.QuarantinedPath = quarantinedFilePath.string();
    metadata.OriginalSize = item.size;
    metadata.QuarantineDate = getCurrentDateTime();
    metadata.QuarantineReason = item.cause;
    metadata.MalwareNameOrRule = item.name;

    bool currentQuarantineSuccess = false; // 파일의 격리 성공 여부를 추적하는 임시 변수

    try
    {
        // 격리 디렉토리가 없으면 생성
        if (!fs::exists(absoluteQuarantineDir))
        {
             fs::create_directories(absoluteQuarantineDir);
             std::cout << "DEBUG: Quarantine directory created: " << absoluteQuarantineDir << std::endl;
        }

        // 파일 이동
        fs::rename(originalFilePath, quarantinedFilePath);
        std::cout << "DEBUG: File moved to quarantine: " << item.path << " -> " << quarantinedFilePath << std::endl;
        currentQuarantineSuccess = true; // 파일 이동 성공

        // 암호화 시도
        if (!applySimpleXOREncryption(quarantinedFilePath)) // 실패시 false 저장
        {
             std::cerr << "ERROR: Warning: File moved but failed to apply simple encryption: " << item.path << std::endl;
             currentQuarantineSuccess = false;
        }

    }
    catch (const fs::filesystem_error& e)   // filesystem_error 예외 발생 시, false 저장
    {
        std::cerr << "ERROR: Error during quarantine process for " << item.path << ": " << e.what() << std::endl;
        currentQuarantineSuccess = false;
    }

    logQuarantineMetadata(metadata);

    return currentQuarantineSuccess; // 이 파일의 최종 격리 성공 여부 반환
}

// 현재 시간 가져오기 구현
std::string QuarantineManager::getCurrentDateTime() const
{
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S");
    return ss.str();
}

// 격리 작업 실행(격리 대상 파일 목록, 격리 디렉토리, DB 경로를 인자로 받음)
void QuarantineManager::Run(const std::vector<DetectionResultRecord>& itemsToQuarantine,
                            const std::string& quarantineDirectory,
                            const std::string& metadataDatabasePath)
{
    // DB 연결
    if (!openDatabase(metadataDatabasePath)) // DB 경로 전달
    {
        std::cerr << "ERROR: Database connection failed. Cannot perform quarantine." << std::endl;
        // DB 연결 실패 시 모든 항목 격리 실패 처리
        mbIsQuarantineSuccess.assign(itemsToQuarantine.size(), false);
        return;
    }

    // mbIsQuarantineSuccess : 격리 성공 여부 벡터
    // itemsToQuarantine : 격리가 필요한 파일들의 목록
    // 격리 성공 여부 벡터 크기 조정 및 false 지정
    mbIsQuarantineSuccess.resize(itemsToQuarantine.size(), false);

    // itemsToQuarantine.size()로 몇개의 파일을 격리할 것인지 출력
    std::cout << "DEBUG: Starting quarantine process for " << itemsToQuarantine.size() << " files..." << std::endl;
    
    // processQuarantine(격리 처리 함수) 호출 및 격리 작업 수행, i번째 항목에 대한 격리 수행 결과(true, false) 반환
    for (size_t i = 0; i < itemsToQuarantine.size(); ++i)
    {
        bool success = processQuarantine(itemsToQuarantine[i], quarantineDirectory);
        mbIsQuarantineSuccess[i] = success;     // i번째 파일의 격리 성공 여부 기록
    }

    std::cout << "DEBUG: Quarantine process finished." << std::endl;

    closeDatabase();    // DB 연결 종료
}

// 격리 성공 여부 가져오기 구현
const std::vector<bool>& QuarantineManager::GetIsQuarantineSuccess() const // public 메서드는 PascalCase 사용
{
    return mbIsQuarantineSuccess;
}
