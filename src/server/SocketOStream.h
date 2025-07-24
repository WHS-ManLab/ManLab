#pragma once
#include <streambuf>
#include <ostream>
#include <unistd.h>
#include <errno.h>
#include <iostream>
	
// std::streambuf를 상속해 소켓 FD를 대상으로 하는 버퍼 구현
// 총 3개의 함수를 오버라이드
class SocketStreamBuf : public std::streambuf {
public:
    // 파일 디스크립터 초기화
    explicit SocketStreamBuf(int fd) : mFd(fd) {}

protected:
    // 단일 문자 출력
    int overflow(int ch) override 
    {
        if (ch == EOF) 
        {
            return 0;
        }
        unsigned char c = static_cast<unsigned char>(ch);
        return full_write(reinterpret_cast<const char*>(&c), 1) == 1 ? ch : EOF;
    }

    // 버퍼(다건) 출력
    // 요청 길이만큼 full_write 수행
    std::streamsize xsputn(const char* s, std::streamsize n) override 
    {
        return full_write(s, n);
    }

    // flush(std::flush)·ostream 파괴 시 호출
    // 소켓은 별도 flush 불필요
    int sync() override 
    { 
        return 0; // 0 = 성공
    }   

private:
    // write() 시스템 콜 끝까지 성공할 때까지 전송
    ssize_t full_write(const char* buf, std::streamsize n) 
    {
        std::streamsize sent = 0;
        // 루프 : 전송이 완료될 때까지 반복
        while (sent < n) 
        {
            ssize_t r = ::write(mFd, buf + sent, n - sent);
            if (r > 0) 
            {
                sent += r;  //누적 바이트 증가
            } 
            else if (r == -1 && errno == EINTR) 
            {
                continue;   //시스템 콜 재시도
            }
            else 
            { 
                perror("write"); // 이외 오류 처리
                return -1; 
            }
        }
        return sent;
    }

    int mFd;
};

class SocketOStream : public std::ostream 
{
public:
    explicit SocketOStream(int fd) : std::ostream(&mBuf), mBuf(fd) {}
private:
    SocketStreamBuf mBuf;
};
