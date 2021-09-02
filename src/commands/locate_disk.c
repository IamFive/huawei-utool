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
#include <securec.h>
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

#define STATE_ON "On"
#define STATE_OFF "Off"
#define STATE_BLINK "Blink"

static const char *INDICATOR_STATE_CHOICES[] = {STATE_ON, STATE_OFF, NULL};

static const char *const usage[] = {
        "locatedisk -i disk-id -s led-state",
        NULL,
};


typedef struct _SetDiskIndicatorOption {
    char *state;
    char *diskId;
} SetDiskIndicatorOption;

static cJSON *BuildPayload(SetDiskIndicatorOption *option, UtoolResult *result);

static void ValidateSubcommandOptions(SetDiskIndicatorOption *option, UtoolResult *result);

/**
* locate the server by UID IndicatorLED, command handler for `locateserver`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdLocateDisk(UtoolCommandOption *commandOption, char **outputStr)
{
    cJSON *payload = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    SetDiskIndicatorOption *option = &(SetDiskIndicatorOption) {0};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('i', "disk-id", &(option->diskId), "specifies the disk to locate.", NULL, 0, 0),
            OPT_STRING ('s', "led-state", &(option->state),
                        "specifies disk led state, available choices: {On, Off}", NULL, 0, 0),
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

    char url[MAX_URL_LEN];
    UtoolWrapSecFmt(url, MAX_URL_LEN, MAX_URL_LEN - 1, "/redfish/v1/Chassis/%s/Drives/%s", server->systemId,
                    option->diskId);
    UtoolRedfishPatch(server, url, payload, NULL, NULL, NULL, result);

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
static void ValidateSubcommandOptions(SetDiskIndicatorOption *option, UtoolResult *result)
{
    if (UtoolStringIsEmpty(option->diskId)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_REQUIRED("disk-id")),
                                              &(result->desc));
        goto FAILURE;
    }

    if (UtoolStringIsEmpty(option->state)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_REQUIRED("led-state")),
                                              &(result->desc));
        goto FAILURE;
    }

    if (!UtoolStringInArray(option->state, INDICATOR_STATE_CHOICES)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE,
                                              cJSON_CreateString(OPT_NOT_IN_CHOICE("led-state", "On, Off")),
                                              &(result->desc));
        goto FAILURE;
    }


    return;

FAILURE:
    result->broken = 1;
    return;
}

static cJSON *BuildPayload(SetDiskIndicatorOption *option, UtoolResult *result)
{
    cJSON *payload = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *node = NULL;
    if (UtoolStringEquals(STATE_ON, option->state)) {
        node = cJSON_AddStringToObject(payload, "IndicatorLED", INDICATOR_STATE_BLINKING);
    } else if (UtoolStringEquals(STATE_OFF, option->state)) {
        node = cJSON_AddStringToObject(payload, "IndicatorLED", INDICATOR_STATE_OFF);
    }
    result->code = UtoolAssetCreatedJsonNotNull(node);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    return payload;

FAILURE:
    result->broken = 1;
    FREE_CJSON(payload)
    return NULL;
}