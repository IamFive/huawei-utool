/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `getsysboot`
* Author:
* Create: 2019-06-14
* Notes:
*/
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
        "getsysboot",
        NULL,
};

static const UtoolOutputMapping getProductMappings[] = {
        {.sourceXpath = "/Boot/BootSourceOverrideTarget", .targetKeyValue="BootDevice"},
        {.sourceXpath = "/Boot/BootSourceOverrideMode", .targetKeyValue="BootMode"},
        {.sourceXpath = "/Boot/BootSourceOverrideEnabled", .targetKeyValue="Effective"},
        NULL
};


/**
* get system boot up settings, command handler for `getsysboot`.
*
* @param commandOption
* @param result
* @return
*/
int UtoolCmdGetSystemBoot(UtoolCommandOption *commandOption, char **result)
{
    int ret;
    cJSON *output = NULL;
    cJSON *getSystemJson = NULL;

    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *getSystemResponse = &(UtoolCurlResponse) {0};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    ret = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, result);
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    ret = UtoolValidateConnectOptions(commandOption, result);
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }


    ret = UtoolGetRedfishServer(commandOption, server, result);
    if (ret != UTOOLE_OK || server->systemId == NULL) {
        goto DONE;
    }

    output = cJSON_CreateObject();
    ret = UtoolAssetCreatedJsonNotNull(output);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    // process get system response
    ret = UtoolMakeCurlRequest(server, "/Systems/%s", HTTP_GET, NULL, NULL, getSystemResponse);
    if (ret != UTOOLE_OK) {
        goto DONE;
    }

    if (getSystemResponse->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getSystemResponse, result);
        goto FAILURE;
    }

    getSystemJson = cJSON_Parse(getSystemResponse->content);
    ret = UtoolAssetParseJsonNotNull(getSystemJson);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }
    ret = UtoolMappingCJSONItems(server, getSystemJson, output, getProductMappings);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    // mapping result to output json
    ret = UtoolBuildOutputResult(STATE_SUCCESS, output, result);
    goto DONE;

FAILURE:
    FREE_CJSON(output)
    goto DONE;

DONE:
    FREE_CJSON(getSystemJson)
    UtoolFreeRedfishServer(server);
    UtoolFreeCurlResponse(getSystemResponse);
    return ret;
}
