#include <typedefs.h>
#include <constants.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <cJSON_Utils.h>
#include "commons.h"
#include "redfish.h"
#include "zf_log.h"
#include "string_utils.h"


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
static int UtoolCurlGetRespCallback(void *buffer, size_t size, size_t nmemb, UtoolCurlResponse *response);

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
static int UtoolCurlGetHeaderCallback(char *buffer, size_t size, size_t nitems, UtoolCurlResponse *response);

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
int UtoolUploadFileToBMC(UtoolRedfishServer *server, const char *uploadFilePath, UtoolCurlResponse *response)
{
    ZF_LOGI("Try to upload file `%s` to BMC now", uploadFilePath);

    int ret;
    struct stat fileInfo;
    struct curl_slist *curlHeaderList = NULL;

    FILE *uploadFileFp = fopen(uploadFilePath, "rb"); /* open file to upload */
    if (!uploadFileFp) {
        return UTOOLE_ILLEGAL_LOCAL_FILE_PATH;
    } /* can't continue */

    ///* get the file size */
    if (fstat(fileno(uploadFileFp), &fileInfo) != 0) {
        ret = UTOOLE_ILLEGAL_LOCAL_FILE_SIZE;
        goto return_statement;
    } /* can't continue */


    curl_mime *form = NULL;
    curl_mimepart *field = NULL;
    CURL *curl = UtoolSetupCurlRequest(server, "/UpdateService/FirmwareInventory", HTTP_POST, response);
    if (!curl) {
        ret = UTOOLE_CURL_INIT_FAILED;
        goto return_statement;
    }

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

    /* enable verbose for easier tracing */
    //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    /* Perform the request, res will get the return code */
    ret = curl_easy_perform(curl);
    if (ret == CURLE_OK) { /* Check for errors */
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->httpStatusCode);
        if (response->httpStatusCode >= 400) {
            ZF_LOGE("Failed to upload local file `%s` to BMC, http status code is %d.",
                    uploadFilePath, response->httpStatusCode);
        }
    }
    else {
        const char *error = curl_easy_strerror((CURLcode) ret);
        ZF_LOGE("Failed upload file to BMC, CURL code is %d, error is %s.", ret, error);
    }

    goto return_statement;

return_statement:
    if (uploadFileFp) {         /* close FP */
        fclose(uploadFileFp);
    }
    if (curlHeaderList) {
        curl_slist_free_all(curlHeaderList);
    }
    curl_easy_cleanup(curl);    /* cleanup the curl */
    curl_mime_free(form);       /* cleanup the form */
    return ret;
}


static char *DOWNLOAD_BMC_FILE_PAYLOAD = "{ \"TransferProtocol\" : \"HTTPS\", \"Path\" : \"%s\" }";

static size_t WriteStreamToFP(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

int UtoolDownloadFileFromBMC(UtoolRedfishServer *server, const char *bmcFileUri, const char *localFileUri,
                             UtoolCurlResponse *response)
{
    FILE *outputFileFP = fopen(localFileUri, "wb");
    if (!outputFileFP) {
        return UTOOLE_ILLEGAL_LOCAL_FILE_PATH;
    }

    char *url = "/Managers/%s/Actions/Oem/Huawei/Manager.GeneralDownload";
    CURL *curl = UtoolSetupCurlRequest(server, url, HTTP_POST, response);
    if (!curl) {
        return UTOOLE_CURL_INIT_FAILED;
    }

    // setup content type
    struct curl_slist *curlHeaderList = NULL;
    curlHeaderList = curl_slist_append(curlHeaderList, CONTENT_TYPE_JSON);
    curlHeaderList = curl_slist_append(curlHeaderList, "Expect:");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curlHeaderList);

    // setup payload, payload should be freed by caller
    char payload[MAX_PAYLOAD_LEN];
    snprintf(payload, MAX_PAYLOAD_LEN, DOWNLOAD_BMC_FILE_PAYLOAD, bmcFileUri);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(payload));

    /* write data to file  */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteStreamToFP);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, outputFileFP);

    // perform request
    int ret = curl_easy_perform(curl);
    if (ret == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->httpStatusCode);
    }
    else {
        const char *error = curl_easy_strerror((CURLcode) ret);
        ZF_LOGE("Failed to perform http request, CURL code is %d, error is %s", ret, error);
    }

    goto return_statement;

return_statement:
    if (outputFileFP) {
        fclose(outputFileFP);
    }
    curl_easy_cleanup(curl);
    curl_slist_free_all(curlHeaderList);
    return ret;
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
        char buffer[MAX_URL_LEN];
        snprintf(buffer, MAX_URL_LEN, "%s: %s", header->name, header->value);
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
                goto return_statement;
            }

            char ifMatch[MAX_HEADER_LEN];
            snprintf(ifMatch, MAX_URL_LEN, "%s: %s", HEADER_IF_MATCH, response->etag);
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
            goto return_statement;
        }
        ZF_LOGD("Sending payload: %s", payloadContent);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloadContent);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(payloadContent));
    }

    // perform request
    ret = curl_easy_perform(curl);
    if (ret == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->httpStatusCode);
    }
    else {
        const char *error = curl_easy_strerror((CURLcode) ret);
        ZF_LOGE("Failed to perform http request, CURL code is %d, error is %s", ret, error);
    }

    goto return_statement;

return_statement:
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
        strncat(fullURL, server->baseUrl, strlen(server->baseUrl));
        if (strstr(resourceURL, "/redfish/v1") == NULL) {
            strncat(fullURL, "/redfish/v1", strlen("/redfish/v1"));
        }

        if (strstr(resourceURL, "%s") != NULL) {
            char _resourceURL[MAX_URL_LEN] = {0};
            snprintf(_resourceURL, MAX_URL_LEN, resourceURL, server->systemId);
            strncat(fullURL, _resourceURL, strlen(_resourceURL));
        }
        else {
            strncat(fullURL, resourceURL, strlen(resourceURL));
        }

        ZF_LOGI("[%s] %s", httpMethod, fullURL);

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
    }
    else {
        ZF_LOGE("Failed to init curl, aboard request.");
    }

    return curl;
}


/**
 * resolve redfish failures response
 *
 * @param response
 * @param result
 * @return
 */
int UtoolResolveFailureResponse(UtoolCurlResponse *response, char **result)
{
    int code = response->httpStatusCode;
    ZF_LOGE("Failed to execute request, http error code -> %d", code);

    // handle standard internet errors
    if (403 == code) {
        return UtoolBuildStringOutputResult(STATE_FAILURE, FAIL_NO_PRIVILEGE, result);
    }
    else if (412 == code) {
        return UtoolBuildStringOutputResult(STATE_FAILURE, FAIL_PRE_CONDITION_FAILED, result);
    }
    else if (413 == code) {
        return UtoolBuildStringOutputResult(STATE_FAILURE, FAIL_ENTITY_TOO_LARGE, result);
    }
    else if (code == 500) {
        return UtoolBuildStringOutputResult(STATE_FAILURE, FAIL_INTERNAL_SERVICE_ERROR, result);
    }
    else if (code == 501) {
        return UtoolBuildStringOutputResult(STATE_FAILURE, FAIL_NOT_SUPPORT, result);
    }

    // if status code not in the list above, read detail from response content
    ZF_LOGE("Failed to execute command, error response -> %s", response->content);
    cJSON *failures = cJSON_CreateArray();
    int ret = UtoolGetFailuresFromResponse(response, failures);
    if (ret != UTOOLE_OK) {
        return ret;
    }

    return UtoolBuildOutputResult(STATE_FAILURE, failures, result);
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
        goto done;
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
                goto done;
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
                    goto done;
                }

                char buffer[MAX_FAILURE_MSG_LEN];
                snprintf(buffer, MAX_FAILURE_MSG_LEN, "[%s] %s Resolution: %s", severity->valuestring,
                         message->valuestring, resolution->valuestring);

                cJSON *failure = cJSON_CreateString(buffer);
                if (failure != NULL) {
                    cJSON_AddItemToArray(failures, failure);
                }
                else {
                    FREE_CJSON(failures);
                    ret = UTOOLE_INTERNAL;
                }
            }
        }
    }
    else {
        ret = UTOOLE_UNKNOWN_JSON_FORMAT;
    }


    goto done;

done:
    FREE_CJSON(json);
    return ret;
}


int UtoolGetRedfishServer(UtoolCommandOption *option, UtoolRedfishServer *server, char **result)
{
    ZF_LOGI("Try to fetch redfish system id now.");

    cJSON *json = NULL;

    char *baseUrl = malloc(MAX_URL_LEN);
    if (baseUrl == NULL) {
        return UTOOLE_INTERNAL;
    }
    snprintf(baseUrl, MAX_URL_LEN, "https://%s:%d", option->host, option->port);

    server->baseUrl = baseUrl;
    server->host = (char *) malloc(strlen(option->host) + 1);
    strncpy(server->host, option->host, strlen(option->host) + 1);
    server->username = (char *) malloc(strlen(option->username) + 1);
    strncpy(server->username, option->username, strlen(option->username) + 1);
    server->password = (char *) malloc(strlen(option->password) + 1);
    strncpy(server->password, option->password, strlen(option->password) + 1);

    char resourceUrl[MAX_URL_LEN] = "/Systems";
    UtoolCurlResponse *response = &(UtoolCurlResponse) {0};
    int ret = UtoolMakeCurlRequest(server, resourceUrl, HTTP_GET, NULL, NULL, response);
    if (ret != CURLE_OK) {
        goto return_statement;
    }

    // parse response content and detect redfish-system-id
    if (response->httpStatusCode >= 200 && response->httpStatusCode < 300) {
        json = cJSON_Parse(response->content);
        ret = UtoolAssetParseJsonNotNull(json);
        if (ret != UTOOLE_OK) {
            goto return_statement;
        }

        cJSON *node = cJSONUtils_GetPointer(json, "/Members/0/@odata.id");
        ret = UtoolAssetJsonNodeNotNull(node, "/Members/0/@odata.id");
        if (ret != UTOOLE_OK) {
            goto return_statement;
        }

        char *pSystemId = UtoolStringLastSplit(node->valuestring, '/');
        ZF_LOGI("Fetch redfish system id succeed, system id is: %s", pSystemId);
        server->systemId = (char *) malloc(strlen(pSystemId) + 1);
        if (server->systemId == NULL) {
            ret = UTOOLE_INTERNAL;
            goto return_statement;
        }

        strncpy(server->systemId, pSystemId, strlen(pSystemId) + 1);
    }
    else {
        ZF_LOGE("Failed to get redfish system id, CURL request result is %s", curl_easy_strerror(ret));
        ret = UtoolResolveFailureResponse(response, result);
    }

    goto return_statement;

return_statement:
    FREE_CJSON(json)
    UtoolFreeCurlResponse(response);
    return ret;
}


static int UtoolCurlGetRespCallback(void *buffer, size_t size, size_t nmemb, UtoolCurlResponse *response)
{
    // realloc method is forbidden in HUAWEI developing documents.
    // because CURL may response multiple times to write response content
    // so we malloc enough memory according to content length header directly
    unsigned long fullSize = size * nmemb;
    if (response->content == NULL) {
        unsigned long length = response->contentLength > 0 ? response->contentLength : fullSize * 10;
        response->content = malloc(length + 1);
        response->content[0] = '\0';
        response->size = length;
    }

    strncat(response->content, buffer, fullSize);

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


static int UtoolCurlGetHeaderCallback(char *buffer, size_t size, size_t nitems, UtoolCurlResponse *response)
{
    if (UtoolStringCaseStartsWith((const char *) buffer, (const char *) HEADER_CONTENT_LENGTH)) {
        // get response content
        unsigned long fullSize = size * nitems;
        char content[fullSize - 1];
        memcpy(content, buffer, fullSize - 1);
        content[fullSize - 2] = '\0';

        //int len = strlen(content) - strlen(HEADER_CONTENT_LENGTH) + 1;
        //char *contentLength = (char *) malloc(len);
        //memcpy(contentLength, content + strlen(HEADER_CONTENT_LENGTH), len);

        char *length = buffer + strlen(HEADER_CONTENT_LENGTH);
        response->contentLength = strtol(length, NULL, 10);
    }

    if (UtoolStringCaseStartsWith((const char *) buffer, (const char *) HEADER_ETAG)) {
        // get response content
        unsigned long fullSize = size * nitems;
        char content[fullSize - 1];
        memcpy(content, buffer, fullSize - 1);
        content[fullSize - 2] = '\0';

        int len = strlen(content) - strlen(HEADER_ETAG) + 1;
        char *etag = (char *) malloc(len);
        memcpy(etag, content + strlen(HEADER_ETAG), len);

        response->etag = etag;
    }

    return nitems * size;
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
        goto failure;
    }

    if (response->httpStatusCode >= 400) {
        result->code = UtoolResolveFailureResponse(response, &(result->desc));
        goto failure;
    }

    result->data = cJSON_Parse(response->content);
    result->code = UtoolAssetParseJsonNotNull(result->data);
    if (result->code != UTOOLE_OK) {
        goto failure;
    }

    if (output && outputMapping) {
        result->code = UtoolMappingCJSONItems(result->data, output, outputMapping);
        if (result->code != UTOOLE_OK) {
            FREE_CJSON(result->data)
            goto failure;
        }
    }

    goto done;

failure:
    result->interrupt = 1;
    goto done;

done:
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


void UtoolRedfishGetMemberResources(UtoolRedfishServer *server, cJSON *owner, cJSON *memberArray,
                                    const UtoolOutputMapping *memberMapping, UtoolResult *result)
{
    cJSON *outputMember = NULL;

    cJSON *link = NULL;
    cJSON *links = cJSON_IsArray(owner) ? owner : cJSON_GetObjectItem(owner, "Members");
    cJSON_ArrayForEach(link, links) {
        outputMember = cJSON_CreateObject();
        result->code = UtoolAssetCreatedJsonNotNull(outputMember);
        if (result->code != UTOOLE_OK) {
            goto failure;
        }

        cJSON *linkNode = cJSON_GetObjectItem(link, "@odata.id");
        char *url = linkNode->valuestring;
        UtoolRedfishGet(server, url, outputMember, memberMapping, result);
        if (result->interrupt) {
            goto failure;
        }

        cJSON_AddItemToArray(memberArray, outputMember);

        // free memory
        FREE_CJSON(result->data)
    }

    goto done;

failure:
    result->interrupt = 1;
    FREE_CJSON(outputMember)  /** make sure output member is freed */
    goto done;

done:
    FREE_CJSON(result->data)  /** make sure result cJSON data is freed */
    return;
}


