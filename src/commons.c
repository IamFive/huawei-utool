#include <string.h>
#include <stdlib.h>
#include <commons.h>
#include <cJSON_Utils.h>
#include <constants.h>
#include <typedefs.h>


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
        goto failure;
    }

    if (cJSON_AddStringToObject(response, RESULT_KEY_STATE, state) == NULL) {
        goto failure;
    }

    if (cJSON_IsArray(messages)) {
        cJSON_AddItemToObject(response, RESULT_KEY_MESSAGES, messages);
    }
    else {
        cJSON *_message = cJSON_AddArrayToObject(response, RESULT_KEY_MESSAGES);
        if (_message == NULL) {
            goto failure;
        }
        cJSON_AddItemToArray(_message, messages);
    }

    return response;

failure:
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
    int ret = UTOOLE_INTERNAL;

    cJSON *jsonResult = UtoolBuildOutputJson(state, messages);
    if (jsonResult == NULL) { return ret; }

    char *pretty = cJSON_Print(jsonResult);
    if (pretty == NULL) { goto return_statement; }

    *result = pretty;
    ret = OK;
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
    int ret = UTOOLE_INTERNAL;

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
        ret = OK;
        *result = pretty;
    }

    FREE_CJSON(jsonResult)
    return ret;
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
int UtoolMappingCJSONItems(cJSON *source, cJSON *target, const UtoolOutputMapping *mappings, int count)
{
    for (int idx = 0; idx < count; idx++) {
        const UtoolOutputMapping *mapping = mappings + idx;
        const char *xpath = mapping->sourceXpath;
        cJSON *ref = cJSONUtils_GetPointer(source, xpath);
        if (ref != NULL) {
            // TODO(Qianbiao.NG) we should add more case coverage later?
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
//                    const char *value = ref->valuestring;
//                    const size_t len = strlen(value) + 1;
//                    char *copy = (char *) malloc(len);
//                    strncpy(copy, value, len);
                    cJSON_AddItemToObjectCS(target, mapping->targetKeyValue, cJSON_CreateString(ref->valuestring));
                    break;
                }
                default:
                    break;
            }
        }
        else {
            cJSON_AddItemToObjectCS(target, mapping->targetKeyValue, cJSON_CreateNull());
        }
    }

    return OK;
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
        case OK:
            return "No error";
        case UTOOLE_INTERNAL:
            return "Internal error, please contact maintainer.";
        case UTOOLE_PARSE_RESPONSE_JSON:
            return "Internal error, failed to parse JSON content.";
        case UTOOLE_UNKNOWN_RESPONSE_FORMAT:
            return "Internal error, unexpect JSON format response by HUAWEI server API.";
        default:
            return "Unknown error";
    }
}

