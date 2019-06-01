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

#define STATE_ON "On"
#define STATE_OFF "Off"
#define STATE_BLINK "Blink"

static const char *INDICATOR_STATE_CHOICES[] = {STATE_ON, STATE_OFF, STATE_BLINK, NULL};
static const char *OPT_STATE_REQUIRED = "Error: option `LEDState` is required.";
static const char *OPT_STATE_ILLEGAL = "Error: option `LEDState` is illegal, available choices: On, Off, Blink.";
static const char *OPT_FEQ_ILLEGAL = "Error: option `Frequency` is illegal, value range should be: 1~255.";
static const char *OPT_FEQ_NO_USE = "Error: option `Frequency` should not be set when state is not Blink.";

static const char *const usage[] = {
        "utool locateserver -s LedState [-f Frequency]",
        NULL,
};


typedef struct _SetIndicatorOption
{
    char *state;
    int frequency;
} UtoolSetIndicatorOption;

static cJSON *BuildPayload(UtoolSetIndicatorOption *option, UtoolResult *result);

static void ValidateSubcommandOptions(UtoolSetIndicatorOption *option, UtoolResult *result);

/**
* locate the server by UID IndicatorLED, command handler for `locateserver`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdSetIndicatorLED(UtoolCommandOption *commandOption, char **outputStr)
{
    cJSON *payload = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolSetIndicatorOption *option = &(UtoolSetIndicatorOption) {.frequency = DEFAULT_INT_V};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('s', "LEDState", &(option->state),
                        "specifies indicator LED state, available choices: {On, Off, Blink}", NULL, 0, 0),
            OPT_INTEGER('f', "Frequency", &(option->frequency),
                        "specifies blink period (in seconds) when state is blink, value range: 1~255.",
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

    if (option->frequency == DEFAULT_INT_V) {
        char *url = "/Chassis/%s";
        UtoolRedfishPatch(server, url, payload, NULL, NULL, NULL, result);
    }
    else {
        char *url = "/Chassis/%s/Oem/Huawei/Actions/Chassis.ControlIndicatorLED";
        UtoolRedfishPost(server, url, payload, NULL, NULL, result);
    }

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
static void ValidateSubcommandOptions(UtoolSetIndicatorOption *option, UtoolResult *result)
{

    if (UtoolStringIsEmpty(option->state)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_STATE_REQUIRED), &(result->desc));
        goto FAILURE;
    }

    if (!UtoolStringInArray(option->state, INDICATOR_STATE_CHOICES)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_STATE_ILLEGAL), &(result->desc));
        goto FAILURE;
    }

    if (option->frequency != DEFAULT_INT_V) {
        if (option->frequency < 1 || option->frequency > 255) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_FEQ_ILLEGAL), &(result->desc));
            goto FAILURE;
        }
    }

    if (!UtoolStringEquals(STATE_BLINK, option->state) && option->frequency != DEFAULT_INT_V) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_FEQ_NO_USE), &(result->desc));
        goto FAILURE;
    }

    return;

FAILURE:
    result->interrupt = 1;
    return;
}

static cJSON *BuildPayload(UtoolSetIndicatorOption *option, UtoolResult *result)
{
    cJSON *payload = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    if (!UtoolStringIsEmpty(option->state)) {
        cJSON *node = NULL;
        if (UtoolStringEquals(STATE_ON, option->state)) {
            node = cJSON_AddStringToObject(payload, "IndicatorLED", INDICATOR_STATE_ON);
        }
        else if (UtoolStringEquals(STATE_OFF, option->state)) {
            node = cJSON_AddStringToObject(payload, "IndicatorLED", INDICATOR_STATE_OFF);
        }
        else if (UtoolStringEquals(STATE_BLINK, option->state)) {
            node = cJSON_AddStringToObject(payload, "IndicatorLED", INDICATOR_STATE_BLINKING);
        }

        result->code = UtoolAssetCreatedJsonNotNull(node);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
    }

    if (option->frequency != DEFAULT_INT_V) {
        cJSON *node = cJSON_AddNumberToObject(payload, "Duration", option->frequency);
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