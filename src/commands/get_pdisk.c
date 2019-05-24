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
        "utool getpdisk",
        NULL,
};

static const UtoolOutputMapping getDriveSummaryMapping[] = {
        {.sourceXpath = "/Oem/Huawei/DriveSummary/Status/HealthRollup", .targetKeyValue="OverallHealth"},
        {.sourceXpath = "/Oem/Huawei/DriveSummary/Count", .targetKeyValue="Maximum"},
        NULL
};

static int CapacityGiBHandler(cJSON *target, const char *key, cJSON *node)
{
    if (node == NULL) { // should not happen
        return UTOOLE_OK;
    }

    int gbytes = (int) (node->valuedouble / 1024 / 1024 / 1024 + 0.5);
    node->valuedouble = gbytes;
    cJSON_AddItemToObjectCS(target, key, node);
    return UTOOLE_OK;
}


static int VolumesHandler(cJSON *target, const char *key, cJSON *node)
{
    if (node == NULL) { // should not happen
        return UTOOLE_OK;
    }

    if (!cJSON_IsString(node) || node->valuestring == NULL) {
        return UTOOLE_UNKNOWN_JSON_FORMAT;
    }

    int idx = 1;
    char *delim = "/";

    char *raid;
    char *volume;
    char *split;

    /** /redfish/v1/Systems/1/Storages/RAIDStorage0/Volumes/LogicalDrive0 */
    strtok(node->valuestring, delim);
    while (idx <= 7) {
        split = strtok(NULL, delim);
        if (idx == 5) {
            raid = split;
        }
        if (idx == 7) {
            volume = split;
        }

        idx++;
    }

    char value[64];
    snprintf(value, 64, "%s-%s", raid, volume);
    FREE_CJSON(node)

    cJSON *newNode = cJSON_AddStringToObject(target, key, value);
    return UtoolAssetCreatedJsonNotNull(newNode);
}

static const UtoolOutputMapping getDriveMappings[] = {
        {.sourceXpath = "/Id", .targetKeyValue="Id"},
        {.sourceXpath = "/Name", .targetKeyValue="CommonName"},
        {.sourceXpath = "/Oem/Huawei/Position", .targetKeyValue="Location"},
        {.sourceXpath = "/Model", .targetKeyValue="Model"},
        {.sourceXpath = "/Manufacturer", .targetKeyValue="Manufacturer"},
        {.sourceXpath = "/FailurePredicted", .targetKeyValue="FailurePredicted"},
        {.sourceXpath = "/CapacityBytes", .targetKeyValue="CapacityGiB", .handle = CapacityGiBHandler}, // TODO
        {.sourceXpath = "/HotspareType", .targetKeyValue="HotspareType"},
        {.sourceXpath = "/IndicatorLED", .targetKeyValue="IndicatorLED"},
        {.sourceXpath = "/PredictedMediaLifeLeftPercent", .targetKeyValue="PredictedMediaLifeLeftPercent"},
        {.sourceXpath = "/MediaType", .targetKeyValue="MediaType"},
        {.sourceXpath = "/SerialNumber", .targetKeyValue="SerialNumber"},
        {.sourceXpath = "/CapableSpeedGbs", .targetKeyValue="CapableSpeedGbs"},
        {.sourceXpath = "/NegotiatedSpeedGbs", .targetKeyValue="NegotiatedSpeedGbs"},
        {.sourceXpath = "/Revision", .targetKeyValue="Revision"},

        {.sourceXpath = "/StatusIndicator", .targetKeyValue="StatusIndicator"},
        {.sourceXpath = "/Oem/Huawei/TemperatureCelsius", .targetKeyValue="TemperatureCelsius"},
        {.sourceXpath = "/Oem/Huawei/HoursOfPoweredUp", .targetKeyValue="HoursOfPoweredUp"},
        {.sourceXpath = "/Oem/Huawei/FirmwareStatus", .targetKeyValue="FirmwareStatus"},
        {.sourceXpath = "/Oem/Huawei/SASAddress", .targetKeyValue="SASAddress"}, // TODO
        {.sourceXpath = "/Oem/Huawei/PatrolState", .targetKeyValue="PatrolState"},
        {.sourceXpath = "/Oem/Huawei/RebuildState", .targetKeyValue="RebuildState"},
        {.sourceXpath = "/Oem/Huawei/RebuildProgress", .targetKeyValue="RebuildProgress"},

        {.sourceXpath = "/Oem/Huawei/SpareforLogicalDrives", .targetKeyValue="SpareforLogicalDrives"},
        {.sourceXpath = "/Links/Volumes/0/@odata.id", .targetKeyValue="Volumes", .handle=VolumesHandler},
        {.sourceXpath = "/Status/State", .targetKeyValue="State"},
        {.sourceXpath = "/Status/Health", .targetKeyValue="Health"},
        NULL
};


/**
 * command handler of `getpdisk`
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

    ret = UtoolMakeCurlRequest(server, "/Chassis/%s", HTTP_GET, NULL, NULL, getChassisResponse);
    if (ret != UTOOLE_OK) {
        goto done;
    }
    if (getChassisResponse->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getChassisResponse, result);
        goto done;
    }


    output = cJSON_CreateObject();
    ret = UtoolAssetCreatedJsonNotNull(output);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    // process get chassis response
    chassisJson = cJSON_Parse(getChassisResponse->content);
    ret = UtoolAssetParseJsonNotNull(chassisJson);
    if (ret != UTOOLE_OK) {
        goto failure;
    }
    UtoolMappingCJSONItems(chassisJson, output, getDriveSummaryMapping);

    // initialize output drive array
    drives = cJSON_AddArrayToObject(output, "Disk");
    ret = UtoolAssetCreatedJsonNotNull(drives);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    cJSON *links = cJSONUtils_GetPointer(chassisJson, "/Links/Drives");
    int memberCount = cJSON_GetArraySize(links);
    for (int idx = 0; idx < memberCount; idx++) {
        cJSON *driveLinkObject = cJSON_GetArrayItem(links, idx);
        cJSON *link = cJSON_GetObjectItem(driveLinkObject, "@odata.id");
        ret = UtoolAssetJsonNodeNotNull(link, "/Links/Drives/*/@odata.id");
        if (ret != UTOOLE_OK) {
            goto failure;
        }

        char *url = link->valuestring;

        ret = UtoolMakeCurlRequest(server, url, HTTP_GET, NULL, NULL, getDriveResponse);
        if (ret != UTOOLE_OK) {
            goto failure;
        }

        driveJson = cJSON_Parse(getDriveResponse->content);
        ret = UtoolAssetParseJsonNotNull(driveJson);
        if (ret != UTOOLE_OK) {
            goto failure;
        }

        drive = cJSON_CreateObject();
        ret = UtoolAssetCreatedJsonNotNull(drive);
        if (ret != UTOOLE_OK) {
            goto failure;
        }

        // create drive item and add it to array
        UtoolMappingCJSONItems(driveJson, drive, getDriveMappings);
        cJSON_AddItemToArray(drives, drive);

        FREE_CJSON(driveJson)
        UtoolFreeCurlResponse(getDriveResponse);
    }

    // output to result
    ret = UtoolBuildOutputResult(STATE_SUCCESS, output, result);
    goto done;

failure:
    FREE_CJSON(output)
    goto done;

done:
    FREE_CJSON(chassisJson)
    FREE_CJSON(driveJson)
    UtoolFreeCurlResponse(getChassisResponse);
    UtoolFreeCurlResponse(getDriveResponse);
    UtoolFreeRedfishServer(server);
    return ret;
}
