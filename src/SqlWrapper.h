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

/* The times of retrying to get available database connection from connection 
pool */
#define SQL_GET_AVAILABLE_CONNECTION_RETRIES 5

/* When debugging is needed */
//#define debugging

typedef struct{

    int serial_id;

    int is_used;

    PGconn *db;

    struct List_Entry list_entry;

} DBConnectionNode;

typedef struct{

    pthread_mutex_t list_lock;
    
    struct List_Entry list_head;

} DBConnectionListHead;


/*
  SQL_execute

     The execution function used by database operations

  Parameter:

     db_conn - a pointer pointing to the connection to the database backend server

     sql_statement - Pointer to the SQL statement to be executed

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/

static ErrorCode SQL_execute(PGconn *db_conn, char *sql_statement);


/*
  SQL_begin_transaction

     Begins transaction makes the operations specified by the following SQL
     database statement a transaction and holds a lock on the table until
     the transaction either is committed or rollbed back. Begin transaction
     marks the starting boundary of a transaction.

  Parameter:

     db_conn - a pointer to the connection to the database backend server

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/

static ErrorCode SQL_begin_transaction(PGconn *db_conn);


/*
  SQL_commit_transaction

     Commits the transaction marked by a previous begin transaction.

  Parameter:

     db_conn - a pointer to the connection to the database backend server

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/

static ErrorCode SQL_commit_transaction(PGconn *db_conn);


/*
  SQL_rollback_transaction

     Rolls back the transaction marked by a previous begin transaction.

  Parameter:

     db_conn - a pointer to the connection to the database backend server

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/

static ErrorCode SQL_rollback_transaction(PGconn *db_conn);

/*
  SQL_create_database_connection_pool

     Create a connection pool to the database backend server

  Parameter:

     conninfo - the information to open database connection

     db_connection_list_head - the list head of database connection pool

     max_connection - the maximum number of database connection in the 
                      connection pool

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code 
                 is WORK_SUCCESSFULLY
*/

ErrorCode SQL_create_database_connection_pool(
    char *conninfo, 
    DBConnectionListHead * db_connection_list_head, 
    int max_connection);

/*
  SQL_destroy_database_connection_pool

  Parameter:

    db_connection_list_head - the list head of database connection pool

  Return Value:
    
    ErrorCode - indicate the result of exeuction, the expected return code 
                is WORK_SUCCESSFULLY

*/

ErrorCode SQL_destroy_database_connection_pool(
    DBConnectionListHead *db_connection_list_head);

/*
  SQL_get_database_connection 

    Get an existing database connection from connection pool
  
  Parameter:
      
    db_connection_list_head - the list head of database connection pool

    db_connection_list_head - the list head of database connection pool

    serial_id - the serial id of the database connection within the pool

  Return Value:

    ErrorCode - indicate the result of execution, the expected return code 
                is WORK_SUCCESSFULLY
*/

ErrorCode SQL_get_database_connection(
    DBConnectionListHead *db_connection_list_head,
    void **db,
    int *serial_id);

/*
  SQL_release_database_connection

    Release the database connection back to connection pool
  
  Paremeter:

    db_connection_list_head - the list head of database connection pool

    serial_id - the serial id of the database connection within the pool

  Return Value:

    ErrorCode - indicate the result of execution, the expected return code 
                is WORK_SUCCESSFULLY
*/

ErrorCode SQL_release_database_connection(
    DBConnectionListHead *db_connection_list_head,
    int serial_id);

/*
  SQL_vacuum_database();

     Identifies the space occupied by deleted rows and catelogues to be garbage
     collected.

  Parameter:

     db_connection_list_head - the list head of database connection pool

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/

ErrorCode SQL_vacuum_database(DBConnectionListHead *db_connection_list_head);

/*
  SQL_delte_old_data();

     Deletes the rows and catelogues which are older than the specified number
     of hours.

  Parameter:

     db_connection_list_head - the list head of database connection pool

     retention_hours - specify the hours for data retention

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/

ErrorCode SQL_delete_old_data(DBConnectionListHead *db_connection_list_head, 
                              int retention_hours);

/*
  SQL_update_gateway_registration_status

     Updates the status of the input gateways as registered.

  Parameter:

     db_connection_list_head - the list head of database connection pool

     buf - an input string with the format below to specify the registered
           gateways

           length;gateway_ip_1;status_1;api_version_1;gateway_ip_2;status_2; \
           api_version_3;

     buf_len - Length in number of bytes of buf input string

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/

ErrorCode SQL_update_gateway_registration_status(
    DBConnectionListHead *db_connection_list_head,
    char *buf,
    size_t buf_len);

/*
  SQL_update_lbeacon_registration_status_less_ver22

     Updates the status of the input lbeacons as registered.

  Parameter:

     db_connection_list_head - the list head of database connection pool

     buf - a pointer to an input string with the format below to specify the
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

ErrorCode SQL_update_lbeacon_registration_status_less_ver22(
    DBConnectionListHead *db_connection_list_head,
    char *buf,
    size_t buf_len,
    char *gateway_ip_address);

/*
  SQL_update_lbeacon_registration_status

     Updates the status of the input lbeacons as registered.

  Parameter:

     db_connection_list_head - the list head of database connection pool

     buf - a pointer to an input string with the format below to specify the
           registered gateways.

           length;gateway_ip;lbeacon_uuid_1;lbeacon_registered_timestamp_GMT; \
           lbeacon_ip_1;lbeacon_api_version_1;lbeacon_uuid_2; \
           lbeacon_reigstered_timestamp_GMT;lbeacon_ip_2;lbeacon_api_version_2;

     buf_len - Length in number of bytes of buf input string

     gateway_ip_address - the real ip address of gateway

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/

ErrorCode SQL_update_lbeacon_registration_status(
    DBConnectionListHead *db_connection_list_head,
    char *buf,
    size_t buf_len,
    char *gateway_ip_address);


/*
  SQL_update_gateway_health_status

     Updates the health status of the input gateways

  Parameter:

     db_connection_list_head - the list head of database connection pool

     buf - a pointer to an input string with the format below to specify the
           health status of gateway

           gateway_ip;health_status;

     buf_len - Length in number of bytes of buf input string

     gateway_ip_address - the real ip address of the gateway where health 
	 status of gateway

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/

ErrorCode SQL_update_gateway_health_status(
    DBConnectionListHead *db_connection_list_head,
    char *buf,
    size_t buf_len,
    char *gateway_ip_address);


/*
  SQL_update_lbeacon_health_status

     Updates the health status of the input lbeacons

  Parameter:

     db_connection_list_head - the list head of database connection pool

     buf - a pointer to an input string with the format below to specify the
           health status of lbeacon

           lbeacon_uuid;lbeacon_timestamp;lbeacon_ip;health_status;

     buf_len - Length in number of bytes of buf input string

     gateway_ip_address - the real ip address of the gateway

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/

ErrorCode SQL_update_lbeacon_health_status(
    DBConnectionListHead *db_connection_list_head,
    char *buf,
    size_t buf_len,
    char *gateway_ip_address);

/*
  SQL_update_object_tracking_data

     Updates _battery_voltage_in data of tracked objects

  Parameter:

     db_connection_list_head - the list head of database connection pool

     buf - a pointer to an input string with the format below.

           lbeacon_uuid;lbeacon_datetime;lbeacon_ip;object_type; \
           object_number;object_mac_address_1;initial_timestamp_GMT_1; \
           final_timestamp_GMT_1;rssi_1;push_button_1;battery_voltage_1; \
           object_type;object_number;object_mac_address_2; \
           initial_timestamp_GMT_2;final_timestamp_GMT_2;rssi_2;push_button_2; \
           battery_voltage_2;

     buf_len - Length in number of bytes of buf input string

     server_installation_path - the absolute file path of server installation path

     is_enabled_panic_monitoring - the flag indicating whether panic monitoring is
                                   enabled

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/

ErrorCode SQL_update_object_tracking_data_with_battery_voltage(
    DBConnectionListHead *db_connection_list_head,
    char *buf,
    size_t buf_len,
    char *server_installation_path,
    int is_enabled_panic_monitoring);

/*
  SQL_summarize_object_location

     This function queries tracking_table (Time-Series database) within time 
     window to find out the lbeacon_uuid of Lbeacons with the strongest RSSI 
     value for each object mac_address. It is also responsible for maintaining 
     the first seen timestamp and last seen timestamp of the object mac_address 
     and lbeacon_uuid pair. The summary information is updated to the summary 
     table object_summary_table.

  Parameter:

     db_connection_list_head - the list head of database connection pool

     database_pre_filter_time_window_in_sec - 
         The length of time window in which tracked data is filtered to limit 
         database processing time
                                           
     time_interval_in_sec - the time window in which we treat this object been
                            seen and visiable by BOT system

     rssi_difference_of_location_accuracy_tolerance - the RSSI difference in 
                                                      which the tag is still 
                                                      treated as scanned by the 
                                                      strongest and nearest 
                                                      lbeacon

     base_location_tolerance_in_millimeter - the location tolerance within 
                                             this distance the tag is treated
                                             as not moved

  Return Value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/

ErrorCode SQL_summarize_object_location(
    DBConnectionListHead *db_connection_list_head, 
    int database_pre_filter_time_window_in_sec,
    int time_interval_in_sec,
    int rssi_difference_of_location_accuracy_tolerance,
    int base_location_tolerance_in_millimeter);


/*
  SQL_identify_geofence_violation

     This function updates geo-fence violation information in 
     object_summary_table and sets perimeter valid timestamp to current
     timestamp to disable previous perimeter violation.
     
  Parameter:

     db_connection_list_head - the list head of database connection pool

     mac_address - The MAC address of a detected object.

  Return Value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/

ErrorCode SQL_identify_geofence_violation(
    DBConnectionListHead *db_connection_list_head,
    char *mac_address);

/*
  SQL_identify_panic_object

     This function checks object_summary_table to determine if patients are
     not staying in his/her dedicated rooms.

  Parameter:

     db_connection_list_head - the list head of database connection pool

  Return Value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/

ErrorCode SQL_identify_location_not_stay_room(
    DBConnectionListHead *db_connection_list_head);

/*
  SQL_identify_location_off_limit

     This function checks object_summary_table to identify the object that have
	 staged at specificed area(s) too long.

  Parameter:

     db_connection_list_head - the list head of database connection pool

  Return Value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/

ErrorCode SQL_identify_location_long_stay_in_danger(
    DBConnectionListHead *db_connection_list_head);

/*
  SQL_identify_last_movement_status

     This function uses each pair of object mac_address and lbeacon_uuid from
     the summary table object_summary_table to check the activity status 
     records stored in tracking_table (Time-Series database). It uses the 
     features called TIME_BUCKET and Delta provided by timescaleDB (TSDB) to
     identify the activity status. The activity status is updated to the 
     object_summary_table.

  Parameter:

     db_connection_list_head - the list head of database connection pool


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

ErrorCode SQL_identify_last_movement_status(
    DBConnectionListHead *db_connection_list_head, 
    int time_interval_in_min, 
    int each_time_slot_in_min,
    unsigned int rssi_delta);


/*
  SQL_insert_geofence_violation_event

     This function inserts a geo-fence violation event directly into notification_table 
     
  Parameter:

     db_connection_list_head - the list head of database connection pool

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
    DBConnectionListHead *db_connection_list_head, 
    ObjectMonitorType monitor_type,
    int time_interval_in_sec,
    int granularity_for_continuous_violations_in_sec);

/*
  SQL_get_and_update_violation_events

     This function checks object_summary_table to see whether there are any 
     violation events. If YES, the violation events of all monitoring 
     types are recorded in a notification_table. 

  Parameter:

     db_connection_list_head - the list head of database connection pool

     buf - an output string with detailed information of violations

     buf_len - length in number of bytes of buf output string
 
  Return Value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/

ErrorCode SQL_get_and_update_violation_events(
    DBConnectionListHead *db_connection_list_head,
    char *buf,
    size_t buf_len);

/*
  SQL_reload_monitor_config

     Updates start time and end time of location monitor of long staying in 
     dangerous area

  Parameter:

     db_connection_list_head - the list head of database connection pool

     server_localtime_against_UTC_in_hour - The time difference between server 
                                            localtime against UTC. 
     
  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/
ErrorCode SQL_reload_monitor_config(
    DBConnectionListHead *db_connection_list_head, 
    int server_localtime_against_UTC_in_hour);

/*
  SQL_dump_active_geo_fence_settings

     This function dumps geo-fence settings from database to specified file

  Parameter:

     db_connection_list_head - the list head of database connection pool

     filename - the specified file name to store the dumped geo-fence settings
     
  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/
ErrorCode SQL_dump_active_geo_fence_settings(DBConnectionListHead *db_connection_list_head, 
                                             char *filename);

/*
  SQL_dump_mac_address_under_geo_fence_monitor

     This function dumps mac address objects which are under geo-fence monitoring

  Parameter:

     db_connection_list_head - the list head of database connection pool

     filename - the specified file name to store the dumped mac address
     
  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/
ErrorCode SQL_dump_mac_address_under_geo_fence_monitor(
    DBConnectionListHead *db_connection_list_head,
    char *filename);

#endif