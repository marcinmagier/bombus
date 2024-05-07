
#include "bombus/client.h"
#include "bombus/log.h"

#include "mx/memory.h"
#include "mx/string.h"
#include "mx/stream.h"
#include "mx/stream_ssl.h"
#include "mx/stream_ws.h"
#include "mx/stream_mqtt.h"
#include "mx/socket.h"
#include "mx/idler.h"
#include "mx/misc.h"

#include <string.h>





static int bombus_handle_incomming_data(void *parent, struct stream *stream);
static int bombus_handle_received_msg(void *object, struct stream_mqtt *stream, unsigned char type, unsigned char flags, void *mqtt_msg);





struct bombus* bombus_new(struct idler *idler)
{
    struct bombus *self = xmalloc(sizeof(struct bombus));
    bombus_init(self);
    self->idler = idler;
    return self;
}


struct bombus* bombus_delete(struct bombus *self)
{
    bombus_clean(self);
    return xfree(self);
}


void bombus_init(struct bombus *self)
{
    self->socket_family = AF_UNSPEC;

    self->address = NULL;
    self->port = 0;

    self->stream = NULL;
    self->idler = NULL;
    self->ssl = NULL;
    self->websocket = false;
    self->websocket_uri = NULL;

    mqtt_conf_init(&self->mqtt_conf);
    mqtt_msg_init(&self->mqtt_will);

    self->msg_id = 1;
    self->wait_msg_type = 0;
    self->connected = false;
    self->reconnection_attempts = 3;
}


void bombus_clean(struct bombus *self)
{
    if (self->address)
        self->address = xfree(self->address);

    if (self->stream) {
        socket_close(stream_get_fd(self->stream));
        self->stream = stream_delete(self->stream);
    }

    if (self->websocket_uri)
        self->websocket_uri = xfree(self->websocket_uri);

    self->idler = NULL;
    self->ssl = NULL;

    mqtt_conf_clean(&self->mqtt_conf);
    mqtt_msg_clean(&self->mqtt_will);
}


void bombus_configure_mqtt(struct bombus *self, struct mqtt_conf *conf, struct mqtt_msg *will)
{
    if (conf)
        mqtt_conf_copy(&self->mqtt_conf, conf);

    if (will)
        mqtt_msg_copy(&self->mqtt_will, will);
}


void bombus_set_mqtt_keep_alive(struct bombus *self, unsigned short keep_alive)
{
    self->mqtt_conf.keep_alive = keep_alive;
}


void bombus_set_mqtt_client_id(struct bombus *self, const char *client_id)
{
    if (self->mqtt_conf.client_id)
        self->mqtt_conf.client_id = xfree(self->mqtt_conf.client_id);

    if (client_id)
        self->mqtt_conf.client_id = xstrdup(client_id);
}


void bombus_set_mqtt_auth(struct bombus *self, const char *user_name, const char *password)
{
    self->mqtt_conf.user_name = xstrdup(user_name);
    self->mqtt_conf.password = (unsigned char*)xstrdup(password);
    self->mqtt_conf.password_len = strlen(password);
}


void bombus_configure_socket(struct bombus *self, int family)
{
    self->socket_family = family;
}


void bombus_configure_address(struct bombus *self, const char *address, unsigned int port)
{
    if (self->address)
        self->address = xfree(self->address);
    self->address = xstrdup(address);
    self->port = port;
}


void bombus_configure_ssl(struct bombus *self, struct ssl *ssl)
{
    self->ssl = ssl;
}


void bombus_configure_websocket(struct bombus *self, const char *uri)
{
    self->websocket = true;
    if (uri)
        self->websocket_uri = xstrdup(uri);
}


bool bombus_connect(struct bombus *self, bool clean_session)
{
    bool ret = false;
    int fd = -1;

    if (self->port > 0) {
        BOMBUS_INFO("Connect to %s:%d", self->address, self->port);
        fd = socket_connect_inet(self->socket_family, self->address, self->port);
    }
    else {
        BOMBUS_INFO("Connect to %s", self->address);
        fd = socket_connect_unix(self->address);
    }

    if (socket_is_valid(fd)) {
        socket_set_non_blocking(fd, 1);

        struct stream *stream = stream_new(fd);

        if (self->ssl) {
            // Wrap stream with ssl
            struct stream_ssl *stream_ssl = stream_ssl_new(self->ssl, stream);
            stream_ssl_connect(stream_ssl);
            stream = stream_ssl_to_stream(stream_ssl);
        }

        if (self->websocket) {
            // Wrap stream with websocket
            struct stream_ws *stream_ws = stream_ws_new(stream);
            stream_ws_connect(stream_ws, self->websocket_uri, NULL, NULL);
            stream = stream_ws_to_stream(stream_ws);
        }

        struct stream_mqtt *stream_mqtt = stream_mqtt_new(stream);
        stream_mqtt_connect(stream_mqtt, clean_session, self->mqtt_conf.keep_alive, self->mqtt_conf.client_id,
                            self->mqtt_will.topic, self->mqtt_will.payload, self->mqtt_will.payload_len,
                            self->mqtt_will.retain, self->mqtt_will.qos,
                            self->mqtt_conf.user_name, self->mqtt_conf.password, self->mqtt_conf.password_len);
        stream_mqtt_set_observer(stream_mqtt, self, bombus_handle_received_msg);

        self->stream = stream_mqtt_to_stream(stream_mqtt);
        stream_set_observer(self->stream, self, bombus_handle_incomming_data);
        idler_add_stream(self->idler, self->stream);

        ret = true;
    }

    return ret;
}


void bombus_disconnect(struct bombus *self)
{
    if (self->stream) {
        // Disconnect cleanly
        stream_mqtt_disconnect(stream_mqtt_from_stream(self->stream));
        stream_flush(self->stream);

        idler_remove_stream(self->idler, self->stream);
        socket_close(stream_get_fd(self->stream));
        self->stream = stream_delete(self->stream);
    }

    self->connected = false;
}


bool bombus_is_connected(struct bombus *self)
{
    return self->connected;
}


int bombus_wait_for_data(struct bombus *self, unsigned long timeout_ms)
{
    return idler_wait(self->idler, timeout_ms);
}


bool bombus_wait_for_msg(struct bombus *self, unsigned char mqtt_msg_type, unsigned long timeout_ms)
{
    self->wait_msg_type = mqtt_msg_type;

    timeout_ms = timeout_ms > 10 ? timeout_ms/10 : 1;
    for (int i=0; i<10; i++) {
        if (!self->stream)
            break;  // Disconnected

        bombus_wait_for_data(self, timeout_ms);
        bombus_handle_incomming_data(self, self->stream);
        if (self->wait_msg_type == 0)
            return true;
    }

    return false;
}


bool bombus_wait_for_connection(struct bombus *self, unsigned long timeout_ms)
{
    bool status = bombus_wait_for_msg(self, MQTT_CONNACK, timeout_ms);
    if (status)
        status = bombus_is_connected(self);
    return status;
}


int bombus_read_data(struct bombus *self)
{
    UNUSED(self);

    return 0;
}


void bombus_subscribe(struct bombus *self, const char *topic, unsigned char qos)
{
    stream_mqtt_subscribe(stream_mqtt_from_stream(self->stream), self->msg_id++, topic, qos);
}


void bombus_unsubscribe(struct bombus *self, const char *topic)
{
    stream_mqtt_unsubscribe(stream_mqtt_from_stream(self->stream), self->msg_id++, topic);
}


void bombus_publish(struct bombus *self, const char *topic, unsigned char qos, bool retain, const void *data, size_t data_len)
{
    stream_mqtt_publish(stream_mqtt_from_stream(self->stream), retain, false, qos, self->msg_id++, topic, data, data_len);
}


void bombus_handle_stream(struct bombus *self)
{
    unsigned int flags = idler_get_stream_status(self->idler, self->stream);
    if (flags & STREAM_OUTGOING_READY)
        stream_handle_outgoing_data(self->stream);
    if (flags & STREAM_INCOMING_READY)
        stream_handle_incoming_data(self->stream);
}


void bombus_handle_time(struct bombus *self)
{
    if (self->stream)
        stream_time(self->stream);
}


int bombus_handle_received_msg(void *object, struct stream_mqtt *stream, unsigned char type, unsigned char flags, void *mqtt_msg)
{
    struct bombus *self = (struct bombus*)object;

    UNUSED(flags);
    UNUSED(stream);

    if (self->wait_msg_type == type)
        self->wait_msg_type = 0;    // Expected message type received

    switch (type) {
        case MQTT_CONNACK: {
            struct mqtt_connack *msg = (struct mqtt_connack*)mqtt_msg;
            if (msg->return_code == MQTT_CONNACK_ACCEPTED) {
                self->connected = true;
                BOMBUS_DEBUG(BOMBUS_DBG_CLIENT, "Client %d connected", stream_get_fd(self->stream));
            }
            else {
                BOMBUS_WARN("Client with %d fd refused because %d", stream_get_fd(self->stream), msg->return_code);
            }
        }   break;

        case MQTT_SUBACK: {
            struct mqtt_suback *msg = (struct mqtt_suback*)mqtt_msg;
            if (msg->return_code != MQTT_SUBACK_FAILURE) {
                BOMBUS_DEBUG(BOMBUS_DBG_CLIENT, "Client %d subscribtion succes", stream_get_fd(self->stream));
            }
            else {
                BOMBUS_WARN("Client %d subscribtion failure %d", stream_get_fd(self->stream), msg->return_code);
            }
        }   break;

        case MQTT_PUBLISH: {
            struct mqtt_publish *msg = (struct mqtt_publish*)mqtt_msg;
            char topic[msg->topic_len+1];
            memcpy(topic, msg->topic, msg->topic_len);
            topic[msg->topic_len] = '\0';

            // TODO: Forward received message to application
            char payload[msg->payload_len+1];
            memcpy(payload, msg->payload, msg->payload_len);
            payload[msg->payload_len] = '\0';
            BOMBUS_INFO("Client %d received %s '%s'", stream_get_fd(self->stream), topic, payload);
        }   break;
    }

    return 1;
}


int bombus_handle_incomming_data(void *object, struct stream *stream)
{
    struct bombus *self = (struct bombus*)object;
    struct stream_mqtt *stream_mqtt = stream_mqtt_from_stream(stream);

    ssize_t bytes;

    do {
        bytes = stream_mqtt_peek_frame(stream_mqtt);
        if (bytes <= 0) {
            if (bytes < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    return bytes;   // All data already read
                BOMBUS_ERROR("Receiving from %d fd failed with %d", stream_get_fd(stream), errno);
            }
            break;
        }
        else {
            BOMBUS_ERROR("Message not handled by callback");
            break;
        }
    } while (bytes > 0);

    BOMBUS_DEBUG(BOMBUS_DBG_CLIENT, "Disconnected from broker");
    bombus_disconnect(self);

    return 0;
}
