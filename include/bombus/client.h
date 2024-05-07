
#ifndef __BOMBUS_CLIENT_H_
#define __BOMBUS_CLIENT_H_


#include "mx/mqtt.h"

#include <stddef.h>
#include <stdbool.h>



struct stream;
struct idler;
struct ssl;


struct bombus
{
    int socket_family;

    char *address;
    unsigned int port;

    struct ssl *ssl;
    bool websocket;
    char *websocket_uri;

    struct stream *stream;
    struct idler *idler;

    struct mqtt_conf mqtt_conf;
    struct mqtt_msg mqtt_will;

    unsigned int msg_id;
    unsigned char wait_msg_type;
    bool connected;
    int reconnection_attempts;
};



struct bombus* bombus_new(struct idler *idler);
struct bombus* bombus_delete(struct bombus *self);

void bombus_init(struct bombus *self);
void bombus_clean(struct bombus *self);

void bombus_set_mqtt_keep_alive(struct bombus *self, unsigned short keep_alive);
void bombus_set_mqtt_client_id(struct bombus *self, const char *client_id);
void bombus_set_mqtt_auth(struct bombus *self, const char *user_name, const char *password);
void bombus_set_mqtt_will(struct bombus *self, const char *topic, const unsigned char *msg, unsigned short msg_len, unsigned short qos, bool retain);

void bombus_configure_socket(struct bombus *self, int family);
void bombus_configure_address(struct bombus *self, const char *address, unsigned int port);
void bombus_configure_ssl(struct bombus *self, struct ssl *ssl);
void bombus_configure_websocket(struct bombus *self, const char *uri);

bool bombus_connect(struct bombus *self, bool clean_session);
void bombus_disconnect(struct bombus *self);
bool bombus_is_connected(struct bombus *self);

int bombus_wait_for_data(struct bombus *self, unsigned long timeout_ms);
bool bombus_wait_for_msg(struct bombus *self, unsigned char mqtt_msg_type, unsigned long timeout_ms);
bool bombus_wait_for_connection(struct bombus *self, unsigned long timeout_ms);
int bombus_read_data(struct bombus *self);


void bombus_subscribe(struct bombus *self, const char *topic, unsigned char qos);
void bombus_unsubscribe(struct bombus *self, const char *topic);
void bombus_publish(struct bombus *self, const char *topic, unsigned char qos, bool retain, const void *data, size_t data_len);

void bombus_handle_stream(struct bombus *self);
void bombus_handle_time(struct bombus *self);


#endif /* __BOMBUS_CLIENT_H_ */
