/*
* Copyright © xFusion Digital Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for ``
* Author:
* Create: 2019-06-14
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


static const char *const usage[] = {
        "waittask -i TaskId",
        NULL,
};

/**
* test purpose
*
* @param commandOption
* @param outputStr
* @return
*/
int UtoolCmdWaitRedfishTask(UtoolCommandOption *commandOption, char **outputStr)
{
    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *response = &(UtoolCurlResponse) {0};

    char *taskId = NULL;
    cJSON *output = NULL, *cJSONTask = NULL;

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('i', "ID", &(taskId), "specifies the task id", NULL, 0, 0),
            OPT_END(),
    };

    // validation sub command options
    result->code = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    // validation connection options
    result->code = UtoolValidateConnectOptions(commandOption, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    // get redfish system id
    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto FAILURE;
    }

    char url[MAX_URL_LEN];
    UtoolWrapSecFmt(url, MAX_URL_LEN, MAX_URL_LEN - 1, "/TaskService/Tasks/%s", taskId);
    UtoolRedfishGet(server, url, NULL, NULL, result);
    if (result->broken) {
        goto FAILURE;
    }

    UtoolRedfishWaitUtilTaskFinished(server, result->data, result);
    cJSONTask = result->data;

    // output to result
    output = cJSON_CreateObject();
    result->code = UtoolMappingCJSONItems(server, result->data, output, g_UtoolGetTaskMappings);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    result->code = UtoolBuildOutputResult(STATE_SUCCESS, output, &(result->desc));
    goto DONE;

FAILURE:
    goto DONE;

DONE:
    FREE_CJSON(cJSONTask)
    UtoolFreeRedfishServer(server);
    UtoolFreeCurlResponse(response);

    *outputStr = result->desc;
    return result->code;
}
