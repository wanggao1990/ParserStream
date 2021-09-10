#include "NetServer.h"

#include <stdio.h>

#ifndef _WIN32

#include <unistd.h>
#include <cstdlib>
#include <cstring>

#else

#include <winsock2.h>
#include <ws2tcpip.h>
#include <wspiapi.h>

#pragma comment(lib, "ws2_32.lib") 

class WSAInit
{
public:
    WSAInit(){
        WSADATA wsaData;
        WORD sockVersion = MAKEWORD(2, 2);

        if(WSAStartup(sockVersion, &wsaData) != 0) {
            fprintf(stderr, "WSAStartup failed.");
        }
    }
    ~WSAInit() {
        WSACleanup();
    }
};
WSAInit wsaInit;

#endif


NetServer::NetServer()
  : fd_(-1), 
    isRunning_(false), 
    cb_(nullptr)
{
}


NetServer::~NetServer()
{
    Cleanup();
}

bool NetServer::Init(const std::string& ip, int port, bool tcp)
{
    std::string portStr = std::to_string(port);

    isTcp_ = tcp;

    // check addrinfo
    addrinfo *local = nullptr;
    addrinfo hints;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if(0 != getaddrinfo(NULL, portStr.c_str(), &hints, &local)) {
        printf("First try, incorrect network address.");
        freeaddrinfo(local);
        return false;
    }

    // create socket and bind
    if(isTcp_) {
        fd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    }
    else {
        fd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    }
    if(fd_ < 0) {
        printf("socket error !");
        return false;
    }

    int optval = 100;
    if(setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, (char *)&optval, sizeof(optval)) < 0) {
        printf("set socket SO_RCVTIMEO option failed!");
    }

    optval = 1;
    if(setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval)) < 0) {
        printf("set socket SO_REUSEADDR option failed!");
    }

  
    sockaddr_in serAddr;
    serAddr.sin_family = AF_INET;
    serAddr.sin_port = htons(port);
    serAddr.sin_addr.S_un.S_addr = inet_addr(ip.c_str());

    if(bind(fd_, (sockaddr *)&serAddr, sizeof(serAddr)) == SOCKET_ERROR) {
        //printf("bind error !");
        perror("bind error");
        closesocket(fd_);
        return false;
    }

    return true;
}

bool NetServer::Start()
{
    if(isRunning_) {
        printf("Already running.\n");
        return false;
    }

    isRunning_ = true;
    readThread_ = std::thread(&NetServer::ReadThreadFunc, this); 
    return true;
}

void NetServer::Stop()
{
    if(!isRunning_) return;

    isRunning_ = false;
    readThread_.join();
}

void NetServer::Cleanup()
{
    Stop();
    UnInit();
}

void NetServer::UnInit()
{
    if(fd_ > 0) {
        closesocket(fd_);
        fd_ = -1;
    }
}

void NetServer::ReadThreadFunc()
{
    const int RECV_BUFFER_MAX_LENGTH = 128000;
    int recvLen = 0;
    char *recvBuffer = new char[RECV_BUFFER_MAX_LENGTH];

    // TODO: 避免多个客户推送流
    // 当有一个客户连接上之后，不再处理其他客户的消息; 一段时间无数据，认为客户断开。

    //sockaddr_in clientAddr;
    //int addrLen = sizeof(clientAddr);

    if(isTcp_) {

        listen(fd_, 128);

        while(isRunning_) {

            int cli_fd = accept(fd_, NULL, NULL);


            while(isRunning_) {

                //recvLen = recvfrom(fd_, recvBuffer, RECV_BUFFER_MAX_LENGTH, 0,(sockaddr *)&remoteAddr, &addrLen);
                recvLen = recv(cli_fd, recvBuffer, RECV_BUFFER_MAX_LENGTH, 0);
                if(recvLen < 0) {
                    if(errno == EAGAIN || errno == NOERROR) {
                        Sleep(10);
                        continue;
                    }
                    printf("recv error\n");
                    return;
                }
                else if(recvLen == 0) {
                    break;
                }
                //printf("\rrecv buf len %d  ", recvLen);

                if(cb_)
                    cb_(reinterpret_cast<uint8_t*>(recvBuffer), recvLen);
            }

            closesocket(cli_fd);
        }
    }
    else {
        while(isRunning_) 
        {
            //recvLen = recvfrom(fd_, recvBuffer, RECV_BUFFER_MAX_LENGTH, 0,(sockaddr *)&remoteAddr, &addrLen);
            recvLen = recv(fd_, recvBuffer, RECV_BUFFER_MAX_LENGTH, 0);
            if(recvLen <= 0) {
                continue;
            }
            //printf("\rrecv buf len %d  ", recvLen);
            
            if(cb_) 
                cb_(reinterpret_cast<uint8_t*>(recvBuffer), recvLen);
        }
    }

    delete recvBuffer;
}
