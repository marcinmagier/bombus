
#ifndef __ARGS_H_
#define __ARGS_H_


#include "utils.h"

#include <stdbool.h>


struct args
{
    char *address;
    char *path;
    unsigned int port;
    bool ssl;
    bool websocket;

    char *websocket_uri;

    char *ssl_key;
    char *ssl_cert;
    char *ssl_ca;
    char *ssl_ca_path;

    bool cli;

    unsigned short keep_alive;
    char *client_id;

    struct mqtt_msg_list *subscribe_topics;
    struct mqtt_msg_list *publish_messages;

};



void args_init(struct args *args);
bool args_parse(struct args *args, int argc, char *argv[], int *retval);
void args_clean(struct args *args);


#endif /* __ARGS_H_ */
