/*
* Copyright © xFusion Digital Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `getraid`
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
#include "string_utils.h"

static const char *const usage[] = {
        "getraid",
        NULL,
};

static int SupportedRAIDLevelsHandler(UtoolRedfishServer *server, cJSON *target, const char *key, cJSON *node)
{
    if (cJSON_IsNull(node)) { // should not happen
        cJSON_AddItemToObjectCS(target, key, node);
        return UTOOLE_OK;
    }

    if (cJSON_IsArray(node)) {
        char buffer[256] = {'\0'};
        cJSON *next = node->child;
        while (next != NULL) {
            UtoolWrapStringNAppend(buffer, 256, next->valuestring, strnlen(next->valuestring, 32));
            UtoolWrapStringAppend(buffer, 256, ",");
            next = next->next;
        }
        buffer[strnlen(buffer, 256) - 1] = '\0';

        FREE_CJSON(node)
        cJSON *newNode = cJSON_AddStringToObject(target, key, buffer);
        return UtoolAssetCreatedJsonNotNull(newNode);
    } else {
        FREE_CJSON(node)
        return UTOOLE_UNKNOWN_JSON_FORMAT;
    }

}

static const UtoolOutputMapping getRAIDControllerMappings[] = {
        {.sourceXpath = "/MemberId", .targetKeyValue="Id"},
        {.sourceXpath = "/Manufacturer", .targetKeyValue="Manufacturer"},
        {.sourceXpath = "/Model", .targetKeyValue="Model"},
        {.sourceXpath = "/SupportedDeviceProtocols", .targetKeyValue="SupportedDeviceProtocols"}, //
        {.sourceXpath = "/Oem/${Oem}/SASAddress", .targetKeyValue="SASAddress"},
        {.sourceXpath = "/Oem/${Oem}/ConfigurationVersion", .targetKeyValue="ConfigurationVersion"},
        {.sourceXpath = "/Oem/${Oem}/MaintainPDFailHistory", .targetKeyValue="MaintainPDFailHistory"},
        {.sourceXpath = "/Oem/${Oem}/CopyBackState", .targetKeyValue="CopyBackState"}, //
        {.sourceXpath = "/Oem/${Oem}/JBODState", .targetKeyValue="JBODState"},
        {.sourceXpath = "/Oem/${Oem}/MinStripeSizeBytes", .targetKeyValue="MinStripeSizeBytes"},
        {.sourceXpath = "/Oem/${Oem}/MaxStripeSizeBytes", .targetKeyValue="MaxStripeSizeBytes"},
        {.sourceXpath = "/Oem/${Oem}/MemorySizeMiB", .targetKeyValue="MemorySizeMiB"},
        {.sourceXpath = "/Oem/${Oem}/SupportedRAIDLevels", .targetKeyValue="SupportedRAIDLevels"},
        {.sourceXpath = "/Oem/${Oem}/DDRECCCount", .targetKeyValue="DDRECCCount"},
        {.sourceXpath = "/FirmwareVersion", .targetKeyValue="FirmwareVersion"},
        NULL
};


static const UtoolOutputMapping getRAIDMappings[] = {
        {.sourceXpath = "/Name", .targetKeyValue="CommonName"},
        {.sourceXpath = "/Null", .targetKeyValue="Location"},
        {.sourceXpath = "/Null", .targetKeyValue="Manufacturer"},
        {.sourceXpath = "/Null", .targetKeyValue="Model"},
        {.sourceXpath = "/Null", .targetKeyValue="SerialNumber"},
        {.sourceXpath = "/Null", .targetKeyValue="State"},
        {.sourceXpath = "/Null", .targetKeyValue="Health"},
        {.sourceXpath = "/StorageControllers", .targetKeyValue="Controller", .nestMapping=getRAIDControllerMappings},
        NULL
};

static const UtoolOutputMapping getRAIDSummaryMappings[] = {
        {.sourceXpath = "/Oem/${Oem}/StorageSummary/Status/HealthRollup", .targetKeyValue="OverallHealth"},
        {.sourceXpath = "/Oem/${Oem}/StorageSummary/Count", .targetKeyValue="Maximum"},
        NULL
};


/**
 * get RAID/HBA card and controller information, command handler of `getraid`
 *
 * @param commandOption
 * @param outputStr
 * @return
 */
int UtoolCmdGetRAID(UtoolCommandOption *commandOption, char **outputStr)
{
    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    // initialize output objects
    cJSON *storageMembers = NULL, *output = NULL, *raidArray = NULL;

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

    UtoolRedfishGet(server, "/Systems/%s", output, getRAIDSummaryMappings, result);
    if (result->broken) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)

    UtoolRedfishGet(server, "/Systems/%s/Storages", NULL, NULL, result);
    if (result->broken) {
        goto FAILURE;
    }
    storageMembers = result->data;

    raidArray = cJSON_AddArrayToObject(output, "RaidCard");
    result->code = UtoolAssetCreatedJsonNotNull(raidArray);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    UtoolRedfishGetMemberResources(server, storageMembers, raidArray, getRAIDMappings, result);
    if (result->broken) {
        goto FAILURE;
    }

    // output to outputStr
    result->code = UtoolBuildOutputResult(STATE_SUCCESS, output, &(result->desc));
    goto DONE;

FAILURE:
    FREE_CJSON(output)
    goto DONE;

DONE:
    FREE_CJSON(storageMembers)
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}