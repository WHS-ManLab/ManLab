#include "UserNotifier.h"
#include <systemd/sd-login.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pwd.h>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>

using namespace std;

// NULL-terminated string array 해제용 함수.
// 세션 감지할 유저가 여러 명이므로 for문으로 처리
static void free_strv(char** strv)
{
    if (!strv)
    {
        return;
    }
    for (char** s = strv; *s; ++s)
    {
        free(*s);   //각 문자열 해제
    }
    free(strv); // 전체 포인터 배열 해제
}

// 현재 로그인된 모든 seat0유저의 uid, gid, runtimeDir을 반환
std::vector<UserNotifier::ActiveUser> UserNotifier::GetAllActiveUsers()
{
    std::vector<ActiveUser> users;

    // systemd 세션 리스트 가져오기
    char** sessions = nullptr;
    if (sd_get_sessions(&sessions) < 0 || !sessions)
    {
        return users;
    }

    // 세션 배열 순회
    for (int i = 0; sessions[i] != nullptr; ++i)
    {
        const char* session = sessions[i];

        // seat0에서 활성화된 세션만 필터링
        char* seat = nullptr;
        if (sd_session_get_seat(session, &seat) < 0 || !seat) 
        {
            continue;
        }
        if (strcmp(seat, "seat0") != 0) 
        {
            free(seat);
            continue;
        }
        free(seat);

        if (sd_session_is_active(session) <= 0) 
        { 
            continue;
        }

        uid_t uid;
        if (sd_session_get_uid(session, &uid) < 0) 
        {
            continue;
        }

        struct passwd* pw = getpwuid(uid);
        if (!pw) 
        {
            continue;
        }

        // 해당 유저가 DBus 세션을 가지고 있는지 확인.
        std::string runtimeDir = "/run/user/" + std::to_string(uid);
        if (!std::filesystem::is_directory(runtimeDir)) 
        {
            continue;
        }

        users.push_back(ActiveUser{ uid, pw->pw_gid, runtimeDir });
    }

    free_strv(sessions);    
    return users;
}

static bool sendToUser(const UserNotifier::ActiveUser& user,
                       const std::string& summary,
                       const std::string& body,
                       bool urgent)
{
    pid_t pid = fork();
    if (pid < 0) 
    {   
        return false;
    }

    if (pid == 0)
    {
        // 자식 프로세스: 권한 강하, 자식 프로세스에서 실제 알림 전송 작업 수행
        if (setgid(user.gid) < 0 || setuid(user.uid) < 0)
        {
            _exit(1);
        }

        //해당 유저의 D-Bus 세션 주소를 환경 변수로 설정.
        std::string dbusAddr = "unix:path=" + user.runtimeDir + "/bus";
        setenv("DBUS_SESSION_BUS_ADDRESS", dbusAddr.c_str(), 1);

        // DISPLAY 설정 (Wayland/X11 호환)
        char* disp = nullptr;
        if (sd_session_get_display(nullptr, &disp) == 0)
        {
            setenv("DISPLAY", disp, 1);
        }
        else
        {
            setenv("DISPLAY", ":0", 0);
        }

        if (disp) free(disp);

        // 알림 실행. -u는 긴급도. -a는 애플리케이션 이름.
        if (urgent) 
        {
            execlp("notify-send", "notify-send",
                   summary.c_str(), body.c_str(),
                   "-u", "critical",
                   "-a", "ManLab",
                   nullptr);
        } 
        else 
        {
            execlp("notify-send", "notify-send",
                   summary.c_str(), body.c_str(),
                   "-u", "normal",
                   "-a", "ManLab",
                   nullptr);
        }

        _exit(1); // 실패 시
    }

    // 부모: 자식 종료 상태 확인
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

// 모든 활성 사용자에게 일반 등급 알림 전송.
int UserNotifier::NotifyAll(const std::string& summary,
                            const std::string& body)
{
    auto users = GetAllActiveUsers();
    int successCount = 0;

    for (const auto& user : users) 
    {
        if (sendToUser(user, summary, body, false))
            ++successCount;
    }

    return successCount;
}

// 모든 활성 사용자에게 긴급 등급 알림 전송.
int UserNotifier::NotifyAllUrgent(const std::string& summary,
                                  const std::string& body)
{
    auto users = GetAllActiveUsers();
    int successCount = 0;

    for (const auto& user : users) 
    {
        if (sendToUser(user, summary, body, true))
            ++successCount;
    }

    return successCount;
}
