
#include "args.h"
#include "utils.h"

#include "bombus/client.h"
#include "bombus/log.h"

#include "mx/log.h"
#include "mx/socket.h"
#include "mx/string.h"
#include "mx/memory.h"
#include "mx/stream.h"
#include "mx/stream_ssl.h"
#include "mx/stream_ws.h"
#include "mx/stream_mqtt.h"
#include "mx/mqtt.h"
#include "mx/idler.h"
#include "mx/misc.h"
#include "mx/ssl.h"
#include "mx/url.h"
#include "mx/timer.h"
#include "mx/queue.h"
#include "mx/rand.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>

//#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define BOMBUS_CONNECTION_TIMEOUT       5000


static volatile bool alive = true;


int app_handle_console(void *object, struct stream *stream);



struct app
{
    struct idler *idler;
    struct bombus *bombus;
    struct stream *console;

    struct mqtt_msg_list *subscribe_topics;
    struct mqtt_msg_list *publish_messages;

    bool retain;
    bool reconnect;
    bool alive;
};


static void app_reconnect_bombus(struct app *self);


void app_init(struct app *self)
{
    self->idler = idler_new();
    self->bombus = NULL;

    self->console = stream_new(STDIN_FILENO);
    stream_set_observer(self->console, self, app_handle_console);
    idler_add_stream(self->idler, self->console);

    self->retain = false;
    self->alive = true;
    self->reconnect = true;
    self->subscribe_topics = NULL;
    self->publish_messages = NULL;
}


void app_clean(struct app *self)
{
    idler_remove_stream(self->idler, self->console);
    stream_delete(self->console);
    close(STDIN_FILENO);

    if (self->bombus) {
        bombus_disconnect(self->bombus);
        bombus_delete(self->bombus);
    }

    idler_delete(self->idler);

    self->alive = false;

    if (self->subscribe_topics)
        self->subscribe_topics = mqtt_msg_list_delete(self->subscribe_topics);
    if (self->publish_messages)
        self->publish_messages = mqtt_msg_list_delete(self->publish_messages);
}


bool app_is_alive(struct app *self)
{
    return self->alive;
}


struct idler* app_get_idler(struct app *self)
{
    return self->idler;
}


void app_configure(struct app *self, struct args *args)
{
    self->bombus = bombus_new(self->idler);
    bombus_set_mqtt_keep_alive(self->bombus, args->keep_alive);
    bombus_set_mqtt_client_id(self->bombus, args->client_id);

    bombus_configure_address(self->bombus, args->address, args->port);

//    if (args->ssl) {
//        bombus_configure_ssl(self->bombus, NULL);
//    }
//    if (args->websocket) {
//        bombus_configure_websocket(self->bombus, NULL);
//    }

    if (args->subscribe_topics) {
        // Grab subscribe topics for future use
        self->subscribe_topics = args->subscribe_topics;
        args->subscribe_topics = NULL;
    }

    if (args->publish_messages) {
        // Grab publish messages for future use
        self->publish_messages = args->publish_messages;
        args->publish_messages = NULL;
    }

    app_reconnect_bombus(self);
}


bool app_prepare_tasks(struct app *self)
{
    if (!bombus_is_connected(self->bombus)) {
        if (self->reconnect)
            app_reconnect_bombus(self);
    }

    return true;
}


void app_handle_tasks(struct app *self)
{
    struct stream *stream = NULL;
    unsigned int flags;
    do {
        flags = 0;
        stream = idler_get_next_stream(self->idler, stream, &flags);
        if (flags & STREAM_OUTGOING_READY)
            stream_handle_outgoing_data(stream);
        if (flags & STREAM_INCOMING_READY)
            stream_handle_incoming_data(stream);
    } while(stream);
}


void app_handle_time(struct app *self)
{
    if (self->bombus)
        bombus_handle_time(self->bombus);
}


void app_reconnect_bombus(struct app *self)
{
    bool success = bombus_connect(self->bombus, true);
     if (success)
         success = bombus_wait_for_connection(self->bombus, BOMBUS_CONNECTION_TIMEOUT);
     if (success) {
         struct mqtt_msg_item *item;
         if (self->subscribe_topics) {
             LIST_FOREACH(item, self->subscribe_topics, _entry_) {
                 bombus_subscribe(self->bombus, item->msg.topic, item->msg.qos);
             }
         }

         if (self->publish_messages) {
             LIST_FOREACH(item, self->publish_messages, _entry_) {
                 bombus_publish(self->bombus, item->msg.topic, item->msg.qos, item->msg.retain, item->msg.payload, item->msg.payload_len);
             }
         }
     }
}





void app_handle_connect(struct app *self, char *params);
void app_handle_disconnect(struct app *self, char *params);
void app_handle_subscribe(struct app *self, char *params);
void app_handle_unsubscribe(struct app *self, char *params);
void app_handle_publish(struct app *self, char *params);


struct command_handler_map {
    const char *command;
    void (*handler)(struct app*, char *params);
};


static const struct command_handler_map command_handlers[] = {
        { "publish",        app_handle_publish},
        { "subscribe",      app_handle_subscribe},
        { "unsubscribe",    app_handle_unsubscribe},
        { "connect",        app_handle_connect},
        { "disconnect",     app_handle_disconnect},
        {  NULL,            NULL}
};



void app_handle_connect(struct app *self, char *params)
{
    UNUSED(params);

    if (!bombus_is_connected(self->bombus))
        app_reconnect_bombus(self);
}


void app_handle_disconnect(struct app *self, char *params)
{
    UNUSED(params);

    if (bombus_is_connected(self->bombus))
        bombus_disconnect(self->bombus);
}


void app_handle_subscribe(struct app *self, char *params)
{
    if (bombus_is_connected(self->bombus)) {
        struct mqtt_msg_item *item = mqtt_msg_item_from_param(params);
        if (item) {
            bombus_subscribe(self->bombus, item->msg.topic, item->msg.qos);
            struct mqtt_msg_item *found = mqtt_msg_list_find_topic(self->subscribe_topics, item->msg.topic);
            if (!found) {
                // Store topic for further use
                LIST_INSERT_HEAD(self->subscribe_topics, item, _entry_);
            } else {
                // Just update qos in already stored topic
                found->msg.qos = item->msg.qos;
                mqtt_msg_item_delete(item);
            }
        }
    }
}


void app_handle_unsubscribe(struct app *self, char *params)
{
    if (bombus_is_connected(self->bombus)) {
        struct mqtt_msg_item *item = mqtt_msg_item_from_param(params);
        if (item) {
            bombus_unsubscribe(self->bombus, item->msg.topic);
            struct mqtt_msg_item *found = mqtt_msg_list_find_topic(self->subscribe_topics, item->msg.topic);
            if (found) {
                LIST_REMOVE(found, _entry_);
                mqtt_msg_item_delete(found);
            }
            mqtt_msg_item_delete(item);
        }
    }
}


void app_handle_publish(struct app *self, char *params)
{
    if (bombus_is_connected(self->bombus)) {
        struct mqtt_msg_item *item = mqtt_msg_item_from_param(params);
        if (item) {
            bombus_publish(self->bombus, item->msg.topic, item->msg.qos, item->msg.retain, item->msg.payload, item->msg.payload_len);
            mqtt_msg_item_delete(item);
        }
    }
}


int app_handle_console(void *object, struct stream *console)
{
    struct app *self = (struct app*)object;

    char buffer[1024];
    int bytes = stream_read(console, buffer, sizeof(buffer)-1);
    buffer[bytes] = '\0';

    char *command = buffer;
    size_t command_len = xstr_word_len(command, " \t\n");
    char *params = xstrltrim(command + command_len);

    if (command_len == 0)
        return 1;

    if (!strncmp(command, "quit", command_len)) {
        self->alive = false;
        return 1;
    }

    const struct command_handler_map *cmd_hndl = command_handlers;
    while (cmd_hndl->command && cmd_hndl->handler) {
        if (!strncmp(cmd_hndl->command, command, command_len)) {
            cmd_hndl->handler(self, params);
            break;
        }
        cmd_hndl++;
    }

    return 1;
}








/**
 * Interrupt quit handler.
 */
void quit_request(int sig)
{
    UNUSED(sig);

    alive = false;
}


int main(int argc, char *argv[])
{
    // Setup signals
    signal(SIGALRM, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, quit_request);
    signal(SIGTERM, quit_request);

    log_init(deflogger, "bombus");
    deflogger->conf.bits.verbosity = DEBUG_LEVEL;

    int retval = -1;
    struct args args;
    args_init(&args);
    if (!args_parse(&args, argc, argv, &retval)) {
        args_clean(&args);
        return retval;
    }

    struct app app;
    app_init(&app);
    app_configure(&app, &args);

    while (alive && app_is_alive(&app)) {
        if (!app_prepare_tasks(&app)) {
            continue;
        }
        int status = idler_wait(app_get_idler(&app), 1000);
        if (status == IDLER_ERROR) {
            BOMBUS_ERROR("Idler error %d", errno);
            break;
        }
        if (status == IDLER_INTERRUPT) {
            BOMBUS_WARN("Interrupt");
            break;
        }
        if (status == IDLER_OPERATION) {
            app_handle_tasks(&app);
        }
        clock_update_sys();
        app_handle_time(&app);
    }

    app_clean(&app);
    args_clean(&args);

    return 0;
}

