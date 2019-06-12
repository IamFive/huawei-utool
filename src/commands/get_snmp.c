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
        "utool gettrap",
        NULL,
};

static int SNMPSeverityMappingHandler(cJSON *target, const char *key, cJSON *node)
{
    if (cJSON_IsNull(node)) {
        cJSON_AddItemToObject(target, key, node);
        return UTOOLE_OK;
    }

    cJSON *newNode = NULL;
    if (UtoolStringEquals(node->valuestring, SEVERITY_CRITICAL)) {
        newNode = cJSON_AddStringToObject(target, key, "Critical");
    }
    else if (UtoolStringEquals(node->valuestring, SEVERITY_MAJOR) ||
             UtoolStringEquals(node->valuestring, SEVERITY_MINOR)) {
        newNode = cJSON_AddStringToObject(target, key, "WarningAndCritical");
    }
    else if (UtoolStringEquals(node->valuestring, SEVERITY_NORMAL)) {
        newNode = cJSON_AddStringToObject(target, key, "All");
    }
    FREE_CJSON(node)

    int ret = UtoolAssetCreatedJsonNotNull(newNode);
    return ret;
}

static const UtoolOutputMapping getTrapServerMappings[] = {
        {.sourceXpath = "/MemberId", .targetKeyValue="Id"},
        {.sourceXpath = "/Enabled", .targetKeyValue="Enabled", .handle=UtoolBoolToEnabledPropertyHandler},
        {.sourceXpath = "/TrapServerAddress", .targetKeyValue="Address"},
        {.sourceXpath = "/TrapServerPort", .targetKeyValue="Port"},
        NULL
};

static const UtoolOutputMapping getTrapNotificationMappings[] = {
        {.sourceXpath = "/SnmpTrapNotification/ServiceEnabled", .targetKeyValue="Enabled",
                .handle=UtoolBoolToEnabledPropertyHandler},
        {.sourceXpath = "/SnmpTrapNotification/TrapVersion", .targetKeyValue="TrapVersion"},
        {.sourceXpath = "/SnmpTrapNotification/CommunityName", .targetKeyValue="Community"},
        {.sourceXpath = "/SnmpTrapNotification/AlarmSeverity", .targetKeyValue="Severity",
                .handle=SNMPSeverityMappingHandler},
        {.sourceXpath = "/SnmpTrapNotification/TrapServer", .targetKeyValue="Destination",
                .nestMapping=getTrapServerMappings},
        NULL
};


/**
* get SNMP trap, command handler for `gettrap`.
*
* @param commandOption
* @param outputStr
* @return
*/
int UtoolCmdGetSNMP(UtoolCommandOption *commandOption, char **outputStr)
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

    UtoolRedfishGet(server, "/Managers/%s/SnmpService", output, getTrapNotificationMappings, result);
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
