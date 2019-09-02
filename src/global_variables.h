/*
  Copyright (c) 2016 Academia Sinica, Institute of Information Science

  License:

     GPL 3.0 : The content of this file is subject to the terms and cnditions
     defined in file 'COPYING.txt', which is part of this source code package.

  Project Name:

     BeDIS

  File Name:

     global_variables.h

  File Description:

     This file include the declarations and definitions of global variables 
     and definition used in BeDIS Server but not commonly used in Gateway and 
     LBeacon.

  Version:

     2.0, 20190830

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

     Jia Ying Shi  , littlestone1225@yahoo.com.tw
     Chun-Yu Lai, chunyu1202@gmail.com
 */

#ifndef GLOBAL_VARIABLE_H
#define GLOBAL_VARIABLE_H

/* Maximum length of message to communicate with SQL wrapper API in bytes */
#define SQL_TEMP_BUFFER_LENGTH 4096

#endif