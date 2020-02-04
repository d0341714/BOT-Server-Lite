#define NOTIFICATION_EXPORTS
#include "Notification.h"

NOTIFICATION_API int SendSMS(char *contact_list, char *notification_message){
    printf("This is BeDITeCH notification DLL template\n");
    printf("Send SMS\n");
    printf("To:%s\n", contact_list);
    printf("Content:%s\n", notification_message);
}