/*
  Copyright (c) 2016 Academia Sinica, Institute of Information Science

  License:

     GPL 3.0 : The content of this file is subject to the terms and conditions
     defined in file 'COPYING.txt', which is part of this source code package.

  Project Name:

     BeDIS

  File Name:

     Notify.c

  File Description:

     This file contains programs to transmit and receive data to and from
     gateways through Wi-Fi network, and programs executed by network setup and
     initialization, beacon health monitor and comminication unit.

  Version:

     1.0, 20200205

  Abstract:

     BeDIS uses LBeacons to deliver 3D coordinates and textual descriptions of
     their locations to users' devices. Basically, a LBeacon is an inexpensive,
     Bluetooth Smart Ready device. The 3D coordinates and location description
     of every LBeacon are retrieved from BeDIS (Building/environment Data and
     Information System) and stored locally during deployment and maintenance
     times. Once initialized, each LBeacon broadcasts its coordinates and
     location description to Bluetooth enabled user devices within its coverage
     area.

  Authors:

     Chun-Yu Lai    , chunyu1202@gmail.com
 */

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
	const char* message = "eb210蝪∟?皜祈岫"; 	// 蝪∟??批捆  

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
