
#include "args.h"
#include "utils.h"

#include "bombus/log.h"
#include "bombus/version.h"

#include "mx/misc.h"
#include "mx/memory.h"
#include "mx/string.h"
#include "mx/url.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>





enum long_option_en
{
    OPT_VERSION = 1000,
    OPT_SSL_CA_PATH,
    OPT_WS_URI,

    OPT_SUB,
    OPT_PUB,
    OPT_CLI,
    OPT_BROKER,
};



static struct option options[] =
{
    {"addr",                    required_argument,  0,  'a'},
    {"addr-ssl",                required_argument,  0,  'A'},
    {"ws-addr",                 required_argument,  0,  'w'},
    {"ws-addr-ssl",             required_argument,  0,  'W'},
    {"ws-uri",                  required_argument,  0,  OPT_WS_URI},

    {"ssl-key",                 required_argument,  0,  'K'},
    {"ssl-cert",                required_argument,  0,  'C'},
    {"ssl-ca",                  required_argument,  0,  'B'},
    {"ssl-ca-path",             required_argument,  0,  OPT_SSL_CA_PATH},

    {"keep-alive",              required_argument,  0,  'k'},
    {"id",                      required_argument,  0,  'i'},
    {"user",                    required_argument,  0,  'u'},
    {"pass",                    required_argument,  0,  'p'},

    {"sub",                     required_argument,  0,  OPT_SUB},
    {"pub",                     required_argument,  0,  OPT_PUB},
    {"cli",                     no_argument,        0,  OPT_CLI},
    {"broker",                  no_argument,        0,  OPT_BROKER},

    {"config",                  required_argument,  0,  'c'},
    {"verbose",                 required_argument,  0,  'v'},
    {"logger",                  required_argument,  0,  'l'},
    {"version",                 no_argument,        0,  OPT_VERSION},
    {"help",                    no_argument,        0,  'h'},
    {0,                         0,                  0,   0}
};





/**
 * Show help.
 *
 */
static void show_help(const char *name)
{
    printf("\nUsage: %s [function] [options] ...\n", name);

    printf("\nOPTIONS:\n");
    printf("  -a  --addr ADDR:PORT/FILE     connect address:port or file\n");
    printf("  -A  --addr-ssl ADDR:PORT      connect address:port\n");
    printf("  -w  --ws-addr ADDR:PORT       connect websocket address:port\n");
    printf("  -W  --ws-addr-ssl ADDR:PORT   connect ssl websocket address:port\n");
    printf("      --ws-uri URI              websocket uri\n");
    //
    printf("  -C  --ssl-cert FILE       ssl certificate\n");
    printf("  -K  --ssl-key FILE        ssl private key\n");
    printf("  -B  --ssl-ca FILE         ssl ca bundle\n");
    printf("      --ssl-ca-path PATH    ssl ca path\n");
    printf("      --ssl-verify CFG      ssl verify [0-disabled,1-cert,2-full]\n");
    printf("      --ssl-psk KEY         ssl psk key\n");
    printf("      --ssl-psk-id ID       ssl psk identity\n");
    //
    printf("  -k  --keep-alive VALUE    MQTT keep alive interval in seconds\n");
    printf("  -i  --id ID               MQTT client id\n");
    printf("  -u  --user USERNAME       MQTT client username\n");
    printf("  -p  --pass PASSWORD       MQTT client password\n");
    //
    printf("      --cli                     command line mode\n");
    printf("      --sub 'TOPIC QOS'         subscribe topic\n");
    printf("      --pub 'TOPIC QOS MESSAGE' publish message on topic\n");
    //
    printf("  -v  --verbose NUM     set verbose value [0-silent,1-error,2-warning,3-info,4-debug]\n");
    printf("  -l  --logger HEX      set logger value\n");
    printf("      --version         print version\n");
    printf("  -h  --help            help\n");

    printf("\nEXAMPLES:\n");
    printf("  %s -a mqtt://test.mosquitto.org --sub 'test'\n", name);

    printf("\n");
}


/**
 * Parse address:port arguments
 *
 */
static bool parse_address_port(struct args *self, char c, const char *url)
{
    struct url_parser parser;

    url_parse(&parser, url);

    self->port = 0;
    if (parser.port) {
        // Port defined in url
        long lport;
        if (!xstrtol(parser.port, &lport, 10)) {
            BOMBUS_ERROR("Invalid port %s", parser.port);
            return false;
        }
        self->port = (unsigned int)lport;
    }

    if (parser.scheme) {
        if (self->port == 0) {
            // Port not defined in url
            url_parse_scheme(parser.scheme, &self->port, &self->ssl);
        }

        if (xstrstarts(parser.scheme, "ws"))
            self->websocket = true;
    }

    if (parser.host) {
        self->address = xstrndup(parser.host, parser.host_len);
        if (self->port == 0) {
            const char *scheme = "mqtt";
            if (c == 'A')
                scheme = "mqtts";
            else if (c == 'w')
                scheme = "ws";
            else if (c == 'W')
                scheme = "wss";
            url_parse_scheme(scheme, &self->port, &self->ssl);
        }
    }
    if (parser.path)
        self->path = xstrndup(parser.path, parser.path_len);

    if (c == 'A' || c == 'W')
        self->ssl = true;
    if (c == 'w' || c == 'W')
        self->websocket = true;

    return true;
}


/**
 * Parse command line parameters.
 *
 */
static bool parse_parameters(struct args *self, int argc, char *argv[], int *retval)
{
    int c;
    int option_idx = 0;
    bool success = true;

    while (1) {
        long val;
        c = getopt_long(argc, argv, "a:A:w:W:C:K:B:k:i:u:p:c:v:l:h", options, &option_idx);
        if (c == -1)
            break;

        switch (c) {
        case 'a':
        case 'A':
        case 'w':
        case 'W':
            if (!parse_address_port(self, c, optarg))
                return false;
            break;

        case 'C':
            self->ssl_cert = xstrdup(optarg);
            break;

        case 'K':
            self->ssl_key = xstrdup(optarg);
            break;

        case 'B':
            self->ssl_ca = xstrdup(optarg);
            break;

        case OPT_SSL_CA_PATH:
            self->ssl_ca_path = xstrdup(optarg);
            break;

        case OPT_WS_URI:
            self->websocket_uri = xstrdup(optarg);
            break;

        case OPT_PUB: {
            struct mqtt_msg_item *msg_item = mqtt_msg_item_from_param(optarg);
            if (msg_item) {
                if (!self->publish_messages)
                    self->publish_messages = mqtt_msg_list_new();
                LIST_INSERT_HEAD(self->publish_messages, msg_item, _entry_);
            }
        }   break;

        case OPT_SUB: {
            struct mqtt_msg_item *msg_item = mqtt_msg_item_from_param(optarg);
            if (msg_item) {
                if (!self->subscribe_topics)
                    self->subscribe_topics = mqtt_msg_list_new();
                LIST_INSERT_HEAD(self->subscribe_topics, msg_item, _entry_);
            }
        }   break;

        case 'v':
            success = xstrtol(optarg, &val, 10);
            if (success) {
                if (RESET_LEVEL <= val  &&  val <= DEBUG_LEVEL)
                    deflogger->conf.bits.verbosity = val;
            }
            break;

        case 'l':
            success = xstrtol(optarg, &val, 16);
            if (success)
                deflogger->conf.cfgmask = (unsigned int)val;
            break;

        case OPT_VERSION:
            printf("%s\n", BOMBUS_VERSION);
            *retval = 0;
            return false;

        case 'h':
            *retval = 0;
            /* FALLTHRU */ /* no break */
        case '?':
            show_help(argv[0]);
            success = false;
            break;

        default:
            BOMBUS_ERROR("Could not parse arguments");
            success = false;
            break;
        }
    }

    return success;
}



/**
 * Check mandatory variables.
 *
 */
static bool check_mandatory_values(struct args *self)
{
    if (!self->websocket && self->websocket_uri) {
        BOMBUS_WARN("Websocket URI not usable");
        self->websocket_uri = xfree(self->websocket_uri);
    }

    return true;
}


/**
 * Check and set default values.
 *
 * Setup default values for variables omitted in command line.
 * String variables should be defined here to avoid redundant memory allocations.
 *
 */
static void check_default_values(struct args *self)
{
    if (!self->address)
        self->address = xstrdup("127.0.0.1");
    if (!self->client_id)
        self->client_id = xstrdup("bombus-client");
}




/**
 * Initialize application arguments.
 *
 * String variables should be defined after parsing function
 * to avoid redundant allocations.
 *
 */
void args_init(struct args *self)
{
    self->address = NULL;
    self->path = NULL;
    self->port = 0;

    self->ssl = false;
    self->ssl_key = NULL;
    self->ssl_cert = NULL;
    self->ssl_ca = NULL;
    self->ssl_ca_path = NULL;

    self->websocket = false;
    self->websocket_uri = NULL;

    self->cli = false;
    self->client_id = NULL;
    self->keep_alive = 60;

    self->subscribe_topics = NULL;
    self->publish_messages = NULL;
}


/**
 * Parse application arguments.
 *
 */
bool args_parse(struct args *self, int argc, char *argv[], int *retval)
{
    bool success = true;

    success = parse_parameters(self, argc, argv, retval);
    if (!success)
        return success;

    success = check_mandatory_values(self);
    if (!success)
        return success;

    check_default_values(self);

    return success;
}


/**
 * Clean application arguments.
 *
 */
void args_clean(struct args *self)
{
    if (self->address)
        self->address = xfree(self->address);
    if (self->path)
        self->path = xfree(self->path);

    if (self->ssl_key)
        self->ssl_key = xfree(self->ssl_key);
    if (self->ssl_cert)
        self->ssl_cert = xfree(self->ssl_cert);
    if (self->ssl_ca)
        self->ssl_ca = xfree(self->ssl_ca);
    if (self->ssl_ca_path)
        self->ssl_ca_path = xfree(self->ssl_ca_path);

    if (self->websocket_uri)
        self->websocket_uri = xfree(self->websocket_uri);

    if (self->client_id)
        self->client_id = xfree(self->client_id);

    if (self->subscribe_topics)
        self->subscribe_topics = mqtt_msg_list_delete(self->subscribe_topics);
    if (self->publish_messages)
        self->publish_messages = mqtt_msg_list_delete(self->publish_messages);
}
