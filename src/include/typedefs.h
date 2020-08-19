/*
* Copyright © Huawei Technologies Co., Ltd. 2012-2018. All rights reserved.
* Description: common typedef
* Author:
* Create: 2019-06-16
* Notes:
*/
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
    int code;           /** result code, 0 means utool success process user's request
                         *  no matter process result is success or failure.
                         *  If process is broken(curl internal issue etc), result code will identify
                         *  the broken reason.
                         **/
    char *desc;         /** result desc */
    cJSON *data;         /** result data */
    int broken;      /** whether the program is broken, default no(0) otherwise yes */
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
    int ipmiPort;
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
    char *psn;
} UtoolRedfishServer;


/**
 * Curl response meta properties
 */
typedef struct _CurlResponse
{
    char *content;
    size_t size;
    long int httpStatusCode;
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

    int (*filter)(cJSON *);                            // whether this property should output, return 1 if accept, 0 not.
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

/* IPMI option */
typedef struct _IPMIRawCmdOption {
    char *command;
    char *data;
    char *netfun;
    char *bridge;
    char *target;
} UtoolIPMIRawCmdOption;

#ifdef __cplusplus
}
#endif
#endif //UTOOL_TYPEDEFS_H
