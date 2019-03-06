/*
  Copyright (c) 2016 Academia Sinica, Institute of Information Science

  License:

     GPL 3.0 : The content of this file is subject to the terms and conditions
     defined in file 'COPYING.txt', which is part of this source code package.

  Project Name:

     BeDIS

  File Name:

     UDP_API.c

  File Description:

     This file contains the program to connect to Wi-Fi and in the project, we
     use it for data transmission most.

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
     Gary Xiao		, garyh0205@hotmail.com
 */
#include "UDP_API.h"


int udp_initial(pudp_config udp_config, int send_port, int recv_port){

    int ret;

	int timeout;

     udp_config -> sockVersion = MAKEWORD(2,2);

	 if(WSAStartup(udp_config -> sockVersion, &udp_config -> wsaData) != 0)
     {
         return 0;
     }

    // zero out the structure
    memset((char *) &udp_config -> si_server, 0, sizeof(udp_config
            -> si_server));

    ret = init_Packet_Queue( &udp_config -> pkt_Queue);

    if(ret != pkt_Queue_SUCCESS)
        return ret;

    ret = init_Packet_Queue( &udp_config -> Received_Queue);

    if(ret != pkt_Queue_SUCCESS)
        return ret;

    //create a send UDP socket
    if ((udp_config -> send_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))
         == -1)
        return send_socket_error;

    //create a recv UDP socket
    if ((udp_config -> recv_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))
         == -1)
        return recv_socket_error;


    timeout = UDP_SELECT_TIMEOUT; //毫秒

    if (setsockopt(udp_config -> recv_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout
                 , sizeof(timeout)) == -1)
        return set_socketopt_error;

    udp_config -> si_server.sin_family = AF_INET;
    udp_config -> si_server.sin_port = htons(recv_port);
    udp_config -> si_server.sin_addr.s_addr = htonl(INADDR_ANY);

    udp_config -> shutdown = false;

    udp_config -> send_port = send_port;
    udp_config -> recv_port = recv_port;

    //bind recv socket to port
    if( bind(udp_config -> recv_socket, (struct sockaddr *)&udp_config ->
             si_server, sizeof(udp_config -> si_server) ) == -1)
        return recv_socket_bind_error;

    pthread_create(&udp_config -> udp_receive, NULL, udp_recv_pkt, (void*)
                   udp_config);

    pthread_create(&udp_config -> udp_send, NULL, udp_send_pkt, (void*)
                   udp_config);

    return 0;
}

int udp_addpkt(pudp_config udp_config, char *raw_addr, char *content, int size){

	char *removed_address;

    if(size > MESSAGE_LENGTH)
        return addpkt_msg_oversize;

    removed_address = udp_address_reduce_point(raw_addr);

    addpkt(&udp_config -> pkt_Queue, UDP, removed_address, content, size);

    return 0;
}


sPkt udp_getrecv(pudp_config udp_config){

    sPkt tmp = get_pkt(&udp_config -> Received_Queue);

    return tmp;
}


void *udp_send_pkt(void *udpconfig){

    pudp_config udp_config = (pudp_config) udpconfig;

	sPkt current_send_pkt;

    struct sockaddr_in si_send;

    while(!(udp_config -> shutdown)){

        if(!(is_null( &udp_config -> pkt_Queue))){

            current_send_pkt = get_pkt(&udp_config -> pkt_Queue);

            if (current_send_pkt.type == UDP){
                char *dest_address = udp_hex_to_address(
                                                      current_send_pkt.address);

                memset(&si_send, 0, sizeof(si_send));
                si_send.sin_family = AF_INET;
                si_send.sin_port   = htons(udp_config -> send_port);
				si_send.sin_addr.s_addr   = inet_addr(dest_address);

#ifdef debugging
				printf("Entrer Send pkts\n(sendto [%s] msg [%s])\n", dest_address, current_send_pkt.content);
#endif

                if (sendto(udp_config -> send_socket, current_send_pkt.content
                  , current_send_pkt.content_size, 0,(struct sockaddr *)&si_send
                  , sizeof(struct sockaddr)) == -1){
#ifdef debugging
                      printf("sendto error.[%s]\n", strerror(errno));
#endif
                 }
				else{
#ifdef debugging
					printf("Send pkt success\n");
#endif
				}
			}
        }
        else{
            Sleep(SEND_NULL_SLEEP);
        }

    }

    return (void *)NULL;
}

void *udp_recv_pkt(void *udpconfig){

    pudp_config udp_config = (pudp_config) udpconfig;

    int recv_len;

    char recv_buf[MESSAGE_LENGTH];

	char *addr_tmp;

    struct sockaddr_in si_recv;

    int socketaddr_len = sizeof(si_recv);

    //keep listening for data
    while(!(udp_config -> shutdown)){

        memset(&si_recv, 0, sizeof(si_recv));

        memset(&recv_buf, 0, sizeof(char) * MESSAGE_LENGTH);

        recv_len = 0;
#ifdef debugging
        printf("recv pkt.\n");
#endif
        //try to receive some data, this is a non-blocking call
        if ((recv_len = recvfrom(udp_config -> recv_socket, recv_buf,
             MESSAGE_LENGTH, 0, (struct sockaddr *) &si_recv
                                    , (socklen_t *)&socketaddr_len)) == -1){
#ifdef debugging
            printf("error recv_len %d\n", recv_len);

            printf("recvfrom error.\n");
#endif
        }
        else if(recv_len > 0){

			addr_tmp = inet_ntoa(si_recv.sin_addr);
#ifdef debugging
            //print details of the client/peer and the data received
            printf("Received packet from %s:%d\n", inet_ntoa(si_recv.sin_addr),
                                                   ntohs(si_recv.sin_port));
            printf("Data: %s\n" , recv_buf);
            printf("Data Length %d\n", recv_len);
#endif
            addpkt(&udp_config -> Received_Queue, UDP
                 , udp_address_reduce_point(addr_tmp)
                 , recv_buf, recv_len);
        }
#ifdef debugging
        else
            printf("else recvfrom error.\n");
#endif
    }
#ifdef debugging
    printf("Exit Receive.\n");
#endif
    return (void *)NULL;
}

int udp_release(pudp_config udp_config){

    pthread_join(udp_config -> udp_send, NULL);

    pthread_join(udp_config -> udp_receive, NULL);

    closesocket(udp_config -> send_socket);

    closesocket(udp_config -> recv_socket);

	WSACleanup();

    Free_Packet_Queue( &udp_config -> pkt_Queue);

    Free_Packet_Queue( &udp_config -> Received_Queue);

    return 0;
}


char *udp_address_reduce_point(char *raw_addr){

    //Record current filled Address Location.
    int address_loc = 0;

	//in each part, at most 3 number.
    int count = 0;
    unsigned char tmp[3];

	int lo, n;

	char *address = malloc(sizeof(char) * NETWORK_ADDR_LENGTH);

#ifdef debugging
	printf("Enter udp_address_reduce_point address [%s]\n", raw_addr);
#endif 

	memset(address, 0, NETWORK_ADDR_LENGTH);

    // Four part in a address.(devided by '.')
    for(n = 0; n < 4; n++){

		count = 0;

        memset(&tmp, 0, sizeof(char) * 3);

        while(count != 3){

            //When read '.' from address_loc means the end of this part.
            if (raw_addr[address_loc] == '.'){

                address_loc ++;
                break;

            }
            else{
                tmp[count] = raw_addr[address_loc];

                count ++;
                address_loc ++;

                if(address_loc >= strlen(raw_addr))
                    break;
            }
        }

        for(lo = 0; lo < 3;lo ++)

            if ((3 - count) > lo )
                address[n * 3 + lo] = '0';
            else
                address[n * 3 + lo] = tmp[lo - (3 - count)];

        if (count == 3)
            address_loc ++;
    }
#ifdef debugging
	printf("Result of udp_address_reduce_point address [%s]\n", address);
#endif 
    return address;
}


char *udp_hex_to_address(unsigned char *hex_addr){

    // Stored a recovered address.
    char *dest_address;
    char *tmp_address = hex_to_char(hex_addr, 6);
    int address_loc = 0;
	int loc, n;
	dest_address = malloc(sizeof(char) * 17);
    memset(dest_address, 0, sizeof(char) * 17);

    for(n=0;n < 4;n ++){

        bool no_zero = false;
        for(loc=0;loc < 3;loc ++){

            if(tmp_address[n * 3 + loc]== '0' && no_zero == false &&
               loc != 2)
                continue;
            no_zero = true;
            dest_address[address_loc] = tmp_address[n * 3 + loc];
            address_loc ++;

        }

        if(n < 3){

            dest_address[address_loc] = '.';
            address_loc ++;

        }
    }
    return dest_address;
}
