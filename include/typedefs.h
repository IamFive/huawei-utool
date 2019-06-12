#ifndef UTOOL_TYPEDEFS_H
#define UTOOL_TYPEDEFS_H

/* For c++ compatibility */
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include "cJSON.h"


/**
* function call result structure
*/
typedef struct _Result
{
    int code;           /** result code */
    char *desc;         /** result desc */
    cJSON *data;         /** result data */
    int interrupt;      /** whether the program is interrupt, default no(0) otherwise yes */
    int retryable;
} UtoolResult;


/**
 * Redfish Server basic properties
 */

typedef enum _CommandOptionFlag
{
    EXECUTABLE = 0,
    ILLEGAL = 1,          /** invalid options */
    FEAT_HELP = 2,        /** for help feature */
    FEAT_VERSION = 3,     /** for version feature */
} UtoolCommandOptionFlag;


typedef struct _CommandOption
{
    char *host;
    int port;
    char *username;
    char *password;
    int commandArgc;
    UtoolCommandOptionFlag flag;  /** whether the command should be executed, default yes(0) otherwise no */
    const char **commandArgv;

} UtoolCommandOption;


typedef enum _CommandType
{
    GET = 1, SET = 2, DEBUG = 3,
} UtoolCommandType;

/**
 * UTOOL Command meta
 */
typedef struct _Command
{
    char *name;
    UtoolCommandType type;

    int (*pFuncExecute)(UtoolCommandOption *, char **);
} UtoolCommand;


/**
 * Redfish Server meta properties
 */
typedef struct _RedfishServer
{
    char *host;
    char *baseUrl;
    char *username;
    char *password;
    char *systemId;
} UtoolRedfishServer;


/**
 * Curl response meta properties
 */
typedef struct _CurlResponse
{
    char *content;
    size_t size;
    int httpStatusCode;
    char *etag;
    long contentLength;
    char *contentType;
    FILE *downloadToFP;  /** used for download file request */
} UtoolCurlResponse;

/**
 * Curl response meta properties
 */
typedef struct _CurlProgress
{
    double dltotal;
    double dlnow;
    double ultotal;
    double ulnow;
} UtoolCurlProgress;


/**
 * Curl header meta properties
 */
typedef struct _CurlHeader
{
    const char *const name;
    const char *const value;
} UtoolCurlHeader;


/**
 * Curl response to output json-path mapping
 */
typedef struct _OutputMapping
{
    const char *const sourceXpath;
    const char *const targetKeyValue;
    const struct _OutputMapping *nestMapping;

    int (*filter)(cJSON *);                            // whether this property should output
    int (*handle)(cJSON *, const char *key, cJSON *);   // customer handler
} UtoolOutputMapping;


/**
* Redfish Task structure
*/

typedef struct _RedfishMessage
{
    char *id;
    char *message;
    char *severity;
    char *resolution;

} UtoolRedfishMessage;

typedef struct _RedfishTask
{
    char *url;
    char *id;
    char *name;
    char *taskState;
    char *startTime;
    char *taskPercentage;
    UtoolRedfishMessage *message;
} UtoolRedfishTask;


#ifdef __cplusplus
}
#endif
#endif //UTOOL_TYPEDEFS_H
