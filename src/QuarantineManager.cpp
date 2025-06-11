#include <chrono>       // 시간 관련 기능
#include <fstream>      // 파일 입출력 기능
#include <iomanip>      // 시간 및 날짜 포맷팅
#include <iostream>
#include <sstream>      // 문자열 스트림 : 문자열에서 원하는 자료형의 데이터 추출

#include "QuarantineManager.h" // QuarantineManager 클래스 헤더파일 호출

// 생성자 구현
QuarantineManager::QuarantineManager(const std::vector<std::string>& filesToQuarantine,
                                     const std::vector<std::string>& reasons,
                                     const std::vector<std::string>& namesOrRules,
                                     const std::vector<long long>& sizes,
                                     const std::string& quarantineDirectory,
                                     const std::string& metadataDatabasePath)
    : mFilesToQuarantine(filesToQuarantine)
    , mQuarantineReasons(reasons)
    , mMalwareNamesOrRules(namesOrRules)
    , mOriginalFileSizes(sizes)
    , mQuarantineDir(quarantineDirectory)
    , mMetadataDbPath(metadataDatabasePath)
    , mDb(nullptr)
{
    if (mFilesToQuarantine.size() != mQuarantineReasons.size() ||
        mFilesToQuarantine.size() != mMalwareNamesOrRules.size() ||
        mFilesToQuarantine.size() != mOriginalFileSizes.size())
    {
        std::cerr << "Error: Input vectors to QuarantineManager constructor have different sizes." << std::endl;
    }

    // 각 악성코드 파일의 격리 성공 여부 목록을 파일 갯수만큼 만들고, 모두 'false'로 초기화
    mbIsQuarantineSuccess.resize(mFilesToQuarantine.size(), false);

    // 격리 디렉토리가 없으면 생성
    if (!fs::exists(mQuarantineDir))
    {
        try
        {
            fs::create_directories(mQuarantineDir);  // 디렉토리 생성(필요한 상위 디렉토리까지 생성)
            std::cout << "Quarantine directory created: " << mQuarantineDir << std::endl;
        }
        catch (const fs::filesystem_error& e)
        {
            std::cerr << "Error creating quarantine directory " << mQuarantineDir << ": " << e.what() << std::endl;
        }
    }
    // 격리 메타데이터 DB 파일에 연결
    openDatabase();
}

// 소멸자 구현
QuarantineManager::~QuarantineManager()
{
    closeDatabase();    // 객체가 사라질 때, 격리 메타데이터 DB 파일 연결 종료
}

// SQLite DB 연결 구현
bool QuarantineManager::openDatabase()
{
    int rc = sqlite3_open(mMetadataDbPath.c_str(), &mDb);
    if (rc)     // 연결 실패 시, false 반환
    {
        std::cerr << "Can't open database: " << sqlite3_errmsg(mDb) << std::endl;
        mDb = nullptr;
        return false;
    }
    else    // 연결 성공 시, true 반환
    {
        std::cout << "Opened database successfully: " << mMetadataDbPath << std::endl;
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
        std::cout << "Database connection closed." << std::endl;
    }
}

// DB에 격리 메타데이터 기록 구현
bool QuarantineManager::logQuarantineMetadata(const QuarantineMetadata& metadata)
{
    if (!mDb)    // DB 연결이 안되어 있으면, false 반환
    {
        std::cerr << "Database not open. Cannot log metadata." << std::endl;
        return false;
    }

    // SQL 명령문
    const char* sql = "INSERT INTO quarantine_log (original_path, quarantined_path, original_size, quarantine_date, quarantine_reason, malware_name_or_rule, success) VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;

    // SQL 명령문을 DB가 실행 가능한 형태로 컴파일
    int rc = sqlite3_prepare_v2(mDb, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
        std::cerr << "SQL error preparing statement: " << sqlite3_errmsg(mDb) << std::endl;
        return false;
    }

    // SQL 명령문의 '?' 부분에 실제 데이터 값을 대입
    sqlite3_bind_text(stmt, 1, metadata.OriginalPath.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, metadata.QuarantinedPath.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, metadata.OriginalSize);
    sqlite3_bind_text(stmt, 4, metadata.QuarantineDate.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, metadata.QuarantineReason.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, metadata.MalwareNameOrRule.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 7, metadata.IsSuccess ? 1 : 0);

    // SQL 명령문 실행
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)  // 실행 실패 시, false 반환
    {
        std::cerr << "SQL error executing statement: " << sqlite3_errmsg(mDb) << std::endl;
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
        std::cerr << "Error: Could not open file for encryption: " << filePath << std::endl;
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
bool QuarantineManager::processQuarantine(const std::string& originalPath, const std::string& reason, const std::string& nameOrRule, long long originalSize)
{
    fs::path originalFilePath = originalPath;   // 원본 파일 경로 객체 생성
    std::string filename = originalFilePath.filename().string();    // 파일 이름만 추출

    std::string quarantinedFilename = filename + "_" + getCurrentDateTime();
    fs::path quarantinedFilePath = fs::path(mQuarantineDir) / quarantinedFilename;

    QuarantineMetadata metadata;
    metadata.OriginalPath = originalPath;
    metadata.QuarantinedPath = quarantinedFilePath.string();
    metadata.OriginalSize = originalSize;
    metadata.QuarantineDate = getCurrentDateTime();
    metadata.QuarantineReason = reason;
    metadata.MalwareNameOrRule = nameOrRule;
    metadata.IsSuccess = false;

    try
    {
        if (!fs::exists(mQuarantineDir))
        {
             fs::create_directories(mQuarantineDir);
             std::cout << "Quarantine directory created: " << mQuarantineDir << std::endl;
        }

        fs::rename(originalFilePath, quarantinedFilePath);
        std::cout << "File moved to quarantine: " << originalPath << " -> " << quarantinedFilePath << std::endl;
        metadata.IsSuccess = true;

        if (!applySimpleXOREncryption(quarantinedFilePath))
        {
             std::cerr << "Warning: File moved but failed to apply simple encryption: " << originalPath << std::endl;
             metadata.IsSuccess = false;
        }

    }
    catch (const fs::filesystem_error& e)
    {
        std::cerr << "Error during quarantine process for " << originalPath << ": " << e.what() << std::endl;
        metadata.IsSuccess = false;
    }

    logQuarantineMetadata(metadata);

    return metadata.IsSuccess;
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

// 격리 작업 실행
void QuarantineManager::Run()
{
    if (!mDb)
    {
        std::cerr << "Database connection failed. Cannot perform quarantine." << std::endl;
        for(size_t i = 0; i < mbIsQuarantineSuccess.size(); ++i)
        {
            mbIsQuarantineSuccess[i] = false;
        }
        return;
    }

    std::cout << "Starting quarantine process for " << mFilesToQuarantine.size() << " files..." << std::endl;
    for (size_t i = 0; i < mFilesToQuarantine.size(); ++i)
    {
        if (i < mQuarantineReasons.size() && i < mMalwareNamesOrRules.size() && i < mOriginalFileSizes.size())
        {
            bool success = processQuarantine(mFilesToQuarantine[i], mQuarantineReasons[i], mMalwareNamesOrRules[i], mOriginalFileSizes[i]);
            mbIsQuarantineSuccess[i] = success;
        }
        else
        {
            std::cerr << "Error: Mismatch in input vector sizes at index " << i << ". Skipping quarantine for this file." << std::endl;
            mbIsQuarantineSuccess[i] = false;
        }
    }

    std::cout << "Quarantine process finished." << std::endl;
}

// 격리 성공 여부 가져오기 구현
const std::vector<bool>& QuarantineManager::GetIsQuarantineSuccess() const // public 메서드는 PascalCase 사용, const 추가
{
    return mbIsQuarantineSuccess;
}
