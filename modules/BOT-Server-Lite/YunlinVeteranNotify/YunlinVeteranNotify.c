#define NOTIFICATION_EXPORTS
#include "Notification.h"

NOTIFICATION_API int SendSMS(char *contact_list, char *notification_message){

    printf("SendSMS to: [%s] with content [%s]\n", contact_list, notification_message);

}