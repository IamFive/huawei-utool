/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `locateserver`
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

#define MODE_AUTOMATIC "Automatic"
#define MODE_MANUAL "Manual"

static const char *MODE_CHOICES[] = {MODE_AUTOMATIC, MODE_MANUAL, NULL};

static const char *OPT_FAN_ID_255 = "Error: option `fan-id` only supports 255(all fans) now.";


static const char *const usage[] = {
        "setfan -i fan-id [-m mode] [-s speed-level]",
        NULL,
};


typedef struct _SetIndicatorOption
{
    int fanId;
    char *mode;
    int speedLevel;
} UtoolSetIndicatorOption;

static cJSON *BuildPayload(UtoolSetIndicatorOption *option, UtoolResult *result);

static void ValidateSubcommandOptions(UtoolSetIndicatorOption *option, UtoolResult *result);

/**
* control fan, command handler for `setfan`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdSetFan(UtoolCommandOption *commandOption, char **outputStr)
{
    cJSON *payload = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolSetIndicatorOption *option = &(UtoolSetIndicatorOption) {
            .fanId = DEFAULT_INT_V,
            .speedLevel = DEFAULT_INT_V,
    };

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_INTEGER('i', "fan-id", &(option->fanId),
                        "specifies fan id to control, 255 for all fans.", NULL, 0, 0),
            OPT_STRING ('m', "mode", &(option->mode),
                        "specifies speed adjust mode, available choices: {Automatic, Manual}.", NULL, 0, 0),
            OPT_INTEGER('s', "speed-level", &(option->speedLevel),
                        "specifies speed level percent, value range: 20~100.",
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
    if (result->broken) {
        goto DONE;
    }

    // build payload
    payload = BuildPayload(option, result);
    if (result->broken) {
        goto FAILURE;
    }

    // get redfish system id
    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto DONE;
    }

    UtoolRedfishPatch(server, "/Chassis/%s/Thermal", payload, NULL, NULL, NULL, result);
    if (result->broken) {
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
static void ValidateSubcommandOptions(UtoolSetIndicatorOption *option, UtoolResult *result)
{

    if (option->fanId == DEFAULT_INT_V) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_REQUIRED("fan-id")),
                                              &(result->desc));
        goto FAILURE;
    }

    if (option->fanId != 255) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_FAN_ID_255), &(result->desc));
        goto FAILURE;
    }

    if (!UtoolStringIsEmpty(option->mode)) {
        if (!UtoolStringInArray(option->mode, MODE_CHOICES)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE,
                                                  cJSON_CreateString(OPT_NOT_IN_CHOICE("mode", "Automatic, Manual")),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    if (option->speedLevel != DEFAULT_INT_V) {
        if (option->speedLevel < 20 || option->speedLevel > 100) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE,
                                                  cJSON_CreateString(OPT_NOT_IN_RANGE("speed-level", "20~100")),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    if (UtoolStringIsEmpty(option->mode) && option->speedLevel == DEFAULT_INT_V) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(NOTHING_TO_PATCH), &(result->desc));
        goto FAILURE;
    }

    return;

FAILURE:
    result->broken = 1;
    return;
}

static cJSON *BuildPayload(UtoolSetIndicatorOption *option, UtoolResult *result)
{
    cJSON *payload = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    if (!UtoolStringIsEmpty(option->mode)) {
        cJSON *mode = cJSON_AddStringToObject(payload, "FanSpeedAdjustmentMode", option->mode);
        result->code = UtoolAssetCreatedJsonNotNull(mode);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
    }

    if (option->speedLevel != DEFAULT_INT_V) {
        cJSON *node = cJSON_AddNumberToObject(payload, "FanSpeedLevelPercents", option->speedLevel);
        result->code = UtoolAssetCreatedJsonNotNull(node);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
    }

    cJSON *wrapped = UtoolWrapOem(payload, result);
    if (result->broken) {
        goto FAILURE;
    }
    payload = wrapped;
    return payload;

FAILURE:
    result->broken = 1;
    FREE_CJSON(payload)
    return NULL;
}