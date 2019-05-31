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

static const char *OPT_TIMEZONE_REQUIRED = "Error: option `DateTimeLocalOffset` is required.";

static const char *const usage[] = {
        "utool settimezone -z DateTimeLocalOffset",
        NULL,
};

static cJSON *BuildPayload(char *option, UtoolResult *result);

static void ValidateSubcommandOptions(char *timezone, UtoolResult *result);

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
    cJSON *output = NULL, *payload = NULL;

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('z', "DateTimeLocalOffset", &timezone,
                        "specifies the timezone(-12:00~+14:00) of the manager resource", NULL, 0, 0),
            OPT_END()
    };

    // validation
    result->code = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    result->code = UtoolValidateConnectOptions(commandOption, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    ValidateSubcommandOptions(timezone, result);
    if (result->interrupt) {
        goto done;
    }

    // build payload
    payload = BuildPayload(timezone, result);
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto failure;
    }

    // get redfish system id
    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto done;
    }

    output = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(output);
    if (result->code != UTOOLE_OK) {
        goto failure;
    }

    UtoolRedfishPatch(server, "/Managers/%s", payload, NULL, NULL, NULL, result);
    if (result->interrupt) {
        goto failure;
    }
    FREE_CJSON(result->data)

    // output to outputStr
    UtoolBuildDefaultSuccessResult(&(result->desc));
    goto done;

failure:
    FREE_CJSON(output)
    goto done;

done:
    FREE_CJSON(payload)
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}


/**
* validate user input options for setpwd command
*
* @param option
* @param result
* @return
*/
static void ValidateSubcommandOptions(char *timezone, UtoolResult *result)
{
    if (UtoolStringIsEmpty(timezone)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_TIMEZONE_REQUIRED),
                                              &(result->desc));
        goto failure;
    }
    return;

failure:
    result->interrupt = 1;
    return;
}

static cJSON *BuildPayload(char *timezone, UtoolResult *result)
{
    cJSON *payload = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto failure;
    }

    cJSON *node = cJSON_AddStringToObject(payload, "DateTimeLocalOffset", timezone);
    result->code = UtoolAssetCreatedJsonNotNull(node);
    if (result->code != UTOOLE_OK) {
        goto failure;
    }

    return payload;

failure:
    FREE_CJSON(payload)
    return NULL;
}