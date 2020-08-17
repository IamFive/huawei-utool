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
        "getpdisk",
        NULL,
};

static int CapacityGiBHandler(cJSON *target, const char *key, cJSON *node)
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


static int DrivesPropertyHandler(cJSON *target, const char *key, cJSON *node)
{
    int ret = UTOOLE_OK;

    if (cJSON_IsNull(node)) { // should not happen
        cJSON_AddItemToObjectCS(target, key, node);
        return UTOOLE_OK;
    }

    if (!cJSON_IsArray(node)) {
        ret = UTOOLE_UNKNOWN_JSON_FORMAT;
        goto DONE;
    }

    //cJSON *drives = cJSON_AddArrayToObject(target, "Drives");
    //ret = UtoolAssetCreatedJsonNotNull(drives);
    //if (ret != UTOOLE_OK) {
    //    goto FAILURE;
    //}


    char buffer[256];
    buffer[0] = '\0';

    cJSON *drive = NULL;
    cJSON_ArrayForEach(drive, node) {
        cJSON *link = cJSON_GetObjectItem(drive, "@odata.id");
        ret = UtoolAssetCreatedJsonNotNull(link);
        if (ret != UTOOLE_OK) {
            goto DONE;
        }

        char *driveId = UtoolStringLastSplit(link->valuestring, '/');
        strncat_s(buffer, 256, driveId, strnlen(driveId, 32));
        strcat_s(buffer, 256, ",");
    }

    buffer[strnlen(buffer, 256) - 1] = '\0';

    cJSON *newNode = cJSON_AddStringToObject(target, key, buffer);
    ret = UtoolAssetCreatedJsonNotNull(newNode);
    goto DONE;

DONE:
    FREE_CJSON(node)
    return ret;
}

static const UtoolOutputMapping getVolumeMappings[] = {
        {.sourceXpath = "/Id", .targetKeyValue="Id"},       // 需要做转化吗
        {.sourceXpath = "/Name", .targetKeyValue="LogicDiskName"},
        {.sourceXpath = "/Oem/Huawei/RaidControllerID", .targetKeyValue="RaidControllerID"},
        {.sourceXpath = "/Oem/Huawei/VolumeRaidLevel", .targetKeyValue="RaidLevel"},
        {.sourceXpath = "/CapacityBytes", .targetKeyValue="CapacityGiB", .handle = CapacityGiBHandler},
        {.sourceXpath = "/OptimumIOSizeBytes", .targetKeyValue="OptimumIOSizeBytes"},
        {.sourceXpath = "/RedundantType", .targetKeyValue="RedundantType"}, // TODO
        {.sourceXpath = "/Oem/Huawei/DefaultReadPolicy", .targetKeyValue="DefaultReadPolicy"},
        {.sourceXpath = "/Oem/Huawei/DefaultWritePolicy", .targetKeyValue="DefaultWritePolicy"},
        {.sourceXpath = "/Oem/Huawei/DefaultCachePolicy", .targetKeyValue="DefaultCachePolicy"},
        {.sourceXpath = "/Oem/Huawei/CurrentReadPolicy", .targetKeyValue="CurrentReadPolicy"},
        {.sourceXpath = "/Oem/Huawei/CurrentWritePolicy", .targetKeyValue="CurrentWritePolicy"},
        {.sourceXpath = "/Oem/Huawei/CurrentCachePolicy", .targetKeyValue="CurrentCachePolicy"},
        {.sourceXpath = "/Oem/Huawei/AccessPolicy", .targetKeyValue="AccessPolicy"},
        {.sourceXpath = "/Oem/Huawei/BGIEnable", .targetKeyValue="BGIEnable"},
        {.sourceXpath = "/Oem/Huawei/BootEnable", .targetKeyValue="BootEnable"},
        {.sourceXpath = "/Oem/Huawei/DriveCachePolicy", .targetKeyValue="DriveCachePolicy"},
        {.sourceXpath = "/Oem/Huawei/SSDCachecadeVolume", .targetKeyValue="SSDCachecadeVolume"},
        {.sourceXpath = "/Oem/Huawei/ConsistencyCheck", .targetKeyValue="ConsistencyCheck"},
        {.sourceXpath = "/Oem/Huawei/SSDCachingEnable", .targetKeyValue="SSDCachingEnable"},
        {.sourceXpath = "/Links/Drives", .targetKeyValue="Drives", .handle=DrivesPropertyHandler}, // TODO
        {.sourceXpath = "/Status/State", .targetKeyValue="State"},
        {.sourceXpath = "/Status/Health", .targetKeyValue="Health"},
        NULL
};


/**
 * get logical disk information, command handler of `getldisk`
 *
 * @param commandOption
 * @param result
 * @return
 */
int UtoolCmdGetLogicalDisks(UtoolCommandOption *commandOption, char **result)
{
    int ret;

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *getStorageMembersResponse = &(UtoolCurlResponse) {0},
            *getVolumesResponse = &(UtoolCurlResponse) {0},
            *getVolumeResponse = &(UtoolCurlResponse) {0};

    // initialize output objects
    cJSON *output = NULL,               // output result json
            *volumes = NULL,             // output volume array
            *volume = NULL,              // output volume item
            *storageMembersJson = NULL, // curl response storage members as json
            *volumeMembersJson = NULL,  // curl response volume members as json
            *volumeJson = NULL;         // curl response volume as json

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

    output = cJSON_CreateObject();
    ret = UtoolAssetCreatedJsonNotNull(output);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    // initialize output memory array
    volumes = cJSON_CreateArray();
    ret = UtoolAssetCreatedJsonNotNull(volumes);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    // get storage members
    ret = UtoolMakeCurlRequest(server, "/Systems/%s/Storages", HTTP_GET, NULL, NULL, getStorageMembersResponse);
    if (ret != UTOOLE_OK) {
        goto DONE;
    }
    if (getStorageMembersResponse->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getStorageMembersResponse, result);
        goto DONE;
    }

    // process get storage members response
    storageMembersJson = cJSON_Parse(getStorageMembersResponse->content);
    ret = UtoolAssetParseJsonNotNull(storageMembersJson);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *storageMember = NULL;
    cJSON *storageMembers = cJSON_GetObjectItem(storageMembersJson, "Members");
    cJSON_ArrayForEach(storageMember, storageMembers) {
        cJSON *storageLinkNode = cJSON_GetObjectItem(storageMember, "@odata.id");
        ret = UtoolAssetJsonNodeNotNull(storageLinkNode, "/Members/*/@odata.id");
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }

        // try to load volume members
        char volumesUrl[MAX_URL_LEN];
        char *url = storageLinkNode->valuestring;
        snprintf_s(volumesUrl, MAX_URL_LEN, MAX_URL_LEN, "%s/Volumes", url);
        ret = UtoolMakeCurlRequest(server, volumesUrl, HTTP_GET, NULL, NULL, getVolumesResponse);
        if (ret != UTOOLE_OK) {
            goto DONE;
        }

        if (getVolumesResponse->httpStatusCode >= 400) {
            ret = UtoolResolveFailureResponse(getVolumesResponse, result);
            goto DONE;
        }

        // process get storage members response
        volumeMembersJson = cJSON_Parse(getVolumesResponse->content);
        ret = UtoolAssetParseJsonNotNull(volumeMembersJson);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }

        //int health = 1;
        cJSON *volumeMember = NULL;
        cJSON *volumeMembers = cJSON_GetObjectItem(volumeMembersJson, "Members");
        cJSON_ArrayForEach(volumeMember, volumeMembers) {
            cJSON *volumeUrl = cJSON_GetObjectItem(volumeMember, "@odata.id");
            ret = UtoolAssetJsonNodeNotNull(volumeUrl, "/Members/*/@odata.id");
            if (ret != UTOOLE_OK) {
                goto FAILURE;
            }

            ret = UtoolMakeCurlRequest(server, volumeUrl->valuestring, HTTP_GET, NULL, NULL, getVolumeResponse);
            if (ret != UTOOLE_OK) {
                goto DONE;
            }

            if (getVolumeResponse->httpStatusCode >= 400) {
                ret = UtoolResolveFailureResponse(getVolumeResponse, result);
                goto DONE;
            }


            // process get storage members response
            volumeJson = cJSON_Parse(getVolumeResponse->content);
            ret = UtoolAssetParseJsonNotNull(volumeJson);
            if (ret != UTOOLE_OK) {
                goto FAILURE;
            }

            volume = cJSON_CreateObject();
            ret = UtoolAssetCreatedJsonNotNull(volume);
            if (ret != UTOOLE_OK) {
                goto FAILURE;
            }

            // create volume item and add it to array
            ret = UtoolMappingCJSONItems(volumeJson, volume, getVolumeMappings);
            if (ret != UTOOLE_OK) {
                goto FAILURE;
            }
            cJSON_AddItemToArray(volumes, volume);

            // free memory
            FREE_CJSON(volumeJson)
            UtoolFreeCurlResponse(getVolumeResponse);
        }


        FREE_CJSON(volumeMembersJson)
        UtoolFreeCurlResponse(getVolumesResponse);
    }

    // calculate maximum count
    cJSON *maximum = cJSON_AddNumberToObject(output, "Maximum", cJSON_GetArraySize(volumes));
    ret = UtoolAssetCreatedJsonNotNull(maximum);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *overallHealth = cJSON_AddStringToObject(output, "OverallHealth", UtoolGetOverallHealth(volumes, "/Health"));
    ret = UtoolAssetCreatedJsonNotNull(overallHealth);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON_AddItemToObject(output, "LogicDisk", volumes);

    // output to result
    ret = UtoolBuildOutputResult(STATE_SUCCESS, output, result);
    goto DONE;

FAILURE:
    FREE_CJSON(volume)
    FREE_CJSON(volumes)
    FREE_CJSON(output)
    FREE_CJSON(volumeJson)
    FREE_CJSON(volumeMembersJson)
    UtoolFreeCurlResponse(getVolumeResponse);
    UtoolFreeCurlResponse(getVolumesResponse);
    goto DONE;

DONE:
    FREE_CJSON(storageMembersJson)
    FREE_CJSON(volumeJson)
    UtoolFreeCurlResponse(getStorageMembersResponse);
    UtoolFreeCurlResponse(getVolumeResponse);
    UtoolFreeRedfishServer(server);
    return ret;
}
