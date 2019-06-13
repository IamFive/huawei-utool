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
* Upload file to BMC temp storage through CURL lib
*
* @param server
* @param uploadFilePath
* @param result
*/
void UtoolUploadFileToBMC(UtoolRedfishServer *server, const char *uploadFilePath, UtoolResult *result);


/**
* download file from BMC temp storage to a local file through CURL lib
*
* @param server
* @param bmcFileUri
* @param localFileUri
* @param result
*/
void UtoolDownloadFileFromBMC(UtoolRedfishServer *server, const char *bmcFileUri, const char *localFileUri,
                              UtoolResult *result);

/**
 * Make a new redfish request through CURL lib
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
* Process Redfish request
*
*   - send request
*   - get response
*   - parsing response
*
* @param server
* @param url
* @param httpMethod
* @param payload
* @param headers
* @param output
* @param outputMapping
* @param result
*/
void UtoolRedfishProcessRequest(UtoolRedfishServer *server,
                                char *url,
                                const char *httpMethod,
                                const cJSON *payload,
                                const UtoolCurlHeader *headers,
                                cJSON *output,
                                const UtoolOutputMapping *outputMapping,
                                UtoolResult *result);


/**
* Get Redfish Resource
*
* @param server
* @param url
* @param output
* @param outputMapping
* @param result
*/
void UtoolRedfishGet(UtoolRedfishServer *server, char *url, cJSON *output,
                     const UtoolOutputMapping *outputMapping, UtoolResult *result);

/**
* Post Redfish request
* @param server
* @param url
* @param payload
* @param output
* @param outputMapping
* @param result
*/
void UtoolRedfishPost(UtoolRedfishServer *server, char *url, cJSON *payload, cJSON *output,
                      const UtoolOutputMapping *outputMapping, UtoolResult *result);

/**
* Patch Redfish request
*
* @param server
* @param url
* @param payload
* @param headers
* @param output
* @param outputMapping
* @param result
*/
void UtoolRedfishPatch(UtoolRedfishServer *server, char *url, cJSON *payload, const UtoolCurlHeader *headers,
                       cJSON *output, const UtoolOutputMapping *outputMapping, UtoolResult *result);


/**
* Delete Redfish request
*
* @param server
* @param url
* @param output
* @param outputMapping
* @param result
*/
void UtoolRedfishDelete(UtoolRedfishServer *server, char *url, cJSON *output, const UtoolOutputMapping *outputMapping,
                        UtoolResult *result);

/**
* Get All Redfish member resources
*
* @param server
* @param owner
* @param memberArray
* @param memberMapping
* @param result
*/
void UtoolRedfishGetMemberResources(UtoolRedfishServer *server, cJSON *owner, cJSON *memberArray,
                                    const UtoolOutputMapping *memberMapping, UtoolResult *result);

/**
* mapping a json format task to struct task
*
* @param cJSONTask
* @param result
* @return
*/
UtoolRedfishTask *UtoolRedfishMapTaskFromJson(cJSON *cJSONTask, UtoolResult *result);

void UtoolRedfishWaitUtilTaskFinished(UtoolRedfishServer *server, cJSON *cJSONTask, UtoolResult *result);


#ifdef __cplusplus
}
#endif //UTOOL_REDFISH_H
#endif
