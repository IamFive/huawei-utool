/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler of `geteventlog`
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
        "geteventlog",
        NULL,
};


static const UtoolOutputMapping getEventMappings[] = {
        {.sourceXpath = "/eventid", .targetKeyValue="Id"},
        {.sourceXpath = "/level", .targetKeyValue="Severity"},
        {.sourceXpath = "/alerttime", .targetKeyValue="EventTimestamp"},
        {.sourceXpath = "/subjecttype", .targetKeyValue="Entity"},
        {.sourceXpath = "/Null", .targetKeyValue="EntitySN"},
        {.sourceXpath = "/eventsubject", .targetKeyValue="Message"},
        {.sourceXpath = "/NUll", .targetKeyValue="MessageId"},
        {.sourceXpath = "/eventcode", .targetKeyValue="EventId"},
        {.sourceXpath = "/status", .targetKeyValue="Status"},
        NULL
};

static const UtoolOutputMapping getEventArrayMappings[] = {
        {.sourceXpath = "/error/@Message.ExtendedInfo/0/Oem/${Oem}/SelLogEntries", .targetKeyValue="EventLog",
                .nestMapping=getEventMappings},
        NULL
};


/**
 * get all event logs. command handler of `geteventlog`
 *
 * @param commandOption
 * @param result
 * @return
 */
int UtoolCmdGetEventLogBak(UtoolCommandOption *commandOption, char **outputStr)
{
    int ret;
    cJSON *output = NULL, *getLogServicesJson = NULL, *payload = NULL;

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
    ret = UtoolAssetCreatedJsonNotNull(output);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    UtoolRedfishGet(server, "/Systems/%s/LogServices", NULL, NULL, result);
    if (result->broken) {
        goto FAILURE;
    }
    getLogServicesJson = result->data;

    cJSON *logService0 = cJSONUtils_GetPointer(getLogServicesJson, "/Members/0/@odata.id");
    ret = UtoolAssetJsonNodeNotNull(logService0, "/Members/0/@odata.id");
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    // get log service 0
    char querySelLogUrl[MAX_URL_LEN];
    char *log0Url = logService0->valuestring;
    UtoolWrapSnprintf(querySelLogUrl, MAX_URL_LEN, MAX_URL_LEN - 1,
                      "%s/Actions/Oem/${Oem}/LogService.QuerySelLogEntries",
                      log0Url);

    /** query sel */
    int currentEntryId = 1;
    payload = cJSON_CreateObject();
    cJSON_AddNumberToObject(payload, "StartEntryId", currentEntryId);
    cJSON_AddNumberToObject(payload, "EntriesCount", 32);
    UtoolRedfishPost(server, querySelLogUrl, payload, output, getEventArrayMappings, result);
    if (result->broken) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)

    // mapping result to output json
    result->code = UtoolBuildOutputResult(STATE_SUCCESS, output, &(result->desc));
    goto DONE;

FAILURE:
    FREE_CJSON(output)
    goto DONE;

DONE:
    FREE_CJSON(payload)
    FREE_CJSON(getLogServicesJson)
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}
