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
        "utool gettaskstate",
        NULL,
};

/**
 * command handler of `getfw`
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

    ret = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, result);
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    ret = UtoolValidateConnectOptions(commandOption, result);
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    ret = UtoolGetRedfishServer(commandOption, server, result);
    if (ret != UTOOLE_OK || server->systemId == NULL) {
        goto done;
    }

    ret = UtoolMakeCurlRequest(server, "/TaskService/Tasks", HTTP_GET, NULL, NULL, getTasksResp);
    if (ret != UTOOLE_OK) {
        goto done;
    }
    if (getTasksResp->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getTasksResp, result);
        goto done;
    }

    // initialize output objects
    cJSON *taskJson = NULL, *taskMembersJson = NULL;
    cJSON *output = NULL, *tasks = NULL, *task = NULL;

    output = cJSON_CreateObject();
    ret = UtoolAssetCreatedJsonNotNull(output);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    tasks = cJSON_AddArrayToObject(output, "task");
    ret = UtoolAssetCreatedJsonNotNull(tasks);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    // process response
    taskMembersJson = cJSON_Parse(getTasksResp->content);
    ret = UtoolAssetParseJsonNotNull(taskMembersJson);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    cJSON *member = NULL;
    cJSON *members = cJSON_GetObjectItem(taskMembersJson, "Members");
    cJSON_ArrayForEach(member, members) {
        cJSON *odataIdNode = cJSON_GetObjectItem(member, "@odata.id");
        char *url = odataIdNode->valuestring;

        ret = UtoolMakeCurlRequest(server, url, HTTP_GET, NULL, NULL, getTaskResp);
        if (ret != UTOOLE_OK) {
            goto failure;
        }

        if (getTaskResp->httpStatusCode >= 400) {
            ret = UtoolResolveFailureResponse(getTaskResp, result);
            goto failure;
        }

        taskJson = cJSON_Parse(getTaskResp->content);
        ret = UtoolAssetParseJsonNotNull(taskJson);
        if (ret != UTOOLE_OK) {
            goto failure;
        }

        task = cJSON_CreateObject();
        ret = UtoolAssetCreatedJsonNotNull(task);
        if (ret != UTOOLE_OK) {
            goto failure;
        }

        // create task item and add it to array
        ret = UtoolMappingCJSONItems(taskJson, task, getTaskMappings);
        if (ret != UTOOLE_OK) {
            goto failure;
        }
        cJSON_AddItemToArray(tasks, task);

        // free memory
        FREE_CJSON(taskJson)
        UtoolFreeCurlResponse(getTaskResp);
    }

    // output to result
    ret = UtoolBuildOutputResult(STATE_SUCCESS, output, result);
    goto done;

failure:
    FREE_CJSON(task)
    FREE_CJSON(output)
    goto done;

done:
    FREE_CJSON(taskMembersJson)
    FREE_CJSON(taskJson)
    UtoolFreeCurlResponse(getTasksResp);
    UtoolFreeCurlResponse(getTaskResp);
    UtoolFreeRedfishServer(server);
    return ret;
}
