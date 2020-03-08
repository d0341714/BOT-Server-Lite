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
	const char* username = "m107gfhfghgfh17036";   // 帳號  
	const char* password = "csieeb210"; 	// 密碼  
	const char* mobile = "0955620891"; 	// 電話  
	const char* message = "eb210簡訊測試"; 	// 簡訊內容  

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