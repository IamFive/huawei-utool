/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `settrapcom`
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

static const char *SEVERITY_CHOICES[] = {"All", "WarningAndCritical", "Critical", NULL};
static const char *OPT_ENABLED_ILLEGAL = "Error: option `enabled` is illegal, available choices: Enabled, Disabled";
static const char *OPT_SEVERITY_ILLEGAL = "Error: option `severity` is illegal, "
                                          "available choices: All, WarningAndCritical, Critical";

static const char *const usage[] = {
        "settrapcom [-c community] [-e enabled] [-s severity]",
        NULL,
};

typedef struct _UpdateSNMPTrapNotification
{
    char *bootDevice;
    char *effective;
    char *bootMode;
} UtoolUpdateSNMPTrapNotification;

static cJSON *BuildPayload(UtoolUpdateSNMPTrapNotification *option, UtoolResult *result);

static void ValidateSubcommandOptions(UtoolUpdateSNMPTrapNotification *option, UtoolResult *result);

/**
* set SNMP trap common configuration, enable,community etc, command handler for `settrapcom`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdSetSNMPTrapNotification(UtoolCommandOption *commandOption, char **outputStr)
{
    cJSON *payload = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolUpdateSNMPTrapNotification *option = &(UtoolUpdateSNMPTrapNotification) {0};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('c', "community", &(option->bootDevice),
                        "specifies community name", NULL, 0, 0),
            OPT_STRING ('e', "enabled", &(option->effective),
                        "specifies whether trap is enabled, available choices: {Enabled, Disabled}", NULL, 0, 0),
            OPT_STRING ('s', "severity", &(option->bootMode),
                        "specifies level of alarm to sent, available choices: {All, WarningAndCritical, Critical}",
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
static void ValidateSubcommandOptions(UtoolUpdateSNMPTrapNotification *option, UtoolResult *result)
{
    if (!UtoolStringIsEmpty(option->effective)) {
        if (!UtoolStringInArray(option->effective, g_UTOOL_ENABLED_CHOICES)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_ENABLED_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    if (!UtoolStringIsEmpty(option->bootMode)) {
        if (!UtoolStringInArray(option->bootMode, SEVERITY_CHOICES)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_SEVERITY_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    if (UtoolStringIsEmpty(option->effective) && UtoolStringIsEmpty(option->bootMode) &&
        UtoolStringIsEmpty(option->bootDevice)) {
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

    cJSON *trap = cJSON_AddObjectToObject(payload, "SnmpTrapNotification");
    result->code = UtoolAssetCreatedJsonNotNull(trap);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    if (!UtoolStringIsEmpty(option->effective)) {
        cJSON *node = cJSON_AddBoolToObject(trap, "ServiceEnabled", UtoolStringEquals(option->effective, ENABLED));
        result->code = UtoolAssetCreatedJsonNotNull(node);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
    }

    if (!UtoolStringIsEmpty(option->bootDevice)) {
        cJSON *node = cJSON_AddStringToObject(trap, "CommunityName", option->bootDevice);
        result->code = UtoolAssetCreatedJsonNotNull(node);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
    }

    if (!UtoolStringIsEmpty(option->bootMode)) {
        cJSON *node = NULL;
        if (UtoolStringEquals("Critical", option->bootMode)) {
            node = cJSON_AddStringToObject(trap, "AlarmSeverity", SEVERITY_CRITICAL);
        }
        else if (UtoolStringEquals("WarningAndCritical", option->bootMode)) {
            node = cJSON_AddStringToObject(trap, "AlarmSeverity", SEVERITY_MINOR);
        }
        else if (UtoolStringEquals("All", option->bootMode)) {
            node = cJSON_AddStringToObject(trap, "AlarmSeverity", SEVERITY_NORMAL);
        }

        result->code = UtoolAssetCreatedJsonNotNull(node);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
    }

    return payload;

FAILURE:
    FREE_CJSON(payload)
    return NULL;
}