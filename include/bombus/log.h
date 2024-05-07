
#ifndef __BOMBUS_LOG_H_
#define __BOMBUS_LOG_H_

#include "mx/log.h"


#define BOMBUS_RESET(...)           _RESET_(deflogger, __VA_ARGS__)
#define BOMBUS_ERROR(...)           _ERROR_(deflogger, __VA_ARGS__)
#define BOMBUS_WARN(...)            _WARN_(deflogger, __VA_ARGS__)
#define BOMBUS_INFO(...)            _INFO_(deflogger, __VA_ARGS__)
#define BOMBUS_DEBUG(mask, ...)     _DEBUG_(deflogger, mask, __VA_ARGS__)

#define BOMBUS_LOG(...)             _LOG_(LOG_COLOR_YELLOW, __VA_ARGS__)


enum bombus_dbg_types_e {
    BOMBUS_DBG_MQTT        = (0x1 << 0),
    BOMBUS_DBG_CLIENT      = (0x1 << 1),
//    BOMBUS_DBG_BROKER      = (0x1 << 2),
};



#endif /* __BOMBUS_LOG_H_ */
