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
        "utool geteventsub",
        NULL,
};

static const UtoolOutputMapping getSubscriptionMappings[] = {
        {.sourceXpath = "/Id", .targetKeyValue="Id"},
        {.sourceXpath = "/Destination", .targetKeyValue="Destination"},
        {.sourceXpath = "/EventTypes", .targetKeyValue="EventTypes"},
        {.sourceXpath = "/HttpHeaders", .targetKeyValue="HttpHeaders"},
        {.sourceXpath = "/Protocol", .targetKeyValue="Protocol"},
        {.sourceXpath = "/Context", .targetKeyValue="Context"},
        {.sourceXpath = "/MessageIds", .targetKeyValue="MessageIds"},
        {.sourceXpath = "/OriginResources", .targetKeyValue="OriginResources"},
        NULL
};


/**
 * command handler of `getuser`
 * get BMC user information
 *
 * @param commandOption
 * @param outputStr
 * @return
 */
int UtoolCmdGetEventSubscriptions(UtoolCommandOption *commandOption, char **outputStr)
{
    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    // initialize output objects
    cJSON *subscriptionMemberJson = NULL, *output = NULL, *subscriptionArray = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};

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
    result->code = UtoolAssetCreatedJsonNotNull(output);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    subscriptionArray = cJSON_AddArrayToObject(output, "Subscriber");
    result->code = UtoolAssetCreatedJsonNotNull(subscriptionArray);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    UtoolRedfishGet(server, "/EventService/Subscriptions", NULL, NULL, result);
    if (result->interrupt) {
        goto FAILURE;
    }

    subscriptionMemberJson = result->data;
    UtoolRedfishGetMemberResources(server, subscriptionMemberJson, subscriptionArray, getSubscriptionMappings, result);
    if (result->interrupt) {
        goto FAILURE;
    }

    // output to outputStr
    result->code = UtoolBuildOutputResult(STATE_SUCCESS, output, &(result->desc));
    goto DONE;

FAILURE:
    FREE_CJSON(output)
    goto DONE;

DONE:
    FREE_CJSON(subscriptionMemberJson)
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}
