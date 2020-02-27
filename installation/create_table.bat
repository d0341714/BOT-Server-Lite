@echo off
echo=
echo "Version 1.0,February 2020"


echo "Create gateway_table the file " 
echo 'Reference: BOT Sever database preparation v5.1_20200220'
echo=
psql -U postgres -p 5432 -d botdb -c "CREATE TABLE lbeacon_table ( id bigserial not null primary key, uuid uuid not null unique, ip_address inet not null, description varchar(256), health_status smallint, gateway_ip_address inet, registered_timestamp timestamp with time zone not null, last_report_timestamp timestamp with time zone not null, danger_area smallint, room varchar(128), coordinate_x integer, coordinate_y integer, api_version varchar(16), server_time_offset integer)";   
echo CREATE TABLE lbeacon_table

set PGPASSWORD=jane1807

psql -U postgres -p 5432 -d botdb -c "CREATE TABLE object_summary_table( id bigserial not null primary key, mac_address macaddr not null, uuid uuid, rssi integer, first_seen_timestamp timestamp with time zone, last_seen_timestamp timestamp with time zone, last_reported_timestamp timestamp with time zone , panic_violation_timestamp timestamp with time zone, movement_violation_timestamp timestamp with time zone, geofence_violation_timestamp timestamp with time zone, location_violation_timestamp timestamp with time zone,  battery_voltage smallint, is_location_updated smallint, base_x integer, base_y integer)";
echo CREATE TABLE object_summary_table
psql -U postgres -p 5432 -d botdb -c "CREATE TABLE edit_object_record (id bigserial not null primary key, edit_user_id bigint, edit_time timestamp with time zone, notes text, new_status text, new_location text, edit_objects text[], path varchar(128))";
echo CREATE TABLE edit_object_record
psql -U postgres -p 5432 -d botdb -c "CREATE TABLE import_table (id bigserial not null primary key, name varchar(128), type varchar(128), asset_control_number varchar(128))";
echo CREATE TABLE import_table

psql -U postgres -p 5432 -d botdb -c "CREATE TABLE gateway_table ( id bigserial not null primary key, ip_address inet not null unique, health_status smallint, registered_timestamp timestamp with time zone not null, last_report_timestamp timestamp with time zone not null, api_version varchar(16));"
echo CREATE TABLE gateway_table
psql -U postgres -p 5432 -d botdb -c "CREATE TABLE object_table( id bigserial not null primary key, mac_address macaddr not null unique, type varchar(128) not null, name varchar(128), status varchar(128) not null, monitor_type integer, transferred_location varchar(128), asset_control_number varchar(128) not null, registered_timestamp timestamp with time zone not null, area_id bigint, note_id bigint, physician_id bigint, object_type bigint not null, reserved_timestamp timestamp with time zone, reserved_user_id smallint, room varchar(128))";
echo CREATE TABLE object_table
psql -U postgres -p 5432 -d botdb -c "CREATE TABLE object_summary_table( id bigserial not null primary key, mac_address macaddr not null, uuid uuid, rssi integer, first_seen_timestamp timestamp with time zone, last_seen_timestamp timestamp with time zone, last_reported_timestamp timestamp with time zone , panic_violation_timestamp timestamp with time zone, movement_violation_timestamp timestamp with time zone, geofence_violation_timestamp timestamp with time zone, location_violation_timestamp timestamp with time zone,  battery_voltage smallint, is_location_updated smallint, base_x integer, base_y integer)";
echo CREATE TABLE object_summary_table
psql -U postgres -p 5432 -d botdb -c "CREATE TABLE geo_fence_config ( id bigserial not null primary key , name varchar(128), perimeters varchar(1024), fences varchar(1024), enable integer, start_time time without time zone, end_time time without time zone, is_active smallint, area_id integer)";
echo CREATE TABLE geo_fence_config
psql -U postgres -p 5432 -d botdb -c "CREATE TABLE user_table(id bigserial not null primary key, name varchar(128), password varchar(128), mydevice varchar[], search_history json, registered_timestamp timestamp with time zone, last_visit_timestamp timestamp with time zone, locale_id integer DEFAULT 1, main_area integer, max_search_history_count integer DEFAULT 4)";
echo CREATE TABLE user_table
psql -U postgres -p 5432 -d botdb -c "CREATE TABLE edit_object_record (id bigserial not null primary key, edit_user_id bigint, edit_time timestamp with time zone, notes text, new_status text, new_location text, edit_objects text[], path varchar(128))";
echo CREATE TABLE edit_object_record
psql -U postgres -p 5432 -d botdb -c "CREATE TABLE shift_change_record (id bigserial not null primary key, user_id bigint, file_path text, shift text, submit_timestamp timestamp with time zone)";
echo CREATE TABLE shift_change_record
psql -U postgres -p 5432 -d botdb -c "CREATE TABLE notification_table (id bigserial not null primary key, monitor_type integer, mac_address macaddr not null, uuid uuid, violation_timestamp timestamp with time zone, processed smallint, web_processed smallint)";
echo CREATE TABLE notification_table
psql -U postgres -p 5432 -d botdb -c "CREATE TABLE area_table(id bigserial not null primary key, name varchar(128) not null, readable_name varchar(128))";
echo CREATE TABLE area_table
psql -U postgres -p 5432 -d botdb -c "CREATE TABLE user_area(id bigserial not null primary key, user_id bigint, area_id bigint)";
echo CREATE TABLE user_area
psql -U postgres -p 5432 -d botdb -c "CREATE TABLE location_not_stay_room_config ( id bigserial not null primary key, area_id bigint not null unique, enable integer, start_time time without time zone, end_time time without time zone, is_active smallint)";
echo CREATE TABLE location_not_stay_room_config
psql -U postgres -p 5432 -d botdb -c "CREATE TABLE location_long_stay_in_danger_config ( id bigserial not null primary key, area_id bigint not null unique, enable integer, start_time time without time zone, end_time time without time zone, stay_duration integer, is_active smallint)";
echo CREATE TABLE location_long_stay_in_danger_config
psql -U postgres -p 5432 -d botdb -c "CREATE TABLE movement_config ( id bigserial not null primary key, area_id bigint not null unique, enable integer, start_time time without time zone, end_time time without time zone, is_active smallint)";
echo CREATE TABLE movement_config
psql -U postgres -p 5432 -d botdb -c "CREATE TABLE import_table (id bigserial not null primary key, name varchar(128), type varchar(128), asset_control_number varchar(128))";
echo CREATE TABLE import_table
psql -U postgres -p 5432 -d botdb -c "CREATE TABLE search_result_queue (id bigserial not null primary key, query_time timestamp without time zone, key_type varchar(128), key_word varchar(128), result_mac_address text[], pin_color_index smallint)";"CREATE TABLE location_history_table ( mac_address macaddr not null, uuid uuid not null, record_timestamp timestamp with time zone not null, battery_voltage smallint, base_x integer, base_y integer, average_rssi integer)";
echo CREATE TABLE search_result_queue
psql -U postgres -p 5432 -d botdb -c "CREATE TABLE user_role (id bigserial not null primary key, user_id bigint not null default 0, role_id bigint not null default 0)";
echo CREATE TABLE user_role
psql -U postgres -p 5432 -d botdb -c "CREATE TABLE roles(id bigserial not null primary key, name varchar(128) not null)";
echo CREATE TABLE roles
psql -U postgres -p 5432 -d botdb -c "CREATE TABLE roles_permission(id bigserial not null primary key, role_id bigint, permission_id bigint)";
echo CREATE TABLE roles_permission
psql -U postgres -p 5432 -d botdb -c "CREATE TABLE permission_table (id bigserial not null primary key, name varchar(128) not null)";
echo CREATE TABLE permission_table
psql -U postgres -p 5432 -d botdb -c "CREATE TABLE locales (id bigserial not null primary key, name varchar(128) not null)";
echo CREATE TABLE locales
psql -U postgres -p 5432 -d botdb -c "CREATE TABLE search_history (id bigserial not null primary key, user_id integer, search_time timestamp with time zone, key_type varchar(128), keyword varchar(128))";"CREATE TABLE branch_and_department (id bigserial not null primary key, branch_name text, department text[])";
echo CREATE TABLE search_history
psql -U postgres -p 5432 -d botdb -c "CREATE TABLE monitor_type_table (id bigserial not null primary key, type_id integer, name varchar(128), readable_name varchar(128))";
echo CREATE TABLE monitor_type_table

echo=
echo=
echo " Check whether the tables are generated. "



psql -U postgres -p 5432 -d botdb -t -c "\dt";


::IF %ERRORLEVEL% = 1 Echo "RESUILT:The establish of table was failed."


pause

