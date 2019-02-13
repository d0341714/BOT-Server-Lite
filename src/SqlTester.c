#include "SqlWrapper.h"


int main(int argc, char *argv[])
{
   void *db;
   SQL_open_database_connection("bot_lite.db", &db);

   char buf[] = "2;192.168.1.101;192.168.1.102;";
   SQL_update_gateway_registration_status(db, buf, sizeof(buf));

   char temp[4096];
   SQL_query_registered_gateways(db, -1, temp, sizeof(temp));
   printf("select result = %s\n", temp);

   char buf2[] = "2;192.168.1.101;0000001500000A65432100006E654321;1549870728;0000001500000A65432100006E654300;1549870730;";
   SQL_update_lbeacon_registration_status(db, buf2, sizeof(buf2));

   char buf3[] = "2;192.168.1.101;1;192.168.1.102;0;";
   SQL_update_gateway_health_status(db, buf3, sizeof(buf3));

   char buf4[] = "2;192.168.1.101;0000001500000A65432100006E654321;1;0000001500000A65432100006E654300;0;";
   SQL_update_lbeacon_health_status(db, buf4, sizeof(buf4));

   SQL_close_database_connection(db);
   return 0;
}
