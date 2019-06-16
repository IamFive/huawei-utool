/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command hander for `getbios`
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
        "getbios",
        NULL,
};


/**
* get pending bios settings, command handler for `getbios`
*
* @param commandOption
* @param result
* @return
*/
int UtoolCmdGetBiosSettings(UtoolCommandOption *commandOption, char **result)
{
    int ret;
    cJSON *getBiosJson = NULL;

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

    // process get system response
    ret = UtoolMakeCurlRequest(server, "/Systems/%s/Bios", HTTP_GET, NULL, NULL, getSystemResponse);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    if (getSystemResponse->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getSystemResponse, result);
        goto FAILURE;
    }

    getBiosJson = cJSON_Parse(getSystemResponse->content);
    ret = UtoolAssetParseJsonNotNull(getBiosJson);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *attributes = cJSON_DetachItemFromObject(getBiosJson, "Attributes");
    ret = UtoolAssetJsonNodeNotNull(attributes, "/Attributes");
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    // mapping result to output json
    ret = UtoolBuildOutputResult(STATE_SUCCESS, attributes, result);
    goto DONE;

FAILURE:
    goto DONE;

DONE:
    FREE_CJSON(getBiosJson)
    UtoolFreeRedfishServer(server);
    UtoolFreeCurlResponse(getSystemResponse);
    return ret;
}
