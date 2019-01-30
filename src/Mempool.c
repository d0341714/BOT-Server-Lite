/*
  Copyright (c) 2016 Academia Sinica, Institute of Information Science

  License:

     GPL 3.0 : The content of this file is subject to the terms and conditions
     defined in file 'COPYING.txt', which is part of this source code package.

  Project Name:

     BeDIS

  File Name:

     Mempool.c

  File Description:

     This file contains the program to allow memory allocation
     for structs of identical size.

     Note: The code is referred to the site:
     https://codereview.stackexchange.com/questions/48919/simple-memory-pool-
     %20using-no-extra-memory

  Version:

     2.0, 20190119

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

     Holly Wang, hollywang@iis.sinica.edu.tw

*/

#include "Mempool.h"


size_t get_current_size_mempool(Memory_Pool *mp){

    pthread_mutex_lock(&mp->mem_lock);

    size_t mem_size = mp->alloc_time * mp->size * mp->slots;

    pthread_mutex_unlock(&mp->mem_lock);

    return mem_size;
}


int mp_init(Memory_Pool *mp, size_t size, size_t slots){

    pthread_mutex_init( &mp->mem_lock, 0);

    /* allocate memory */
    if((mp->memory[0] = malloc(size * slots)) == NULL)
        return MEMORY_POOL_ERROR;

    /* initialize and set parameters */
    mp->head = NULL;
    mp->size = size;
    mp->slots = slots;
    mp->alloc_time = 1;

    /* add every slot to the free list */
    char *end = (char *)mp->memory[0] + size * slots;

    for(char *ite = mp->memory[0]; ite < end; ite += size){

        /* store first address */
        void *temp = mp->head;

        /* link the new node */
        mp->head = (void *)ite;

        /* link to the list from new node */
        *mp->head = temp;
    }

    return MEMORY_POOL_SUCCESS;
}


int mp_expand(Memory_Pool *mp, size_t slots){

    int alloc_count;

    alloc_count = mp->alloc_time;

    if(alloc_count == MAX_EXP_TIME)
        return MEMORY_POOL_ERROR;

    mp->memory[alloc_count] = malloc(mp->size * slots);
    if(mp->memory[alloc_count] == NULL )
        return MEMORY_POOL_ERROR;

    /* add every slot to the free list */
    char *end = (char *) mp->memory[alloc_count] + mp->size * slots;

    for(char *ite = mp->memory[alloc_count]; ite < end; ite += mp->size){

        /* store first address */
        void *temp = mp->head;

        /* link the new node */
        mp->head = (void *)ite;
        /* link to the list from new node */
        *mp->head = temp;

    }

    mp->alloc_time = mp->alloc_time + 1;


    return MEMORY_POOL_SUCCESS;
}

void mp_destroy(Memory_Pool *mp){

    pthread_mutex_lock( &mp->mem_lock);

    for(int i = 0; i < MAX_EXP_TIME; i++){

        mp->memory[i] = NULL;
        free(mp->memory[i]);
    }

    pthread_mutex_unlock( &mp->mem_lock);

    pthread_mutex_destroy( &mp->mem_lock);

}


void *mp_alloc(Memory_Pool *mp){

    pthread_mutex_lock(&mp->mem_lock);

    if( *mp->head == NULL){

        /* If the next position which mp->head is pointing to is NULL,
           expand the memory pool. */
      if(mp_expand(mp, mp->slots) == MEMORY_POOL_ERROR){

          pthread_mutex_unlock(&mp->mem_lock);
          return NULL;
      }
    }

    /* store first address, i.e., address of the start of first element */
    void *temp = mp->head;

    /* link one past it */
    mp->head = *mp->head;

    pthread_mutex_unlock( &mp->mem_lock);

    /* return the first address */
    return temp;
}



int mp_free(Memory_Pool *mp, void *mem){

    int closest = -1;

    pthread_mutex_lock(&mp->mem_lock);

    /* Check all the expanded memory space, to find the closest and
    most relevant mem_head for the current freeing memory. */
    for(int i = 0; i < mp->alloc_time; i++){

        /* Calculate the offset from mem to mp->memory */
        int differenceinbyte = (mem - mp->memory[i]) * sizeof(mem);
        /* Only consider the positive offset */
        if((differenceinbyte > 0) && ((differenceinbyte < closest) ||
           (closest == -1)))
            closest = differenceinbyte;
    }
    /* check if mem is correct, i.e. is pointing to the struct of a slot */
    if((closest % mp->size) != 0){
        pthread_mutex_unlock(&mp->mem_lock);
        return MEMORY_POOL_ERROR;
    }

    /* store first address */
    void *temp = mp->head;
    /* link new node */
    mp->head = mem;
    /* link to the list from new node */
    *mp->head = temp;

    pthread_mutex_unlock(&mp->mem_lock);

    return MEMORY_POOL_SUCCESS;
}
