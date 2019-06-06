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

     This file contains the program to transmission data using UDP protcol. The 
     device communicates by this UDP API should be in the same network.

  Version:
    
     2.0, 20190606

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
     Gary Xiao      , garyh0205@hotmail.com
 */
#include "UDP_API.h"


int udp_initial(pudp_config udp_config, int recv_port)
{

    int return_value;

    int timeout;

     udp_config -> sockVersion = MAKEWORD(2,2);

     if(WSAStartup(udp_config -> sockVersion, &udp_config -> wsaData) != 0)
         return 0;

    /* zero out the structure */
    memset((char *) &udp_config -> si_server, 0, sizeof(udp_config
           -> si_server));

    if (return_value = init_Packet_Queue( &udp_config -> pkt_Queue) != 
        pkt_Queue_SUCCESS)
        return return_value;

    if (return_value = init_Packet_Queue( &udp_config -> Received_Queue) != 
        pkt_Queue_SUCCESS)
        return return_value;

    /* create a send UDP socket */
    if ((udp_config -> send_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))
        == -1)
        return send_socket_error;

    /* create a recv UDP socket */
    if ((udp_config -> recv_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))
        == -1)
        return recv_socket_error;


    timeout = UDP_SELECT_TIMEOUT; // sec

    if (setsockopt(udp_config -> recv_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout
                 , sizeof(timeout)) == -1)
        return set_socketopt_error;

    udp_config -> si_server.sin_family = AF_INET;
    udp_config -> si_server.sin_port = htons(recv_port);
    udp_config -> si_server.sin_addr.s_addr = htonl(INADDR_ANY);

    udp_config -> shutdown = false;

    udp_config -> recv_port = recv_port;

    /* bind recv socket to the port */
    if( bind(udp_config -> recv_socket, (struct sockaddr *)&udp_config ->
             si_server, sizeof(udp_config -> si_server) ) == -1)
        return recv_socket_bind_error;

    /* The thread is used for receiving data */
    pthread_create(&udp_config -> udp_receive_thread, NULL,    
                   udp_recv_pkt_routine, (void*) udp_config);

    /* The thread is used for sending data */
    pthread_create(&udp_config -> udp_send_thread, NULL, udp_send_pkt_routine, 
                   (void*) udp_config);

    return 0;
}


int udp_addpkt(pudp_config udp_config, char *address, unsigned int port, 
               char *content, int size)
{

    if(size > MESSAGE_LENGTH)
        return addpkt_msg_oversize;

    addpkt(&udp_config -> pkt_Queue, address, port, content, size);

    return 0;
}


sPkt udp_getrecv(pudp_config udp_config)
{

    sPkt tmp = get_pkt(&udp_config -> Received_Queue);

    return tmp;
}


void *udp_send_pkt_routine(void *udpconfig)
{

    pudp_config udp_config = (pudp_config) udpconfig;

    sPkt current_send_pkt;

    struct sockaddr_in si_send;

    while(!(udp_config -> shutdown)){

        if(!(is_null( &udp_config -> pkt_Queue))){

            current_send_pkt = get_pkt(&udp_config -> pkt_Queue);

            memset(&si_send, 0, sizeof(si_send));
            si_send.sin_family = AF_INET;
            si_send.sin_port   = htons(current_send_pkt.port);
            si_send.sin_addr.s_addr   = inet_addr(current_send_pkt.address);

#ifdef debugging
            printf("Start Send pkts\n(sendto [%s] msg [", 
                                                      current_send_pkt.address);
            print_content(current_send_pkt.content,   
                                                 current_send_pkt.content_size);
            printf("])\n");
#endif

            if (sendto(udp_config -> send_socket, current_send_pkt.content, 
                current_send_pkt.content_size, 0,(struct sockaddr *)&si_send, 
                sizeof(struct sockaddr)) == -1){
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
        else{
            Sleep(SEND_THREAD_IDLE_SLEEP_TIME);
        }

    }

    return (void *)NULL;
}


void *udp_recv_pkt_routine(void *udpconfig)
{

    pudp_config udp_config = (pudp_config) udpconfig;

    int recv_len;

    char recv_buf[MESSAGE_LENGTH];

    char *address_ntoa_ptr;

    char address_ntoa[NETWORK_ADDR_LENGTH];

    int port;

    struct sockaddr_in si_recv;

    int socketaddr_len = sizeof(si_recv);


    /* keep listening for data */
    while(!(udp_config -> shutdown)){

        memset(&si_recv, 0, sizeof(si_recv));

        memset(&recv_buf, 0, sizeof(char) * MESSAGE_LENGTH);

        recv_len = 0;
#ifdef debugging
        printf("recv pkt.\n");
#endif
        /* try to receive some data, this is a non-blocking call */
        if ((recv_len = recvfrom(udp_config -> recv_socket, recv_buf,
             MESSAGE_LENGTH, 0, (struct sockaddr *) &si_recv, 
             (socklen_t *)&socketaddr_len)) == -1){
#ifdef debugging
            printf("No data received.\n");
#endif
            Sleep(SEND_THREAD_IDLE_SLEEP_TIME);
        }
        else if(recv_len > 0){

            memset(address_ntoa, 0, sizeof(address_ntoa));
        
            address_ntoa_ptr = inet_ntoa(si_recv.sin_addr);

            memcpy(address_ntoa, address_ntoa_ptr, strlen(address_ntoa_ptr) * 
                   sizeof(char));

            port = ntohs(si_recv.sin_port);

#ifdef debugging
            /* print details of the client/peer and the data received */
            printf("Received packet from %s:%d\n", address_ntoa, port);
            printf("Data: [");
            print_content(recv_buf, recv_len);
            printf("]\n");
            printf("Data Length %d\n", recv_len);
#endif
            addpkt(&udp_config -> Received_Queue, address_ntoa, port,
                   recv_buf, recv_len);
			
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


int udp_release(pudp_config udp_config)
{

    udp_config -> shutdown = true;

    pthread_join(udp_config -> udp_send_thread, NULL);

    pthread_join(udp_config -> udp_receive_thread, NULL);

    closesocket(udp_config -> send_socket);

    closesocket(udp_config -> recv_socket);

    WSACleanup();

    Free_Packet_Queue( &udp_config -> pkt_Queue);

    Free_Packet_Queue( &udp_config -> Received_Queue);

    return 0;
}