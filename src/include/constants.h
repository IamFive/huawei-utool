/*
* Copyright © Huawei Technologies Co., Ltd. 2012-2018. All rights reserved.
* Description: constants define
* Author:
* Create: 2019-06-16
* Notes:
*/
#ifndef UTOOL_CONSTANTS_H
#define UTOOL_CONSTANTS_H
/* For c++ compatibility */
#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
    #define FILEPATH_SEP '\\'
#else
    #define FILEPATH_SEP '/'
#endif

#define ENABLE_DEBUG 0
#define UTOOL_VENDOR "Huawei"

#define MAX_XPATH_LEN 256
#define MAX_URL_LEN 256
#define MAX_HEADER_LEN 128
#define MAX_COMMAND_NAME_LEN 50
#define MAX_FAILURE_MSG_LEN 1024
#define MAX_OUTPUT_LEN 2048
#define MAX_PAYLOAD_LEN 2048
#define MAX_FAILURE_COUNT 32
#define MAX_FILE_PATH_LEN 512
#define MAX_OEM_NAME_LEN 128

#define IPMI_PORT 623
#define HTTPS_PORT 443

#define CURL_TIMEOUT 120
#define CURL_UPLOAD_TIMEOUT 300
#define CURL_CONN_TIMEOUT 60

#define CONTENT_TYPE_JSON "Content-Type: application/json"
#define USER_AGENT "UTOOL based on libcurl - HUAWEI server management command line tool"
#define HEADER_ETAG "ETag: "
#define HEADER_CONTENT_LENGTH "CONTENT-LENGTH: "
#define HEADER_CONTENT_TYPE "CONTENT-TYPE: "
#define HEADER_IF_MATCH "If-Match"

#define VAR_OEM "${Oem}"

/** UTOOL response json constants// */
#define RESULT_KEY_STATE "State"
#define RESULT_KEY_MESSAGES "Message"
#define RESULT_KEY_IPMI_RSP "ipmirsp"

#define STATE_SUCCESS "Success"
#define STATE_FAILURE "Failure"

#define OUTPUT_JSON "{\"State\": \"%s\", \"Message\": [\"%s: %s\"]}"
#define OUTPUT_SUCCESS_JSON "{\"State\": \"Success\", \"Message\": [\"Success: Successfully Completed Request\"]}"
#define OUTPUT_INTERNAL_FAILED_JSON "{\"State\": \"Failure\", \"Message\": [\"Error: internal error\"]}"

#define NOTHING_TO_PATCH "Error: nothing to update, at least one update option must be specified"

//Success: successfully completed request
/** //UTOOL response json constants */

/** //Redfish health rollup state */
#define HEALTH_ROLLUP_CRITICAL "Critical"
#define HEALTH_ROLLUP_WARNING "Warning"
#define HEALTH_ROLLUP_OK "OK"
/** Redfish health rollup state// */

/** Redfish message severity level// */

#define SEVERITY_OK "OK"
#define SEVERITY_WARNING "Warning"

/** //Redfish message severity level */

/** Redfish SNMP trap severity level// */
#define SEVERITY_CRITICAL "Critical"
#define SEVERITY_MAJOR "Major"
#define SEVERITY_MINOR "Minor"
#define SEVERITY_NORMAL "Normal"
/** //Redfish SNMP trap severity level */

/** Redfish SNMP trap version // */
#define TRAP_VERSION_1 "V1"
#define TRAP_VERSION_2 "V2C"
#define TRAP_VERSION_3 "V3"
/** //Redfish SNMP trap version */

/** Redfish indicator LED state// */
#define INDICATOR_STATE_ON "Lit"
#define INDICATOR_STATE_OFF "Off"
#define INDICATOR_STATE_BLINKING "Blinking"
/** //Redfish indicator LED state */

/** //Redfish TASK state  */
#define TASK_STATE_NEW "New"
#define TASK_STATE_STARTING "Starting"
#define TASK_STATE_RUNNING "Running"
#define TASK_STATE_SUSPENDED "Suspended"
#define TASK_STATE_INTERRUPTED "Interrupted"
#define TASK_STATE_PENDING "Pending"
#define TASK_STATE_STOPPING "Stopping"
#define TASK_STATE_OK "OK"
#define TASK_STATE_COMPLETED "Completed"
#define TASK_STATE_KILLED "Killed"
#define TASK_STATE_EXCEPTION "Exception"
#define TASK_STATE_SERVICE "Service"
/** Redfish TASK state // */

/** // Redfish boot devices */
#define BOOT_DEVICE_NONE "None"
#define BOOT_DEVICE_PXE "Pxe"
#define BOOT_DEVICE_FLOPPY "Floppy"
#define BOOT_DEVICE_CD "Cd"
#define BOOT_DEVICE_HDD "Hdd"
#define BOOT_DEVICE_BIOSSETUP "BiosSetup"
/** Redfish boot devices // */

#define ENABLED "Enabled"
#define DISABLED "Disabled"

#define DEFAULT_INT_V -10086

#define MEMORY_STATE_ABSENT "Absent"
#define MEMORY_STATE_ENABLED "Enabled"

#define UPGRADE_FIRMWARE_RETRY_TIMES 3

#define UPGRADE_ACTIVATE_MODE_AUTO "Auto"
#define UPGRADE_ACTIVATE_MODE_MANUAL "Manual"

/**
 * Argument parse help messages
 */
#define TOOL_DESC "\nCommand-line interface to the HUAWEI server management APIs."
#define TOOL_EPI_LOG "\nUsing `utool getcapabilities` to get more information about sub-commands."
#define CMD_EPI_LOG "\nPlease reading user guide document to get more information about this sub-command."
#define HELP_DESC "Show help message."
#define HELP_SUB_COMMAND_DESC "Show help message for this sub-command."
#define OPTIONAL_ARG_GROUP_TITLE "Optional arguments:"
#define LOG_CMD_HELP_ACTION "Help action for command %s detected, output help info now."
#define HELP_ACTION_OUTPUT_MESSAGE "Please read help info from command line"
#define TOO_MANY_ARGUMENTS "Too many arguments is provided."


/** Error Messages */
#define FAIL_NO_PRIVILEGE "Failure: you do not have the required permissions to perform this operation"
#define FAIL_INTERNAL_SERVICE_ERROR "Failure: the request failed due to an internal service error"
#define FAIL_NOT_SUPPORT "Failure: the server did not support the functionality required"
#define FAIL_PRE_CONDITION_FAILED "Failure: 412 Precondition Failed"
#define FAIL_ENTITY_TOO_LARGE "Failure: 413 Request Entity Too Large"


/** opt validation */
#define OPT_REQUIRED(OPT_NAME) "Error: option `"OPT_NAME"` is required."
#define OPT_ILLEGAL(OPT_NAME) "Error: option `"OPT_NAME"` is illegal."
#define OPT_NOT_IN_CHOICE(OPT_NAME, CHOICES) "Error: option `"OPT_NAME"` is illegal, available choices: "CHOICES"."
#define OPT_NOT_IN_RANGE(OPT_NAME, RANGE) "Error: option `"OPT_NAME"` is illegal, available value range: "RANGE"."

typedef enum _Code
{
    UTOOLE_OK = 0,
    UTOOLE_OPTION_ERROR = 120,                  /** user input illegal options */
    UTOOLE_INTERNAL = 130,                      /** tool internal errors like failed to malloc memory, failed to init curl */
    UTOOLE_CREATE_LOG_FILE = 131,                /** Failed to create log file `utool.log.txt` */
    UTOOLE_PARSE_JSON_FAILED = 132,                 /** failed to parse json text */
    UTOOLE_UNKNOWN_JSON_FORMAT = 133,               /** unexpect content returned by redfish */
    UTOOLE_CREATE_JSON_NULL = 134,                  /** failed to create json object */
    UTOOLE_PRINT_JSON_FAILED = 135,                 /** failed to print json object */
    UTOOLE_CURL_INIT_FAILED = 140,                  /** failed to init curl */
    UTOOLE_ILLEGAL_LOCAL_FILE_PATH = 141,          /** illegal local upload file path */
    UTOOLE_ILLEGAL_LOCAL_FILE_SIZE = 142,          /** illegal local upload file size */
    UTOOLE_ILLEGAL_DOWNLOAD_FILE_PATH = 143,          /** illegal local upload file path */
    UTOOLE_SSH_PROTOCOL_DISABLED = 144,          /** failed to transfer file to bmc because ssh service is disabled */
    UTOOLE_FAILED_TO_WRITE_FILE = 145,          /** Failed to write local file */
} UtoolCode;

#ifdef __cplusplus
}
#endif //UTOOL_CONSTANTS_H
#endif
