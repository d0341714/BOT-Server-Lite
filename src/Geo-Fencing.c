/*
  Copyright (c) 2016 Academia Sinica, Institute of Information Science

  License:

     GPL 3.0 : The content of this file is subject to the terms and cnditions
     defined in file 'COPYING.txt', which is part of this source code package.

  Project Name:

     BeDIS

  File Name:

     Geo-Fencing.c

  File Description:

     This file contains programs for detecting whether objects we tracked and
     restricted in the fence triggers Geo-Fence.

  Version:

     1.0, 20190407

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

     Gary Xiao     , garyh0205@hotmail.com

 */

#include "Geo-Fencing.h"


ErrorCode geo_fence_initial(pgeo_fence_config geo_fence_config,
                            int number_worker_threads, int recv_port,
                            int api_recv_port, int decision_threshold){

    geo_fence_config -> decision_threshold = decision_threshold;

    geo_fence_config -> is_running = true;

    geo_fence_config -> recv_port = recv_port;

    geo_fence_config -> api_recv_port = api_recv_port;

    geo_fence_config -> number_schedule_workers = number_worker_threads;

    geo_fence_config -> worker_thread = thpool_init(number_worker_threads);

    if(mp_init( &(geo_fence_config -> pkt_content_mempool),
               sizeof(spkt_content), SLOTS_IN_MEM_POOL) != MEMORY_POOL_SUCCESS)
        return E_MALLOC;

    if (udp_initial(&geo_fence_config -> udp_config, recv_port, api_recv_port)
        != WORK_SUCCESSFULLY)
        return E_WIFI_INIT_FAIL;

    return WORK_SUCCESSFULLY;

}


ErrorCode geo_fence_free(pgeo_fence_config geo_fence_config){

    geo_fence_config -> is_running = false;

    Sleep(WAITING_TIME);

    udp_release( &geo_fence_config -> udp_config);

    thpool_destroy(&geo_fence_config -> worker_thread);

    mp_destroy(&geo_fence_config -> pkt_content_mempool);
}


