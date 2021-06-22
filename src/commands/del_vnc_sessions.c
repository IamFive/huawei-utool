/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command hander for `delvncsession`
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

static const char *VNCSessionUserTag = "VNC";
static const UtoolOutputMapping getSessionsMapping[] = {
        {.sourceXpath = "/@odata.id", .targetKeyValue="Url"},
        {.sourceXpath = "/Oem/${Oem}/UserTag", .targetKeyValue="UserTag"},
        NULL
};


static const char *const usage[] = {
        "delvncsession",
        NULL,
};


/**
* delete all VNC sessions, command handler for `delvncsession`
*
* @param commandOption
* @param outputStr
* @result->codeurn
* */
int UtoolCmdDeleteVNCSession(UtoolCommandOption *commandOption, char **outputStr)
{
    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};

    cJSON *sessionMembersResJson = NULL, *sessions = NULL;


    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
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
        goto FAILURE;
    }

    /** get all session members */
    UtoolRedfishGet(server, "/SessionService/Sessions", NULL, NULL, result);
    if (result->broken) {
        goto FAILURE;
    }
    sessionMembersResJson = result->data;

    sessions = cJSON_CreateArray();
    result->code = UtoolAssetCreatedJsonNotNull(sessions);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    UtoolRedfishGetMemberResources(server, sessionMembersResJson, sessions, getSessionsMapping, result);
    if (result->broken) {
        goto FAILURE;
    }

    cJSON *session = NULL;
    cJSON_ArrayForEach(session, sessions) {
        cJSON *tag = cJSON_GetObjectItem(session, "UserTag");
        result->code = UtoolAssetJsonNodeNotNull(tag, "/Oem/${Oem}/UserTag");
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }

        if (UtoolStringEquals(VNCSessionUserTag, tag->valuestring)) {
            cJSON *url = cJSON_GetObjectItem(session, "Url");
            result->code = UtoolAssetJsonNodeNotNull(url, "/@odata.id");
            if (result->code != UTOOLE_OK) {
                goto FAILURE;
            }

            UtoolRedfishDelete(server, url->valuestring, NULL, NULL, result);
            if (result->broken) {
                goto FAILURE;
            }

            FREE_CJSON(result->data);
        }
    }

    // output to outputStr
    UtoolBuildDefaultSuccessResult(&(result->desc));
    goto DONE;

FAILURE:
    goto DONE;

DONE:
    FREE_CJSON(sessions)
    FREE_CJSON(sessionMembersResJson)
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}
