/*
* Copyright © Huawei Technologies Co., Ltd. 2012-2018. All rights reserved.
* Description: command handler for `clearsel`
* Author:
* Create: 2019-06-16
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

static const char *const usage[] = {
        "clearsel",
        NULL,
};

/**
* clear SEL record, command handler for `clearsel`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdClearSEL(UtoolCommandOption *commandOption, char **outputStr)
{
    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};

    // initialize output objects
    cJSON *payload = NULL, *logServices = NULL;

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
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

    // get redfish system id
    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto DONE;
    }

    UtoolRedfishGet(server, "/Systems/%s/LogServices", NULL, NULL, result);
    if (result->broken) {
        goto FAILURE;
    }
    logServices = result->data;
    cJSON *logService0LinkNode = cJSONUtils_GetPointer(logServices, "/Members/0/@odata.id");
    result->code = UtoolAssetJsonNodeNotNull(logService0LinkNode, "/Members/0/@odata.id");
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    /** send clear SEL request */
    char clearSELUrl[MAX_URL_LEN];
    UtoolWrapSnprintf(clearSELUrl, MAX_URL_LEN, MAX_URL_LEN, "%s/Actions/LogService.ClearLog",
               logService0LinkNode->valuestring);
    payload = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    UtoolRedfishPost(server, clearSELUrl, payload, NULL, NULL, result);
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
    FREE_CJSON(logServices)
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}
