/*
  Copyright (c) 2016 Academia Sinica, Institute of Information Science

  License:

     GPL 3.0 : The content of this file is subject to the terms and conditions
     defined in file 'COPYING.txt', which is part of this source code package.

  Project Name:

     BeDIS

  File Name:

     sqlWrapper.h

  File Description:

     This file contains the header of function declarations and variable used
     in sqlWrapper.c

  Version:

     1.0, 20191002

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

     Chun-Yu Lai   , chunyu1202@gmail.com
 */

#ifndef SQL_WRAPPER_H
#define SQL_WRAPPER_H

#include "BeDIS.h"
#include <libpq-fe.h>

/* Maximum length of message to communicate with SQL wrapper API in bytes */
#define SQL_TEMP_BUFFER_LENGTH 4096

#define SQL_TRACK_DATA_BULK_INSERT_BUFFER_LENGTH 16384

#define SQL_TRACK_DATA_BULK_INSERT_ONE_RECORD_LENGTH 256

/* When debugging is needed */
//#define debugging

/*
  SQL_execute

     The execution function used by database operations

  Parameter:

     db - a pointer pointing to the connection to the database backend server
     sql_statement - The SQL statement to be executed

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/

static ErrorCode SQL_execute(void *db, char *sql_statement);


/*
  SQL_begin_transaction

     Begins transaction makes the operations specified by the following SQL
     database statement a transaction and holds a lock on the table until
     the transaction either is committed or rollbed back. Begin transaction
     marks the starting boundary of a transaction.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/

static ErrorCode SQL_begin_transaction(void *db);


/*
  SQL_commit_transaction

     Commits the transaction marked by a previous begin transaction.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/

static ErrorCode SQL_commit_transaction(void *db);


/*
  SQL_rollback_transaction

     Rolls back the transaction marked by a previous begin transaction.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/

static ErrorCode SQL_rollback_transaction(void *db);


/*
  SQL_open_database_connection

     Opens a database connection to the database backend server

  Parameter:

     db_filepath - the full file path of database file

     db - a pointer pointing to the connection to the database backend server

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/

ErrorCode SQL_open_database_connection(char *db_filepath, void **db);


/*
  SQL_close_database_connection

     Closes a database connection

  Parameter:

     db - a pointer pointing to the connection to the database backend server

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/

ErrorCode SQL_close_database_connection(void *db);


/*
  SQL_vacuum_database();

     Identifies space occupied by deleted rows and catelogues to be garbage
     collected.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/

ErrorCode SQL_vacuum_database(void *db);


/*
  SQL_delte_old_data();

     Deletes the rows and catelogues which are older than the specified number
     of hours ago.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     retention_hours - specify the hours for data retention

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/

ErrorCode SQL_delete_old_data(void *db, int retention_hours);


/*
  SQL_update_gateway_registration_status

     Updates the status of the input gateways as registered.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     buf - an input string with the format below to specify the registered
           gateways

           length;gateway_ip_1;gateway_ip_2;gateway_ip_3;

     buf_len - Length in number of bytes of buf input string

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/

ErrorCode SQL_update_gateway_registration_status(void *db,
                                                 char *buf,
                                                 size_t buf_len);

/*
  SQL_update_lbeacon_registration_status

     Updates the status of the input lbeacons as registered.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     buf - pointer to an input string with the format below to specify the
           registered gateways.

           length;gateway_ip;lbeacon_uuid_1;lbeacon_registered_timestamp_GMT; \
           lbeacon_ip_1;lbeacon_uuid_2;lbeacon_reigstered_timestamp_GMT; \
           lbeacon_ip_2;

     buf_len - Length in number of bytes of buf input string

     gateway_ip_address - the real ip address of gateway

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/

ErrorCode SQL_update_lbeacon_registration_status(void *db,
                                                 char *buf,
                                                 size_t buf_len,
                                                 char *gateway_ip_address);


/*
  SQL_update_gateway_health_status

     Updates the health status of the input gateways

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     buf - pointer to an input string with the format below to specify the
           health status of gateway

           gateway_ip;health_status;

     buf_len - Length in number of bytes of buf input string

     gateway_ip_address - the real ip address of gateway

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/

ErrorCode SQL_update_gateway_health_status(void *db,
                                           char *buf,
                                           size_t buf_len,
                                           char *gateway_ip_address);


/*
  SQL_update_lbeacon_health_status

     Updates the health status of the input lbeacons

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     buf - pointer to an input string with the format below to specify the
           health status of lbeacon

           lbeacon_uuid;lbeacon_datetime;lbeacon_ip;health_status;

     buf_len - Length in number of bytes of buf input string

     gateway_ip_address - the real ip address of gateway

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/

ErrorCode SQL_update_lbeacon_health_status(void *db,
                                           char *buf,
                                           size_t buf_len,
                                           char *gateway_ip_address);


/*
  SQL_update_object_tracking_data

     Updates data of tracked objects

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     buf - pointer to an input string with the format below.

           lbeacon_uuid;lbeacon_datetime;lbeacon_ip;object_type; \
           object_number;object_mac_address_1;initial_timestamp_GMT_1; \
           final_timestamp_GMT_1;rssi_1;push_button_1;object_type; \
           object_number;object_mac_address_2;initial_timestamp_GMT_2; \
           final_timestamp_GMT_2;rssi_2;push_button_2;

     buf_len - Length in number of bytes of buf input string

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/

ErrorCode SQL_update_object_tracking_data(void *db,
                                          char *buf,
                                          size_t buf_len);


/*
  SQL_update_object_tracking_data_with_battery_voltage

     Updates data of tracked objects

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     buf - pointer to an input string with the format below.

           lbeacon_uuid;lbeacon_datetime;lbeacon_ip;object_type; \
           object_number;object_mac_address_1;initial_timestamp_GMT_1; \
           final_timestamp_GMT_1;rssi_1;push_button_1;battery_voltage_1; \
           object_type;object_number;object_mac_address_2; \
           initial_timestamp_GMT_2;final_timestamp_GMT_2;rssi_2;push_button_2; \
           battery_voltage_2;

     buf_len - Length in number of bytes of buf input string

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/

ErrorCode SQL_update_object_tracking_data_with_battery_voltage(void *db,
                                                               char *buf,
                                                               size_t buf_len);

/*
  SQL_summarize_object_location

     This function queries tracking_table (Time-Series database) within time 
     window to find out the lbeacon_uuid of Lbeacons with the strongest RSSI 
     value for each object mac_address. It is also responsible for maintaining 
     the first seen timestamp and last seen timestamp of the object mac_address 
     and lbeacon_uuid pair. The summary information is updated to the summary 
     table object_summary_table.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     database_pre_filter_time_window_in_sec - 
         The length of time window in which tracked data is filtered to limit 
         database processing time
                                           
     time_interval_in_sec - the time window in which we treat this object been
                            seen and visiable by BOT system

  Return Value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/

ErrorCode SQL_summarize_object_location(void *db, 
                                        int database_pre_filter_time_window_in_sec,
                                        int time_interval_in_sec);


/*
  SQL_identify_geofence_violation

     This function updates geo-fence violation information in 
     object_summary_table and sets perimeter valid timestamp as current
     timestamp to disable previous perimeter violation.
     
  Parameter:

     db - a pointer pointing to the connection to the database backend server

     mac_address -  MAC address of detected object.

     geofence_uuid - UUID of the LBeacon scanned the detected object.

     detected_rssi - rssi signal fo the detected object

  Return Value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/

ErrorCode SQL_identify_geofence_violation(
    void *db,
    char *mac_address,
    char *geofence_uuid,
    int detected_rssi);

/*
  SQL_identify_panic

     This function queries tracking_table (Time-Series database) within time 
     window to find out whether the object (user) has pressed panic_button 
     in the past time interval. The panic button status is updated to the 
     summary table object_summary_table.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     database_pre_filter_time_window_in_sec - 
         the length of time window in which tracked data is filtered to limit 
         database processing time

     time_interval_in_sec - the time window in which we treat this object 
                            as in panic situation if object (user) presses 
                            panic button within the interval.

  Return Value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/

ErrorCode SQL_identify_panic(void *db,
                             int database_pre_filter_time_window_in_sec,
                             int time_interval_in_sec);


/*
  SQL_identify_location_not_stay_room

     This function checks object_summary_table to determine if patients are
     not staying in his/her dedicated rooms.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

  Return Value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/

ErrorCode SQL_identify_location_not_stay_room(void *db);

/*
  SQL_identify_location_long_stay

     This function checks object_summary_table to determine if patients are 
     staying in potential dangerous areas for too long.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

  Return Value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/

ErrorCode SQL_identify_location_long_stay_in_danger(void *db);

/*
  SQL_identify_last_movement_status

     This function uses each pair of object mac_address and lbeacon_uuid from
     the summary table object_summary_table to check the activity status 
     records stored in tracking_table (Time-Series database). It uses the 
     features called TIME_BUCKET and Delta provided by timescaleDB (TSDB) to
     identify the activity status. The activity status is updated to the 
     object_summary_table.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     time_interval_in_min - the time window in which we want to monitor the 
                            movement activity

     each_time_slot_in_min - the time slot in minutes in which we calculate 
                             the running average of RSSI for comparison

     rssi_delta - the delta value of RSSI which we used as a criteria to 
                  identify the movement of object

  Return Value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/

ErrorCode SQL_identify_last_movement_status(void *db, 
                                            int time_interval_in_min, 
                                            int each_time_slot_in_min,
                                            unsigned int rssi_delta);


/*
  SQL_insert_geofence_violation_event

     This function inserts a geo-fence violation event into notification_table 
     directly, because geofence violation is time-critical.
     
  Parameter:

     db - a pointer pointing to the connection to the database backend server

     mac_address -  MAC address of detected object.

     geofence_uuid - UUID of the LBeacon scanned the detected object.

     granularity_for_continuous_violations_in_sec - 
          the length of the time window in which only one violation event is 
          inserted into notification_table when there are continuous 
          violations.
          
 
  Return Value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/

ErrorCode SQL_collect_violation_events(
    void *db, 
    ObjectMonitorType monitor_type,
    int time_interval_in_sec,
    int granularity_for_continuous_violations_in_sec);

/*
  SQL_get_and_update_violation_events

     This function checks object_summary_table to see if there are any 
     violation events. If YES, the violation events of all monitoring 
     types are recorded in notification_table. 

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     buf - an output string with detailed information of violations

     buf_len - length in number of bytes of buf output string
 
  Return Value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/

ErrorCode SQL_get_and_update_violation_events(void *db,
                                              char *buf,
                                              size_t buf_len);

/*
  SQL_get_object_monitor_type

     This function queries object_table to extract monitor_type of the input
     mac_address

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     mac_address - a pointer to mac address of object

     monitor_type - a pointer to output of this function. It stores 
                    monitor_type of the input mac_address in object_table

  Return Value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/

ErrorCode SQL_get_object_monitor_type(void *db, 
                                      char *mac_address, 
                                      ObjectMonitorType *monitor_type);

/*
  SQL_reload_monitor_config

     Updates start hour and end hour of location monitor of long staying in 
     dangerous area

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     server_localtime_against_UTC_in_hour - The time difference between server 
                                            localtime against UTC. 
     
  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/
ErrorCode SQL_reload_monitor_config(void *db, 
                                    int server_localtime_against_UTC_in_hour);

#endif