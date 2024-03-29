/*
* Copyright © xFusion Digital Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `setservice`
* Author:
* Create: 2019-06-14
* Notes:
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "cJSON_Utils.h"
#include "commons.h"
#include "curl/curl.h"
#include "zf_log.h"
#include "constants.h"
#include "command-helps.h"
#include "command-interfaces.h"
#include "argparse.h"
#include "redfish.h"
#include "string_utils.h"

#define SERVICE_KVM "KVM"
#define SERVICE_VM "VirtualMedia"
#define SERVICE_IPMI "IPMI"

static const char *OEM_SERVICE_CHOICES[] = {"VNC", "Video", "NAT", NULL};
static const char *SERVICE_CHOICES[] = {
        "HTTP", "SNMP", "VirtualMedia", "IPMI", "SSH", "KVM", "VNC",
        "Video", "NAT", "SSDP", NULL
};
static const char *SUPPORT_SSL_SERVICES[] = {SERVICE_KVM, SERVICE_VM, NULL};

static const char *OPT_SERVICE_REQUIRED = "Error: option `service` is required.";
static const char *OPT_SERVICE_ILLEGAL = "Error: option `service` is illegal, available choices: "
                                         "HTTP, SNMP, VirtualMedia, IPMI, SSH, KVM, VNC, Video, NAT, SSDP.";
static const char *OPT_ENABLED_ILLEGAL = "Error: option `enabled` is illegal, available choices: Enabled, Disabled";
static const char *OPT_PORT_ILLEGAL = "Error: option `port` is illegal, value range should be: 1~65535";

static const char *OPT_SSL_NOT_SUPPORT = "Error: setting `ssl-enabled` is only support for service `KVM` and 'Vedio`.";
static const char *OPT_PORT2_NOT_SUPPORT = "Error: setting `port2` is only support for service `IPMI`";

static const char *const usage[] = {
        "setservice -s service [-e enabled] [-p port] [-q port2] [-t ssl-enabled]",
        NULL,
};

typedef struct _UpdateServiceOption {
    char *service;
    char *enabled;
    int port;
    int port2;
    char *sslEnabled;
} UtoolUpdateServiceOption;

static cJSON *BuildPayload(UtoolRedfishServer *server, UtoolUpdateServiceOption *option, UtoolResult *result);

static void ValidateSubcommandOptions(UtoolUpdateServiceOption *option, UtoolResult *result);

static void UpdateKvmService(UtoolRedfishServer *server, UtoolUpdateServiceOption *option, UtoolResult *result);

static void
UpdateVirtualMediaService(UtoolRedfishServer *server, UtoolUpdateServiceOption *option, UtoolResult *result);

/**
* set BMC network protocol services, command handler for `setservice`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdSetService(UtoolCommandOption *commandOption, char **outputStr) {
    cJSON *output = NULL,
            *payload = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolUpdateServiceOption *option = &(UtoolUpdateServiceOption) {
            .port=DEFAULT_INT_V,
            .port2=DEFAULT_INT_V,
            0
    };

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('s', "service", &(option->service),
                        "specifies service to update, available choices: {HTTP, SNMP, VirtualMedia, "
                        "IPMI, SSH, KVM, VNC, Video, NAT, SSDP}", NULL, 0, 0),
            OPT_STRING ('e', "enabled", &(option->enabled),
                        "specifies whether the service is enabled, available choices: {Enabled, Disabled}", NULL, 0, 0),
            OPT_INTEGER('p', "port", &(option->port), "specifies service port, value range: 1~65535", NULL, 0, 0),
            OPT_INTEGER('q', "port2", &(option->port2),
                        "specifies service port2, value range: 1~65535. Only IPMI service is supported for now.",
                        NULL, 0, 0),
            OPT_STRING ('t', "ssl-enabled", &(option->sslEnabled),
                        "specifies whether SSL is enabled, available choices: {Enabled, Disabled}", NULL, 0, 0),
            OPT_END()
    };

    result->code = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    result->code = UtoolValidateConnectOptions(commandOption, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    ValidateSubcommandOptions(option, result);
    if (result->broken) {
        goto DONE;
    }

    // get redfish system id
    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto DONE;
    }

    /** update SSL enabled for KVM and VirtualMedia */
    if (option->sslEnabled != NULL) {
        /** handle KVM service */
        if (UtoolStringEquals(option->service, SERVICE_KVM)) {
            UpdateKvmService(server, option, result);
            if (result->broken) {
                goto FAILURE;
            }
        }

        /** handle Virtual Media service */
        if (UtoolStringEquals(option->service, SERVICE_VM)) {
            UpdateVirtualMediaService(server, option, result);
            if (result->broken) {
                goto FAILURE;
            }
        }

        FREE_CJSON(result->data)
    }

    /** update service */
    if (option->enabled != NULL || option->port != DEFAULT_INT_V) {
        // build payload
        payload = BuildPayload(server, option, result);
        if (result->broken) {
            goto FAILURE;
        }

        UtoolRedfishPatch(server, "/Managers/%s/NetworkProtocol", payload, NULL, NULL, NULL, result);
        if (result->broken) {
            /* we need to patch output message array if ssl-enabled present */
            if (option->sslEnabled != NULL && result->code == UTOOLE_OK && result->desc != NULL) {
                cJSON *message = cJSON_CreateString("Success: successfully update ssl-enabled property");
                result->code = UtoolAssetCreatedJsonNotNull(message);
                if (result->code != UTOOLE_OK) {
                    goto FAILURE;
                }

                output = cJSON_Parse(result->desc);
                cJSON *messages = cJSON_GetObjectItem(output, "Message");
                cJSON_InsertItemInArray(messages, 0, message);
                FREE_OBJ(result->desc)
                result->desc = cJSON_Print(output);
            }
            goto FAILURE;
        }
        FREE_CJSON(result->data)


    }

    // output to outputStr
    UtoolBuildDefaultSuccessResult(&(result->desc));
    goto DONE;

FAILURE:
    goto DONE;

DONE:
    FREE_CJSON(output)
    FREE_CJSON(payload)
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}

static void UpdateKvmService(UtoolRedfishServer *server, UtoolUpdateServiceOption *option, UtoolResult *result) {
    cJSON *payload = cJSON_CreateObject();
    cJSON *enabled = cJSON_AddBoolToObject(payload, "EncryptionEnabled",
                                           UtoolStringEquals(option->sslEnabled, ENABLED));
    result->code = UtoolAssetCreatedJsonNotNull(enabled);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    UtoolRedfishPatch(server, "/Managers/%s/KvmService", payload, NULL, NULL, NULL, result);
    if (result->broken) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)

    goto DONE;

FAILURE:
    result->broken = 1;
    goto DONE;

DONE:
    FREE_CJSON(payload)
}

static void
UpdateVirtualMediaService(UtoolRedfishServer *server, UtoolUpdateServiceOption *option, UtoolResult *result) {
    cJSON *payload = cJSON_CreateObject();
    cJSON *enabled = cJSON_AddBoolToObject(payload, "EncryptionEnabled",
                                           UtoolStringEquals(option->sslEnabled, ENABLED));
    result->code = UtoolAssetCreatedJsonNotNull(enabled);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *wrapped = UtoolWrapOem(server->oemName, payload, result);
    if (result->broken) {
        goto FAILURE;
    }
    payload = wrapped;

    UtoolRedfishPatch(server, "/Managers/%s/VirtualMedia/CD", payload, NULL, NULL, NULL, result);
    if (result->broken) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)

    goto DONE;

FAILURE:
    result->broken = 1;
    goto DONE;

DONE:
    FREE_CJSON(payload)
}


/**
* validate user input options for the command
*
* @param option
* @param result
* @return
*/
static void ValidateSubcommandOptions(UtoolUpdateServiceOption *option, UtoolResult *result) {
    if (UtoolStringIsEmpty(option->service)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_SERVICE_REQUIRED),
                                              &(result->desc));
        goto FAILURE;
    }

    if (!UtoolStringInArray(option->service, SERVICE_CHOICES)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_SERVICE_ILLEGAL),
                                              &(result->desc));
        goto FAILURE;
    }

    if (!UtoolStringIsEmpty(option->enabled)) {
        if (!UtoolStringInArray(option->enabled, g_UTOOL_ENABLED_CHOICES)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_ENABLED_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    if (!UtoolStringIsEmpty(option->sslEnabled)) {
        if (!UtoolStringInArray(option->sslEnabled, g_UTOOL_ENABLED_CHOICES)) {
            char *message = OPT_NOT_IN_CHOICE("ssl-enabled", "Enabled, Disabled");
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(message), &(result->desc));
            goto FAILURE;
        }

        if (!UtoolStringInArray(option->service, SUPPORT_SSL_SERVICES)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_SSL_NOT_SUPPORT),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    if (option->port != DEFAULT_INT_V) {
        if (option->port < 1 || option->port > 65535) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_PORT_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    if (option->port2 != DEFAULT_INT_V) {
        if (!UtoolStringEquals(option->service, SERVICE_IPMI)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_PORT2_NOT_SUPPORT),
                                                  &(result->desc));
            goto FAILURE;
        }

        if (option->port2 < 1 || option->port2 > 65535) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE,
                                                  cJSON_CreateString(OPT_NOT_IN_RANGE("port2", "1, 65535")),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    if (UtoolStringIsEmpty(option->enabled) && UtoolStringIsEmpty(option->sslEnabled) &&
        option->port == DEFAULT_INT_V) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(NOTHING_TO_PATCH), &(result->desc));
        goto FAILURE;
    }

    return;

FAILURE:
    result->broken = 1;
    return;
}

static cJSON *BuildPayload(UtoolRedfishServer *server, UtoolUpdateServiceOption *option, UtoolResult *result) {

    cJSON *payload = NULL;

    char *serviceName = UtoolStringEquals(option->service, SERVICE_KVM) ? "KVMIP" : option->service;

    /* IPMI port2 handler*/
    if (option->port2 != DEFAULT_INT_V) {
        cJSON *ipmiOemNode = cJSON_CreateObject();
        result->code = UtoolAssetCreatedJsonNotNull(ipmiOemNode);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }

        payload = UtoolWrapOem(server->oemName, ipmiOemNode, result);
        if (result->broken) {
            FREE_CJSON(ipmiOemNode)
            goto FAILURE;
        }

        cJSON *nest = cJSON_AddObjectToObject(ipmiOemNode, serviceName);
        result->code = UtoolAssetCreatedJsonNotNull(nest);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }

        cJSON *port2 = cJSON_AddNumberToObject(nest, "Port2", option->port2);
        result->code = UtoolAssetCreatedJsonNotNull(port2);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
    }
    else {
        payload = cJSON_CreateObject();
        result->code = UtoolAssetCreatedJsonNotNull(payload);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
    }

    cJSON *service = cJSON_AddObjectToObject(payload, serviceName);
    result->code = UtoolAssetCreatedJsonNotNull(service);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    if (!UtoolStringIsEmpty(option->enabled)) {
        cJSON *node = cJSON_AddBoolToObject(service, "ProtocolEnabled", UtoolStringEquals(option->enabled, ENABLED));
        result->code = UtoolAssetCreatedJsonNotNull(node);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
    }

    if (option->port != DEFAULT_INT_V) {
        cJSON *node = cJSON_AddNumberToObject(service, "Port", option->port);
        result->code = UtoolAssetCreatedJsonNotNull(node);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
    }

    if (UtoolStringInArray(serviceName, OEM_SERVICE_CHOICES)) {
        cJSON *wrapped = UtoolWrapOem(server->oemName, payload, result);
        if (result->broken) {
            goto FAILURE;
        }
        payload = wrapped;
    }

    return payload;

FAILURE:
    result->broken = 1;
    FREE_CJSON(payload)
    return NULL;
}