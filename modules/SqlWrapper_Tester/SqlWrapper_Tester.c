#include "SqlWrapper.h"

int main(int argc, char *argv[]){
    char *conninfo;
    void *db;

    char buf[] = "2;192.168.8.1;192.168.10.1;";

    char temp[4096];

    char buf2[] = "2;192.168.8.1;0000001500000A65432100006E654321;" \
                  "1552895096;0000001500000A65432100006E654300;1552895096;";

    char buf3[] = "2;192.168.8.101;1;192.168.8.102;0;";

    char buf4[] = "2;0000001500000A65432100006E654321;1552895096;192.168.8.101;1;" \
				  "0000001500000A65432100006E654300;1552895096;192.168.8.102;0;";

    char buf5[] = "0000001500000A65432100006E654321;1549870728;192.168.8.101;" \
                  "0;1;00:00:00:11:11:11;1552895096;1549870728;-50;1;0;";

    printf("\nstart testing..\n\n");

    conninfo = "host=140.109.22.34 port=5432 dbname=botdb user=postgres password=bedis402";
    SQL_open_database_connection(conninfo, &db);

    printf("\nSQL_update_gateway_registration_status\n\n");
    // use buf
    SQL_update_gateway_registration_status(db, buf, sizeof(buf));

    printf("\nSQL_query_registered_gateways\n\n");
    // use temp
    SQL_query_registered_gateways(db, MAX_STATUS, temp, sizeof(temp));
    printf("select result = %s\n", temp);

    printf("\nSQL_update_lbeacon_registration_status\n\n");
    // use buf2
    SQL_update_lbeacon_registration_status(db, buf2, sizeof(buf2));

    printf("\nSQL_update_gateway_health_status\n\n");
    // use buf3
    SQL_update_gateway_health_status(db, buf3, sizeof(buf3));

    printf("\nSQL_update_lbeacon_health_status\n\n");
    // use buf4
    SQL_update_lbeacon_health_status(db, buf4, sizeof(buf4));

    printf("\nSQL_update_object_tracking_data\n\n");
    // use buf5
    SQL_update_object_tracking_data(db, buf5, sizeof(buf5));


    SQL_vacuum_database(db);

    SQL_retain_data(db, 0);

    SQL_close_database_connection(db);

    printf("\nfinish testing..\n\n");

    return 0;
}
