#ifndef UTOOL_TYPEDEFS_H
#define UTOOL_TYPEDEFS_H

/* For c++ compatibility */
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "cJSON.h"

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
    GET = 1, SET = 2
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
} UtoolCurlResponse;

typedef struct _CurlHeader
{
    const char *const name;
    const char *const value;
} UtoolCurlHeader;


typedef struct _OutputMapping
{
    const char *const sourceXpath;
    const char *const targetKeyValue;

    int (*handle)(cJSON *, const char *key, cJSON *);   // customer handler
} UtoolOutputMapping;


#ifdef __cplusplus
}
#endif
#endif //UTOOL_TYPEDEFS_H
