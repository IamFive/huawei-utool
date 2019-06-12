#include <string.h>
#include <stdlib.h>
#include <commons.h>
#include <cJSON_Utils.h>
#include <constants.h>
#include <typedefs.h>
#include <command-interfaces.h>
#include "string_utils.h"

const char *UTOOL_ENABLED_CHOICES[] = {ENABLED, DISABLED, NULL};

static int TaskTriggerPropertyHandler(cJSON *target, const char *key, cJSON *node);

/** Redfish Rsync Task Mapping define */
const UtoolOutputMapping utoolGetTaskMappings[] = {
        {.sourceXpath = "/Id", .targetKeyValue="TaskId"},
        {.sourceXpath = "/Name", .targetKeyValue="TaskDesc"},
        {.sourceXpath = "/Name", .targetKeyValue="TaskType"},
        {.sourceXpath = "/TaskState", .targetKeyValue="State"},
        {.sourceXpath = "/StartTime", .targetKeyValue="StartTime"},
        {.sourceXpath = "/Null", .targetKeyValue="EstimatedTimeSenconds"},
        {.sourceXpath = "/Null", .targetKeyValue="Trigger", .handle=TaskTriggerPropertyHandler},
        {.sourceXpath = "/Messages", .targetKeyValue="Messages"},
        {.sourceXpath = "/Oem/Huawei/TaskPercentage", .targetKeyValue="TaskPercentage"},
        NULL,
};

static int TaskTriggerPropertyHandler(cJSON *target, const char *key, cJSON *node)
{
    cJSON_AddRawToObject(target, "Trigger", "[\"automatic\"]");
    FREE_CJSON(node)
    return UTOOLE_OK;
}

int UtoolBoolToEnabledPropertyHandler(cJSON *target, const char *key, cJSON *node)
{
    if (cJSON_IsTrue(node)) {
        cJSON_AddStringToObject(target, key, ENABLED);
    }

    if (cJSON_IsFalse(node)) {
        cJSON_AddStringToObject(target, key, DISABLED);
    }

    FREE_CJSON(node)
    return UTOOLE_OK;
}


/**
 *
 * build output result JSON
 *
 * @param state
 * @param messages
 * @return
 */
static cJSON *UtoolBuildOutputJson(const char *const state, cJSON *messages)
{
    cJSON *response = cJSON_CreateObject();
    if (response == NULL) {
        goto FAILURE;
    }

    if (cJSON_AddStringToObject(response, RESULT_KEY_STATE, state) == NULL) {
        goto FAILURE;
    }

    if (cJSON_IsArray(messages)) {
        cJSON_AddItemToObject(response, RESULT_KEY_MESSAGES, messages);
    }
    else {
        cJSON *_message = cJSON_AddArrayToObject(response, RESULT_KEY_MESSAGES);
        if (_message == NULL) {
            goto FAILURE;
        }
        cJSON_AddItemToArray(_message, messages);
    }

    return response;

FAILURE:
    FREE_CJSON(messages)
    FREE_CJSON(response)
    return NULL;
}


/**
 *
 * build new json result and assign to (char **) result.
 * Be aware that messages will be free at last
 *
 * @param state
 * @param messages
 * @param result
 * @return
 */
int UtoolBuildOutputResult(const char *state, cJSON *messages, char **result)
{
    int ret = UTOOLE_CREATE_JSON_NULL;
    if (messages == NULL) {
        return ret;
    }

    cJSON *jsonResult = UtoolBuildOutputJson(state, messages);
    if (jsonResult == NULL) {
        return ret;
    }

    char *pretty = cJSON_Print(jsonResult);
    if (pretty == NULL) {
        goto return_statement;
    }

    *result = pretty;
    ret = UTOOLE_OK;
    goto return_statement;

return_statement:
    FREE_CJSON(jsonResult)
    return ret;
}

/**
 *
 * build new json result and assign to (char **) result.
 * Be aware that messages will be free at last
 *
 * @param state
 * @param messages
 * @param result
 * @return
 */
int UtoolBuildStringOutputResult(const char *state, const char *messages, char **result)
{
    int ret = UTOOLE_CREATE_JSON_NULL;

    cJSON *jsonString = cJSON_CreateString(messages);
    if (jsonString == NULL) {
        return ret;
    }

    cJSON *jsonResult = UtoolBuildOutputJson(state, jsonString);
    if (jsonResult == NULL) {
        FREE_CJSON(jsonString)
        return ret;
    }

    char *pretty = cJSON_Print(jsonResult);
    if (pretty != NULL) {
        ret = UTOOLE_OK;
        *result = pretty;
    }

    FREE_CJSON(jsonResult)
    return ret;
}

void UtoolBuildDefaultSuccessResult(char **result)
{
    *result = strndup(OUTPUT_SUCCESS_JSON, sizeof(OUTPUT_SUCCESS_JSON));
}


/**
 *
 * build a new json from source JSON struct with mapping meta info
 *
 * @param source
 * @param target
 * @param mapping
 * @param count
 * @return
 */
int UtoolMappingCJSONItems(cJSON *source, cJSON *target, const UtoolOutputMapping *mappings)
{
    int ret;
    for (int idx = 0;; idx++) {
        const UtoolOutputMapping *mapping = mappings + idx;
        if (mapping->sourceXpath == NULL || mapping->targetKeyValue == NULL) {
            break;
        }

        const char *xpath = mapping->sourceXpath;
        cJSON *ref = cJSONUtils_GetPointer(source, xpath);

        /** plain mapping */
        if (mapping->nestMapping == NULL) {
            cJSON *cloned = ref != NULL ? cJSON_Duplicate(ref, 1) : cJSON_CreateNull();
            ret = UtoolAssetCreatedJsonNotNull(cloned);
            if (ret != UTOOLE_OK) {
                return ret;
            }
            if (mapping->handle == NULL) {
                cJSON_AddItemToObjectCS(target, mapping->targetKeyValue, cloned);
                continue;
            }

            ret = mapping->handle(target, mapping->targetKeyValue, cloned);
            if (ret != UTOOLE_OK) {
                return ret;
            }
        }
        else { /** nest array mapping */
            cJSON *array = cJSON_GetObjectItem(target, mapping->targetKeyValue);
            if (array == NULL) {
                array = cJSON_AddArrayToObject(target, mapping->targetKeyValue);
                ret = UtoolAssetCreatedJsonNotNull(array);
                if (ret != UTOOLE_OK) {
                    return ret;
                }
            }

            if (mapping->filter != NULL) {

            }

            const UtoolOutputMapping *nestMapping = mapping->nestMapping;
            cJSON *element = NULL;
            cJSON_ArrayForEach(element, ref) {
                cJSON *mapped = cJSON_CreateObject();
                cJSON_AddItemToArray(array, mapped);
                // TODO
                UtoolMappingCJSONItems(element, mapped, nestMapping);
            }
        }


        /** TODO(Qianbiao.NG) we should add more case coverage later?
        switch (ref->type) {
            case cJSON_NULL:
                cJSON_AddItemToObjectCS(target, mapping->targetKeyValue, cJSON_CreateNull());
                break;
            case cJSON_False:
                cJSON_AddItemToObjectCS(target, mapping->targetKeyValue, cJSON_CreateFalse());
                break;
            case cJSON_True:
                cJSON_AddItemToObjectCS(target, mapping->targetKeyValue, cJSON_CreateTrue());
                break;
            case cJSON_Number:
                if (ref->valueint != 0) {
                    cJSON_AddItemToObjectCS(target, mapping->targetKeyValue, cJSON_CreateNumber(ref->valueint));
                }
                else {
                    cJSON_AddItemToObjectCS(target, mapping->targetKeyValue, cJSON_CreateNumber(ref->valuedouble));
                }
                break;
            case cJSON_String: {
                cJSON_AddItemToObjectCS(target, mapping->targetKeyValue, cJSON_CreateString(ref->valuestring));
                break;
            }
                //case cJSON_Array: {
                //    cJSON *cloned = cJSON_Duplicate(ref, 1);
                //    cJSON_AddItemToObjectCS(target, mapping->targetKeyValue, cloned);
                //    break;
                //}
            default:
                break;
        }
        */
    }

    return UTOOLE_OK;
}


cJSON *UtoolWrapOem(cJSON *source, UtoolResult *result)
{
    cJSON *wrapped = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(wrapped);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *oem = cJSON_AddObjectToObject(wrapped, "Oem");
    result->code = UtoolAssetCreatedJsonNotNull(oem);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON_AddItemToObject(oem, "Huawei", source);
    return wrapped;

FAILURE:
    FREE_CJSON(wrapped)
    result->interrupt = 1;
    return NULL;
}


static int HEALTH_ROLLUP_LEVEL_OK = 0;
static int HEALTH_ROLLUP_LEVEL_WARNING = 1;
static int HEALTH_ROLLUP_LEVEL_CRITICAL = 2;
static char *HEALTH_ROLLUP_LIST[] = {HEALTH_ROLLUP_OK, HEALTH_ROLLUP_WARNING, HEALTH_ROLLUP_CRITICAL};

/**
* calculate overall health from item health state
*
* @param items
* @param xpath
* @return
*/
char *UtoolGetOverallHealth(cJSON *items, const char *xpath)
{
    int level = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, items) {
        cJSON *healthNode = cJSONUtils_GetPointer(item, xpath);
        if (healthNode != NULL && healthNode->valuestring != NULL) {
            if (UtoolStringEquals(HEALTH_ROLLUP_OK, healthNode->valuestring) && level < HEALTH_ROLLUP_LEVEL_OK) {
                level = HEALTH_ROLLUP_LEVEL_OK;
                continue;
            }

            if (UtoolStringEquals(HEALTH_ROLLUP_WARNING, healthNode->valuestring) &&
                level < HEALTH_ROLLUP_LEVEL_WARNING) {
                level = HEALTH_ROLLUP_LEVEL_OK;
                continue;
            }

            if (UtoolStringEquals(HEALTH_ROLLUP_CRITICAL, healthNode->valuestring) &&
                level < HEALTH_ROLLUP_LEVEL_CRITICAL) {
                level = HEALTH_ROLLUP_LEVEL_CRITICAL;
                break;
            }
        }
    }

    return HEALTH_ROLLUP_LIST[level];
}

/**
* get string error of UtoolCode
*
* @param code
* @return
*/
const char *UtoolGetStringError(UtoolCode code)
{
    switch (code) {
        case UTOOLE_OK:
            return "No error";
        case UTOOLE_INTERNAL:
            return "Internal error, please contact maintainer.";
        case UTOOLE_PARSE_JSON_FAILED:
            return "Internal error, failed to parse JSON content.";
        case UTOOLE_UNKNOWN_JSON_FORMAT:
            return "Internal error, unexpect JSON format response by HUAWEI server API.";
        case UTOOLE_CREATE_JSON_NULL:
            return "Internal error, failed to create a JSON object.";
        case UTOOLE_PRINT_JSON_FAILED:
            return "Internal error, failed to print JSON object.";
        case UTOOLE_CURL_INIT_FAILED:
            return "Internal error, failed to init curl.";
        case UTOOLE_ILLEGAL_LOCAL_FILE_PATH:
            return "Illegal local file path, please make sure the file exists";
        case UTOOLE_ILLEGAL_LOCAL_FILE_SIZE:
            return "Illegal local file size, file should not be empty.";
        case UTOOLE_ILLEGAL_DOWNLOAD_FILE_PATH:
            return "Failed to open local download file.";
        default:
            return "Unknown error";
    }
}
