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
#include "string_utils.h"

static const char *const usage[] = {
        "utool getvnc",
        NULL,
};


static int SessionModePropertyHandler(cJSON *target, const char *key, cJSON *node)
{
    if (cJSON_IsNull(node)) {
        cJSON_AddItemToObjectCS(target, key, node);
        return UTOOLE_OK;
    }

    char *sessionMode = node->valuestring;
    if (UtoolStringEquals(sessionMode, "Private")) {
        FREE_CJSON(node)
        cJSON_AddStringToObject(target, key, "Dedicated");
    }
    else if (UtoolStringEquals(sessionMode, "Shared")) {
        FREE_CJSON(node)
        cJSON_AddStringToObject(target, key, "Share");
    }
    else {
        cJSON_AddItemToObject(target, key, node);
    }

    return UTOOLE_OK;
}

static const UtoolOutputMapping getVNCMappings[] = {
        {.sourceXpath = "/KeyboardLayout", .targetKeyValue="KeyboardLayout"},
        {.sourceXpath = "/SessionTimeoutMinutes", .targetKeyValue="SessionTimeoutMinutes"},
        {.sourceXpath = "/SSLEncryptionEnabled", .targetKeyValue="SSLEncryptionEnabled"},
        {.sourceXpath = "/MaximumNumberOfSessions", .targetKeyValue="MaximumNumberOfSessions"},
        {.sourceXpath = "/NumberOfActivatedSessions", .targetKeyValue="NumberOfActivatedSessions"},
        {.sourceXpath = "/SessionMode", .targetKeyValue="SessionMode", .handle=SessionModePropertyHandler},
        NULL
};


/**
* get VNC setting, command handler for `getvnc`.
*
* @param commandOption
* @param outputStr
* @return
*/
int UtoolCmdGetVNC(UtoolCommandOption *commandOption, char **outputStr)
{
    int ret;
    cJSON *output = NULL, *getEthernetInterfacesRespJson = NULL;

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

    UtoolRedfishGet(server, "/Managers/%s/VncService", output, getVNCMappings, result);
    if (result->interrupt) {
        goto failure;
    }
    FREE_CJSON(result->data)

    // mapping result to output json
    result->code = UtoolBuildOutputResult(STATE_SUCCESS, output, &(result->desc));
    goto done;

failure:
    FREE_CJSON(output)
    goto done;

done:
    FREE_CJSON(getEthernetInterfacesRespJson)
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}
