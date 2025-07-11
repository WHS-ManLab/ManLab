#pragma once

#include <string>
#include <vector>
#include <optional>
#include <sys/types.h>

class UserNotifier
{
public:
    struct ActiveUser 
    {
        uid_t uid;              // UID of the session owner
        gid_t gid;              // GID (used for permission drop)
        std::string runtimeDir; // e.g., /run/user/<uid>, 사용자별 DBus 세션 버스 주소를 환경 변수로 설정하기 위해
    };

    //seat0에 연결된 모든 ‘활성 세션’의 사용자 목록을 반환
    static std::vector<ActiveUser> GetAllActiveUsers();

    //@brief seat0의 모든 활성 사용자에게 일반 GUI 알림을 전송합니다.
    static int NotifyAll(const std::string& summary,
                         const std::string& body);

    //eat0의 모든 활성 사용자에게 긴급 GUI 알림을 전송합니다. (-u critical)
    static int NotifyAllUrgent(const std::string& summary,
                               const std::string& body);
};