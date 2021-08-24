/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `getpdisk`
* Author:
* Create: 2019-06-14
* Notes:
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <securec.h>
#include <string_utils.h>
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
        "getpdisk",
        NULL,
};

static const UtoolOutputMapping getDriveSummaryMapping[] = {
        {.sourceXpath = "/Oem/${Oem}/DriveSummary/Status/HealthRollup", .targetKeyValue="OverallHealth"},
        {.sourceXpath = "/Oem/${Oem}/DriveSummary/Count", .targetKeyValue="Maximum"},
        NULL
};

static int CapacityGiBHandler(UtoolRedfishServer *server, cJSON *target, const char *key, cJSON *node)
{
    if (cJSON_IsNull(node)) { // should not happen
        cJSON_AddItemToObjectCS(target, key, node);
        return UTOOLE_OK;
    }

    int gbytes = (int) (node->valuedouble / 1024 / 1024 / 1024 + 0.5);
    node->valuedouble = gbytes;
    cJSON_AddItemToObjectCS(target, key, node);
    return UTOOLE_OK;
}


static int VolumesHandler(UtoolRedfishServer *server, cJSON *target, const char *key, cJSON *node)
{
    if (cJSON_IsNull(node)) { // should not happen
        cJSON_AddItemToObjectCS(target, key, node);
        return UTOOLE_OK;
    }

    if (!cJSON_IsString(node) || node->valuestring == NULL) {
        return UTOOLE_UNKNOWN_JSON_FORMAT;
    }

    int idx = 1;
    char *delim = "/";

    char *raid = NULL;
    char *volume = NULL;
    char *split = NULL;
    char *nextp = NULL;

    /**
     * node value_string example::
     *  /redfish/v1/Systems/1/Storages/RAIDStorage0/Volumes/LogicalDrive0
     * */

    UtoolStringTokens(node->valuestring, delim, &nextp);
    while (idx <= 7) {
        split = UtoolStringTokens(NULL, delim, &nextp);
        if (split == NULL) {
            break;
        }
        if (idx == 5) {
            raid = split;
        }
        if (idx == 7) {
            volume = split;
        }

        idx++;
    }

    char value[64] = {0};
    UtoolWrapSnprintf(value, sizeof(value), sizeof(value), "%s-%s", raid, volume);
    FREE_CJSON(node)

    cJSON *newNode = cJSON_AddStringToObject(target, key, value);
    return UtoolAssetCreatedJsonNotNull(newNode);
}

static const UtoolOutputMapping getDriveMappings[] = {
        {.sourceXpath = "/Id", .targetKeyValue="Id"},
        {.sourceXpath = "/Name", .targetKeyValue="CommonName"},
        {.sourceXpath = "/Oem/${Oem}/Position", .targetKeyValue="Location"},
        {.sourceXpath = "/Model", .targetKeyValue="Model"},
        {.sourceXpath = "/Protocol", .targetKeyValue="Protocol"},
        {.sourceXpath = "/Manufacturer", .targetKeyValue="Manufacturer"},
        {.sourceXpath = "/FailurePredicted", .targetKeyValue="FailurePredicted"},
        {.sourceXpath = "/CapacityBytes", .targetKeyValue="CapacityGiB", .handle = CapacityGiBHandler},
        {.sourceXpath = "/HotspareType", .targetKeyValue="HotspareType"},
        {.sourceXpath = "/IndicatorLED", .targetKeyValue="IndicatorLED"},
        {.sourceXpath = "/PredictedMediaLifeLeftPercent", .targetKeyValue="PredictedMediaLifeLeftPercent"},
        {.sourceXpath = "/MediaType", .targetKeyValue="MediaType"},
        {.sourceXpath = "/SerialNumber", .targetKeyValue="SerialNumber"},
        {.sourceXpath = "/CapableSpeedGbs", .targetKeyValue="CapableSpeedGbs"},
        {.sourceXpath = "/NegotiatedSpeedGbs", .targetKeyValue="NegotiatedSpeedGbs"},
        {.sourceXpath = "/Revision", .targetKeyValue="Revision"},

        {.sourceXpath = "/StatusIndicator", .targetKeyValue="StatusIndicator"},
        {.sourceXpath = "/Oem/${Oem}/TemperatureCelsius", .targetKeyValue="TemperatureCelsius"},
        {.sourceXpath = "/Oem/${Oem}/HoursOfPoweredUp", .targetKeyValue="HoursOfPoweredUp"},
        {.sourceXpath = "/Oem/${Oem}/FirmwareStatus", .targetKeyValue="FirmwareStatus"},
        {.sourceXpath = "/Oem/${Oem}/SASAddress", .targetKeyValue="SASAddress"},
        {.sourceXpath = "/Oem/${Oem}/PatrolState", .targetKeyValue="PatrolState"},
        {.sourceXpath = "/Oem/${Oem}/RebuildState", .targetKeyValue="RebuildState"},
        {.sourceXpath = "/Oem/${Oem}/RebuildProgress", .targetKeyValue="RebuildProgress"},

        {.sourceXpath = "/Oem/${Oem}/SpareforLogicalDrives", .targetKeyValue="SpareforLogicalDrives"},
        {.sourceXpath = "/Links/Volumes/0/@odata.id", .targetKeyValue="Volumes", .handle=VolumesHandler},
        {.sourceXpath = "/Status/State", .targetKeyValue="State"},
        {.sourceXpath = "/Status/Health", .targetKeyValue="Health"},
        NULL
};


/**
 * get physical disk information, command handler of `getpdisk`
 *
 * @param commandOption
 * @param result
 * @return
 */
int UtoolCmdGetPhysicalDisks(UtoolCommandOption *commandOption, char **result)
{
    int ret;

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *getChassisResponse = &(UtoolCurlResponse) {0},
            *getDriveResponse = &(UtoolCurlResponse) {0};

    // initialize output objects
    cJSON *output = NULL,               // output result json
            *drives = NULL,             // output drive array
            *drive = NULL,              // output drive item
            *chassisJson = NULL,        // curl response thermal as json
            *driveJson = NULL;         // curl response drive as json

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

    ret = UtoolMakeCurlRequest(server, "/Chassis/%s", HTTP_GET, NULL, NULL, getChassisResponse);
    if (ret != UTOOLE_OK) {
        goto DONE;
    }
    if (getChassisResponse->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getChassisResponse, result);
        goto DONE;
    }

    output = cJSON_CreateObject();
    ret = UtoolAssetCreatedJsonNotNull(output);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    // process get chassis response
    chassisJson = cJSON_Parse(getChassisResponse->content);
    ret = UtoolAssetParseJsonNotNull(chassisJson);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }
    ret = UtoolMappingCJSONItems(server, chassisJson, output, getDriveSummaryMapping);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    // initialize output drive array
    drives = cJSON_AddArrayToObject(output, "Disk");
    ret = UtoolAssetCreatedJsonNotNull(drives);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *links = cJSONUtils_GetPointer(chassisJson, "/Links/Drives");
    int memberCount = cJSON_GetArraySize(links);
    for (int idx = 0; idx < memberCount; idx++) {
        cJSON *driveLinkObject = cJSON_GetArrayItem(links, idx);
        cJSON *link = cJSON_GetObjectItem(driveLinkObject, "@odata.id");
        ret = UtoolAssetJsonNodeNotNull(link, "/Links/Drives/*/@odata.id");
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }

        char *url = link->valuestring;

        ret = UtoolMakeCurlRequest(server, url, HTTP_GET, NULL, NULL, getDriveResponse);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }

        if (getDriveResponse->httpStatusCode >= 400) {
            ret = UtoolResolveFailureResponse(getDriveResponse, result);
            goto FAILURE;
        }

        driveJson = cJSON_Parse(getDriveResponse->content);
        ret = UtoolAssetParseJsonNotNull(driveJson);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }

        drive = cJSON_CreateObject();
        ret = UtoolAssetCreatedJsonNotNull(drive);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }

        // create drive item and add it to array
        ret = UtoolMappingCJSONItems(server, driveJson, drive, getDriveMappings);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }
        cJSON_AddItemToArray(drives, drive);

        FREE_CJSON(driveJson)
        UtoolFreeCurlResponse(getDriveResponse);
    }

    // output to result
    ret = UtoolBuildOutputResult(STATE_SUCCESS, output, result);
    goto DONE;

FAILURE:
    FREE_CJSON(drive)
    FREE_CJSON(output)
    goto DONE;

DONE:
    FREE_CJSON(chassisJson)
    FREE_CJSON(driveJson)
    UtoolFreeCurlResponse(getChassisResponse);
    UtoolFreeCurlResponse(getDriveResponse);
    UtoolFreeRedfishServer(server);
    return ret;
}
