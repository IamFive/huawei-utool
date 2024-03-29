/*
* Copyright © xFusion Digital Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `settimezone`
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

static const char *OPT_TIMEZONE_REQUIRED = "Error: option `datetime-local-offset` is required.";

static const char *const usage[] = {
        "settimezone -z datetime-local-offset",
        NULL,
};

static cJSON *BuildPayload(const char *option, UtoolResult *result);

static void ValidateSubcommandOptions(const char *timezone, UtoolResult *result);

/**
* set BMC time zone, command handler for `settimezone`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdSetTimezone(UtoolCommandOption *commandOption, char **outputStr)
{
    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};

    char *timezone = NULL;
    // initialize output objects
    cJSON *payload = NULL;

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('z', "datetime-local-offset", &timezone,
                        "specifies the timezone(-12:00~+14:00) of the manager resource", NULL, 0, 0),
            OPT_END()
    };

    // validation
    result->code = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    result->code = UtoolValidateConnectOptions(commandOption, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    ValidateSubcommandOptions(timezone, result);
    if (result->broken) {
        goto DONE;
    }

    // build payload
    payload = BuildPayload(timezone, result);
    if (result->broken) {
        goto FAILURE;
    }

    // get redfish system id
    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto DONE;
    }

    UtoolRedfishPatch(server, "/Managers/%s", payload, NULL, NULL, NULL, result);
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
static void ValidateSubcommandOptions(const char *timezone, UtoolResult *result)
{
    if (UtoolStringIsEmpty(timezone)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_TIMEZONE_REQUIRED),
                                              &(result->desc));
        goto FAILURE;
    }
    return;

FAILURE:
    result->broken = 1;
    return;
}

static cJSON *BuildPayload(const char *timezone, UtoolResult *result)
{
    cJSON *payload = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *node = cJSON_AddStringToObject(payload, "DateTimeLocalOffset", timezone);
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