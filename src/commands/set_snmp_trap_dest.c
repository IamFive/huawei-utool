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

static const char *OPT_ENABLED_ILLEGAL = "Error: option `Enabled` is illegal, available choices: Enabled, Disabled";
static const char *OPT_ID_REQUIRED = "Error: option `DestinationId` is required";
static const char *OPT_ID_ILLEGAL = "Error: option `DestinationId` is illegal, available value range is: 1~4";

static const char *OPT_PORT_NUMBER_ILLEGAL = "Error: option `PortNumber` is illegal, available value range is: 1~65535";

static const char *const usage[] = {
        "utool settrapdest -d DestinationId [-a Address] [-p PortNumber] [-e Enabled]",
        NULL,
};

typedef struct _UpdateTrapNotifyDestOption
{
    int id;
    char *address;
    char *enabled;
    int portNumber;
} UtoolUpdateTrapNotifyDestOption;

static cJSON *BuildPayload(UtoolUpdateTrapNotifyDestOption *option, UtoolResult *result);

static void ValidateSubcommandOptions(UtoolUpdateTrapNotifyDestOption *option, UtoolResult *result);

/**
* set SNMP trap destination, command handler for `settrapdest`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdSetSNMPTrapNotificationDest(UtoolCommandOption *commandOption, char **outputStr)
{
    cJSON *payload = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolUpdateTrapNotifyDestOption *option = &(UtoolUpdateTrapNotifyDestOption) {.id=DEFAULT_INT_V, .portNumber=DEFAULT_INT_V};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_INTEGER('d', "DestinationId", &(option->id),
                        "specifies trap server ID, value range: 1~4", NULL, 0, 0),
            OPT_STRING ('a', "Address", &(option->address),
                        "specifies trap server address", NULL, 0, 0),
            OPT_INTEGER('p', "PortNumber", &(option->portNumber),
                        "specifies trap server port number, value range: 1~65535", NULL, 0, 0),
            OPT_STRING ('e', "Enabled", &(option->enabled),
                        "specifies whether this trap server is enabled, available choices: {Enabled, Disabled}",
                        NULL, 0, 0),
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

    UtoolRedfishPatch(server, "/Managers/%s/SnmpService", payload, NULL, NULL, NULL, result);
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
static void ValidateSubcommandOptions(UtoolUpdateTrapNotifyDestOption *option, UtoolResult *result)
{
    if (option->id != DEFAULT_INT_V) {
        if (option->id < 1 || option->id > 4) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_ID_ILLEGAL), &(result->desc));
            goto FAILURE;
        }
    }
    else {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_ID_REQUIRED), &(result->desc));
        goto FAILURE;
    }


    if (option->portNumber != DEFAULT_INT_V) {
        if (option->portNumber < 1 || option->portNumber > 65535) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_PORT_NUMBER_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    if (!UtoolStringIsEmpty(option->enabled)) {
        if (!UtoolStringInArray(option->enabled, UTOOL_ENABLED_CHOICES)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_ENABLED_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    if (UtoolStringIsEmpty(option->enabled) && option->portNumber == DEFAULT_INT_V &&
        UtoolStringIsEmpty(option->address)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(NOTHING_TO_PATCH), &(result->desc));
        goto FAILURE;
    }

    return;

FAILURE:
    result->interrupt = 1;
    return;
}

static cJSON *BuildPayload(UtoolUpdateTrapNotifyDestOption *option, UtoolResult *result)
{
    cJSON *payload = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *trap = cJSON_AddObjectToObject(payload, "SnmpTrapNotification");
    result->code = UtoolAssetCreatedJsonNotNull(trap);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *servers = cJSON_AddArrayToObject(trap, "TrapServer");
    result->code = UtoolAssetCreatedJsonNotNull(servers);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    for (int idx = 0; idx < 4; idx++) {
        cJSON *item = cJSON_CreateObject();
        result->code = UtoolAssetCreatedJsonNotNull(item);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }

        cJSON_AddItemToArray(servers, item);
        if (idx == (option->id - 1)) {
            /** add option properties */
            if (!UtoolStringIsEmpty(option->address)) {
                cJSON *node = cJSON_AddStringToObject(item, "TrapServerAddress", option->address);
                result->code = UtoolAssetCreatedJsonNotNull(node);
                if (result->code != UTOOLE_OK) {
                    goto FAILURE;
                }
            }

            if (option->portNumber != DEFAULT_INT_V) {
                cJSON *node = cJSON_AddNumberToObject(item, "TrapServerPort", option->portNumber);
                result->code = UtoolAssetCreatedJsonNotNull(node);
                if (result->code != UTOOLE_OK) {
                    goto FAILURE;
                }
            }

            if (!UtoolStringIsEmpty(option->enabled)) {
                cJSON *node = cJSON_AddBoolToObject(item, "Enabled", UtoolStringEquals(option->enabled, ENABLED));
                result->code = UtoolAssetCreatedJsonNotNull(node);
                if (result->code != UTOOLE_OK) {
                    goto FAILURE;
                }
            }
        }
    }

    return payload;

FAILURE:
    FREE_CJSON(payload)
    return NULL;
}