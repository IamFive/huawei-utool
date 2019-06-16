/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `getmgmtport`
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

static const char *const usage[] = {
        "getmgmtport",
        NULL,
};

static int AdaptivePortFilter(cJSON *node)
{
    if (cJSON_IsNull(node)) {
        return 0;
    }

    cJSON *flag = cJSON_GetObjectItem(node, "AdaptiveFlag");
    return cJSON_IsTrue(flag) ? 1 : 0;
}


static const UtoolOutputMapping getAllowableValuesNestMappings[] = {
        {.sourceXpath = "/Type", .targetKeyValue="Type"},
        {.sourceXpath = "/PortNumber", .targetKeyValue = "PortNumber"},
        {.sourceXpath = "/LinkStatus", .targetKeyValue = "LinkStatus"},
        {.sourceXpath = "/Null", .targetKeyValue = "Location"},
        NULL
};

static const UtoolOutputMapping getAdaptivePortMappings[] = {
        {.sourceXpath = "/Type", .targetKeyValue="Type"},
        {.sourceXpath = "/PortNumber", .targetKeyValue = "PortNumber"},
        {.sourceXpath = "/Null", .targetKeyValue = "Location"},
        NULL
};

static const UtoolOutputMapping getMgmtPortMappings[] = {
        {.sourceXpath = "/Oem/Huawei/AdaptivePort", .targetKeyValue="AdaptivePort",
                .filter=AdaptivePortFilter, .nestMapping = getAdaptivePortMappings},
        {.sourceXpath = "/Oem/Huawei/ManagementNetworkPort@Redfish.AllowableValues",
                .targetKeyValue = "AllowablePorts", .nestMapping = getAllowableValuesNestMappings},
        NULL
};


/**
* get management port mode and specified port, command handler for `getmgmtport`.
*
* @param commandOption
* @param outputStr
* @return
*/
int UtoolCmdGetMgmtPort(UtoolCommandOption *commandOption, char **outputStr)
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

    UtoolRedfishGet(server, "/Managers/%s/EthernetInterfaces", NULL, NULL, result);
    if (result->interrupt) {
        goto FAILURE;
    }
    getEthernetInterfacesRespJson = result->data;

    cJSON *linkNode = cJSONUtils_GetPointer(getEthernetInterfacesRespJson, "/Members/0/@odata.id");
    char *url = linkNode->valuestring;

    UtoolRedfishGet(server, url, output, getMgmtPortMappings, result);
    if (result->interrupt) {
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
    FREE_CJSON(getEthernetInterfacesRespJson)
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}
