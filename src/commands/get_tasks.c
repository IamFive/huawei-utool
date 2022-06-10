/*
* Copyright © xFusion Digital Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `gettaskstate`
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
        "gettaskstate",
        NULL,
};

/**
 * get tasks information, command handler of `gettaskstate`
 *
 * @param commandOption
 * @param result
 * @return
 */
int UtoolCmdGetTasks(UtoolCommandOption *commandOption, char **result)
{
    int ret;

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *getTasksResp = &(UtoolCurlResponse) {0};
    UtoolCurlResponse *getTaskResp = &(UtoolCurlResponse) {0};

    // initialize output objects
    cJSON *taskJson = NULL, *taskMembersJson = NULL;
    cJSON *output = NULL, *tasks = NULL, *task = NULL;

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

    ret = UtoolMakeCurlRequest(server, "/TaskService/Tasks", HTTP_GET, NULL, NULL, getTasksResp);
    if (ret != UTOOLE_OK) {
        goto DONE;
    }
    if (getTasksResp->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getTasksResp, result);
        goto DONE;
    }

    output = cJSON_CreateObject();
    ret = UtoolAssetCreatedJsonNotNull(output);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    tasks = cJSON_AddArrayToObject(output, "task");
    ret = UtoolAssetCreatedJsonNotNull(tasks);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    // process response
    taskMembersJson = cJSON_Parse(getTasksResp->content);
    ret = UtoolAssetParseJsonNotNull(taskMembersJson);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *member = NULL;
    cJSON *members = cJSON_GetObjectItem(taskMembersJson, "Members");
    cJSON_ArrayForEach(member, members) {
        cJSON *odataIdNode = cJSON_GetObjectItem(member, "@odata.id");
        char *url = odataIdNode->valuestring;

        ret = UtoolMakeCurlRequest(server, url, HTTP_GET, NULL, NULL, getTaskResp);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }

        if (getTaskResp->httpStatusCode >= 400) {
            ret = UtoolResolveFailureResponse(getTaskResp, result);
            goto FAILURE;
        }

        taskJson = cJSON_Parse(getTaskResp->content);
        ret = UtoolAssetParseJsonNotNull(taskJson);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }

        task = cJSON_CreateObject();
        ret = UtoolAssetCreatedJsonNotNull(task);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }

        // create task item and add it to array
        ret = UtoolMappingCJSONItems(server, taskJson, task, g_UtoolGetTaskMappings);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }
        cJSON_AddItemToArray(tasks, task);

        // free memory
        FREE_CJSON(taskJson)
        UtoolFreeCurlResponse(getTaskResp);
    }

    // output to result
    ret = UtoolBuildOutputResult(STATE_SUCCESS, output, result);
    goto DONE;

FAILURE:
    FREE_CJSON(task)
    FREE_CJSON(output)
    goto DONE;

DONE:
    FREE_CJSON(taskMembersJson)
    FREE_CJSON(taskJson)
    UtoolFreeCurlResponse(getTasksResp);
    UtoolFreeCurlResponse(getTaskResp);
    UtoolFreeRedfishServer(server);
    return ret;
}
