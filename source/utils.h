
#ifndef __BOMBUS_UTILS_H_
#define __BOMBUS_UTILS_H_

#include "mx/mqtt.h"
#include "mx/queue.h"





struct mqtt_msg_item
{
    struct mqtt_msg msg;

    LIST_ENTRY(mqtt_msg_item) _entry_;
};


struct mqtt_msg_item* mqtt_msg_item_new(void);
struct mqtt_msg_item* mqtt_msg_item_delete(struct mqtt_msg_item *self);
struct mqtt_msg_item* mqtt_msg_item_from_param(const char *param);




// struct mqtt_msg_list
LIST_HEAD(mqtt_msg_list, mqtt_msg_item);


struct mqtt_msg_list* mqtt_msg_list_new(void);
struct mqtt_msg_list* mqtt_msg_list_delete(struct mqtt_msg_list *self);

struct mqtt_msg_item* mqtt_msg_list_find_topic(struct mqtt_msg_list *self, const char *topic);


#endif /* __BOMBUS_UTILS_H_ */
