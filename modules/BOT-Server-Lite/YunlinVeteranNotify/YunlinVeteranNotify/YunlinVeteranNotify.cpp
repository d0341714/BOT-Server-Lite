// YunlinVeteranNotify.cpp : 此檔案包含 'main' 函式。程式會於該處開始執行及結束執行。
//

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <winsock2.h>
#include <windows.h>
#include <string.h>
#include <time.h>
#include <winbase.h>

#include <ws2tcpip.h>

#define SERV_PORT 8000
#define MAX_SEND_QUEUE_LEN 255
#define MAX_RET_MSG_LEN 128
#define INTERFACE_TYPE 0
#define _CRT_SECURE_NO_WARNINGS  

#pragma comment(lib, "ws2_32.lib")


#define bzero(b,len) (memset((b), '\0', (len)), (void) 0)

int main()
{
	const char* username = "m10717036";   // 撣唾?  
	const char* password = "csieeb210"; 	// 撖Ⅳ  
	const char* mobile = "0955620891"; 	// ?餉店  
	const char* message = "eb210簡訊測試"; 	// 蝪∟??批捆  

	WSADATA wsaData = { 0, };

	int sockfd;
	int len = 0;
	const char* host = "api.twsms.com";
	char msg[512], MSGData[512], buf[512];
	char* res, * checkRes;
	struct sockaddr_in address;
	struct hostent* hostinfo;

	WSAStartup(MAKEWORD(2, 2), &wsaData);

	bzero(&address, sizeof(address));
	hostinfo = gethostbyname(host);
	if (!hostinfo) {
		fprintf(stderr, "no host: %s\n", host);
		exit(1);
	}
	address.sin_family = AF_INET;
	address.sin_port = htons(80);
	address.sin_addr = *(struct in_addr*) * hostinfo->h_addr_list;

	/* Create socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	/* Connect to server */
	if (connect(sockfd, (struct sockaddr*) & address, sizeof(address)) == -1) {
		perror("connect faild!\n");
		exit(1);
	}

	/* Request string */
	len = snprintf(msg, 512,
		"username=%s&password=%s&mobile=%s&message=%s", username, password, mobile, message);

	/* HTTP request content */
	snprintf(MSGData, 512,
		"POST /json/sms_send.php HTTP/1.1\r\n"
		"Host: %s\r\n"
		"Content-Length: %d\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n"
		"Connection: Close\r\n\r\n"
		"%s\r\n", host, len, msg);

	/* Send message */
	send(sockfd, MSGData, strlen(MSGData), 0);

	/* Response message */
	recv(sockfd, buf, 512, 0);

	//for (res = strtok(buf, "\n"); printf(res);

	//	close(sockfd))
	return 0;
}
// 執行程式: Ctrl + F5 或 [偵錯] > [啟動但不偵錯] 功能表
// 偵錯程式: F5 或 [偵錯] > [啟動偵錯] 功能表

// 開始使用的提示: 
//   1. 使用 [方案總管] 視窗，新增/管理檔案
//   2. 使用 [Team Explorer] 視窗，連線到原始檔控制
//   3. 使用 [輸出] 視窗，參閱組建輸出與其他訊息
//   4. 使用 [錯誤清單] 視窗，檢視錯誤
//   5. 前往 [專案] > [新增項目]，建立新的程式碼檔案，或是前往 [專案] > [新增現有項目]，將現有程式碼檔案新增至專案
//   6. 之後要再次開啟此專案時，請前往 [檔案] > [開啟] > [專案]，然後選取 .sln 檔案
