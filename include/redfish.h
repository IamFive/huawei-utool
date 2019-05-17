//
// Created by qianbiao on 5/10/19.
//

#ifndef UTOOL_REDFISH_H
#define UTOOL_REDFISH_H
/* For c++ compatibility */
#ifdef __cplusplus
extern "C" {
#endif

#include "typedefs.h"
#include "zf_log.h"

static const char *const HTTP_GET = "GET";
static const char *const HTTP_POST = "POST";
static const char *const HTTP_PATCH = "PATCH";
static const char *const HTTP_PUT = "PUT";
static const char *const HTTP_DELETE = "DELETE";


/**
 * get redfish server information from command options
 *
 * @param option
 * @param server
 * @return
 */
int UtoolGetRedfishServer(UtoolCommandOption *option, UtoolRedfishServer *server, char **result);

/**
 * Make a new redfish request though CURL lib
 *
 * @param server
 * @param resourceURL
 * @param httpMethod
 * @param payload
 * @param response a pointer passing response of the request
 * @return
 */
int UtoolMakeCurlRequest(UtoolRedfishServer *server,
                         char *resourceURL,
                         const char *httpMethod,
                         const cJSON *payload,
                         const UtoolCurlHeader *headers,
                         UtoolCurlResponse *response);

/**
 * resolve redfish failures response
 *
 * @param response
 * @param result
 * @return
 */
int UtoolResolveFailureResponse(UtoolCurlResponse *response, char **result);


/**
 * generate failure message array from redfish failure response
 *
 * @param response
 * @param failures
 * @return
 */
int UtoolGetFailuresFromResponse(UtoolCurlResponse *response, cJSON *failures);

/**
 * Free redfish server struct
 *
 * @param self
 * @param option
 * @return
 */
static inline int UtoolFreeRedfishServer(UtoolRedfishServer *server)
{
    if (server != NULL) {
        FREE_OBJ(server->host)
        FREE_OBJ(server->baseUrl)
        FREE_OBJ(server->username)
        FREE_OBJ(server->password)
        FREE_OBJ(server->systemId)
    }
}

/**
 * Free redfish server struct
 *
 * @param self
 * @param option
 * @return
 */
static inline int UtoolFreeCurlResponse(UtoolCurlResponse *response)
{
    if (response != NULL) {
        FREE_OBJ(response->content)
        FREE_OBJ(response->etag)
    }
}

/**
 * asset json object is not null
 *
 * @param self
 * @param option
 * @return
 */
static inline int UtoolAssetCreatedJsonNotNull(cJSON *json)
{
    if (json == NULL) {
        ZF_LOGE("Failed to create json object");
        return UTOOLE_CREATE_JSON_NULL;
    }
    return OK;
}

/**
 * asset json object is not null
 *
 * @param self
 * @param option
 * @return
 */
static inline int UtoolAssetJsonNotNull(cJSON *json)
{
    if (json == NULL) {
        ZF_LOGE("Failed to create json object");
        return UTOOLE_PARSE_RESPONSE_JSON;
    }
    return OK;
}

/**
 * asset json node object is not null
 *
 * @param json
 * @param xpath
 * @return
 */
static inline int UtoolAssetJsonNodeNotNull(cJSON *json, char *xpath)
{
    if (json == NULL) {
        ZF_LOGE("Failed to get the node(%s) from json", xpath);
        return UTOOLE_UNKNOWN_RESPONSE_FORMAT;
    }
    return OK;
}


#ifdef __cplusplus
}
#endif //UTOOL_REDFISH_H
#endif
