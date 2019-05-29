//
// Created by qianbiao on 5/8/19.
//
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "cJSON_Utils.h"
#include "commons.h"
#include "curl/curl.h"
#include "zf_log.h"
#include "constants.h"
#include "command-helps.h"
#include "command-interfaces.h"
#include "argparse.h"
#include "redfish.h"

static const char *const usage[] = {
        "utool getpwrcap",
        NULL,
};

static const UtoolOutputMapping getPowerCappingMappings[] = {
        // Enabled
        {.sourceXpath = "/PowerControl/0/PowerLimit/LimitInWatts", .targetKeyValue="LimitInWatts"},
        {.sourceXpath = "/PowerControl/0/PowerLimit/LimitException", .targetKeyValue="LimitException"},
        NULL
};


/**
* get power capping, command handler for `getpwrcap`.
*
* @param commandOption
* @param outputStr
* @return
*/
int UtoolCmdGetPowerCapping(UtoolCommandOption *commandOption, char **outputStr)
{
    int ret;
    cJSON *output = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    result->code = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    result->code = UtoolValidateConnectOptions(commandOption, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto done;
    }

    output = cJSON_CreateObject();
    ret = UtoolAssetCreatedJsonNotNull(output);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    UtoolRedfishGet(server, "/Chassis/%s/Power", output, getPowerCappingMappings, result);
    if (result->interrupt) {
        goto failure;
    }

    // mapping result to output json
    result->code = UtoolBuildOutputResult(STATE_SUCCESS, output, &(result->desc));
    goto done;

failure:
    FREE_CJSON(output)
    goto done;

done:
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}
