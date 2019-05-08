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
int utool_get_redfish_server(utool_CommandOption *option, utool_RedfishServer *server, char **result);

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
int utool_curl_make_request(utool_RedfishServer *server,
                            char *resourceURL,
                            const char *httpMethod,
                            const cJSON *payload,
                            const utool_CurlHeader *headers,
                            utool_CurlResponse *response);

/**
 * resolve redfish failures response
 *
 * @param response
 * @param result
 * @return
 */
int utool_resolve_failure_response(utool_CurlResponse *response, char **result);


/**
 * generate failure message array from redfish failure response
 *
 * @param response
 * @param failures
 * @return
 */
int utool_get_failures_from_response(utool_CurlResponse *response, cJSON *failures);

/**
 * Free redfish server struct
 *
 * @param self
 * @param option
 * @return
 */
static inline int utool_free_redfish_server(utool_RedfishServer *server)
{
    if (server != NULL)
    {
        free(server->host);
        free(server->baseUrl);
        free(server->username);
        free(server->password);
        free(server->systemId);
    }
}

/**
 * Free redfish server struct
 *
 * @param self
 * @param option
 * @return
 */
static inline int utool_free_curl_response(utool_CurlResponse *response)
{
    if (response != NULL)
    {
        FREE_OBJ(response->content)
        FREE_OBJ(response->etag)
    }
}

/**
 * Free redfish server struct
 *
 * @param self
 * @param option
 * @return
 */
static inline int utool_asset_json_not_null(cJSON *json)
{
    if (json == NULL)
    {
        ZF_LOGE("Failed to parse content into json");
        return UTOOLE_PARSE_RESPONSE_JSON;
    }
    return OK;
}

static inline int utool_asset_json_node_not_null(cJSON *json, char *xpath)
{
    if (json == NULL)
    {
        ZF_LOGE("Failed to get the node(%s) from json", xpath);
        return UTOOLE_UNKNOWN_RESPONSE_FORMAT;
    }
    return OK;
}


#ifdef __cplusplus
}
#endif //UTOOL_REDFISH_H
#endif
