/*
* Copyright © xFusion Digital Technologies Co., Ltd. 2012-2018. All rights reserved.
* Description: redfish utilities header
* Author:
* Create: 2019-06-16
* Notes:
*/
#ifndef UTOOL_REDFISH_H
#define UTOOL_REDFISH_H
/* For c++ compatibility */
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
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
 * get redfish server information from command options
 *
 * @param option
 * @param server
 * @return
 */
void UtoolGetRedfishServer2(UtoolCommandOption *option, UtoolRedfishServer *server, UtoolResult* result);


/**
* Upload file to BMC temp storage. Will try upload through http, then sftp if failed.
*
* @param server
* @param uploadFilePath
* @param result
*/
void UtoolUploadFileToBMC(UtoolRedfishServer *server, const char *uploadFilePath, UtoolResult *result);

/**
* Upload file to BMC temp storage through HTTP API.
*
* @param server
* @param uploadFilePath
* @param result
*/
void UtoolHttpUploadFileToBMC(UtoolRedfishServer *server, const char *uploadFilePath, UtoolResult *result);


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
 * Upload file to BMC temp storage through CURL lib with sftp protocol.
 *
 * Be caution that to enable this feature, the CURL lib must be configured with `--with-libssh2`
 *
 * @param server            redfish server meta information
 * @param uploadFilePath    local file path to upload
 * @param result            customer function execution result for utool
 */
void UtoolSftpUploadFileToBMC(UtoolRedfishServer *server, char *uploadFilePath, UtoolResult *result);

/**
 * Upload file to BMC temp storage through OS curl command.
 *
 * @param server            redfish server meta information
 * @param uploadFilePath    local file path to upload
 * @param result            customer function execution result for utool
 */
void UtoolCurlCmdUploadFileToBMC(UtoolRedfishServer *server, char *uploadFilePath, UtoolResult *result);

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
* @param server     redfish server meta
* @param cJSONTask  redfish task node in cJSON node format
* @param result
* @return
*/
UtoolRedfishTask *UtoolRedfishMapTaskFromJson(UtoolRedfishServer *server, cJSON *cJSONTask, UtoolResult *result);

/**
* Wait a redfish task finished (completed or exception/failed)
*
* @param server
* @param cJSONTask
* @param result
*/
void UtoolRedfishWaitUtilTaskFinished(UtoolRedfishServer *server, cJSON *cJSONTask, UtoolResult *result);

/**
*  Wait a redfish task util it starts.
*
*   - result->data will carry the last success response of "get-task" request, caller should free it themself.
*
* @param server
* @param cJSONTask
* @param result
*/
void UtoolRedfishWaitUtilTaskStart(UtoolRedfishServer *server, cJSON *cJSONTask, UtoolResult *result);

/**
*  check whether a redfish task is success
* @param task
* @return true if success else false
*/
bool UtoolIsRedfishTaskSuccess(cJSON *task);

/**
* check whether a redfish task state in desired list
*
* @param task
* @param states
* @return
*/
bool UtoolIsRedfishTaskInArray(cJSON *task, const char *states[]);

#ifdef __cplusplus
}
#endif //UTOOL_REDFISH_H
#endif
