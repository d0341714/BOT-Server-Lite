#include <WinSock2.h>
#include <windows.h>
#include "SqlWrapper.h"

#pragma comment(lib, "ws2_32.lib")

#define LISTEN_PORT 9999
#define tracked_object_data 4
#define UDP_RECEIVE_TIMEOUT_IN_MILLISECONDS 5000

int main(int argc, char* argv[])
{
    WSADATA wsaData;
    WORD sockVersion = MAKEWORD(2,2);
    SOCKET serSocket;
    struct sockaddr_in serAddr;
    struct sockaddr_in remoteAddr;
    int nAddrLen;
    char recvData[WIFI_MESSAGE_LENGTH];
    int ret;
    struct timeval timeout;
    int optval = 1;

    char *conninfo = "dbname=botdb user=postgres password=bedis402";
    void *db;

    int poll_type;

    if(WSAStartup(sockVersion, &wsaData) != 0){
         return 0;
     }

    serSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(serSocket == INVALID_SOCKET){
        printf("socket error !");
        return 0;
    }

    optval = 1;
    setsockopt(serSocket, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval,
               sizeof(int));

    timeout.tv_sec = UDP_RECEIVE_TIMEOUT_IN_MILLISECONDS;
    setsockopt(serSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    memset(&serAddr, 0, sizeof(serAddr));

    serAddr.sin_family = AF_INET;
    serAddr.sin_port = htons(LISTEN_PORT);
    serAddr.sin_addr.S_un.S_addr = INADDR_ANY;
    if(bind(serSocket, (SOCKADDR*)&serAddr, sizeof(serAddr)) == SOCKET_ERROR){
        printf("bind error !");
        closesocket(serSocket);
        return 0;
    }

    SQL_open_database_connection(conninfo, &db);

    while (1){
        nAddrLen = sizeof(remoteAddr);
        ret = recvfrom(serSocket, recvData, sizeof(recvData), 0,
                       (SOCKADDR*) &remoteAddr, &nAddrLen);
        if (ret < 0){
            printf("recvfrom last error code: [%d]\n", WSAGetLastError());
        }else{
            poll_type = recvData[0];
            if(recvData[0] == tracked_object_data){
                SQL_update_object_tracking_data(db, &recvData[1],
                                                strlen(&recvData[1]));
            }
        }
    }

    closesocket(serSocket);
    WSACleanup();

    SQL_close_database_connection(db);

    return 0;
}
