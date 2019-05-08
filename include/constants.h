//
// Created by qianbiao on 5/9/19.
//

#ifndef UTOOL_CONSTANTS_H
#define UTOOL_CONSTANTS_H
/* For c++ compatibility */
#ifdef __cplusplus
extern "C" {
#endif


#define MAX_URL_LEN 256
#define MAX_HEADER_LEN 64
#define MAX_COMMAND_NAME_LEN 50
#define MAX_FAILURE_MSG_LEN 1024
#define MAX_FAILURE_COUNT 32

#define IPMI_PORT 623;
#define HTTPS_PORT 443;

#define CURL_TIMEOUT 60
#define CURL_CONN_TIMEOUT 30

#define CONTENT_TYPE_JSON "Content-Type: application/json"
#define USER_AGENT "UTOOL based on libcurl - HUAWEI server management command line tool"
#define HEADER_ETAG "ETag: "
#define HEADER_IF_MATCH "If-Match"

typedef enum _Code
{
    OK = 0,
    UTOOLE_HANDLED_REDFISH_ERROR = -1,
    UTOOLE_OPTION_ERROR = 120,          /** user input illegal options */
    UTOOLE_INTERNAL = 130,              /** tool internal errors like failed to malloc memory, failed to init curl */
    UTOOLE_LOAD_SYSTEM_ID = 131,        /** failed to load redfish system id */
    UTOOLE_PARSE_RESPONSE_JSON = 132,   /** failed to parse json text */
    UTOOLE_UNKNOWN_RESPONSE_FORMAT = 133,   /** unexpect content returned by redfish */
} utool_Code;


/** UTOOL response json constants// */
static const char *const RESPONSE = "{\"State\": \"%s\", \"Message\": [%s]}";
static const char *const RESULT_KEY_STATE = "State";
static const char *const RESULT_KEY_MESSAGES = "Message";

static const char *const STATE_SUCCESS = "Success";
static const char *const STATE_FAILURE = "Failure";

/** //UTOOL response json constants */


/** Redfish message severity level// */

static const char *const SEVERITY_OK = "OK";

/** //Redfish message severity level */


/**
 * Argument parse help messages
 */

static const char *const TOOL_DESC = "\nCommand-line interface to the HUAWEI server management APIs.";
static const char *const TOOL_EPI_LOG = "\nThanks for using.";
static const char *const HELP_DESC = "Show help message.";
static const char *const HELP_SUB_COMMAND_DESC = "Show help message for this sub-command.";
static const char *const OPTIONAL_ARG_GROUP_TITLE = "Optional arguments:";
static const char *const LOG_CMD_HELP_ACTION = "Help action for command %s detected, output help info now.";
static const char *const HELP_ACTION_OUTPUT_MESSAGE = "please read help info from command line";

static const char *const TOO_MANY_ARGUMENTS = "Too many arguments is provided.";


/** Error Messages */
static const char *const FAIL_NO_PRIVILEGE =
        "Failure: you do not have the required permissions to perform this operation";
static const char *const FAIL_INTERNAL_SERVICE_ERROR = "Failure: the request failed due to an internal service error";
static const char *const FAIL_NOT_SUPPORT = "Failure: the server did not support the functionality required";
static const char *const FAIL_PRE_CONDITION_FAILED = "Failure: 412 Precondition Failed";


#ifdef __cplusplus
}
#endif //UTOOL_CONSTANTS_H
#endif
