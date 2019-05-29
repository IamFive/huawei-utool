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
        "utool geteventlog",
        NULL,
};

static const UtoolOutputMapping getHealthEventMappings[] = {
        {.sourceXpath = "/Null", .targetKeyValue="Id"},
        {.sourceXpath = "/Severity", .targetKeyValue="Severity"},
        {.sourceXpath = "/Created", .targetKeyValue="EventTimestamp"},
        {.sourceXpath = "/EventSubject", .targetKeyValue="Entity"},
        {.sourceXpath = "/Message", .targetKeyValue="Message"},
        {.sourceXpath = "/MessageId", .targetKeyValue="MessageId"},
        {.sourceXpath = "/EventId", .targetKeyValue="EventId"},
        NULL
};

/**
 * get all event logs. command handler of `geteventlog`
 *
 * @param commandOption
 * @param result
 * @return
 */
int UtoolCmdGetEventLog(UtoolCommandOption *commandOption, char **result)
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
        goto done;
    }

    ret = UtoolValidateConnectOptions(commandOption, result);
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    output = cJSON_CreateObject();
    ret = UtoolAssetCreatedJsonNotNull(output);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    healthEvents = cJSON_AddArrayToObject(output, "HealthEvents");
    ret = UtoolAssetCreatedJsonNotNull(healthEvents);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    ret = UtoolGetRedfishServer(commandOption, server, result);
    if (ret != UTOOLE_OK || server->systemId == NULL) {
        goto done;
    }

    ret = UtoolMakeCurlRequest(server, "/Systems/%s/LogServices", HTTP_GET, NULL, NULL, getLogServices);
    if (ret != UTOOLE_OK) {
        goto done;
    }
    if (getLogServices->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getLogServices, result);
        goto done;
    }

    // process get log services response
    logServicesJson = cJSON_Parse(getLogServices->content);
    ret = UtoolAssetParseJsonNotNull(logServicesJson);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    cJSON *logService0 = cJSONUtils_GetPointer(logServicesJson, "/Members/0/@odata.id");
    ret = UtoolAssetJsonNodeNotNull(logService0, "/Members/0/@odata.id");
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    // get log service 0
    char *url = logService0->valuestring;
    ret = UtoolMakeCurlRequest(server, url, HTTP_GET, NULL, NULL, getLogService0Resp);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    if (getLogService0Resp->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getLogServices, result);
        goto failure;
    }

    // process get log services response
    logService0Json = cJSON_Parse(getLogService0Resp->content);
    ret = UtoolAssetParseJsonNotNull(logService0Json);
    if (ret != UTOOLE_OK) {
        goto failure;
    }


    cJSON *member = NULL;
    cJSON *members = cJSONUtils_GetPointer(logService0Json, "/Oem/Huawei/HealthEvent");
    cJSON_ArrayForEach(member, members) {
        healthEvent = cJSON_CreateObject();
        ret = UtoolAssetCreatedJsonNotNull(healthEvent);
        if (ret != UTOOLE_OK) {
            goto failure;
        }

        // create healthEvent item and add it to array
        ret = UtoolMappingCJSONItems(member, healthEvent, getHealthEventMappings);
        if (ret != UTOOLE_OK) {
            goto failure;
        }
        cJSON_AddItemToArray(healthEvents, healthEvent);
    }

    // output to result
    ret = UtoolBuildOutputResult(STATE_SUCCESS, output, result);
    goto done;

failure:
    FREE_CJSON(healthEvent)
    FREE_CJSON(output)
    goto done;

done:
    FREE_CJSON(logServicesJson)
    FREE_CJSON(logService0Json)
    UtoolFreeCurlResponse(getLogServices);
    UtoolFreeCurlResponse(getLogService0Resp);
    UtoolFreeRedfishServer(server);
    return ret;
}
