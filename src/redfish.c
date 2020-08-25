/*
* Copyright © Huawei Technologies Co., Ltd. 2012-2018. All rights reserved.
* Description: redfish API abstract
* Author:
* Create: 2019-06-16
* Notes:
*/
#include <typedefs.h>
#include <constants.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <curl/curl.h>
#include <cJSON_Utils.h>
#include <securec.h>
#include <libgen.h>
#include "commons.h"
#include "redfish.h"
#include "zf_log.h"
#include "string_utils.h"

//static const char *redfishTaskSuccessStatus[] = {TASK_STATE_OK,
//                                                 TASK_STATE_COMPLETED,
//                                                 NULL};
//
//static const char *redfishTaskFinishedStatus[] = {TASK_STATE_OK,
//                                                  TASK_STATE_COMPLETED,
//                                                  TASK_STATE_EXCEPTION,
//                                                  TASK_STATE_INTERRUPTED,
//                                                  TASK_STATE_KILLED,
//                                                  NULL};

/**
 * Common CURL write data function.
 * Used as get CURL response callback.
 *
 * @param ptr
 * @param size
 * @param nmemb
 * @param response
 * @return
 */
static int UtoolCurlGetRespCallback(const void *buffer, size_t size, size_t nmemb, UtoolCurlResponse *response);

/**
 * Common CURL write header function.
 * Used as get CURL header callback.
 *
 * @param ptr
 * @param size
 * @param nmemb
 * @param response
 * @return
 */
static int UtoolCurlGetHeaderCallback(const char *buffer, size_t size, size_t nitems, UtoolCurlResponse *response);

static int
UtoolCurlPrintUploadProgressCallback(void *output, double dltotal, double dlnow, double ultotal, double ulnow);

/**
* Setup a new Curl Request with common basic config for redfish API.
*
* @param server
* @param resourceURL
* @param httpMethod
* @return
*/
static CURL *UtoolSetupCurlRequest(const UtoolRedfishServer *server,
                                   const char *resourceURL,
                                   const char *httpMethod,
                                   UtoolCurlResponse *response);


/**
* Upload a file to BMC Temp storage
*
* @param server
* @param uploadFilePath upload file path
* @param response
* @return
*/
void UtoolUploadFileToBMC(UtoolRedfishServer *server, const char *uploadFilePath, UtoolResult *result)
{
    ZF_LOGI("Try to upload file `%s` to BMC now", uploadFilePath);

    FILE *uploadFileFp = NULL;
    UtoolCurlResponse *response = &(UtoolCurlResponse) {0};

    CURL *curl = NULL;
    curl_mime *form = NULL;
    struct stat fileInfo;
    struct curl_slist *curlHeaderList = NULL;

    char path[PATH_MAX] = {0};
    UtoolFileRealpath(uploadFilePath, path);
    if (path == NULL) {
        result->code = UTOOLE_ILLEGAL_LOCAL_FILE_PATH;
        goto FAILURE;
    }

    uploadFileFp = fopen(path, "rb"); /* open file to upload */
    if (!uploadFileFp) {
        result->code = UTOOLE_ILLEGAL_LOCAL_FILE_PATH;
        goto FAILURE;
    } /* can't continue */

    ///* get the file size */
    if (fstat(fileno(uploadFileFp), &fileInfo) != 0) {
        result->code = UTOOLE_ILLEGAL_LOCAL_FILE_SIZE;
        goto FAILURE;
    } /* can't continue */

    curl_mimepart *field = NULL;
    curl = UtoolSetupCurlRequest(server, "/UpdateService/FirmwareInventory", HTTP_POST, response);
    if (!curl) {
        result->code = UTOOLE_CURL_INIT_FAILED;
        goto FAILURE;
    }

    /** reset CURL timeout for uploading file */
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, CURL_UPLOAD_TIMEOUT);

    // setup content type
    //curlHeaderList = curl_slist_append(curlHeaderList, "Expect:");
    //curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curlHeaderList);

    /* Create the form */
    form = curl_mime_init(curl);

    /* Fill in the file upload field */
    field = curl_mime_addpart(form);
    curl_mime_name(field, "imgfile");
    curl_mime_filedata(field, uploadFilePath);

    // setup mime post form
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);

    // setup progress callback
    bool finished = false;
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, UtoolCurlPrintUploadProgressCallback);
    curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &finished);

    /* enable verbose for easier tracing */
    //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    /* Perform the request, res will get the return code */
    result->code = curl_easy_perform(curl);
    if (result->code == CURLE_OK) { /* Check for errors */
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->httpStatusCode);
        if (response->httpStatusCode >= 400) {
            ZF_LOGE("Failed to upload local file `%s` to BMC, http status code is %ld.",
                    uploadFilePath, response->httpStatusCode);
            result->code = UtoolResolveFailureResponse(response, &(result->desc));
            result->retryable = 1; // TODO?? should we only retry when BMC return failure?
            goto FAILURE;
        }
    } else {
        const char *error = curl_easy_strerror((CURLcode) result->code);
        ZF_LOGE("Failed upload file to BMC, CURL code is %d, error is %s.", result->code, error);
        goto FAILURE;
    }

    goto DONE;


FAILURE:
    result->broken = 1;
    goto DONE;

DONE:
    if (uploadFileFp) {         /* close FP */
        fclose(uploadFileFp);
    }
    if (curlHeaderList) {
        curl_slist_free_all(curlHeaderList);
    }
    curl_easy_cleanup(curl);    /* cleanup the curl */
    curl_mime_free(form);       /* cleanup the form */
    UtoolFreeCurlResponse(response);
}


static char *DOWNLOAD_BMC_FILE_PAYLOAD = "{ \"TransferProtocol\" : \"HTTPS\", \"Path\" : \"%s\" }";

static size_t WriteStreamToFP(const void *buffer, size_t size, size_t nmemb, UtoolCurlResponse *response)
{
    if (UtoolStringEquals("application/octet-stream", response->contentType)) {
        size_t written = fwrite(buffer, size, nmemb, response->downloadToFP);
        return written;
    } else {
        return (size_t) UtoolCurlGetRespCallback(buffer, size, nmemb, response);
    }
}

void UtoolDownloadFileFromBMC(UtoolRedfishServer *server, const char *bmcFileUri, const char *localFileUri,
                              UtoolResult *result)
{

    UtoolCurlResponse *response = &(UtoolCurlResponse) {0};

    FILE *outputFileFP = NULL;
    char realFilepath[PATH_MAX] = {0};
    UtoolFileRealpath(localFileUri, realFilepath);
    if (realFilepath == NULL) {
        result->broken = 1;
        result->code = UTOOLE_ILLEGAL_LOCAL_FILE_PATH;
        return;
    }

    outputFileFP = fopen(realFilepath, "wb");
    if (!outputFileFP) {
        result->broken = 1;
        result->code = UTOOLE_ILLEGAL_LOCAL_FILE_PATH;
        return;
    }
    response->downloadToFP = outputFileFP;

    char *url = "/Managers/%s/Actions/Oem/Huawei/Manager.GeneralDownload";
    CURL *curl = UtoolSetupCurlRequest(server, url, HTTP_POST, response);
    if (!curl) {
        result->broken = 1;
        result->code = UTOOLE_CURL_INIT_FAILED;
        return;
    }

    // setup content type
    struct curl_slist *curlHeaderList = NULL;
    curlHeaderList = curl_slist_append(curlHeaderList, CONTENT_TYPE_JSON);
    curlHeaderList = curl_slist_append(curlHeaderList, "Expect:");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curlHeaderList);

    // setup payload, payload should be freed by caller
    char payload[MAX_PAYLOAD_LEN] = {0};
    snprintf_s(payload, MAX_PAYLOAD_LEN, MAX_PAYLOAD_LEN, DOWNLOAD_BMC_FILE_PAYLOAD, bmcFileUri);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(payload));

    /* ask libcurl to show us the verbose output */
    // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    /* write data to file  */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteStreamToFP);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);

    // perform request
    result->code = curl_easy_perform(curl);
    if (result->code == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->httpStatusCode);
        if (response->httpStatusCode >= 400) {
            result->code = UtoolResolveFailureResponse(response, &(result->desc));
            goto FAILURE;
        }
    } else {
        const char *error = curl_easy_strerror((CURLcode) result->code);
        ZF_LOGE("Failed to perform http request, CURL code is %d, error is %s", result->code, error);
        goto FAILURE;
    }

    goto DONE;

FAILURE:
    result->broken = 1;
    goto DONE;

DONE:
    if (outputFileFP) {
        fclose(outputFileFP);
    }
    curl_easy_cleanup(curl);
    curl_slist_free_all(curlHeaderList);
    UtoolFreeCurlResponse(response);
}


/**
 * CURL read function for reading local file
 *
 * @param ptr
 * @param size
 * @param nmemb
 * @param stream
 * @return
 */
static size_t CURLReadLocalFileToStream(void *ptr, size_t size, size_t nmemb, void *stream)
{
    FILE *f = (FILE *) stream;
    size_t n;

    if (ferror(f)) {
        return CURL_READFUNC_ABORT;
    }

    n = fread(ptr, size, nmemb, f) * size;
    return n;
}


void UtoolSftpUploadFileToBMC(UtoolRedfishServer *server, char *uploadFilePath, UtoolResult *result)
{
    ZF_LOGI("Try to sftp put local file `%s` to BMC /tmp/web folder now", uploadFilePath);

    FILE *uploadFileFp = NULL;
    char sftpRemoteFileUrl[MAX_URL_LEN] = {0};
    cJSON *getNetworkProtocolRespJson = NULL;
    UtoolCurlResponse *response = &(UtoolCurlResponse) {0};

    CURL *curl = NULL;
    struct stat fileInfo;

    char path[PATH_MAX] = {0};
    UtoolFileRealpath(uploadFilePath, path);
    if (path == NULL) {
        result->code = UTOOLE_ILLEGAL_LOCAL_FILE_PATH;
        goto FAILURE;
    }

    uploadFileFp = fopen(path, "rb"); /* open file to upload */
    if (!uploadFileFp) {
        result->code = UTOOLE_ILLEGAL_LOCAL_FILE_PATH;
        goto FAILURE;
    } /* can't continue */

    /* get the file size */
    if (fstat(fileno(uploadFileFp), &fileInfo) != 0) {
        result->code = UTOOLE_ILLEGAL_LOCAL_FILE_SIZE;
        goto FAILURE;
    } /* can't continue */


    UtoolRedfishGet(server, "/Managers/%s/NetworkProtocol", NULL, NULL, result);
    if (result->broken) {
        goto FAILURE;
    }

    getNetworkProtocolRespJson = result->data;
    cJSON *isSshEnabledNode = cJSONUtils_GetPointer(getNetworkProtocolRespJson, "/SSH/ProtocolEnabled");
    // we can scp file to bmc only when ssh protocol is enabled
    if (isSshEnabledNode != NULL && cJSON_IsTrue(isSshEnabledNode)) {
        cJSON *sshPortNode = cJSONUtils_GetPointer(getNetworkProtocolRespJson, "/SSH/Port");
        int sshPort = sshPortNode->valueint;
        // sftp://user:pwd@example.com:port/path/filename
        char *encodedPassword = curl_easy_escape(NULL, server->password, strnlen(server->password, 64));
        snprintf_s(sftpRemoteFileUrl, MAX_URL_LEN, MAX_URL_LEN, "sftp://%s:%s@%s:%d/tmp/web/%s",
                   server->username, encodedPassword, server->host, sshPort, basename(uploadFilePath));
    } else {
        result->code = UTOOLE_SSH_PROTOCOL_DISABLED;
        goto FAILURE;
    }


    curl = curl_easy_init();
    if (!curl) {
        result->code = UTOOLE_CURL_INIT_FAILED;
        goto FAILURE;
    }

    /** reset CURL timeout for uploading file */
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, CURL_UPLOAD_TIMEOUT);

    /** setup CURL upload options */
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_URL, sftpRemoteFileUrl);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, CURLReadLocalFileToStream);
    curl_easy_setopt(curl, CURLOPT_READDATA, uploadFileFp);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE, (long) fileInfo.st_size);

    bool finished = false;
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, UtoolCurlPrintUploadProgressCallback);
    curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &finished);

    /* enable verbose for easier tracing */
    /* curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); */

    /* Perform the request, res will get the return code */
    result->code = curl_easy_perform(curl);
    if (result->code != CURLE_OK) { /* Check for errors */
        const char *error = curl_easy_strerror((CURLcode) result->code);
        ZF_LOGE("Failed upload file to BMC, CURL code is %d, error is %s.", result->code, error);
        goto FAILURE;
    }

    goto DONE;


FAILURE:
    result->broken = 1;
    goto DONE;

DONE:
    if (uploadFileFp) {         /* close FP */
        fclose(uploadFileFp);
    }
    curl_easy_cleanup(curl);    /* cleanup the curl */
    UtoolFreeCurlResponse(response);
}

/**
 * make new request to redfish APIs
 *
 * @param server redfish API server info
 * @param resourceURL http url of the request, could be odata-id or resource segment
 * @param httpMethod http method of the request
 * @param payload  payload of the request
 * @param response response of the request
 * @return CURL perform
 */
int UtoolMakeCurlRequest(UtoolRedfishServer *server,
                         char *resourceURL,
                         const char *httpMethod,
                         const cJSON *payload,
                         const UtoolCurlHeader *headers,
                         UtoolCurlResponse *response)
{


    int ret = UTOOLE_INTERNAL;

    char *payloadContent = NULL;
    struct curl_slist *curlHeaderList = NULL;

    const UtoolCurlHeader *ifMatchHeader = NULL;
    for (int idx = 0;; idx++) {
        const UtoolCurlHeader *header = headers + idx;
        if (header == NULL || header->name == NULL) {
            break;
        }

        if (UtoolStringEquals(header->name, HEADER_IF_MATCH)) {
            ifMatchHeader = header;
        }
        char buffer[MAX_HEADER_LEN] = {0};
        snprintf_s(buffer, MAX_HEADER_LEN, MAX_HEADER_LEN, "%s: %s", header->name, header->value);
        curlHeaderList = curl_slist_append(curlHeaderList, buffer);
    }

    // if request method is PATCH, if-match header is required
    if (UtoolStringEquals(httpMethod, HTTP_PATCH) || UtoolStringEquals(httpMethod, HTTP_PUT)) {
        if (ifMatchHeader == NULL) {
            // if if-match header is not present, try to load it through get request
            ZF_LOGE("Try to load etag through get request");
//                UtoolCurlResponse *getIfMatchResponse = &(UtoolCurlResponse) {0};
            ret = UtoolMakeCurlRequest(server, resourceURL, HTTP_GET, NULL, headers, response);
            if (ret != UTOOLE_OK) {
                goto DONE;
            }

            char ifMatch[MAX_HEADER_LEN] = {0};
            snprintf_s(ifMatch, MAX_HEADER_LEN, MAX_HEADER_LEN, "%s: %s", HEADER_IF_MATCH, response->etag);
            curlHeaderList = curl_slist_append(curlHeaderList, ifMatch);
            UtoolFreeCurlResponse(response);
        }
    }

    CURL *curl = UtoolSetupCurlRequest(server, resourceURL, httpMethod, response);
    if (!curl) {
        return UTOOLE_CURL_INIT_FAILED;
    }

    // setup headers
    curlHeaderList = curl_slist_append(curlHeaderList, CONTENT_TYPE_JSON);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curlHeaderList);

    // setup payload, payload should be freed by caller
    if (payload != NULL) {
        /** https://github.com/bagder/everything-curl/blob/master/libcurl-http-requests.md */
        payloadContent = cJSON_Print(payload);
        ret = UtoolAssetPrintJsonNotNull(payloadContent);
        if (ret != UTOOLE_OK) {
            goto DONE;
        }
        ZF_LOGD("Sending payload: %s", payloadContent);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloadContent);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(payloadContent));
    }

    // perform request
    ret = curl_easy_perform(curl);
    if (ret == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->httpStatusCode);
        ZF_LOGD("Response: %s", response->content);
    } else {
        const char *error = curl_easy_strerror((CURLcode) ret);
        ZF_LOGE("Failed to perform http request, CURL code is %d, error is %s", ret, error);
    }

    goto DONE;

DONE:
    FREE_OBJ(payloadContent)
    curl_easy_cleanup(curl);
    curl_slist_free_all(curlHeaderList);
    return ret;
}

static CURL *UtoolSetupCurlRequest(const UtoolRedfishServer *server, const char *resourceURL,
                                   const char *httpMethod, UtoolCurlResponse *response)
{
    CURL *curl = curl_easy_init();
    if (curl) {
        // replace %s with redfish-system-id if necessary
        char fullURL[MAX_URL_LEN] = {0};
        strncat_s(fullURL, MAX_URL_LEN, server->baseUrl, strnlen(server->baseUrl, MAX_URL_LEN));
        if (strstr(resourceURL, "/redfish/v1") == NULL) {
            strcat_s(fullURL, MAX_URL_LEN, "/redfish/v1");
        }

        if (strstr(resourceURL, "%s") != NULL) {
            char _resourceURL[MAX_URL_LEN] = {0};
            snprintf_s(_resourceURL, MAX_URL_LEN, MAX_URL_LEN, resourceURL, server->systemId);
            strncat_s(fullURL, MAX_URL_LEN, _resourceURL, strnlen(_resourceURL, MAX_URL_LEN));
        } else {
            strncat_s(fullURL, MAX_URL_LEN, resourceURL, strnlen(resourceURL, MAX_URL_LEN));
        }

        ZF_LOGI("[%s] %s", httpMethod, fullURL);

        /* enable verbose for easier tracing */
        /*curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);*/

        // setup basic http meta
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, httpMethod);
        curl_easy_setopt(curl, CURLOPT_URL, fullURL);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        // setup timeout
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CURL_CONN_TIMEOUT);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, CURL_TIMEOUT);

        // setup basic auth
        curl_easy_setopt(curl, CURLOPT_HTTPAUTH, (long) CURLAUTH_BASIC);
        curl_easy_setopt(curl, CURLOPT_USERNAME, server->username);
        curl_easy_setopt(curl, CURLOPT_PASSWORD, server->password);

        // setup callback
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, UtoolCurlGetHeaderCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, UtoolCurlGetRespCallback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, response);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    } else {
        ZF_LOGE("Failed to init curl, aboard request.");
    }

    return curl;
}


/**
 * resolve redfish failures response,
 *
 * For new implementation, use UtoolResolvePartialFailureResponse. This function is for backword compatible.
 *
 * @param response
 * @param result
 * @return
 */
int UtoolResolveFailureResponse(UtoolCurlResponse *response, char **result)
{
    long int code = response->httpStatusCode;
    ZF_LOGE("Failed to execute request, http error code -> %ld", code);

    // handle standard internet errors
    if (403 == code) {
        return UtoolBuildStringOutputResult(STATE_FAILURE, FAIL_NO_PRIVILEGE, result);
    } else if (412 == code) {
        return UtoolBuildStringOutputResult(STATE_FAILURE, FAIL_PRE_CONDITION_FAILED, result);
    } else if (413 == code) {
        return UtoolBuildStringOutputResult(STATE_FAILURE, FAIL_ENTITY_TOO_LARGE, result);
    } else if (code == 500) {
        return UtoolBuildStringOutputResult(STATE_FAILURE, FAIL_INTERNAL_SERVICE_ERROR, result);
    } else if (code == 501) {
        return UtoolBuildStringOutputResult(STATE_FAILURE, FAIL_NOT_SUPPORT, result);
    }

    // if status code not in the list above, read detail from response content
    ZF_LOGE("Failed to execute request, error response -> %s", response->content);
    cJSON *failures = cJSON_CreateArray();
    int ret = UtoolGetFailuresFromResponse(response, failures);
    if (ret != UTOOLE_OK) {
        return ret;
    }

    return UtoolBuildOutputResult(STATE_FAILURE, failures, result);
}


/**
* resolve redfish response
*  - failed response
*  - partial failed response
*
* @param response
* @param result
* @return true if resolved else false
*/
bool UtoolResolvePartialFailureResponse(UtoolCurlResponse *response, UtoolResult *result)
{
    long int code = response->httpStatusCode;
    ZF_LOGI("Try to resolve partial failure, http code -> %ld", code);
    ZF_LOGI("Try to resolve partial failure, http response -> %s", response->content);

    // handle standard internet errors
    if (403 == code) {
        result->code = UtoolBuildStringOutputResult(STATE_FAILURE, FAIL_NO_PRIVILEGE, &(result->desc));
        goto RESOLVED;
    } else if (412 == code) {
        result->code = UtoolBuildStringOutputResult(STATE_FAILURE, FAIL_PRE_CONDITION_FAILED, &(result->desc));
        goto RESOLVED;
    } else if (413 == code) {
        result->code = UtoolBuildStringOutputResult(STATE_FAILURE, FAIL_ENTITY_TOO_LARGE, &(result->desc));
        goto RESOLVED;
    } else if (code == 500) {
        result->code = UtoolBuildStringOutputResult(STATE_FAILURE, FAIL_INTERNAL_SERVICE_ERROR, &(result->desc));
        goto RESOLVED;
    } else if (code == 501) {
        result->code = UtoolBuildStringOutputResult(STATE_FAILURE, FAIL_NOT_SUPPORT, &(result->desc));
        goto RESOLVED;
    }

    cJSON *failures = cJSON_CreateArray();
    result->code = UtoolGetFailuresFromResponse(response, failures);
    if (result->code != UTOOLE_OK) {
        FREE_CJSON(failures)
        goto RESOLVED;
    }

    if (cJSON_GetArraySize(failures) > 0) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, failures, &(result->desc));
        goto RESOLVED;
    }

    FREE_CJSON(failures)
    return false;

RESOLVED:
    result->broken = 1;
    return true;
}

/**
 * generate failure message array from redfish failure response
 *
 * @param response
 * @param failures
 * @return
 */
int UtoolGetFailuresFromResponse(UtoolCurlResponse *response, cJSON *failures)
{
    cJSON *json = cJSON_Parse(response->content);
    int ret = UtoolAssetParseJsonNotNull(json);
    if (ret != UTOOLE_OK) {
        goto DONE;
    }

    cJSON *extendedInfoArray = cJSONUtils_GetPointer(json, "/error/@Message.ExtendedInfo");
    if (extendedInfoArray == NULL) {
        extendedInfoArray = cJSONUtils_GetPointer(json, "/@Message.ExtendedInfo");
    }

    if (extendedInfoArray != NULL && cJSON_IsArray(extendedInfoArray)) {
        int count = cJSON_GetArraySize(extendedInfoArray);
        if (count == 1) {
            cJSON *extendInfo = cJSON_GetArrayItem(extendedInfoArray, 0);
            cJSON *severity = cJSON_GetObjectItem(extendInfo, "Severity");
            if (severity != NULL && UtoolStringEquals(severity->valuestring, SEVERITY_OK)) {
                goto DONE;
            }
        }

        if (count > 0) {
            int idx = 0;
            for (; idx < count; idx++) {
                cJSON *extendInfo = cJSON_GetArrayItem(extendedInfoArray, idx);
                cJSON *severity = cJSON_GetObjectItem(extendInfo, "Severity");
                cJSON *resolution = cJSON_GetObjectItem(extendInfo, "Resolution");
                cJSON *message = cJSON_GetObjectItem(extendInfo, "Message");
                if (severity == NULL || resolution == NULL || message == NULL) {
                    ret = UTOOLE_UNKNOWN_JSON_FORMAT;
                    goto DONE;
                }

                char buffer[MAX_FAILURE_MSG_LEN];
                snprintf_s(buffer, MAX_FAILURE_MSG_LEN, MAX_FAILURE_MSG_LEN, "[%s] %s Resolution: %s",
                           severity->valuestring, message->valuestring, resolution->valuestring);

                cJSON *failure = cJSON_CreateString(buffer);
                if (failure != NULL) {
                    cJSON_AddItemToArray(failures, failure);
                } else {
                    FREE_CJSON(failures);
                    ret = UTOOLE_INTERNAL;
                }
            }
        }
    }

    goto DONE;

DONE:
    FREE_CJSON(json);
    return ret;
}


int UtoolGetRedfishServer(UtoolCommandOption *option, UtoolRedfishServer *server, char **result)
{
    ZF_LOGI("Try to fetch redfish system id now.");

    cJSON *json = NULL;

    char *baseUrl = (char *) malloc(MAX_URL_LEN);
    if (baseUrl == NULL) {
        return UTOOLE_INTERNAL;
    }
    snprintf_s(baseUrl, MAX_URL_LEN, MAX_URL_LEN, "https://%s:%d", option->host, option->port);
    server->baseUrl = baseUrl;

    size_t sizeHostUrl = strlen(option->host);
    server->host = (char *) malloc(sizeHostUrl + 1);
    if (server->host == NULL) {
        return UTOOLE_INTERNAL;
    }
    strncpy_s(server->host, sizeHostUrl + 1, option->host, sizeHostUrl);

    size_t sizeUsername = strlen(option->username);
    server->username = (char *) malloc(sizeUsername + 1);
    if (server->username == NULL) {
        return UTOOLE_INTERNAL;
    }
    strncpy_s(server->username, sizeUsername + 1, option->username, sizeUsername);

    size_t sizePassword = strlen(option->password);
    server->password = (char *) malloc(sizePassword + 1);
    if (server->password == NULL) {
        return UTOOLE_INTERNAL;
    }
    strncpy_s(server->password, sizePassword + 1, option->password, sizePassword);

    char resourceUrl[MAX_URL_LEN] = "/Systems";
    UtoolCurlResponse *response = &(UtoolCurlResponse) {0};
    int ret = UtoolMakeCurlRequest(server, resourceUrl, HTTP_GET, NULL, NULL, response);
    if (ret != CURLE_OK) {
        goto DONE;
    }

    // parse response content and detect redfish-system-id
    if (response->httpStatusCode >= 200 && response->httpStatusCode < 300) {
        json = cJSON_Parse(response->content);
        ret = UtoolAssetParseJsonNotNull(json);
        if (ret != UTOOLE_OK) {
            goto DONE;
        }

        cJSON *node = cJSONUtils_GetPointer(json, "/Members/0/@odata.id");
        ret = UtoolAssetJsonNodeNotNull(node, "/Members/0/@odata.id");
        if (ret != UTOOLE_OK) {
            goto DONE;
        }

        char *pSystemId = UtoolStringLastSplit(node->valuestring, '/');
        ZF_LOGI("Fetch redfish system id succeed, system id is: %s", pSystemId);
        size_t sizeSystemId = strlen(pSystemId);
        server->systemId = (char *) malloc(sizeSystemId + 1);
        if (server->systemId == NULL) {
            ret = UTOOLE_INTERNAL;
            goto DONE;
        }
        strncpy_s(server->systemId, sizeSystemId + 1, pSystemId, sizeSystemId);
    } else {
        ZF_LOGE("Failed to get redfish system id, CURL request result is %s", curl_easy_strerror((CURLcode) ret));
        ret = UtoolResolveFailureResponse(response, result);
    }

    goto DONE;

DONE:
    FREE_CJSON(json)
    UtoolFreeCurlResponse(response);
    return ret;
}


static int UtoolCurlGetHeaderCallback(const char *buffer, size_t size, size_t nitems, UtoolCurlResponse *response)
{
    if (buffer != NULL) {
        if (UtoolStringCaseStartsWith((const char *) buffer, (const char *) HEADER_CONTENT_LENGTH)) {
            // get response content
            unsigned long fullSize = size * nitems;
            char content[fullSize - 1];
            memcpy_s(content, fullSize - 1, buffer, fullSize - 1);
            content[fullSize - 2] = '\0';

            //int len = strlen(content) - strlen(HEADER_CONTENT_LENGTH) + 1;
            //char *contentLength = (char *) malloc(len);
            //memcpy(contentLength, content + strlen(HEADER_CONTENT_LENGTH), len);

            const char *length = buffer + strlen(HEADER_CONTENT_LENGTH);
            response->contentLength = strtol(length, NULL, 10);
        }

        if (UtoolStringCaseStartsWith((const char *) buffer, (const char *) HEADER_ETAG)) {
            // get response content
            unsigned long fullSize = size * nitems;
            char content[fullSize - 1];
            memcpy_s(content, fullSize - 1, buffer, fullSize - 1);
            content[fullSize - 2] = '\0';

            int len = strlen(content) - strlen(HEADER_ETAG) + 1;
            char *etag = (char *) malloc(len);
            if (etag != NULL) {
                memcpy_s(etag, len, content + strlen(HEADER_ETAG), len);
            }
            response->etag = etag;
        }

        if (UtoolStringCaseStartsWith((const char *) buffer, (const char *) HEADER_CONTENT_TYPE)) {
            // get response content
            unsigned long fullSize = size * nitems;
            char content[fullSize - 1];
            memcpy_s(content, fullSize - 1, buffer, fullSize - 1);
            content[fullSize - 2] = '\0';

            int len = strlen(content) - strlen(HEADER_CONTENT_TYPE) + 1;
            char *contentType = (char *) malloc(len);
            if (contentType != NULL) {
                memcpy_s(contentType, len, content + strlen(HEADER_CONTENT_TYPE), len);
            }
            response->contentType = contentType;
        }
    }

    return nitems * size;
}


static int
UtoolCurlPrintUploadProgressCallback(void *output, double dltotal, double dlnow, double ultotal, double ulnow)
{
    // (double) dltotal;
    // (double) dlnow;
    if (ultotal > 0 && ulnow > 0) {
        bool *finished = (bool *) output;
        if (!*finished) {
            //fprintf(stdout, "\33[2K\rUploading file... Progress: %.0f%%.", (ulnow * 100) / ultotal);
            fprintf(stdout, "\rUploading file... Progress: %.0f%%.", (ulnow * 100) / ultotal);
            if (ultotal == ulnow) {
                *((bool *) output) = true;
                fprintf(stdout, "\n");
            }
        }
        fflush(stdout);
    }

    return 0;
}

static int UtoolCurlGetRespCallback(const void *buffer, size_t size, size_t nmemb, UtoolCurlResponse *response)
{
    // realloc method is forbidden in HUAWEI developing documents.
    // because CURL may response multiple times to write response content
    // so we malloc enough memory according to content length header directly
    unsigned long fullSize = size * nmemb;
    unsigned long length = response->contentLength > 0 ? response->contentLength : fullSize * 10;
    if (response->content == NULL) {
        response->content = (char *) malloc(length + 1);
        if (response->content != NULL) {
            response->content[0] = '\0';
            response->size = length;
        } else {
            return 0;
        }
    }

    strncat_s(response->content, length + 1, (char *) buffer, fullSize);

    // get response content
    //char *content = (char *) malloc(fullSize + 1);
    //memcpy(content, buffer, fullSize);
    //content[fullSize] = '\0';

    // log to file in debug level
    //ZF_LOGD("Redfish response: %s", content);

    // setup structure
    //response->content = content;
    //response->size = fullSize;

    // return content size
    return fullSize;
}

void UtoolRedfishProcessRequest(UtoolRedfishServer *server,
                                char *url,
                                const char *httpMethod,
                                const cJSON *payload,
                                const UtoolCurlHeader *headers,
                                cJSON *output,
                                const UtoolOutputMapping *outputMapping,
                                UtoolResult *result)
{
    UtoolCurlResponse *response = &(UtoolCurlResponse) {0};

    result->code = UtoolMakeCurlRequest(server, url, httpMethod, payload, headers, response);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    //if (response->httpStatusCode >= 400) {
    //    result->code = UtoolResolveFailureResponse(response, &(result->desc));
    //    goto FAILURE;
    //}

    bool resolved = UtoolResolvePartialFailureResponse(response, result);
    if (resolved || result->broken) {
        goto FAILURE;
    }

    result->data = cJSON_Parse(response->content);
    result->code = UtoolAssetParseJsonNotNull(result->data);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    if (output && outputMapping) {
        result->code = UtoolMappingCJSONItems(result->data, output, outputMapping);
        if (result->code != UTOOLE_OK) {
            FREE_CJSON(result->data)
            goto FAILURE;
        }
    }

    goto DONE;

FAILURE:
    result->broken = 1;
    goto DONE;

DONE:
    UtoolFreeCurlResponse(response);
}


/**
* Get Redfish resource and parse content to JSON.
*
* if success, result->data will carry the (cJSON *) format response content.
* if interrupt, result->data will be safe freed. Caller do not need to care about the data.
*
* @param server
* @param url
* @param result
*/
void UtoolRedfishGet(UtoolRedfishServer *server, char *url, cJSON *output,
                     const UtoolOutputMapping *outputMapping, UtoolResult *result)
{
    UtoolRedfishProcessRequest(server, url, HTTP_GET, NULL, NULL, output, outputMapping, result);
}


void UtoolRedfishPost(UtoolRedfishServer *server, char *url, cJSON *payload, cJSON *output,
                      const UtoolOutputMapping *outputMapping, UtoolResult *result)
{
    UtoolRedfishProcessRequest(server, url, HTTP_POST, payload, NULL, output, outputMapping, result);
}

void UtoolRedfishPatch(UtoolRedfishServer *server, char *url, cJSON *payload, const UtoolCurlHeader *headers,
                       cJSON *output, const UtoolOutputMapping *outputMapping, UtoolResult *result)
{
    UtoolRedfishProcessRequest(server, url, HTTP_PATCH, payload, headers, output, outputMapping, result);
}

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
                        UtoolResult *result)
{
    UtoolRedfishProcessRequest(server, url, HTTP_DELETE, NULL, NULL, output, outputMapping, result);
}

/**
* Get All Redfish member resources
*
* all result->data created by this function will be auto freed.
*
* @param server
* @param owner
* @param memberArray
* @param memberMapping
* @param result
*/
void UtoolRedfishGetMemberResources(UtoolRedfishServer *server, cJSON *owner, cJSON *memberArray,
                                    const UtoolOutputMapping *memberMapping, UtoolResult *result)
{
    cJSON *outputMember = NULL;

    cJSON *memberLink = NULL;
    cJSON *links = cJSON_IsArray(owner) ? owner : cJSON_GetObjectItem(owner, "Members");
    cJSON_ArrayForEach(memberLink, links) {
        outputMember = cJSON_CreateObject();
        result->code = UtoolAssetCreatedJsonNotNull(outputMember);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }

        cJSON *linkNode = cJSON_GetObjectItem(memberLink, "@odata.id");
        char *url = linkNode->valuestring;
        UtoolRedfishGet(server, url, outputMember, memberMapping, result);
        if (result->broken) {
            goto FAILURE;
        }

        cJSON_AddItemToArray(memberArray, outputMember);

        // free memory
        FREE_CJSON(result->data)
    }

    goto DONE;

FAILURE:
    result->broken = 1;
    FREE_CJSON(outputMember)  /** make sure output member is freed */
    goto DONE;

DONE:
    return;
}


/**
* mapping a json format task to struct task
*
* @param cJSONTask
* @param result
* @return
*/
UtoolRedfishTask *UtoolRedfishMapTaskFromJson(cJSON *cJSONTask, UtoolResult *result)
{
    size_t sizeRedfishTask = sizeof(UtoolRedfishTask);
    UtoolRedfishTask *task = (UtoolRedfishTask *) malloc(sizeRedfishTask);
    if (task == NULL) {
        result->code = UTOOLE_INTERNAL;
        goto FAILURE;
    }
    memset_s(task, sizeRedfishTask, 0, sizeRedfishTask);

    size_t sizeRedfishMessage = sizeof(UtoolRedfishMessage);
    task->message = (UtoolRedfishMessage *) malloc(sizeRedfishMessage);
    if (task->message == NULL) {
        result->code = UTOOLE_INTERNAL;
        goto FAILURE;
    }
    memset_s(task->message, sizeRedfishMessage, 0, sizeRedfishMessage);

    if (!cJSON_IsObject(cJSONTask)) {
        result->code = UTOOLE_UNKNOWN_JSON_FORMAT;
        goto FAILURE;
    }

    // url
    cJSON *node = cJSON_GetObjectItem(cJSONTask, "@odata.id");
    result->code = UtoolAssetParseJsonNotNull(node);
    if (result->code != UTOOLE_OK || node->valuestring == NULL) {
        goto FAILURE;
    }
    size_t sizeTaskUrl = strnlen(node->valuestring, 128);
    task->url = (char *) malloc(sizeTaskUrl + 1);
    if (task->url != NULL) {
        strncpy_s(task->url, sizeTaskUrl + 1, node->valuestring, sizeTaskUrl);
    }

    // id
    node = cJSON_GetObjectItem(cJSONTask, "Id");
    result->code = UtoolAssetParseJsonNotNull(node);
    if (result->code != UTOOLE_OK || node->valuestring == NULL) {
        goto FAILURE;
    }

    size_t sizeTaskId = strnlen(node->valuestring, 16) + 1;
    task->id = (char *) malloc(sizeTaskId + 1);
    if (task->id != NULL) {
        strncpy_s(task->id, sizeTaskId + 1, node->valuestring, sizeTaskId);
    }

    // name
    node = cJSON_GetObjectItem(cJSONTask, "Name");
    result->code = UtoolAssetParseJsonNotNull(node);
    if (result->code != UTOOLE_OK || node->valuestring == NULL) {
        goto FAILURE;
    }

    size_t sizeTaskName = strnlen(node->valuestring, 128);
    task->name = (char *) malloc(sizeTaskName + 1);
    if (task->name != NULL) {
        strncpy_s(task->name, sizeTaskName + 1, node->valuestring, sizeTaskName);
    }

    // state
    node = cJSON_GetObjectItem(cJSONTask, "TaskState");
    if (node != NULL && node->valuestring != NULL) {
        size_t sizeTaskState = strnlen(node->valuestring, 32);
        task->taskState = (char *) malloc(sizeTaskState + 1);
        if (task->taskState != NULL) {
            strncpy_s(task->taskState, sizeTaskState + 1, node->valuestring, sizeTaskState);
        }
    }

    // startTime
    node = cJSON_GetObjectItem(cJSONTask, "StartTime");
    if (node != NULL && node->valuestring != NULL) {
        size_t sizeTaskStartTime = strnlen(node->valuestring, 32);
        task->startTime = (char *) malloc(sizeTaskStartTime + 1);
        if (task->startTime != NULL) {
            strncpy_s(task->startTime, sizeTaskStartTime + 1, node->valuestring, sizeTaskStartTime);
        }
    }

    // TaskPercent
    node = cJSONUtils_GetPointer(cJSONTask, "/Oem/Huawei/TaskPercentage");
    if (node != NULL && node->valuestring != NULL) {
        size_t sizeTaskPercent = strnlen(node->valuestring, 32);
        task->taskPercentage = (char *) malloc(sizeTaskPercent + 1);
        if (task->taskPercentage != NULL) {
            strncpy_s(task->taskPercentage, sizeTaskPercent + 1, node->valuestring, sizeTaskPercent);
        }
    }

    // messages
    cJSON *messages = cJSON_GetObjectItem(cJSONTask, "Messages");
    if (messages != NULL && !cJSON_IsNull(messages)) {
        node = cJSON_GetObjectItem(messages, "MessageId");
        if (node != NULL && node->valuestring != NULL) {
            size_t sizeTaskMessageId = strnlen(node->valuestring, 16);
            task->message->id = (char *) malloc(sizeTaskMessageId + 1);
            if (task->message->id != NULL) {
                strncpy_s(task->message->id, sizeTaskMessageId + 1, node->valuestring, sizeTaskMessageId);
            }
        }

        node = cJSON_GetObjectItem(messages, "Message");
        if (node != NULL && node->valuestring != NULL) {
            size_t sizeTaskMessage = strnlen(node->valuestring, 128);
            task->message->message = (char *) malloc(sizeTaskMessage + 1);
            if (task->message->message != NULL) {
                strncpy_s(task->message->message, sizeTaskMessage + 1, node->valuestring, sizeTaskMessage);
            }
        }

        node = cJSON_GetObjectItem(messages, "Severity");
        if (node != NULL && node->valuestring != NULL) {
            size_t sizeTaskSeverity = strnlen(node->valuestring, 32);
            task->message->severity = (char *) malloc(sizeTaskSeverity + 1);
            if (task->message->severity != NULL) {
                strncpy_s(task->message->severity, sizeTaskSeverity + 1, node->valuestring, sizeTaskSeverity);
            }
        }

        node = cJSON_GetObjectItem(messages, "Resolution");
        if (node != NULL && node->valuestring != NULL) {
            size_t sizeTaskResolution = strnlen(node->valuestring, 256);
            task->message->resolution = (char *) malloc(sizeTaskResolution + 1);
            if (task->message->resolution != NULL) {
                strncpy_s(task->message->resolution, sizeTaskResolution + 1, node->valuestring, sizeTaskResolution);
            }
        }
    }

    return task;

FAILURE:
    result->broken = 1;
    UtoolFreeRedfishTask(task);
    return NULL;
}

/**
* wait redfish task util completed or failed. If task has sub task, it will wait sub task finished first.
*
*  Be caution: if task success, result->data will carry the last success response of "get-task" request,
*    caller should free it themself.
*
* @param server
* @param cJSONTask
* @param result
*/
void UtoolRedfishWaitUtilTaskFinished(UtoolRedfishServer *server, cJSON *cJSONTask, UtoolResult *result)
{
    // waiting util task complete or exception
    UtoolRedfishTask *task = NULL;
    cJSON *jsonTask = cJSONTask;

    while (true) {
        task = UtoolRedfishMapTaskFromJson(jsonTask, result);
        if (result->broken) {
            goto FAILURE;
        }

        // print task progress to stdout
        if (task->taskPercentage == NULL) { /** try to print message if task has not started */
            if (task->message->message != NULL) {
                // main task has not start, we need to load percent from message args
                // fprintf(stdout, "\33[2K\r%s", task->message->message);
                fprintf(stdout, "%-96s\r", task->message->message);
                fflush(stdout);
            }
        } else {
            char *taskName = task->message->message == NULL ? task->name : task->message->message;
            char taskProgress[256] = {0};
            snprintf_s(taskProgress, sizeof(taskProgress), sizeof(taskProgress),
                       "%s Progress: %s complete.", taskName, task->taskPercentage);
            fprintf(stdout, "%-96s\r", taskProgress);
            fflush(stdout);
        }

        /** if task is processed */
        if (UtoolStringInArray(task->taskState, g_UtoolRedfishTaskFinishedStatus)) {
            // fprintf(stdout, "\33[2K\r");
            fprintf(stdout, "\n");
            fflush(stdout);

            /* if task failed, we build output directly */
            if (!UtoolIsRedfishTaskSuccess(result->data)) {
                result->code = UtoolBuildRsyncTaskOutputResult(result->data, &(result->desc));
                result->data = NULL;
                goto FAILURE;
            }

            goto DONE;
        }

        /** if task is still processing */
        UtoolRedfishGet(server, task->url, NULL, NULL, result);
        if (result->broken) {
            goto FAILURE;
        }
        FREE_CJSON(jsonTask)        /** free last json TASK */
        jsonTask = result->data;

        UtoolFreeRedfishTask(task); /** free task structure */
        sleep(1); /** next task query interval */
    }

FAILURE:
    result->broken = 1;
    goto DONE;

DONE:
    UtoolFreeRedfishTask(task);
}


/**
*  Wait a redfish task util it really starts.
*  For example: update firmware with remote network file will download file before it starts update firmware.
*
*  If task failed, result->interupt will be set 1,
*
*   - result->data will carry the last success response of "get-task" request, caller should free it themself.
*
* @param server
* @param cJSONTask
* @param result
*/
void UtoolRedfishWaitUtilTaskStart(UtoolRedfishServer *server, cJSON *cJSONTask, UtoolResult *result)
{
    // waiting util task complete or exception
    UtoolRedfishTask *task = NULL;
    cJSON *jsonTask = cJSONTask;

    while (true) {
        task = UtoolRedfishMapTaskFromJson(jsonTask, result);
        if (result->broken) {
            goto FAILURE;
        }

        // print task progress to stdout
        if (task->taskPercentage == NULL) { /** try to print message if task has not started */
            if (task->message->message != NULL) {
                // main task has not start, we need to load percent from message args
                //fprintf(stdout, "\33[2K\r%s", task->message->message);
                fprintf(stdout, "%-96s\r", task->message->message);
                fflush(stdout);
            }
        }

        /** if task is processed or task percent is not NULL */
        if (UtoolStringInArray(task->taskState, g_UtoolRedfishTaskFinishedStatus) || task->taskPercentage != NULL) {
            // fprintf(stdout, "\33[2K\r");
            fprintf(stdout, "\n");
            fflush(stdout);

            /* if task failed, we build output directly */
            if (UtoolIsRedfishTaskInArray(result->data, g_UtoolRedfishTaskFailedStatus)) {
                result->code = UtoolBuildRsyncTaskOutputResult(result->data, &(result->desc));
                result->data = NULL;
                goto FAILURE;
            }

            goto DONE;
        }

        /** if task is still processing */
        UtoolRedfishGet(server, task->url, NULL, NULL, result);
        if (result->broken) {
            goto FAILURE;
        }
        FREE_CJSON(jsonTask)        /** free last json TASK */
        jsonTask = result->data;

        UtoolFreeRedfishTask(task); /** free task structure */
        sleep(1); /** next task query interval */
    }

FAILURE:
    result->broken = 1;
    goto DONE;

DONE:
    UtoolFreeRedfishTask(task);
}


/**
*  check whether a redfish task is success
* @param task
* @return true if success else false
*/
bool UtoolIsRedfishTaskSuccess(cJSON *task)
{
    cJSON *status = cJSON_GetObjectItem(task, "TaskState");
    int ret = UtoolAssetJsonNodeNotNull(status, "/TaskState");
    if (ret != UTOOLE_OK) {
        return false;
    }

    return UtoolStringInArray(status->valuestring, g_UtoolRedfishTaskSuccessStatus);
}


/**
*
* check whether a redfish task state in desired list
*
* @param task
* @param states
* @return
*/
bool UtoolIsRedfishTaskInArray(cJSON *task, const char *states[])
{
    cJSON *status = cJSON_GetObjectItem(task, "TaskState");
    int ret = UtoolAssetJsonNodeNotNull(status, "/TaskState");
    if (ret != UTOOLE_OK) {
        return false;
    }

    return UtoolStringInArray(status->valuestring, states);
}

