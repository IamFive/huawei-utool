/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `gethealthevent`
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

static const char *const usage[] = {
        "gethealthevent",
        NULL,
};

static const UtoolOutputMapping getHealthEventMappings[] = {
        {.sourceXpath = "/Null", .targetKeyValue="Id"},
        {.sourceXpath = "/Severity", .targetKeyValue="Severity"},
        {.sourceXpath = "/Created", .targetKeyValue="EventTimestamp"},
        {.sourceXpath = "/EventSubject", .targetKeyValue="Entity"},
        {.sourceXpath = "/Message", .targetKeyValue="Message"},
        {.sourceXpath = "/EventId", .targetKeyValue="EventId"},
        NULL
};

/**
 * get health events, command handler of `gethealthevent`
 *
 * @param commandOption
 * @param result
 * @return
 */
int UtoolCmdGetHealthEvent(UtoolCommandOption *commandOption, char **result)
{
    int ret;

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *getLogServices = &(UtoolCurlResponse) {0};
    UtoolCurlResponse *getLogService0Resp = &(UtoolCurlResponse) {0};

    // initialize output objects
    cJSON *logService0Json = NULL, *logServicesJson = NULL;
    cJSON *output = NULL, *healthEvents = NULL, *healthEvent = NULL;

    ret = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, result);
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    ret = UtoolValidateConnectOptions(commandOption, result);
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    output = cJSON_CreateObject();
    ret = UtoolAssetCreatedJsonNotNull(output);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    healthEvents = cJSON_AddArrayToObject(output, "HealthEvents");
    ret = UtoolAssetCreatedJsonNotNull(healthEvents);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    ret = UtoolGetRedfishServer(commandOption, server, result);
    if (ret != UTOOLE_OK || server->systemId == NULL) {
        goto DONE;
    }

    ret = UtoolMakeCurlRequest(server, "/Systems/%s/LogServices", HTTP_GET, NULL, NULL, getLogServices);
    if (ret != UTOOLE_OK) {
        goto DONE;
    }
    if (getLogServices->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getLogServices, result);
        goto DONE;
    }

    // process get log services response
    logServicesJson = cJSON_Parse(getLogServices->content);
    ret = UtoolAssetParseJsonNotNull(logServicesJson);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *logService0 = cJSONUtils_GetPointer(logServicesJson, "/Members/0/@odata.id");
    ret = UtoolAssetJsonNodeNotNull(logService0, "/Members/0/@odata.id");
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    // get log service 0
    char *url = logService0->valuestring;
    ret = UtoolMakeCurlRequest(server, url, HTTP_GET, NULL, NULL, getLogService0Resp);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    if (getLogService0Resp->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getLogServices, result);
        goto FAILURE;
    }

    // process get log services response
    logService0Json = cJSON_Parse(getLogService0Resp->content);
    ret = UtoolAssetParseJsonNotNull(logService0Json);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }


    cJSON *member = NULL;

    char healthEventXpath[MAX_XPATH_LEN] = {0};
    UtoolWrapSnprintf(healthEventXpath, MAX_XPATH_LEN, MAX_XPATH_LEN - 1, "/Oem/%s/HealthEvent", server->oemName);
    cJSON *members = cJSONUtils_GetPointer(logService0Json, healthEventXpath);
    cJSON_ArrayForEach(member, members) {
        healthEvent = cJSON_CreateObject();
        ret = UtoolAssetCreatedJsonNotNull(healthEvent);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }

        // create healthEvent item and add it to array
        ret = UtoolMappingCJSONItems(server, member, healthEvent, getHealthEventMappings);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }
        cJSON_AddItemToArray(healthEvents, healthEvent);
    }

    // output to result
    ret = UtoolBuildOutputResult(STATE_SUCCESS, output, result);
    goto DONE;

FAILURE:
    FREE_CJSON(healthEvent)
    FREE_CJSON(output)
    goto DONE;

DONE:
    FREE_CJSON(logServicesJson)
    FREE_CJSON(logService0Json)
    UtoolFreeCurlResponse(getLogServices);
    UtoolFreeCurlResponse(getLogService0Resp);
    UtoolFreeRedfishServer(server);
    return ret;
}
