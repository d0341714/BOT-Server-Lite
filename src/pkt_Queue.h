/*
  Copyright (c) 2016 Academia Sinica, Institute of Information Science

  License:

     GPL 3.0 : The content of this file is subject to the terms and conditions
     defined in file 'COPYING.txt', which is part of this source code package.

  Project Name:

     BeDIS

  File Name:

     pkt_Queue.h

  File Description:

     This file contains the header of function declarations used in pkt_Queue.c

  Version:

     2.0, 20190527

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

#ifndef pkt_Queue_H
#define pkt_Queue_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <windows.h>


/* When debugging is needed */
//#define debugging

/* Length of address of the network */
#define NETWORK_ADDR_LENGTH 16

/* Length of address of the network in Hex */
#define NETWORK_ADDR_LENGTH_HEX 8

/* Maximum length of message to be sent over WiFi in bytes */
#define MESSAGE_LENGTH 4096

//define the maximum length of pkt Queue.
#define MAX_QUEUE_LENGTH 512

enum {UNKNOWN, Data, Local_AT, UDP, NONE};

enum{ pkt_Queue_SUCCESS = 0, pkt_Queue_FULL = -1, queue_len_error = -2
    , pkt_Queue_is_free = -3, pkt_Queue_is_NULL = -4
    , pkt_Queue_display_over_range = -5, MESSAGE_OVERSIZE = -6};


/* packet format */
typedef struct pkt {

    // Brocast:     000000000000FFFF;
    // Coordinator: 0000000000000000
    unsigned char address[NETWORK_ADDR_LENGTH_HEX];

    //"Data" type
    unsigned int type;

    // Data
    char content[MESSAGE_LENGTH];

    int  content_size;

} sPkt;

typedef sPkt *pPkt;


typedef struct pkt_header {

    // front store the location of the first of thr Pkt Queue
    // rear  store the location of the end of the Pkt Queue
    int front;

    int rear;

    sPkt Queue[MAX_QUEUE_LENGTH];

    unsigned char address[NETWORK_ADDR_LENGTH_HEX];

    bool is_free;

    pthread_mutex_t mutex;

} spkt_ptr;

typedef spkt_ptr *pkt_ptr;


/* init_Packet_Queue

      Initialize the queue for storing pkts.

  Parameter:

      pkt_queue : The pointer points to the pkt queue.

  Return Value:

      int: If return 0, everything work successful.
           If not 0, Something wrong during init mutex.

 */
int init_Packet_Queue(pkt_ptr pkt_queue);


/*
  Free_Packet_Queue

      Release all the packets in the packet queue, the header and the tail of 
      the packet queue and release the struct stores the pointer of the packet 
      queue.

  Parameter:

      pkt_queue : The pointer points to the struct stores the first and the 
                  last of the packet queue.

  Return Value:

      int: If return 0, everything work successful.
           If not 0, Something wrong during delpkt or destroy mutex.
 */
int Free_Packet_Queue(pkt_ptr pkt_queue);


/*
  addpkt

      Add new packet into the packet queue. This function is only allow for the 
      data length shorter than the MESSAGE_LENGTH.

  Parameter:

      pkt_queue : The pointer points to the pkt queue we prepare to store the 
                  pkt.
      type      : Record the type of packets working environment.
      raw_addr  : The destnation address of the packet.
      content   : The content we decided to send.

  Return Value:

      int: If return 0, everything work successfully.
           If return pkt_Queue_FULL, the pkt is FULL.
           If not 0, Somthing Wrong.

 */
int addpkt(pkt_ptr pkt_queue, unsigned int type
         , char *raw_addr, char *content, int content_size);


/* get_pkt

      Get the first pkt of the pkt queue.

  Parameter:

      pkt_queue : The pointer points to the pkt queue we going to get 
                  a pkt from.

  Return Value:

      sPkt : return the content of the first pkt.

 */
sPkt get_pkt(pkt_ptr pkt_queue);


/*
  delpkt

      Delete the first of the packet queue.

  Parameter:

      pkt_queue: The pointer points to the pkt queue.

  Return Value:

      int: If return 0, work successfully.

 */
int delpkt(pkt_ptr pkt_queue);


/*
  type_to_str

      TO convert type to it's original type name.

  Parameter:

      type: The integer stores the type of the packet.

  Return Value:

      Return a pointer with type char which is the name of the type.

 */
char *type_to_str(int type);


/*
  str_to_type

      TO convert the name of the type to the num of the type.

  Parameter:

      conType: A string to tell the type of the connection.

  Return Value:

      Return a int which is the name of the type.

 */
int str_to_type(const char *conType);


/*
  char_to_hex

      Convert array from char to hex.

  Parameter:

      raw    : The original char type array.
      result : The variable to store the converted result(hex).
      size   : The size of the raw array. (result size must the half of the raw 
               size.)

  Return Value:

      None

 */
void char_to_hex(char *raw, unsigned char *raw_hex, int size);


/*
  hex_to_char

      Convert hex to char.

  Parameter:

      hex  : An array stores in Hex.
      size : The size of the hex length.
      buf  : An output char array to store the output converted from the 
             input hex array

  Return Value:

      0: success

 */
int hex_to_char(unsigned char *hex, int size, char * buf);


/* display_pkt

      Display the packet we decide to see.

  Parameter:

      display_title : The title we want to show in front of the packet content.
      pkt     : The packet we want to see it's content.
      pkt_num : Choose whitch pkts we want to display.

  Return Value:

      int

 */
int display_pkt(char *display_title, pkt_ptr pkt_queue, int pkt_num);


/*
  array_copy

      Copy the src array to the destination array.

  Parameter:

      src  : The src array we copy from.
      dest : The dest array we copy to.
      size : The size of the src array. (Must the same as the dest array)

  Return Value:

      None

 */
void array_copy(unsigned char *src, unsigned char *dest, int size);


/*
  address_compare

      Compare the address whether is the same.

  Parameter:

      addr1: the address we want to compare.
      addr2: the address we want to compare.

  Return Value:

      bool: if true, the same.

 */
bool address_compare(unsigned char *addr1,unsigned char *addr2);


/*
  is_null

      check if pkt_Queue is null.

  Parameter:

      pkt_queue : The pointer points to the pkt queue.

  Return Value:

      bool : true if null.

 */
bool is_null(pkt_ptr pkt_queue);


/*
  is_null

      check if pkt_Queue is full.

  Parameter:

      pkt_queue : The pointer points to the pkt queue.

  Return Value:

      bool : true if full.

 */
bool is_full(pkt_ptr pkt_queue);


/*
  queue-len

      Count the length of the queue.

  Parameter:

      pkt_queue : The queue we want to count.

  Return Value:

      int : The length of the Queue.

 */
int queue_len(pkt_ptr pkt_queue);


/*

  print_content

      print the content.

  Parameter:

      content : The pointer of content we desire to read.
      size    : The size of the content.

  Return Value:

      NONE

 */
void print_content(char *content, int size);

#endif
