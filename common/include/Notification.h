#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#ifdef NOTIFICATION_EXPORTS
#define NOTIFICATION_API __declspec(dllexport)
#else
#define NOTIFICATION_API __declspec(dllimport)
#endif

NOTIFICATION_API int SendSMS(char *contact_list, char *notification_message);

#endif