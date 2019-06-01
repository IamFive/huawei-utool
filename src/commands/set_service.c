//
// Created by qianbiao on 5/8/19.
//
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

static const char *OEM_SERVICE_CHOICES[] = {"VNC", "Video", "NAT", NULL};
static const char *SERVICE_CHOICES[] = {"HTTP", "HTTPS", "SNMP", "VirtualMedia", "IPMI", "SSH", "KVMIP", "VNC",
                                        "Video", "NAT", "SSDP", NULL};

static const char *OPT_SERVICE_REQUIRED = "Error: option `Service` is required.";
static const char *OPT_SERVICE_ILLEGAL = "Error: option `Service` is illegal, available choices: "
                                         "HTTP, HTTPS, SNMP, VirtualMedia, IPMI, SSH, KVMIP, VNC, Video, NAT, SSDP.";
static const char *OPT_ENABLED_ILLEGAL = "Error: option `Enabled` is illegal, available choices: Enabled, Disabled";
static const char *OPT_PORT_ILLEGAL = "Error: option `Port` is illegal, value range should be: 1~65535";

static const char *const usage[] = {
        "utool setservice -s Service [-e Enabled] [-p Port] [-q PORT2] [-t SSLEnabled]",
        NULL,
};

typedef struct _UpdateServiceOption
{
    char *service;
    char *enabled;
    int port;
    int port2;
    char *sslEnabled;
} UtoolUpdateSNMPTrapNotification;

static cJSON *BuildPayload(UtoolUpdateSNMPTrapNotification *option, UtoolResult *result);

static void ValidateSubcommandOptions(UtoolUpdateSNMPTrapNotification *option, UtoolResult *result);

/**
* set BMC network protocol services, command handler for `setservice`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdSetService(UtoolCommandOption *commandOption, char **outputStr)
{
    cJSON *payload = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolUpdateSNMPTrapNotification *option = &(UtoolUpdateSNMPTrapNotification) {.port=DEFAULT_INT_V, 0};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('s', "Service", &(option->service),
                        "specifies service to update, available choices: {HTTP, HTTPS, SNMP, VirtualMedia, "
                        "IPMI, SSH, KVMIP, VNC, Video, NAT, SSDP}", NULL, 0, 0),
            OPT_STRING ('e', "Enabled", &(option->enabled),
                        "specifies whether the service is enabled, available choices: {Enabled, Disabled}", NULL, 0, 0),
            OPT_INTEGER('p', "Port", &(option->port),
                        "specifies service port, value range: 1~65535", NULL, 0, 0),
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
    if (result->interrupt) {
        goto DONE;
    }

    // build payload
    payload = BuildPayload(option, result);
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    // get redfish system id
    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto DONE;
    }

    UtoolRedfishPatch(server, "/Managers/%s/NetworkProtocol", payload, NULL, NULL, NULL, result);
    if (result->interrupt) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)

    // output to outputStr
    UtoolBuildDefaultSuccessResult(&(result->desc));
    goto DONE;

FAILURE:
    goto DONE;

DONE:
    FREE_CJSON(payload)
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}


/**
* validate user input options for the command
*
* @param option
* @param result
* @return
*/
static void ValidateSubcommandOptions(UtoolUpdateSNMPTrapNotification *option, UtoolResult *result)
{
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
        if (!UtoolStringInArray(option->enabled, UTOOL_ENABLED_CHOICES)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_ENABLED_ILLEGAL),
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

    if (UtoolStringIsEmpty(option->enabled) && option->port == DEFAULT_INT_V) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(NOTHING_TO_PATCH), &(result->desc));
        goto FAILURE;
    }

    return;

FAILURE:
    result->interrupt = 1;
    return;
}

static cJSON *BuildPayload(UtoolUpdateSNMPTrapNotification *option, UtoolResult *result)
{
    cJSON *payload = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *service = cJSON_AddObjectToObject(payload, option->service);
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

    if (UtoolStringInArray(option->service, OEM_SERVICE_CHOICES)) {
        payload = UtoolWrapOem(payload, result);
        if (result->interrupt) {
            goto FAILURE;
        }
    }

    return payload;

FAILURE:
    FREE_CJSON(payload)
    return NULL;
}