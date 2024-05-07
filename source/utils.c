
#include "utils.h"

#include "bombus/log.h"

#include "mx/memory.h"
#include "mx/string.h"
#include "mx/misc.h"





/*
 * Constructor
 *
 */
struct mqtt_msg_item* mqtt_msg_item_new(void)
{
    struct mqtt_msg_item *self = xmalloc(sizeof(struct mqtt_msg_item));
    mqtt_msg_init(&self->msg);
    return self;
}


/**
 * Destructor
 *
 */
struct mqtt_msg_item* mqtt_msg_item_delete(struct mqtt_msg_item *self)
{
    mqtt_msg_clean(&self->msg);

    return xfree(self);
}


struct mqtt_msg_item* mqtt_msg_item_from_param(const char *param)
{
    struct mqtt_msg_item *self = mqtt_msg_item_new();

    const char *topic = xstrltrim(param);
    size_t topic_len = xstr_word_len(topic, NULL);
    self->msg.topic = xmemdup(topic, topic_len + 1);
    self->msg.topic[topic_len] = '\0';
    if (strlen(param) == topic_len)
        return self;

    const char *qos = xstrltrim(topic + topic_len);
    size_t qos_len = xstr_word_len(qos, NULL);
    if (qos_len > 0) {
        if ('0' <= qos[0] && qos[0] <= '2') {
            self->msg.qos = qos[0] - '0';
        }
        else {
            BOMBUS_WARN("Invalid QoS argument, %c", qos[0]);
            return mqtt_msg_item_delete(self);
        }
        if (qos_len > 1) {
            if (qos[1] == 'R') {
                self->msg.retain = true;
            } else {
                BOMBUS_WARN("Invalid retain argument, %c", qos[1]);
                return mqtt_msg_item_delete(self);
            }
        }
    }

    const char *payload = xstrltrim(qos + qos_len);
    size_t payload_len = strlen(payload);
    if (payload_len > 0) {
        self->msg.payload = xmemdup(payload, payload_len + 1);
        self->msg.payload[payload_len] = '\0';
        xstrrtrim((char*)self->msg.payload);
        self->msg.payload_len = strlen((char*)self->msg.payload);
    }

    return self;
}



struct mqtt_msg_list* mqtt_msg_list_new(void)
{
    struct mqtt_msg_list *self = xmalloc(sizeof(struct mqtt_msg_list));
    LIST_INIT(self);
    return self;
}


struct mqtt_msg_list* mqtt_msg_list_delete(struct mqtt_msg_list *self)
{
    struct mqtt_msg_item *item, *tmp;
    LIST_FOREACH_SAFE(item, self, _entry_, tmp) {
        LIST_REMOVE(item, _entry_);
        mqtt_msg_item_delete(item);
    }

    return xfree(self);
}


struct mqtt_msg_item* mqtt_msg_list_find_topic(struct mqtt_msg_list *self, const char *topic)
{
    struct mqtt_msg_item *item;
    LIST_FOREACH(item, self, _entry_) {
        if (!strcmp(topic, item->msg.topic))
            return item;
    }

    return NULL;
}

