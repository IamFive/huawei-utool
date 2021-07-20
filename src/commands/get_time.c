/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `getproduct`
* Author:
* Create: 2019-06-14
* Notes:
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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

#define LEN_TIME 19

static const char *const usage[] = {
        "gettime",
        NULL,
};

static int TimePropertyHandler(UtoolRedfishServer *server, cJSON *target, const char *key, cJSON *node) {
    char time[32] = {0};
    if (cJSON_IsString(node)) {
        snprintf_s(time, 32, LEN_TIME, "%s", node->valuestring);
        time[10] = ' ';

        FREE_CJSON(node)
        cJSON *newNode = cJSON_AddStringToObject(target, key, time);
        return UtoolAssetCreatedJsonNotNull(newNode);
    }

    cJSON_AddItemToObject(target, key, node);
    return UTOOLE_OK;
}

static int TimezonePropertyHandler(UtoolRedfishServer *server, cJSON *target, const char *key, cJSON *node) {
    if (cJSON_IsString(node)) {
        cJSON *newNode = cJSON_AddStringToObject(target, key, (char *) (node->valuestring + 19));
        FREE_CJSON(node)
        return UtoolAssetCreatedJsonNotNull(newNode);
    }

    cJSON_AddItemToObject(target, key, node);
    return UTOOLE_OK;
}


static const UtoolOutputMapping getTimeInfoMappings[] = {
        {.sourceXpath = "/DateTime", .targetKeyValue="Time", .handle=TimePropertyHandler},
        {.sourceXpath = "/DateTime", .targetKeyValue="Timezone", .handle=TimezonePropertyHandler},
        NULL
};

/*{
"Time":"2019-04-11 10:20:55",
"Timezone":"+08:00"
}*/


/**
* get BMC time and timezone, command handler for `gettime`
*
* @param commandOption
* @param outputStr
* @return
*/
int UtoolCmdGetTime(UtoolCommandOption *commandOption, char **outputStr) {
    cJSON *output = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    result->code = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    result->code = UtoolValidateConnectOptions(commandOption, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto DONE;
    }

    output = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(output);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    UtoolRedfishGet(server, "/Managers/%s", output, getTimeInfoMappings, result);
    if (result->broken) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)

    result->code = UtoolBuildOutputResult(STATE_SUCCESS, output, &(result->desc));
    goto DONE;

FAILURE:
    FREE_CJSON(output)
    goto DONE;

DONE:
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}
