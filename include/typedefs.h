#ifndef UTOOL_TYPEDEFS_H
#define UTOOL_TYPEDEFS_H

/* For c++ compatibility */
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/**
 * Redfish Server basic properties
 */

typedef enum _CommandOptionFlag
{
    EXECUTABLE = 0,
    ILLEGAL = 1,          /** invalid options */
    FEAT_HELP = 2,        /** for help feature */
    FEAT_VERSION = 3,     /** for version feature */
} utool_CommandOptionFlag;


typedef struct _CommandOption
{
    char *host;
    int port;
    char *username;
    char *password;
    int commandArgc;
    utool_CommandOptionFlag flag;  /** whether the command should be executed, default yes(0) otherwise no */
    const char **commandArgv;

} utool_CommandOption;


typedef enum _CommandType
{
    GET = 1, SET = 2
} utool_CommandType;

/**
 * UTOOL Command meta
 */
typedef struct _Command
{
    char *name;
    utool_CommandType type;

    int (*pFuncExecute)(utool_CommandOption *, char **);
} utool_Command;


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
} utool_RedfishServer;


/**
 * Curl response meta properties
 */
typedef struct _CurlResponse
{
    char *content;
    size_t size;
    int httpStatusCode;
    char *etag;
} utool_CurlResponse;

typedef struct _CurlHeader
{
    const char *const name;
    const char *const value;
} utool_CurlHeader;


typedef struct _OutputMapping
{
    const char *const sourceXpath;
    const char *const targetKeyValue;
} utool_OutputMapping;


#ifdef __cplusplus
}
#endif
#endif //UTOOL_TYPEDEFS_H
