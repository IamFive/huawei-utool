/*
* Copyright © xFusion Digital Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `fwupdate`
* Author:
* Create: 2019-06-14
* Notes:
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>
#include <ipmi.h>
#include "cJSON_Utils.h"
#include "commons.h"
#include "curl/curl.h"
#include "zf_log.h"
#include "constants.h"
#include "command-helps.h"
#include "command-interfaces.h"
#include "argparse.h"
#include "redfish.h"
#include "string_utils.h"
#include "url_parser.h"

#define IPMI_PROGRESS_NOT_START -1
#define IPMI_PROGRESS_NOT_RETURNED -2

#define BMC_USE_IPMI_VERSION "2.58"
#define BMC_USE_IPMI_MAJOR_VERSION 2
#define BMC_USE_IPMI_MINOR_VERSION 58
#define BMC_TRANSITION_FM_VERSION "6.36"
#define BMC_TRANSITION_VERSION 639
#define BMC_TRANSITION_MAJOR_VERSION 6
#define BMC_TRANSITION_MINOR_VERSION 39
#define TIME_LIMIT_SHUTDOWN 120
#define TIME_LIMIT_POWER_ON 420
#define REBOOT_CHECK_INTERVAL 3
#define LOG_HEAD "{\"log\":[   \n"
#define LOG_TAIL "\n]}"
#define MAX_LOG_ENTRY_LEN 512
#define LOG_ENTRY_FORMAT "\t{\n\t\t\"Time\": \"%s\",\n\t\t\"Stage\": \"%s\",\n\t\t\"State\": \"%s\",\n\t\t\"Note\": \"%s\"\n\t}, \n"

#define UPGRADE_TRANSITION_FIRMWARE_PAYLOAD "{\"ImageURI\": \"/tmp/web/%s\"}"

#define SERVER_NAME_2288HV5 "2288H V5"
#define SERVER_NAME_2298V5 "2298 V5"
#define TRANSITION_FM_2288HV5 "2288H_V5_2288C_V5_5288_V5-iBMC-V636.hpm"
#define TRANSITION_FM_2298V5 "2298_V5-iBMC-V636.hpm"

#define STAGE_CREATE_SESSION "Create session"
#define STAGE_GET_PRODUCT_SN "Get product SN"
#define STAGE_CREATE_LOG_FILE "Create log file"
#define STAGE_GET_BMC_VERSION "Get BMC version"
#define STAGE_UPLOAD_FILE "Upload firmware to BMC"
#define STAGE_UPLOAD_TRANSITION_FILE "Upload transition firmware to BMC"
#define STAGE_DOWNLOAD_FILE "Download remote firmware to BMC"
#define STAGE_UPGRADE_TRANSITION_FIRMWARE "Upgrade transition firmware"
#define STAGE_UPGRADE_FIRMWARE "Upgrade firmware"
#define STAGE_UPGRADE_BACKUP_PLANE "Upgrade backup plane firmware"
#define STAGE_ACTIVATE "Activate"

#define PROGRESS_RETRY "Retry"
#define PROGRESS_START "Start"
#define PROGRESS_RUN "In Progress"
#define PROGRESS_SUCCESS "Success"
#define PROGRESS_FAILED "Failed"
#define PROGRESS_INVALID_URI "Invalid URI"
#define PROGRESS_GET_CURRENT_VERSION "Get current version"
#define PROGRESS_REBOOT_BMC "Reboot BMC start."
#define PROGRESS_WAITING_SHUTDOWN "Waiting for BMC shutdown"
#define PROGRESS_SHUTDOWN_SUCCEED "BMC shutdown succeed"
#define PROGRESS_WAITING_ALIVE "Waiting for BMC startup"
#define PROGRESS_ALIVE "BMC startup succeed"
#define PROGRESS_REBOOT_DONE "BMC reboot succeed"
#define PROGRESS_GET_NEW_VERSION "Get new version"

#define DISPLAY_CREATE_SESSION_DONE "Create session successfully"
#define DISPLAY_CREATE_SESSION_FAILED "Create session failed"

#define DISPLAY_GET_FIRMWARE_INVENTORY_FAILED "Fetch bmc firmware inventory failed."

#define DISPLAY_GET_PRODUCT_SN_DONE "Fetch bmc product serial number successfully."
#define DISPLAY_GET_PRODUCT_SN_FAILED "Fetch bmc product serial number failed."
#define DISPLAY_PRODUCT_SN_NULL "Failed to get bmc product serial number, it's an empty value."
#define DISPLAY_PRODUCT_MODEL_NULL "Failed to get bmc product name, it's an empty value."

#define DISPLAY_CREATE_LOG_FILE_DONE "Create log file successfully"
#define DISPLAY_CREATE_LOG_FILE_FAILED "Create log file failed"

#define DISPLAY_UPLOAD_FILE_START "Upload file start"
#define DISPLAY_UPLOAD_FILE_DONE "Upload file successfully"
#define DISPLAY_UPLOAD_FILE_FAILED "Upload file failed"

#define DISPLAY_DOWNLOAD_FILE_START "Download remote firmware file start."
#define DISPLAY_DOWNLOAD_FILE_DONE "Download remote firmware file succeed."
#define DISPLAY_DOWNLOAD_FILE_FAILED "Download remote firmware file failed."

#define DISPLAY_UPGRADE_CURRENT_PLANE_DONE "Upgrade firmware for current plane succeed"
#define DISPLAY_UPGRADE_BACKUP_PLANE_START "Upgrade firmware for backup plane start."
#define DISPLAY_UPGRADE_BACKUP_PLANE_DONE "Upgrade firmware for backup plane succeed."
#define DISPLAY_UPGRADE_BACKUP_PLANE_FAILED "Upgrade firmware for backup plane failed."

#define DISPLAY_UPGRADE_START "Upgrade firmware start."
#define DISPLAY_UPGRADE_DONE "Upgrade firmware successfully."
#define DISPLAY_UPGRADE_FAILED "Upgrade firmware failed."
#define DISPLAY_UPGRADE_TRANSITION_FAILED "Upgrade transition firmware failed."


#define DISPLAY_ACTIVE_START "Activate firmware start."
#define DISPLAY_ACTIVE_IN_PROGRESS "Activate inprogress."
#define DISPLAY_ACTIVE_DONE "Activate successfully."

#define DISPLAY_BMC_REBOOT_REQUIRED "Reboot is required to get the new version of BMC."
#define DISPLAY_BMC_REBOOT_START "Reboot BMC start."
#define DISPLAY_WAIT_BMC_SHUTDOWN "Waiting BMC shutdown..."
#define DISPLAY_BMC_HAS_BEEN_SHUTDOWN "BMC has been shutdown."
#define DISPLAY_WAIT_BMC_POWER_ON "Waiting BMC power on..."
#define DISPLAY_BMC_REBOOT_DONE "Reboot BMC complete."
#define DISPLAY_GET_FIRMWARE_VERSION_FAILED "Failed to get firmware(%s)'s current version."
#define DISPLAY_VERIFY_DONE "Version verify ok"
#define DISPLAY_UPDATE_FAILED_RETRY_NOW "Update failed, will retry now."
#define BMC_INSUFFICIENT_CAPACITY "insufficient memory capacity"

#define FM_TYPE_BMC "BMC"
#define FM_TYPE_BIOS "BIOS"
#define FM_TYPE_CPLD "CPLD"

/**
    2019-09-22 15:25:02 Ping https successfully
    2019-09-22 15:25:11 Create session successfully
    2019-09-22 15:25:14 Upload file start
    2019-09-22 15:26:11 Upload file inprogress, process: 26%
    2019-09-22 15:27:34 Upload file successfully
    2019-09-22 15:28:44 File verify successfully
    2019-09-22 15:28:44 Apply(Flash) pending                                [可选，异步升级时打印]	阻塞，等待触发条件
    2019-09-22 15:28:44 Apply(Flash) start                                      [可选，同步升级时打印]
    2019-09-22 15:29:44 Apply(Flash) ingrogress, process: 50%      [可选，同步升级时打印]
    2019-09-22 15:30:49 Apply(Flash) successfully                           [可选，同步升级时打印]
    2019-09-22 15:30:55 Activate inprogress                                  [可选，同步升级时打印]
    2019-09-22 15:30:55 BMC reboot complete                             [可选，BMC同步升级时打印]
    2019-09-22 15:30:59 Version verify ok                                       [可选，同步升级时打印]
 */

#define FW_BMC "BMC"
#define FW_BIOS "BIOS"
#define FW_CPLD "CPLD"

#define MAX_FIRMWARE_NAME_LEN 64

#define MSG_FAILED_TO_GET_CURRENT_VERSION "Failed to get current firmware version"
#define MSG_FAILED_TO_GET_NEW_VERSION "Failed to get new firmware version"
#define MSG_STARTUP_TO_EXCEED "BMC does not startup in 7 minutes. timeout exceed."
#define MSG_SHUTDOWN_TO_EXCEED "BMC does not shutdown in 2 minutes, timeout exceed."

#define STEP_START(STEP_NAME) STEP_NAME" start."
#define STEP_COMPLETE(STEP_NAME) STEP_NAME" successfully."
#define STEP_FAILED(STEP_NAME) STEP_NAME" failed."

#if defined(__linux__)
#define _off_t __off_t
#endif

typedef struct _UpdateFirmwareOption {
    char *imageURI;
    char *activateMode;
    char *firmwareType;
    char *dualImage;

    char *psn;
    char *productName;
    char *activeBmcVersion;
    int activeBmcVersionDotCount;
    int activeBmcVersionMajor;
    int activeBmcVersionMinor;
    bool activeBmcVersionEq258;
    char *targetBmcVersion;
    bool disableLogEntry;
    bool isLocalFile;
    bool isRemoteFile;
    FILE *logFileFP;
    cJSON *payload;
    time_t startTime;
    UtoolCommandOption *commandOption;
} UpdateFirmwareOption;

typedef struct _FirmwareMapping {
    char *firmwareName;
    char *firmwareURL;
} FirmwareMapping;

typedef struct _IpmiUpgradeErrorMapping {
    char *key;
    char *error;
} IpmiUpgradeErrorMapping;


static const FirmwareMapping firmwareMapping[] = {
        {.firmwareName = FW_BMC, .firmwareURL="/UpdateService/FirmwareInventory/ActiveBMC"},
        {.firmwareName = "Backplane CPLD", .firmwareURL="/UpdateService/FirmwareInventory/MainBoardCPLD"},
        {.firmwareName = "Motherboard CPLD", .firmwareURL="/UpdateService/FirmwareInventory/MainBoardCPLD"},
        {.firmwareName = "PS Firmware", .firmwareURL="/UpdateService/FirmwareInventory/chassisPS1"},
        {.firmwareName = FW_BIOS, .firmwareURL="/UpdateService/FirmwareInventory/Bios"},
        {.firmwareName = FW_CPLD, .firmwareURL="/UpdateService/FirmwareInventory/MainBoardCPLD"},
        NULL
};

static const IpmiUpgradeErrorMapping ipmiUpgradeErrorMapping[] = {
        {.key = "80", .error="The target is in upgrading, please wait a few minutes and try again."},
        {.key = "81", .error="Unknown upgrade error,please check iBMC:1)network 2)ipmi service 3)iBMC upgrade log."},
        {.key = "82", .error="The upgrade file does not exist, please restart the task."},
        {.key = "83", .error="The upgrade file error, please confirm the upgrade file matches."},
        {.key = "84", .error="Insufficient iBMC memory. The target may be upgrading, please wait a few minutes and "
                             "try again."},
        {.key = "84", .error="Insufficient iBMC memory. The target may be upgrading, please wait a few minutes and "
                             "try again."},
        {.key = "85", .error="SMM board transfer file failed. Please check SMM board status and try again."},
        {.key = "86", .error="The iBMC upgrade space is insufficient, please restart iBMC and try again."},
        {.key = "87", .error="The upgrade file name conflict."},
        {.key = "88", .error="Does not support the Bios 8M upgrade under the condition of power on. Please power off "
                             "the server and try again."},
        {.key = "94", .error="Unkown iBMC error, please check iBMC log."},
        NULL
};

static const char *ALLOW_UPGRADE_TO_GE_639[] = {"6.32", "6.38", NULL};
static const char *MODE_CHOICES[] = {UPGRADE_ACTIVATE_MODE_AUTO, UPGRADE_ACTIVATE_MODE_MANUAL, NULL};
static const char *DUAL_CHOICES[] = {"Single", "Dual", NULL};
static const char *TYPE_CHOICES[] = {"BMC", "BIOS", "CPLD", "PSUFW", NULL};
static const char *IPMI_SUPPORT_TYPES[] = {"BMC", "BIOS", "CPLD", NULL};
static const char *IMAGE_PROTOCOL_CHOICES[] = {"HTTPS", "SCP", "SFTP", "CIFS", "TFTP", "NFS", NULL};

static const char *OPTION_IMAGE_URI_REQUIRED = "Error: option `image-uri` is required.";
static const char *OPTION_IMAGE_URI_ILLEGAL = "Error: option `image-uri` is illegal.";
static const char *OPTION_CANNOT_PARSE_BMC_VERSION_FROM_IMAGE_URI = "Error: failed to parse BMC version from option `image-uri`";
static const char *OPTION_IMAGE_URI_NO_SCHEMA = "Error: URI is not a local file nor a remote network protocol file.";
static const char *OPTION_IMAGE_URI_ILLEGAL_SCHEMA = "Error: Protocol `%s` is not supported.";

static const char *OPTION_MODE_REQUIRED = "Error: option `activate-mode` is required.";
static const char *OPTION_MODE_ILLEGAL = "Error: option `activate-mode` is illegal, available choices: Auto, Manual.";
static const char *OPTION_TYPE_ILLEGAL = "Error: option `firmware-type` is illegal, "
                                         "available choices: BMC, BIOS, CPLD, PSUFW.";
static const char *OPTION_DUAL_ILLEGAL = "Error: option `dual-image` is illegal, "
                                         "available choices: Single, Dual.";
static const char *OPTION_TYPE_258_REQUIRED = "Current active BMC version is 2.58, option `firmware-type` is required "
                                              "for this version, available choices: BMC, BIOS, CPLD.";
static const char *OPTION_TYPE_258_ILLEGAL = "Current active BMC version is 2.58, available choices for option "
                                             "`firmware-type` is: BMC, BIOS, CPLD.";

static const char *PRODUCT_SN_IS_NOT_SET = "Error: product SN is not correct.";
static const char *FAILED_TO_CREATE_FOLDER = "Error: failed to create log folder.";
static const char *FAILED_TO_CREATE_FILE = "Error: failed to create log file.";
static const char *LOG_FILE_PATH_ILLEGAL = "Error: log file path is illegal, Please make sure the path exists.";

static const char *BMC_UPGRADE_TO_GE_639_NOT_ALLOWED = "Error: BMC version after 6.39 only can be upgraded from 6.32 "
                                                       "and 6.38.";

static const char *const usage[] = {
        "fwupdate -u image-uri -e activate-mode [-t firmware-type] [-d dual-image]",
        NULL,
};

static void ValidateUpdateFirmwareOptions(UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result);

static cJSON *BuildPayload(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption,
                           UtoolResult *result);

static void UpdateFirmware2(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption, UtoolResult
*result);

static void WriteLogEntry(UpdateFirmwareOption *option, const char *stage, const char *state, const char *note);

static void
WriteFailedLogEntry(UpdateFirmwareOption *option, const char *stage, const char *state, UtoolResult *result);

void PrintFirmwareVersion(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption, cJSON *firmwareType,
                          UtoolResult *result);

/**
 * reboot BMC and wait util alive
 *
 * @param stage indicates the stage reboot for
 * @param server
 * @param updateFirmwareOption
 * @param result
 * @return
 */
static int RebootBMC(char *stage, UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption,
                     UtoolResult *result);

static void RetryFunc(
        const char *stage,
        void (*func)(
                UtoolRedfishServer *server,
                UtoolCommandOption *commandOption,
                UpdateFirmwareOption *updateFirmwareOption,
                UtoolResult *result
        ),
        UtoolRedfishServer *server,
        UtoolCommandOption *commandOption,
        UpdateFirmwareOption *updateFirmwareOption,
        UtoolResult *result,
        bool writeLogEntry);

/**
 *  Auto Retry Steps defintion
 */
static int
CreateSession(UtoolRedfishServer *server, UtoolCommandOption *commandOption, UpdateFirmwareOption *updateFirmwareOption,
              UtoolResult *result);

static int
GetProductSn(UtoolRedfishServer *server, UtoolCommandOption *commandOption, UpdateFirmwareOption *updateFirmwareOption,
             UtoolResult *result);

static void
CreateLogFile(UtoolRedfishServer *server, UtoolCommandOption *commandOption, UpdateFirmwareOption *updateFirmwareOption,
              UtoolResult *result);

/**
 * Get Bakup BMC version
 * @param server
 * @param updateFirmwareOption
 * @param result
 * @return
 */
static int
GetBmcVersion(UtoolRedfishServer *server, UtoolCommandOption *commandOption, UpdateFirmwareOption *updateFirmwareOption,
              UtoolResult *result);

/**
 * perform upload local file to BMC action
 * @param server
 * @param updateFirmwareOption
 * @param result
 */
static void UploadLocalFile(UtoolRedfishServer *server, UtoolCommandOption *commandOption,
                            UpdateFirmwareOption *updateFirmwareOption,
                            UtoolResult *result);

/**
 * get transition file name for product
 * @param productName
 * @return
 */
static char *GetTransitionFileName(char *productName);

/**
 * perform upload local transition firmware file to BMC action
 * @param server
 * @param updateFirmwareOption
 * @param result
 */
static void UploadTransitionFile(UtoolRedfishServer *server, UtoolCommandOption *commandOption,
                                 UpdateFirmwareOption *updateFirmwareOption,
                                 UtoolResult *result);

/**
 * perform download remote file to BMC action
 * @param server
 * @param updateFirmwareOption
 * @param result
 */
static void DownloadRemoteFile(UtoolRedfishServer *server, UtoolCommandOption *commandOption,
                               UpdateFirmwareOption *updateFirmwareOption,
                               UtoolResult *result);

/**
 * perform upgrade firmware action.
 *
 * @param server
 * @param updateFirmwareOption
 * @param result
 */
static void
UpgradeFirmware(UtoolRedfishServer *server, UtoolCommandOption *commandOption,
                UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result);

/**
 * perform upgrade transition firmware action.
 *
 * @param server
 * @param updateFirmwareOption
 * @param result
 */
static void
UpgradeTransitionFirmware(UtoolRedfishServer *server, UtoolCommandOption *commandOption,
                          UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result);

/**
 * start whole upgrade firmware workflow
 *
 * @param server
 * @param updateFirmwareOption
 * @param result
 */
static void StartUpgradeTargetFirmwareWorkflow(UtoolRedfishServer *server, UtoolCommandOption *commandOption,
                                               UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result);

static void
StartUpgradeTransitionFirmwareWorkflow(UtoolRedfishServer *server, UtoolCommandOption *commandOption,
                                       UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result);

static void DisplayRunningProgress(int quiet, const char *progress);

static void DisplayProgress(int quiet, const char *progress);

static void freeUpdateFirmwareOption(UpdateFirmwareOption *updateFirmwareOption);

static void parseTargetBmcVersionFromFilepath(UpdateFirmwareOption *pOption, char *path);

static bool isTransitionFirmwareUpgradeRequired(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption,
                                                UtoolResult *result);

void
UpgradeTransitionFirmwareIfNecessary(UtoolRedfishServer *server, UtoolCommandOption *commandOption,
                                     UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result);

void
ParseActiveBmcVersion(UpdateFirmwareOption *updateFirmwareOption, UtoolRedfishServer *pServer, UtoolResult *pResult);

void
UpgradeFirmwareByRedfish(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result);

void
UpgradeFirmwareByIpmi(UtoolRedfishServer *pServer, UtoolCommandOption *commandOption, UpdateFirmwareOption *pOption,
                      UtoolResult *pResult);

void ValidateUpdateFirmwareOptionFor258(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption,
                                        UtoolResult *result);

const IpmiUpgradeErrorMapping *GetIpmiError(const char *ipmiCmdOutput);

/**
 * wait util ipmi upgrading task finished or failed or time exceed.
 * @param server
 * @param commandOption
 * @param updateFirmwareOption
 * @param result
 */
void WaitUtilIpmiUpgradeTaskFinish(UtoolRedfishServer *server, UtoolCommandOption *commandOption,
                                   UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result);

int ParseProgressFromIpmiOutputStr(char *queryCmdOutput, time_t upgradeStartTime, UtoolResult *result);

void ExecIpmiUpgradeFinishedCmdIfRequired(UtoolCommandOption *commandOption,
                                          const UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result);

static void DisplayRunningProgress(int quiet, const char *progress)
{
    if (!quiet) {
        char nowStr[100] = {0};
        time_t now = time(NULL);
        struct tm *tm_now = localtime(&now);
        if (tm_now != NULL) {
            strftime(nowStr, sizeof(nowStr), "%Y-%m-%d %H:%M:%S", tm_now);
        }

        fprintf(stdout, "%s %s", nowStr, progress);
        fflush(stdout);
    }
}


static void DisplayProgress(int quiet, const char *progress)
{
    if (!quiet) {
        DisplayRunningProgress(quiet, progress);
        fprintf(stdout, "\n");
    }
}

static void RetryFunc(
        const char *stage,
        void (*func)(
                UtoolRedfishServer *server,
                UtoolCommandOption *commandOption,
                UpdateFirmwareOption *updateFirmwareOption,
                UtoolResult *result
        ),
        UtoolRedfishServer *server,
        UtoolCommandOption *commandOption,
        UpdateFirmwareOption *updateFirmwareOption,
        UtoolResult *result,
        bool writeLogEntry)
{
    char displayMessage[MAX_OUTPUT_LEN] = {0};
    UtoolWrapSecFmt(displayMessage, MAX_OUTPUT_LEN, MAX_OUTPUT_LEN - 1, "%s start.", stage);
    DisplayProgress(server->quiet, displayMessage);
    if (writeLogEntry) {
        WriteLogEntry(updateFirmwareOption, stage, PROGRESS_START, "");
    }
    ZF_LOGI(displayMessage);

    int retryTimes = 0; /* current retry round */
    while (retryTimes++ < UPGRADE_FIRMWARE_STEPS_RETRY_TIMES) {
        if (retryTimes > 1) {
            FREE_CJSON(result->data)
            FREE_OBJ(result->desc)
            result->broken = 0;
            result->code = UTOOLE_OK;

            if (writeLogEntry) {
                char round[16];
                UtoolWrapSecFmt(round, sizeof(round), sizeof(round) - 1, "Retry times: %d", retryTimes - 1);
                WriteLogEntry(updateFirmwareOption, stage, PROGRESS_RETRY, round);
            }
        }

        func(server, commandOption, updateFirmwareOption, result);
        if (!result->broken) {
            break;
        }

        if (retryTimes == 3) {
            UtoolWrapSecFmt(displayMessage, MAX_OUTPUT_LEN, MAX_OUTPUT_LEN - 1,
                            "Retry time exceeds, %s failed.", stage);

        } else {
            UtoolWrapSecFmt(displayMessage, MAX_OUTPUT_LEN, MAX_OUTPUT_LEN - 1,
                            "Retry %s now, current retry time: %d.", stage, retryTimes);
        }
        // failure log entry will be append by StageFunction
        // if (writeLogEntry) {
        //     WriteLogEntry(updateFirmwareOption, stage, PROGRESS_FAILED, "");
        // }
        DisplayProgress(server->quiet, displayMessage);
        ZF_LOGI(displayMessage);

        /**
         * if activate mode is auto, we can try reboot the BMC to fix problem.
         */
        if (retryTimes < 3 && result->reboot) {
            if (UtoolStringEquals(UPGRADE_ACTIVATE_MODE_AUTO, updateFirmwareOption->activateMode)) {
                ZF_LOGI("Option `activate-mode` is auto, try reboot BMC for fixing the issue.");

                // Free "output" carried by result in case.
                FREE_OBJ(result->desc)
                FREE_CJSON(result->data)
                result->broken = 0; // reset action status
                result->code = UTOOLE_OK;

                RebootBMC(stage, server, updateFirmwareOption, result);
                if (result->broken) {
                    break; // if reboot fails, break the workflow
                }
            }
        }
    }

    if (!result->broken) {
        UtoolWrapSecFmt(displayMessage, MAX_OUTPUT_LEN, MAX_OUTPUT_LEN - 1, "%s successfully.", stage);
        DisplayProgress(server->quiet, displayMessage);
        if (writeLogEntry) {
            WriteLogEntry(updateFirmwareOption, stage, PROGRESS_SUCCESS, "");
        }
        ZF_LOGI(displayMessage);
    }
}

static int
CreateSession(UtoolRedfishServer *server, UtoolCommandOption *commandOption, UpdateFirmwareOption *updateFirmwareOption,
              UtoolResult *result)
{
    UtoolGetRedfishServer2(updateFirmwareOption->commandOption, server, result);
    if (result->broken || result->code != UTOOLE_OK) {
        UtoolFreeRedfishServer(server);
    }
}

static int
GetProductSn(UtoolRedfishServer *server, UtoolCommandOption *commandOption, UpdateFirmwareOption *updateFirmwareOption,
             UtoolResult *result)
{
    cJSON *getSystemRespJson = NULL;
    updateFirmwareOption->startTime = time(NULL);

    UtoolRedfishGet(server, "/Systems/%s", NULL, NULL, result);
    if (result->broken) {
        ZF_LOGE("Failed to load system resource.");
        DisplayProgress(server->quiet, DISPLAY_GET_PRODUCT_SN_FAILED);
        goto FAILURE;
    }
    getSystemRespJson = result->data;

    if (updateFirmwareOption->productName != NULL) {
        FREE_OBJ(updateFirmwareOption->productName)
    }
    cJSON *model = cJSONUtils_GetPointer(getSystemRespJson, "/Model");
    if (cJSON_IsNull(model) || model->valuestring == NULL) {
        ZF_LOGE("Could not get product model from /system/system-id.");
        DisplayProgress(server->quiet, DISPLAY_PRODUCT_MODEL_NULL);
        goto FAILURE;
    }

    ZF_LOGI("Parsing product name, value is %s.", model->valuestring);
    updateFirmwareOption->productName = UtoolStringNDup(model->valuestring,
                                                        strnlen(model->valuestring, MAX_PRODUCT_NAME_LEN));

    if (updateFirmwareOption->psn != NULL) {
        FREE_OBJ(updateFirmwareOption->psn)
    }
    cJSON *sn = cJSONUtils_GetPointer(getSystemRespJson, "/SerialNumber");
    if (cJSON_IsNull(sn) || sn->valuestring == NULL) {
        ZF_LOGE("Could not get product SerialNumber from /system/system-id.");
        DisplayProgress(server->quiet, DISPLAY_PRODUCT_SN_NULL);
        goto FAILURE;
    }

    ZF_LOGI("Parsing product SN, value is %s.", sn->valuestring);
    updateFirmwareOption->psn = UtoolStringNDup(sn->valuestring, strnlen(sn->valuestring, MAX_PSN_LEN));

    goto DONE;

FAILURE:
    result->broken = 1;
    goto DONE;

DONE:
    FREE_CJSON(result->data)
}

/**
 * result->data has been freed in func
 *
 * @param server
 * @param updateFirmwareOption
 * @param result
 * @return
 */
static int
GetBmcVersion(UtoolRedfishServer *server, UtoolCommandOption *commandOption, UpdateFirmwareOption *updateFirmwareOption,
              UtoolResult *result)
{
    UtoolRedfishGet(server, "/redfish/v1/UpdateService/FirmwareInventory/ActiveBMC", NULL, NULL, result);
    if (result->broken) {
        ZF_LOGE(DISPLAY_GET_FIRMWARE_INVENTORY_FAILED);
        DisplayProgress(server->quiet, DISPLAY_GET_FIRMWARE_INVENTORY_FAILED);
        goto FAILURE;
    }

    cJSON *version = cJSON_GetObjectItem(result->data, "Version");
    if (!cJSON_IsString(version)) {
        goto FAILURE;
    }

    FREE_OBJ(updateFirmwareOption->activeBmcVersion);
    ZF_LOGI("Current active bmc version is: %s.", version->valuestring);
    updateFirmwareOption->activeBmcVersion = UtoolStringNDup(version->valuestring,
                                                             strnlen(version->valuestring, MAX_FM_VERSION_LEN));
    goto DONE;

FAILURE:
    result->broken = 1;
    goto DONE;

DONE:
    FREE_CJSON(result->data)
}

static void
CreateLogFile(UtoolRedfishServer *server, UtoolCommandOption *commandOption, UpdateFirmwareOption *updateFirmwareOption,
              UtoolResult *result)
{
    char folderName[PATH_MAX];
    struct tm *tm_now = localtime(&updateFirmwareOption->startTime);
    if (tm_now == NULL) {
        result->code = UTOOLE_INTERNAL;
        goto FAILURE;
    }

    if (updateFirmwareOption->psn == NULL) {
        UtoolWrapSecFmt(folderName, PATH_MAX, PATH_MAX - 1, "%d%02d%02d%02d%02d%02d",
                        tm_now->tm_year + 1900,
                        tm_now->tm_mon + 1,
                        tm_now->tm_mday,
                        tm_now->tm_hour,
                        tm_now->tm_min,
                        tm_now->tm_sec);
    } else {
        UtoolWrapSecFmt(folderName, PATH_MAX, PATH_MAX - 1, "%d%02d%02d%02d%02d%02d_%s",
                        tm_now->tm_year + 1900,
                        tm_now->tm_mon + 1,
                        tm_now->tm_mday,
                        tm_now->tm_hour,
                        tm_now->tm_min,
                        tm_now->tm_sec,
                        updateFirmwareOption->psn);
    }


    ZF_LOGE("Try to create folder for current updating, folder: %s.", folderName);
#if defined(__MINGW32__)
    int ret = mkdir(folderName);
#else
    int old = umask(S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);
    int ret = mkdir(folderName, 0750);
    umask(old);
#endif
    if (ret != 0) {
        cJSON *reason = cJSON_CreateString(FAILED_TO_CREATE_FOLDER);
        result->data = reason;
        result->code = UtoolAssetCreatedJsonNotNull(reason);
        goto FAILURE;
    }


    char filepath[PATH_MAX] = {0};
    char realFilepath[PATH_MAX] = {0};
    UtoolWrapSecFmt(filepath, PATH_MAX, PATH_MAX - 1, "%s/update-firmware.log", folderName);

    UtoolFileRealpath(filepath, realFilepath, PATH_MAX);
    updateFirmwareOption->logFileFP = fopen(realFilepath, "a");
    if (!updateFirmwareOption->logFileFP) {
        ZF_LOGW("Failed to create log file %s.", filepath);
        cJSON *reason = cJSON_CreateString(FAILED_TO_CREATE_FOLDER);
        result->data = reason;
        result->code = UtoolAssetCreatedJsonNotNull(reason);
        goto FAILURE;
    }

    /* write log file head content*/
    fprintf(updateFirmwareOption->logFileFP, LOG_HEAD);
    goto DONE;

FAILURE:
    result->broken = 1;
    goto DONE;

DONE:
    return;
}

static void UploadLocalFile(UtoolRedfishServer *server, UtoolCommandOption *commandOption,
                            UpdateFirmwareOption *updateFirmwareOption,
                            UtoolResult *result)
{
    ZF_LOGI("Firmware image uri is a local file, try upload it to BMC temp now...");
    char *imageUri = updateFirmwareOption->imageURI;

    // ipmi upgrading support for BMC 2.58
    if (updateFirmwareOption->activeBmcVersionEq258 && updateFirmwareOption->isLocalFile) {
        // UtoolCurlCmdUploadFileToBMC(server, imageUri, "image.hpm", result);
        // use curl innate sftp uploading method.
        UtoolSftpUploadFileToBMC(server, imageUri, "image.hpm", result);
    } else {
        // upload file to bmc through HTTPS protocol
        UtoolUploadFileToBMC(server, imageUri, result);
    }

    if (result->broken) {
        WriteFailedLogEntry(updateFirmwareOption, STAGE_UPLOAD_FILE, PROGRESS_FAILED, result);
        goto FAILURE;
    }

    goto DONE;

FAILURE:
    result->reboot = 1;
    result->broken = 1;
    goto DONE;

DONE:
    return;
}

static char *GetTransitionFileName(char *productName)
{
    if (UtoolStringEquals(productName, SERVER_NAME_2288HV5)) {
        return TRANSITION_FM_2288HV5;
    } else if (UtoolStringEquals(productName, SERVER_NAME_2298V5)) {
        return TRANSITION_FM_2298V5;
    }
    return NULL;
}

static void UploadTransitionFile(UtoolRedfishServer *server, UtoolCommandOption *commandOption,
                                 UpdateFirmwareOption *updateFirmwareOption,
                                 UtoolResult *result)
{
    ZF_LOGI("Try upload transition firmware file to BMC temp now...");
    char transitionFirmwarePath[PATH_MAX] = {0};
    char *transitionFileName = GetTransitionFileName(updateFirmwareOption->productName);
    UtoolWrapSecFmt(transitionFirmwarePath, PATH_MAX, PATH_MAX - 1, "resources/%s", transitionFileName);
    ZF_LOGI("Use transition firmware file: %s", transitionFirmwarePath);

    // upload file to bmc through HTTPS protocol
    UtoolUploadFileToBMC(server, transitionFirmwarePath, result);
    if (result->broken) {
        // WriteFailedLogEntry(updateFirmwareOption, STAGE_UPLOAD_TRANSITION_FILE, PROGRESS_FAILED, result);
        goto FAILURE;
    }

    goto DONE;

FAILURE:
    result->reboot = 1;
    result->broken = 1;
    goto DONE;

DONE:
    return;
}

static void DownloadRemoteFile(UtoolRedfishServer *server, UtoolCommandOption *commandOption,
                               UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result)
{
    if (updateFirmwareOption->isRemoteFile) {
        /* send update firmware request, it will try download remote file first. */
        char *url = "/UpdateService/Actions/UpdateService.SimpleUpdate";
        UtoolRedfishPost(server, url, updateFirmwareOption->payload, NULL, NULL, result);
        if (result->broken) {
            goto FAILURE;
        }

        UtoolRedfishWaitUtilTaskStart(server, result->data, result);
        if (result->broken) {
            DisplayProgress(server->quiet, DISPLAY_DOWNLOAD_FILE_FAILED);
            WriteFailedLogEntry(updateFirmwareOption, STAGE_DOWNLOAD_FILE, PROGRESS_FAILED, result);
            ZF_LOGE(DISPLAY_DOWNLOAD_FILE_FAILED);
            result->reboot = 1;
            goto FAILURE;
        }
    }

FAILURE:
    goto DONE;

DONE:
    return;
}

/**
 * be caution that result->data will carry last task cJSON object if whole progress success.
 *
 * @param server
 * @param updateFirmwareOption
 * @param result
 */
void UpgradeFirmware(UtoolRedfishServer *server, UtoolCommandOption *commandOption,
                     UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result)
{
    if (!updateFirmwareOption->activeBmcVersionEq258 || updateFirmwareOption->isRemoteFile) {
        UpgradeFirmwareByRedfish(server, updateFirmwareOption, result);
    } else {
        UpgradeFirmwareByIpmi(server, commandOption, updateFirmwareOption, result);
    }
}

// use fixed tmp firmware file path: /tmp/image.hpm
#define IPMI_UPGRADE_QUERY_PROGRESS "0x30 0x91 0xdb 0x07 0x00 0x06 0x00"
#define IPMI_FM_FILE_PATH "0x2f 0x74 0x6d 0x70 0x2f 0x69 0x6d 0x61 0x67 0x65 0x2e 0x68 0x70 0x6d"
#define IPMI_ACTIVE_MODE_AUTO "0x8e"
#define IPMI_ACTIVE_MODE_MANUAL "0x0e"
#define IPMI_UPGRADE_TYPE_BMC "0x01"
#define IPMI_UPGRADE_TYPE_BIOS "0x02"
#define IPMI_UPGRADE_TYPE_CPLD "0x05"
#define IPMI_UPGRADE_CMD "0x30 0x91 0xdb 0x07 0x00 0x06 0xAA 0x00 %s 0x00 %s %s"
//                                                                |        | |---> tmp path
//                                                        firmware-type    |
//                                                                   active-mode
#define IPMI_UPGRADE_FINISHED_CMD "0x30 0x91 0xdb 0x07 0x00 0x06 0x55"

void UpgradeFirmwareByIpmi(UtoolRedfishServer *server, UtoolCommandOption *commandOption,
                           UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result)
{
    char *ipmiUpgradeCmdOutput = NULL;
    if (!(updateFirmwareOption->activeBmcVersionEq258 && updateFirmwareOption->isLocalFile)) {
        char *error = "Wrong method call, `UpgradeFirmwareByIpmi` should only be called when active BMC version is 2.58 "
                      "and firmware file is local file.";
        ZF_LOGE(error);
        DisplayProgress(server->quiet, error);
        return;
    }

    ZF_LOGI("Start to update outband firmware with IPMI now");

    char *firmwareType = IPMI_UPGRADE_TYPE_BMC;
    if (UtoolStringEquals(FM_TYPE_BMC, updateFirmwareOption->firmwareType)) {
        firmwareType = IPMI_UPGRADE_TYPE_BMC;
    } else if (UtoolStringEquals(FM_TYPE_BIOS, updateFirmwareOption->firmwareType)) {
        firmwareType = IPMI_UPGRADE_TYPE_BIOS;
    } else if (UtoolStringEquals(FM_TYPE_CPLD, updateFirmwareOption->firmwareType)) {
        firmwareType = IPMI_UPGRADE_TYPE_CPLD;
    }

    char *activeMode = UtoolStringEquals(UPGRADE_ACTIVATE_MODE_AUTO, updateFirmwareOption->activateMode) ?
                       IPMI_ACTIVE_MODE_AUTO : IPMI_ACTIVE_MODE_MANUAL;

    char upgradeIpmiCmd[MAX_IPMI_CMD_LEN] = {0};
    UtoolWrapSecFmt(upgradeIpmiCmd, MAX_IPMI_CMD_LEN, MAX_IPMI_CMD_LEN - 1, IPMI_UPGRADE_CMD, firmwareType,
                    activeMode,
                    IPMI_FM_FILE_PATH);

    UtoolIPMIRawCmdOption *upgradeCmdOption = &(UtoolIPMIRawCmdOption) {
            .data = upgradeIpmiCmd
    };
    ipmiUpgradeCmdOutput = UtoolIPMIExecRawCommand(commandOption, upgradeCmdOption, result);
    ZF_LOGI("Execute IPMI upgrade firmware command, broken?: %d, result: %s", result->broken, ipmiUpgradeCmdOutput);
    if (result->broken) { // upgrade failed.
        FREE_OBJ(result->desc);
        // parse error Mapping
        const IpmiUpgradeErrorMapping *errorMapping = GetIpmiError(ipmiUpgradeCmdOutput);
        char *reason = errorMapping != NULL ? errorMapping->error : "Failed to execute IPMI upgrade firmware command.";
        ZF_LOGI("Upgrade firmware failed, reason: %s", reason);
        DisplayProgress(server->quiet, DISPLAY_UPGRADE_FAILED);
        WriteLogEntry(updateFirmwareOption, STAGE_UPGRADE_FIRMWARE, PROGRESS_FAILED, reason);
        // WriteFailedLogEntry(updateFirmwareOption, STAGE_UPGRADE_FIRMWARE, PROGRESS_FAILED, result);
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(reason), &(result->desc));
        goto FAILURE;
    }

    /* waiting util task complete or exception */
    ZF_LOGI("Waiting util upgrade firmware task finished...");
    WriteLogEntry(updateFirmwareOption, STAGE_UPGRADE_FIRMWARE, PROGRESS_RUN, "Start execute update firmware task.");

    WaitUtilIpmiUpgradeTaskFinish(server, commandOption, updateFirmwareOption, result);
    if (result->broken) {
        ZF_LOGI("Upgrade firmware failed, reason: %s", result->desc);
        DisplayProgress(server->quiet, DISPLAY_UPGRADE_FAILED);
        WriteFailedLogEntry(updateFirmwareOption, STAGE_UPGRADE_FIRMWARE, PROGRESS_FAILED, result);
        goto FAILURE;
    }

    goto DONE;

FAILURE:
    result->broken = 1;
    goto DONE;

DONE:
    FREE_OBJ(ipmiUpgradeCmdOutput);
    return;
}

void WaitUtilIpmiUpgradeTaskFinish(UtoolRedfishServer *server, UtoolCommandOption *commandOption,
                                   UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result)
{
    int progress = 0;
    bool finished = false;
    char *queryCmdOutput = NULL;

    int queryTimes = 0;
    int maxQueryTimes = 360;
    int continuousZeroProgressTimes = 0;
    // 连续10分钟都读取进度失败，说明iBMC异常, CPLD 5min
    int maxContinuousFailTimeLong = UtoolStringEquals(FM_TYPE_CPLD, updateFirmwareOption->firmwareType) ? 300 : 600;

    time_t upgradeStartTime = time(NULL);
    time_t lastSuccessfulQueryTime = time(NULL);

    char *taskName = NULL;
    if (UtoolStringEquals(FM_TYPE_BMC, updateFirmwareOption->firmwareType)) {
        taskName = "Upgrading iBMC";
    } else if (UtoolStringEquals(FM_TYPE_BIOS, updateFirmwareOption->firmwareType)) {
        taskName = "Upgrading BIOS";
    } else if (UtoolStringEquals(FM_TYPE_CPLD, updateFirmwareOption->firmwareType)) {
        taskName = "Upgrading CPLD";
    }

    char formattedLocalTimeNow[100] = {0};
    while (true) {
        if (queryTimes > maxQueryTimes) { // time exceed
            char *reason = "Query IPMI upgrading progress time exceed.";
            ZF_LOGE(reason);
            UtoolPrintf(server->quiet, stdout, "%s %-96s\r", formattedLocalTimeNow, reason);
            result->code = UtoolBuildOutputResult(STATE_FAILURE, reason, &(result->desc));
            goto FAILURE;
        }

        if (queryTimes > 0) {
            FREE_OBJ(queryCmdOutput);
            sleep(5);
        }
        queryTimes++;

        UtoolIPMIRawCmdOption *queryProgressCmd = &(UtoolIPMIRawCmdOption) {.data = IPMI_UPGRADE_QUERY_PROGRESS};
        queryCmdOutput = UtoolIPMIExecRawCommand(commandOption, queryProgressCmd, result);
        ZF_LOGI("Query IPMI upgrading firmware progress returns: %s", queryCmdOutput);
        const IpmiUpgradeErrorMapping *errorMapping = GetIpmiError(queryCmdOutput); // we find error from command output

        time_t now = time(NULL);
        struct tm *localtime_now = localtime(&now);
        if (localtime_now != NULL) {
            strftime(formattedLocalTimeNow, sizeof(formattedLocalTimeNow), "%Y-%m-%d %H:%M:%S", localtime_now);
        }

        // if cmd output does not contains "db 07 00", treat it as fail
        if (queryCmdOutput != NULL && strstr(queryCmdOutput, "db 07 00") == NULL) {
            ZF_LOGE("Query IPMI upgrading firmware progress returns illegal result: %s, could not find db 07 00 "
                    "in it", queryCmdOutput);
            result->broken = 1;
        }

        if (result->broken || errorMapping != NULL) {
            if (errorMapping != NULL) {
                ZF_LOGE("Query IPMI upgrading firmware task fails, reason: %s", errorMapping->error);
            } else {
                ZF_LOGE("Query IPMI upgrading firmware task fails, ipmi command output: %s", queryCmdOutput);
            }

            double continuousFailTimeLong = difftime(now, lastSuccessfulQueryTime);
            if (continuousFailTimeLong < maxContinuousFailTimeLong) {
                result->broken = 0;
                FREE_OBJ(result->desc)
                continue;
            } else {
                ZF_LOGE("Query IPMI upgrading firmware progress fails continuously, time: %f(s)",
                        continuousFailTimeLong);
                char reason[256] = {0};
                UtoolWrapSecFmt(reason, sizeof(reason), sizeof(reason) - 1,
                                "Failed to query upgrading progress for %d seconds long", continuousFailTimeLong);
                UtoolPrintf(server->quiet, stdout, "%s %-96s\r", formattedLocalTimeNow, reason);
                result->code = UtoolBuildOutputResult(STATE_FAILURE, reason, &(result->desc));
                goto FAILURE;
            }
        } else { // run query command succeed.
            lastSuccessfulQueryTime = time(NULL); // reset last successful query time

            int parsedProgress = ParseProgressFromIpmiOutputStr(queryCmdOutput, upgradeStartTime, result);
            ZF_LOGE("Parse progress from ipmi output: %d", parsedProgress);
            if (result->broken) {
                goto FAILURE;
            }
            if (parsedProgress == 100) { // upgrade finished.
                ExecIpmiUpgradeFinishedCmdIfRequired(commandOption, updateFirmwareOption, result);
                if (result->broken) {
                    goto FAILURE;
                }
                progress = 100; // finished
                finished = true;
            }
                // iBMC连续5次进度为0，终止升级
            else if (parsedProgress == 0) {
                progress = 0; // wait
                continuousZeroProgressTimes += 1;
                if (continuousZeroProgressTimes >= 5) {
                    ZF_LOGE("Query IPMI upgrading firmware progress returns 0 more than 5 times continuously.");
                    if (UtoolStringEquals(FM_TYPE_BMC, updateFirmwareOption->firmwareType)) {
                        ExecIpmiUpgradeFinishedCmdIfRequired(commandOption, updateFirmwareOption, result);
                        if (result->broken) {
                            goto FAILURE;
                        }
                        // iBMC连续5次进度为0，终止升级，并当作升级失败。
                        char *reason = "Upgrading failed, the upgrade progress is always 0 for more than 25s.";
                        ZF_LOGE(reason);
                        UtoolPrintf(server->quiet, stdout, "%s %-96s\r", formattedLocalTimeNow, reason);
                        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(reason),
                                                              &(result->desc));
                        goto FAILURE;
                    } else if (UtoolStringEquals(FM_TYPE_CPLD, updateFirmwareOption->firmwareType)) {
                        ExecIpmiUpgradeFinishedCmdIfRequired(commandOption, updateFirmwareOption, result);
                        // CPLD连续5次进度为0，终止升级，并当作升级成功。
                        progress = 100;
                        finished = true;
                    }
                }
            }
                // not start, return E4 and spent time le than 15s.
            else if (parsedProgress == IPMI_PROGRESS_NOT_START) {
                progress = 0; // wait
            }
                // not returned, 4th segment of ipmi output is null.
            else if (parsedProgress == IPMI_PROGRESS_NOT_RETURNED) { // parse failed.
                //TODO(turnbig): confirm with Qianbiao: what should we do?
            } else {
                progress = parsedProgress;
                continuousZeroProgressTimes = 0; // reset continue progress 0 times
            }

            char taskProgressConsoleMsg[256] = {0};
            UtoolWrapSecFmt(taskProgressConsoleMsg, sizeof(taskProgressConsoleMsg), sizeof(taskProgressConsoleMsg) - 1,
                            "%s, Progress: %d%% complete.", taskName, progress);
            UtoolPrintf(server->quiet, stdout, "%s %-96s\r", formattedLocalTimeNow, taskProgressConsoleMsg);

            if (finished) {
                goto DONE;
            }
        }
    }

FAILURE:
    result->broken = 1;
    goto DONE;

DONE:
    FREE_OBJ(queryCmdOutput);
    UtoolPrintf(server->quiet, stdout, "\n");
}

void ExecIpmiUpgradeFinishedCmdIfRequired(UtoolCommandOption *commandOption,
                                          const UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result)
{
    if (UtoolStringEquals(updateFirmwareOption->activateMode, UPGRADE_ACTIVATE_MODE_AUTO)) {
        UtoolIPMIRawCmdOption *finishUpgradeCmd = &(UtoolIPMIRawCmdOption) {
                .data = IPMI_UPGRADE_FINISHED_CMD
        };
        // should we handle result exception?
        char *output = UtoolIPMIExecRawCommand(commandOption, finishUpgradeCmd, result);
        FREE_OBJ(output)
    }
}

int ParseProgressFromIpmiOutputStr(char *queryCmdOutput, time_t upgradeStartTime, UtoolResult *result)
{
    int progress = 0;

    char **segments = UtoolStringSplit(queryCmdOutput, ' ');
    if (segments == NULL) {
        result->code = UTOOLE_INTERNAL;
        goto FAILURE;
    }

    char *progressStr = segments[3];
    if (progressStr == NULL) { // should not happen?
        progress = IPMI_PROGRESS_NOT_RETURNED;
        goto DONE;
    }

    if ("e4" == progressStr) { //  e4 means upgrade not started or finished
        time_t now = time(NULL);
        double spentTimeLong = difftime(now, upgradeStartTime);
        // 除CPLD和白牌包外的升级，15s内无法完成，此时认为没有开始升级，设置进度为0
        progress = spentTimeLong <= 15 ? IPMI_PROGRESS_NOT_START : 100;
    } else {
        int value = (int) strtol(progressStr, NULL, 16);
        // BMC bug, if value is ge than 128, substract 128 as progress
        progress = value >= 128 ? value - 128 : value;
    }

    goto DONE;

FAILURE:
    result->broken = 1;
    FREE_OBJ(queryCmdOutput);
    goto DONE;

DONE:
    UtoolStringFreeArrays(segments);
    return progress;
}

const IpmiUpgradeErrorMapping *GetIpmiError(const char *ipmiCmdOutput)
{
    const IpmiUpgradeErrorMapping *error = NULL;
    if (ipmiCmdOutput != NULL) {
        for (int idx = 0;; idx++) {
            const IpmiUpgradeErrorMapping *item = ipmiUpgradeErrorMapping + idx;
            if (item != NULL && item->key != NULL && item->error != NULL) {
                if (strstr(ipmiCmdOutput, item->key)) {
                    error = item;
                    break;
                }
            } else {
                break;
            }
        }
    }
    return error;
}

void
UpgradeFirmwareByRedfish(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result)
{
    ZF_LOGI("Start to update outband firmware with redfish API now");

    // if user choose to use remote file, the upgrade request has been sent at preview step.
    if (!updateFirmwareOption->isRemoteFile) {
        char *upgradeUrl = "/UpdateService/Actions/UpdateService.SimpleUpdate";
        UtoolRedfishPost(server, upgradeUrl, updateFirmwareOption->payload, NULL, NULL, result);
        if (result->broken) {
            DisplayProgress(server->quiet, DISPLAY_UPGRADE_FAILED);
            WriteFailedLogEntry(updateFirmwareOption, STAGE_UPGRADE_FIRMWARE, PROGRESS_FAILED, result);
            ZF_LOGI("Upgrade firmware failed, reason: %s", result->desc);
            goto FAILURE;
        }
    }

    /*
      make sure update has official start, message args may carry the update firmware type.
    */
    /* sleep(3); */
    /*
      we do not know when the message args will carry the real firmware type.
      So, we will get firmware type after upgrade complete.
    */

    /* waiting util task complete or exception */
    ZF_LOGI("Waiting util updating task finished...");
    WriteLogEntry(updateFirmwareOption, STAGE_UPGRADE_FIRMWARE, PROGRESS_RUN,
                  "Start execute update firmware task.");

    UtoolRedfishWaitUtilTaskFinished(server, result->data, result);
    if (result->broken) {
        DisplayProgress(server->quiet, DISPLAY_UPGRADE_FAILED);
        WriteFailedLogEntry(updateFirmwareOption, STAGE_UPGRADE_FIRMWARE, PROGRESS_FAILED, result);
        ZF_LOGI("Upgrade firmware failed, reason: %s", result->desc);
        goto FAILURE;
    }

    goto DONE;

FAILURE:
    goto DONE;

DONE:
    return;
}

/**
 * be caution that result->data will carry last task cJSON object if whole progress success.
 *
 * @param server
 * @param updateFirmwareOption
 * @param result
 */
void
UpgradeTransitionFirmware(UtoolRedfishServer *server, UtoolCommandOption *commandOption,
                          UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result)
{
    cJSON *payloadJson = NULL;

    ZF_LOGI("Start to upload transition firmware file to BMC temp.");
    char *transitionFileName = GetTransitionFileName(updateFirmwareOption->productName);
    char payload[MAX_FILE_PATH_LEN];
    UtoolWrapSecFmt(payload, MAX_PAYLOAD_LEN, MAX_PAYLOAD_LEN - 1, UPGRADE_TRANSITION_FIRMWARE_PAYLOAD,
                    transitionFileName);

    // rebuild payload
    payloadJson = cJSON_CreateRaw(payload);
    result->code = UtoolAssetCreatedJsonNotNull(payloadJson);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    ZF_LOGI("Start to update transition firmware.");
    char *upgradeUrl = "/UpdateService/Actions/UpdateService.SimpleUpdate";
    UtoolRedfishPost(server, upgradeUrl, payloadJson, NULL, NULL, result);
    if (result->broken) {
        DisplayProgress(server->quiet, DISPLAY_UPGRADE_TRANSITION_FAILED);
        WriteFailedLogEntry(updateFirmwareOption, STAGE_UPGRADE_TRANSITION_FIRMWARE, PROGRESS_FAILED, result);
        ZF_LOGI("Upgrade transition firmware failed, reason: %s", result->desc);
        goto FAILURE;
    }

    /* waiting util task complete or exception */
    ZF_LOGI("Waiting util upgrade transition firmware task finished...");
    WriteLogEntry(updateFirmwareOption, STAGE_UPGRADE_TRANSITION_FIRMWARE, PROGRESS_RUN,
                  "Start execute update transition firmware task.");

    UtoolRedfishWaitUtilTaskFinished(server, result->data, result);
    if (result->broken) {
        DisplayProgress(server->quiet, DISPLAY_UPGRADE_TRANSITION_FAILED);
        WriteFailedLogEntry(updateFirmwareOption, STAGE_UPGRADE_TRANSITION_FIRMWARE, PROGRESS_FAILED, result);
        ZF_LOGI("Upgrade transition firmware failed, reason: %s", result->desc);
        goto FAILURE;
    }

    goto DONE;

FAILURE:
    goto DONE;

DONE:
    FREE_CJSON(payloadJson)
    return;
}

/**
 * be caution that result->data will carry last task cJSON object if whole progress success.
 *
 * @param server
 * @param updateFirmwareOption
 * @param result
 */
static void StartUpgradeTargetFirmwareWorkflow(UtoolRedfishServer *server, UtoolCommandOption *commandOption,
                                               UpdateFirmwareOption *updateFirmwareOption,
                                               UtoolResult *result)
{
    // clean
    if (updateFirmwareOption->isLocalFile) {
        RetryFunc(STAGE_UPLOAD_FILE, UploadLocalFile, server, commandOption, updateFirmwareOption, result, true);
        if (result->broken) {
            goto FAILURE;
        }
    }

    // not supported.
    if (updateFirmwareOption->isRemoteFile) {
        RetryFunc(STAGE_DOWNLOAD_FILE, DownloadRemoteFile, server, commandOption, updateFirmwareOption, result, true);
        if (result->broken) {
            goto FAILURE;
        }
    }

    // upgrade firmware
    RetryFunc(STAGE_UPGRADE_FIRMWARE, UpgradeFirmware, server, commandOption, updateFirmwareOption, result, true);
    if (result->broken) {
        goto FAILURE;
    }

    goto DONE;

FAILURE:
    result->broken = 1;
    goto DONE;

DONE:
    return;
}

/**
 * be caution that result->data will carry last task cJSON object if whole progress success.
 *
 * @param server
 * @param updateFirmwareOption
 * @param result
 */
static void
StartUpgradeTransitionFirmwareWorkflow(UtoolRedfishServer *server, UtoolCommandOption *commandOption,
                                       UpdateFirmwareOption *updateFirmwareOption,
                                       UtoolResult *result)
{
    // clean
    RetryFunc(STAGE_UPLOAD_TRANSITION_FILE, UploadTransitionFile, server, commandOption, updateFirmwareOption, result,
              false);
    if (result->broken) {
        goto FAILURE;
    }

    // upgrade firmware
    RetryFunc(STAGE_UPGRADE_TRANSITION_FIRMWARE, UpgradeTransitionFirmware, server, commandOption, updateFirmwareOption,
              result, false);
    if (result->broken) {
        goto FAILURE;
    }

    goto DONE;

FAILURE:
    result->broken = 1;
    goto DONE;

DONE:
    return;
}

/**
 * 要求：升级失败要求能重试，最多3次。
 *
 * 如果升级BMC带了-e Auto参数，且出现BMC的HTTP不通或空间不足等情况时要求utool能直接先将BMC重启后再试。
 * 升级过程详细记录到升级日志文件，内容及格式详见sheet "升级日志文件"。
 *
 * @param self
 * @param option
 * @return
 */
int UtoolCmdUpdateOutbandFirmware(UtoolCommandOption *commandOption, char **outputStr)
{
    cJSON *output = NULL;                   /** output result json*/
    cJSON *payload = NULL;                  /** payload json */

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *response = &(UtoolCurlResponse) {0};
    UpdateFirmwareOption *updateFirmwareOption = &(UpdateFirmwareOption) {0};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback,
                        0, 0),
            OPT_STRING ('u', "image-uri", &(updateFirmwareOption->imageURI), "firmware image file URI", NULL, 0,
                        0),
            OPT_STRING ('e', "activate-mode", &(updateFirmwareOption->activateMode),
                        "firmware active mode, choices: {Auto, Manual}", NULL, 0, 0),
            OPT_STRING ('t', "firmware-type", &(updateFirmwareOption->firmwareType),
                        "firmware type, available choices: {BMC, BIOS, CPLD, PSUFW}",
                        NULL, 0, 0),
            OPT_STRING ('d', "dual-image", &(updateFirmwareOption->dualImage),
                        "indicates whether should update firmware for both plane, "
                        "available choices: {Single, Dual}",
                        NULL, 0, 0),
            OPT_END(),
    };

    updateFirmwareOption->disableLogEntry = false;

    // validation sub command options
    result->code = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    // validation connection options
    result->code = UtoolValidateConnectOptions(commandOption, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    /* validation update firmware options */
    ValidateUpdateFirmwareOptions(updateFirmwareOption, result);
    if (result->broken) {
        goto FAILURE;
    }

    server->quiet = commandOption->quiet;
    updateFirmwareOption->commandOption = commandOption;

    // when build payload, it will validate imageUri input options too.
    // auto detect BMC firmware type if possible
    payload = BuildPayload(server, updateFirmwareOption, result);
    updateFirmwareOption->payload = payload;
    if (result->broken) {
        goto FAILURE;
    }

    // clean
    RetryFunc(STAGE_CREATE_SESSION, CreateSession, server, commandOption, updateFirmwareOption, result, false);
    if (result->broken) {
        goto FAILURE;
    }

    // clean
    RetryFunc(STAGE_GET_PRODUCT_SN, GetProductSn, server, commandOption, updateFirmwareOption, result, false);
    if (result->broken && updateFirmwareOption->productName == NULL) {
        goto FAILURE;
    } else {
        // even we can not get product SN, we just create log file without sn, the upgrade progress should continue.
        result->broken = 0;
        result->code = UTOOLE_OK;
    }

    // clean
    RetryFunc(STAGE_CREATE_LOG_FILE, CreateLogFile, server, commandOption, updateFirmwareOption, result, false);
    if (result->broken) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, result->data, &(result->desc));
        goto FAILURE;
    }

    // parse current BMC version.
    ParseActiveBmcVersion(updateFirmwareOption, server, result);
    if (result->broken) {
        goto FAILURE;
    }

    ValidateUpdateFirmwareOptionFor258(server, updateFirmwareOption, result);
    if (result->broken) {
        goto FAILURE;
    }

    // BMC 2.58 will be filter out in transition firmware upgrade condition check.
    // because only 6.32, 6.38 is allowed to upgraded to version after 6.39
    // upgrade transition firmware if necessary.
    UpgradeTransitionFirmwareIfNecessary(server, commandOption, updateFirmwareOption, result);
    if (result->broken) {
        FREE_CJSON(result->data)
        goto FAILURE;
    }

    // upgrade firmware workflow starts,
    // if workflow runs normally, result->data will carry the latest task instance.
    StartUpgradeTargetFirmwareWorkflow(server, commandOption, updateFirmwareOption, result);

    // if it still fails when reaching the maximum retries
    if (result->broken) {
        FREE_CJSON(result->data)
        goto FAILURE;
    }

    // if dual image update required
    if (updateFirmwareOption->dualImage != NULL && UtoolStringEquals(updateFirmwareOption->dualImage, "Dual")) {
        FREE_CJSON(result->data)

        ZF_LOGI("Dual plane update is required, start now.");

        DisplayProgress(server->quiet, DISPLAY_UPGRADE_CURRENT_PLANE_DONE);
        WriteLogEntry(updateFirmwareOption, STAGE_UPGRADE_FIRMWARE, PROGRESS_RUN,
                      DISPLAY_UPGRADE_CURRENT_PLANE_DONE);
        ZF_LOGI(DISPLAY_UPGRADE_CURRENT_PLANE_DONE);

        DisplayProgress(server->quiet, DISPLAY_UPGRADE_BACKUP_PLANE_START);
        WriteLogEntry(updateFirmwareOption, STAGE_UPGRADE_FIRMWARE, PROGRESS_RUN,
                      DISPLAY_UPGRADE_BACKUP_PLANE_START);
        ZF_LOGI(DISPLAY_UPGRADE_BACKUP_PLANE_START);

        // we need to reboot BMC to update another plane
        RebootBMC(STAGE_UPGRADE_BACKUP_PLANE, server, updateFirmwareOption, result);
        if (result->broken) {
            goto FAILURE;
        }

        // parse current BMC version.
        ParseActiveBmcVersion(updateFirmwareOption, server, result);
        if (result->broken) {
            goto FAILURE;
        }

        // update bakup plane now
        StartUpgradeTargetFirmwareWorkflow(server, commandOption, updateFirmwareOption, result);

        if (result->broken) {
            FREE_CJSON(result->data)
            DisplayProgress(server->quiet, DISPLAY_UPGRADE_BACKUP_PLANE_FAILED);
            WriteLogEntry(updateFirmwareOption, STAGE_UPGRADE_FIRMWARE, PROGRESS_FAILED,
                          DISPLAY_UPGRADE_BACKUP_PLANE_FAILED);
            ZF_LOGI(DISPLAY_UPGRADE_BACKUP_PLANE_FAILED);
            goto FAILURE;
        }

        DisplayProgress(server->quiet, DISPLAY_UPGRADE_BACKUP_PLANE_DONE);
        WriteLogEntry(updateFirmwareOption, STAGE_UPGRADE_FIRMWARE, PROGRESS_RUN,
                      DISPLAY_UPGRADE_BACKUP_PLANE_DONE);
        ZF_LOGI(DISPLAY_UPGRADE_BACKUP_PLANE_DONE);
    }


    /* step4: wait util firmware updating effect */
    cJSON *firmwareType = NULL;
    if (updateFirmwareOption->activeBmcVersionEq258) {
        firmwareType = cJSON_CreateString(updateFirmwareOption->firmwareType);
        result->code = UtoolAssetCreatedJsonNotNull(firmwareType);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
    } else {
        firmwareType = cJSON_Duplicate(cJSONUtils_GetPointer(result->data, "/Messages/MessageArgs/0"),
                                       false);
        ZF_LOGI("Detected firmware type from `/Messages/MessageArgs/0`, value is: %s", firmwareType);
        // Free task cJSON object carried by result->data;
        FREE_CJSON(result->data)
    }

    if (cJSON_IsString(firmwareType)) {
        PrintFirmwareVersion(server, updateFirmwareOption, firmwareType, result);
        FREE_CJSON(result->data)
    }
    FREE_CJSON(firmwareType)


    /* if upgrade success
    DisplayProgress(server->quiet, DISPLAY_UPGRADE_DONE);
    WriteLogEntry(updateFirmwareOption, STAGE_UPGRADE_FIRMWARE, PROGRESS_SUCCESS, "");
    ZF_LOGI(DISPLAY_UPGRADE_DONE);*/

    /* everything finished */
    UtoolBuildDefaultSuccessResult(&(result->desc));
    goto DONE;

FAILURE:
    FREE_CJSON(output)
    goto DONE;

DONE:
    FREE_CJSON(payload)
    UtoolFreeRedfishServer(server);
    UtoolFreeCurlResponse(response);
    freeUpdateFirmwareOption(updateFirmwareOption);
    *outputStr = result->desc;
    return result->code;
}

void ValidateUpdateFirmwareOptionFor258(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption,
                                        UtoolResult *result)
{
    if (updateFirmwareOption->firmwareType == NULL) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_TYPE_258_REQUIRED),
                                              &(result->desc));
        goto FAILURE;
    }

    if (!UtoolStringInArray(updateFirmwareOption->firmwareType, IPMI_SUPPORT_TYPES)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_TYPE_258_ILLEGAL),
                                              &(result->desc));
        goto FAILURE;
    }

    return;

FAILURE:
    result->broken = 1;
    return;
}

void
ParseActiveBmcVersion(UpdateFirmwareOption *updateFirmwareOption, UtoolRedfishServer *server, UtoolResult *result)
{
    char *activeBmcVersion = NULL;
    RetryFunc(STAGE_GET_BMC_VERSION, GetBmcVersion, server, NULL, updateFirmwareOption, result, true);
    if (result->broken) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, result->data, &(result->desc));
        goto FAILURE;
    }

    updateFirmwareOption->activeBmcVersionDotCount = UtoolStringCountOccurrencesOf
            (updateFirmwareOption->activeBmcVersion, '.');

    activeBmcVersion = UtoolStringNDup(updateFirmwareOption->activeBmcVersion,
                                       strnlen(updateFirmwareOption->activeBmcVersion, MAX_FM_VERSION_LEN));
    char **version = UtoolStringSplit(activeBmcVersion, '.');
    if (version == NULL) {
        result->code = UTOOLE_INTERNAL;
        goto FAILURE;
    }

    // 只需要获取前两段版本号
    updateFirmwareOption->activeBmcVersionMajor = version[0] != NULL ? atoi(version[0]) : 0;
    updateFirmwareOption->activeBmcVersionMinor = version[1] != NULL ? atoi(version[1]) : 0;
    UtoolStringFreeArrays(version);

    // mark whether current active BMC version is 2.58
    updateFirmwareOption->activeBmcVersionEq258 = UtoolStringEquals(updateFirmwareOption->activeBmcVersion,
                                                                    BMC_USE_IPMI_VERSION);
    goto DONE;

FAILURE:
    result->broken = 1;
    goto DONE;
DONE:
    FREE_OBJ(activeBmcVersion)
    return;
}

void UpgradeTransitionFirmwareIfNecessary(UtoolRedfishServer *server, UtoolCommandOption *commandOption,
                                          UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result)
{
    int quiet = server->quiet;
    int disableLogEntry = updateFirmwareOption->disableLogEntry;

    // check whether transition firmware upgrade
    bool shouldUpgradeTransition = isTransitionFirmwareUpgradeRequired(server, updateFirmwareOption, result);
    if (result->broken) {
        FREE_CJSON(result->data)
        goto FAILURE;
    }

    server->quiet = true;   // disable output when upgrade transition firmware.
    updateFirmwareOption->disableLogEntry = true;   // disable log entry when upgrade transition firmware.

    if (shouldUpgradeTransition) {
        ZF_LOGI("Transition firmware upgrade is required, start now.");
        // upgrade transition firmware required.
        StartUpgradeTransitionFirmwareWorkflow(server, commandOption, updateFirmwareOption, result);
        FREE_CJSON(result->data)
        if (result->broken) {
            goto FAILURE;
        }
        ZF_LOGI("Transition firmware upgrade finished.");

        // Force Reboot BMC to make sure that transition firmware has been actived
        ZF_LOGI("Reboot BMC to active transition firmware now.");
        RebootBMC(STAGE_UPGRADE_TRANSITION_FIRMWARE, server, updateFirmwareOption, result);
        if (result->broken) {
            goto FAILURE;
        }
        ZF_LOGI("Reboot BMC succeed.");
    }

    goto DONE;

FAILURE:
    result->broken = 1;
    goto DONE;
DONE:
    server->quiet = quiet;
    updateFirmwareOption->disableLogEntry = disableLogEntry;
    return;
}

static bool isTransitionFirmwareUpgradeRequired(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption,
                                                UtoolResult *result)
{
    if (updateFirmwareOption->firmwareType == NULL ||
        !UtoolStringEquals(updateFirmwareOption->firmwareType, FM_TYPE_BMC)) {
        return false;
    }

    //NOTE(qianbiao): only 2288HV5 & 2298 V5 requires transition firmware upgrade
    if (!UtoolStringEquals(updateFirmwareOption->productName, SERVER_NAME_2288HV5) &&
        !UtoolStringEquals(updateFirmwareOption->productName, SERVER_NAME_2298V5)) {
        return false;
    }

    // NOTE(qianbiao.ng): if we can not detect target bmc version from filename, just continue upgrade as normal.
    if (updateFirmwareOption->targetBmcVersion == NULL) {
        return false;
    }

    // 需求：如果iBMC版本号或文件名为4段式版本（如：1288HV6-2288HV6-5288V6-32DIMM-iBMC_3.03.10.15.hpm），则不需要升级过渡包；
    // new 4 segments bmc version format does not need transition firmware
    // example: 1288HV6-2288HV6-5288V6-32DIMM-iBMC_3.03.10.15.hpm
    int targetBmcVersionDotCount = UtoolStringCountOccurrencesOf(updateFirmwareOption->targetBmcVersion, '.');
    if (targetBmcVersionDotCount == 3) { // new format
        ZF_LOGI("target BMC version(%s) is 4 segments, no transition upgrade required.",
                updateFirmwareOption->targetBmcVersion);
        return false;
    }

    // old version format
    int targetBmcVersion = atoi(updateFirmwareOption->targetBmcVersion);
    // 需求：判断待升级包是否需要升级过渡版本，
    // 通过文件名（如：2288H_V5_2288C_V5_5288_V5-iBMC-V643.hpm）获取版本号，判断条件：版本号>=6.39
    if (targetBmcVersion < BMC_TRANSITION_VERSION) {
        return false;
    }

    // 判断iBMC是否需要升级过渡版本，判断条件：当前BMC版本号 <6.39；
    if (updateFirmwareOption->activeBmcVersionDotCount == 3) { // new format
        ZF_LOGI("Active BMC version(%s) is 4 segments, no transition upgrade required.",
                updateFirmwareOption->activeBmcVersion);
        return false;
    }

    if (UtoolStringEquals(updateFirmwareOption->activeBmcVersion, BMC_TRANSITION_FM_VERSION)) {
        return false;
    }

    // 原需求：小于6.39版本的需要升级过渡版本
    // int major = updateFirmwareOption->activeBmcVersionMajor;
    // int minor = updateFirmwareOption->activeBmcVersionMinor;
    // if (major < BMC_TRANSITION_MAJOR_VERSION ||
    //     (major == BMC_TRANSITION_MAJOR_VERSION && minor < BMC_TRANSITION_MINOR_VERSION)) {
    //     return true;
    // }

    // 新需求：只有6.32和6.38才允许升级过渡版本，其他的直接报错。
    if (UtoolStringInArray(updateFirmwareOption->activeBmcVersion, ALLOW_UPGRADE_TO_GE_639)) {
        return true;
    }

    // if active bmc version less than 6.39, break.
    int major = updateFirmwareOption->activeBmcVersionMajor;
    int minor = updateFirmwareOption->activeBmcVersionMinor;
    if (major < BMC_TRANSITION_MAJOR_VERSION ||
        (major == BMC_TRANSITION_MAJOR_VERSION && minor < BMC_TRANSITION_MINOR_VERSION)) {
        result->broken = 1;
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(BMC_UPGRADE_TO_GE_639_NOT_ALLOWED),
                                              &(result->desc));
        return false;
    }

    return false;
}

void freeUpdateFirmwareOption(UpdateFirmwareOption *updateFirmwareOption)
{
    if (updateFirmwareOption) {
        if (updateFirmwareOption->psn != "") {
            FREE_OBJ(updateFirmwareOption->psn)
        }
        FREE_OBJ(updateFirmwareOption->productName)
        FREE_OBJ(updateFirmwareOption->activeBmcVersion)
        FREE_OBJ(updateFirmwareOption->targetBmcVersion)

        if (updateFirmwareOption->logFileFP != NULL) {
            fseeko(updateFirmwareOption->logFileFP, -3, SEEK_END);
            _off_t position = ftello(updateFirmwareOption->logFileFP);
            if (position != -1) {
                int ret = ftruncate(fileno(updateFirmwareOption->logFileFP), position); /* delete last dot */
                if (ret != 0) {
                    ZF_LOGE("Failed to truncate update firmware log file");
                }
            } else {
                ZF_LOGE("Failed to run ftello method against update firmware log file");
            }
            fprintf(updateFirmwareOption->logFileFP, LOG_TAIL); /* write log file head content*/
            fclose(updateFirmwareOption->logFileFP);            /* close log file FP */
        }
    }
}


void UpdateFirmware2(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result)
{
    cJSON *payload = NULL;
    int retryTimes = 0; /* current retry round */

    while (retryTimes++ < UPGRADE_FIRMWARE_RETRY_TIMES) {
        ZF_LOGI("Start to update outband firmware now, round: %d.", retryTimes);

        /* if (retryTimes > 1) {
             sleep(5);
         }*/

        char round[16];
        UtoolWrapSecFmt(round, sizeof(round), sizeof(round) - 1, "Round %d", retryTimes);

        /* step1: build payload - upload local file if necessary */
        payload = BuildPayload(server, updateFirmwareOption, result);
        if (payload == NULL || result->broken) {
            goto RETRY;
        }

        DisplayProgress(server->quiet, DISPLAY_UPGRADE_START);
        WriteLogEntry(updateFirmwareOption, STAGE_UPGRADE_FIRMWARE, PROGRESS_START, round);

        /* step2: send update firmware request */
        char *url = "/UpdateService/Actions/UpdateService.SimpleUpdate";
        UtoolRedfishPost(server, url, payload, NULL, NULL, result);
        if (result->broken) {
            goto RETRY;
        }

        /* step3: Wait util download file progress finished */
        if (!updateFirmwareOption->isLocalFile) {
            ZF_LOGI("Waiting for BMC download update firmware file ...");
            DisplayProgress(server->quiet, DISPLAY_DOWNLOAD_FILE_START);
            WriteLogEntry(updateFirmwareOption, STAGE_DOWNLOAD_FILE, PROGRESS_START,
                          "Start download remote file to BMC");

            UtoolRedfishWaitUtilTaskStart(server, result->data, result);
            if (result->broken) {
                ZF_LOGE("Failed to download update firmware file.");
                DisplayProgress(server->quiet, DISPLAY_DOWNLOAD_FILE_FAILED);
                WriteFailedLogEntry(updateFirmwareOption, STAGE_DOWNLOAD_FILE, PROGRESS_FAILED, result);
                goto RETRY;
            }

            ZF_LOGE("Download update firmware file successfully.");
            DisplayProgress(server->quiet, DISPLAY_DOWNLOAD_FILE_DONE);
            WriteLogEntry(updateFirmwareOption, STAGE_DOWNLOAD_FILE, PROGRESS_SUCCESS,
                          "Download remote file to BMC success");
        }


        /*
          make sure update has official start, message args will carry the update firmware type.
        */
        /* sleep(3); */
        /*
          we do not know when the message args will carry the real firmware type.
          So, we will get firmware type after upgrade complete.
        */

        /* step4: waiting util task complete or exception */
        ZF_LOGI("Waiting util updating task finished...");
        WriteLogEntry(updateFirmwareOption, STAGE_UPGRADE_FIRMWARE, PROGRESS_RUN,
                      "Start execute update firmware task.");
        UtoolRedfishWaitUtilTaskFinished(server, result->data, result);
        if (result->broken) {
            ZF_LOGI("Updating firmware task failed.");
            DisplayProgress(server->quiet, DISPLAY_UPGRADE_FAILED);
            WriteFailedLogEntry(updateFirmwareOption, STAGE_UPGRADE_FIRMWARE, PROGRESS_FAILED, result);
            goto RETRY;
        }

        WriteLogEntry(updateFirmwareOption, STAGE_UPGRADE_FIRMWARE, PROGRESS_RUN,
                      "Update firmware task completed.");
        break;

RETRY:
        if (retryTimes < UPGRADE_FIRMWARE_RETRY_TIMES) {
            DisplayProgress(server->quiet, DISPLAY_UPDATE_FAILED_RETRY_NOW);
            /* reset temp values */
            result->broken = 0;
            if (result->desc != NULL) {
                FREE_OBJ(result->desc);
            }

            if (result->data != NULL) {
                FREE_CJSON(result->data)
            }

            if (payload != NULL) {
                FREE_CJSON(payload)
            }

            // should validate if result->retryable
            if (UtoolStringEquals(UPGRADE_ACTIVATE_MODE_AUTO, updateFirmwareOption->activateMode)) {
                ZF_LOGI("Failed to complete firmware updating task, will reset bmc and retry now");
                // we do not care about whether server is alive?
                // ResetBMCAndWaitingAlive(server);
                RebootBMC("", server, updateFirmwareOption, result);
                if (result->broken) {
                    goto FAILURE;
                }
            }
        }

        continue;
    }

    goto DONE;

FAILURE:
    goto DONE;

DONE:
    FREE_CJSON(payload)
    return;
}

/**
* wait update firmware job tasking effect
*  - compare version if necessary
*
* @param server
* @param updateFirmwareOption
* @param firmwareType
* @param result
*/
void PrintFirmwareVersion(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption, cJSON *firmwareType,
                          UtoolResult *result)
{

    char displayMessage[MAX_OUTPUT_LEN] = {0};
    const FirmwareMapping *mapping = NULL;
    for (int idx = 0;; idx++) {
        const FirmwareMapping *_firmware = firmwareMapping + idx;
        if (_firmware && _firmware->firmwareName) {
            if (UtoolStringEquals(_firmware->firmwareName, firmwareType->valuestring)) {
                mapping = _firmware;
                break;
            }
        }
    }

    DisplayProgress(server->quiet, DISPLAY_ACTIVE_START);
    WriteLogEntry(updateFirmwareOption, STAGE_ACTIVATE, PROGRESS_START, mapping->firmwareName);
    ZF_LOGI("Activate firmware %s now.", mapping->firmwareName);

    if (mapping) {
        /* get current firmware version */
        ZF_LOGI("Try to get current firmware version now.");
        UtoolRedfishGet(server, mapping->firmwareURL, NULL, NULL, result);
        if (result->broken) {
            UtoolWrapSecFmt(displayMessage, MAX_OUTPUT_LEN, MAX_OUTPUT_LEN - 1,
                            DISPLAY_GET_FIRMWARE_VERSION_FAILED,
                            mapping->firmwareName);
            DisplayProgress(server->quiet, displayMessage);
            WriteLogEntry(updateFirmwareOption, STAGE_ACTIVATE, PROGRESS_GET_CURRENT_VERSION, displayMessage);
            FREE_OBJ(result->desc)
            UtoolBuildStringOutputResult(STATE_FAILURE, MSG_FAILED_TO_GET_CURRENT_VERSION, &(result->desc));
            goto FAILURE;
        }

        cJSON *versionNode = cJSON_GetObjectItem(result->data, "Version");
        result->code = UtoolAssetParseJsonNotNull(versionNode);
        if (result->code != UTOOLE_OK) {
            DisplayProgress(server->quiet, DISPLAY_GET_FIRMWARE_VERSION_FAILED);
            goto FAILURE;
        }

        UtoolWrapSecFmt(displayMessage, MAX_OUTPUT_LEN, MAX_OUTPUT_LEN - 1,
                        "Firmware(%s)'s current version is %s",
                        mapping->firmwareName, versionNode->valuestring);
        DisplayProgress(server->quiet, displayMessage);
        WriteLogEntry(updateFirmwareOption, STAGE_ACTIVATE, PROGRESS_GET_CURRENT_VERSION, displayMessage);
        FREE_CJSON(result->data)

        // 2021-12-09(turnbig): we just keep the old logics here.
        /* we need to get new firmware version if auto restart */
        if (UtoolStringEquals(mapping->firmwareName, FW_BMC)) {
            DisplayProgress(server->quiet, DISPLAY_BMC_REBOOT_REQUIRED);
            ZF_LOGI(DISPLAY_BMC_REBOOT_REQUIRED);

            RebootBMC(STAGE_ACTIVATE, server, updateFirmwareOption, result);
            if (result->broken) {
                goto FAILURE;
            }

            // even BMC reboot succeed, may not all service is ready, wait 10s in case.
            sleep(10);

            UtoolRedfishGet(server, mapping->firmwareURL, NULL, NULL, result);
            if (result->broken) {
                DisplayProgress(server->quiet, MSG_FAILED_TO_GET_NEW_VERSION);
                WriteLogEntry(updateFirmwareOption, STAGE_ACTIVATE, PROGRESS_GET_NEW_VERSION,
                              MSG_FAILED_TO_GET_NEW_VERSION);
                FREE_OBJ(result->desc)
                UtoolBuildStringOutputResult(STATE_FAILURE, MSG_FAILED_TO_GET_NEW_VERSION, &(result->desc));
                goto FAILURE;
            }

            cJSON *versionNode = cJSON_GetObjectItem(result->data, "Version");
            result->code = UtoolAssetParseJsonNotNull(versionNode);
            if (result->code != UTOOLE_OK) {
                goto FAILURE;
            }

            char message[MAX_OUTPUT_LEN];
            UtoolWrapSecFmt(message, MAX_OUTPUT_LEN, MAX_OUTPUT_LEN - 1, "%s firmware's new version is %s",
                            mapping->firmwareName, versionNode->valuestring);
            DisplayProgress(server->quiet, message);
            WriteLogEntry(updateFirmwareOption, STAGE_ACTIVATE, PROGRESS_GET_NEW_VERSION, message);
            FREE_CJSON(result->data)
        }
    }

    DisplayProgress(server->quiet, DISPLAY_ACTIVE_DONE);
    WriteLogEntry(updateFirmwareOption, STAGE_ACTIVATE, PROGRESS_SUCCESS, "");
    ZF_LOGI("Activate firmware %s succeed.", mapping->firmwareName);
    return;

FAILURE:
    result->broken = 1;
    return;
}

/**
*
* create update log file&folder
*
* @param server
* @param updateFirmwareOption
* @param result
*/
static void createUpdateLogFile(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption,
                                UtoolResult *result)
{
    cJSON *getSystemRespJson = NULL;
    updateFirmwareOption->startTime = time(NULL);

    UtoolRedfishGet(server, "/Systems/%s", NULL, NULL, result);
    if (result->broken) {
        ZF_LOGE("Failed to load system resource.");
        goto FAILURE;
    }
    getSystemRespJson = result->data;

    cJSON *sn = cJSONUtils_GetPointer(getSystemRespJson, "/SerialNumber");
    if (sn != NULL && sn->valuestring != NULL) {
        ZF_LOGI("Parsing product SN, value is %s.", sn->valuestring);
        updateFirmwareOption->psn = sn->valuestring;
        DisplayProgress(server->quiet, DISPLAY_GET_PRODUCT_SN_DONE);
    } else {
        updateFirmwareOption->psn = "";
        DisplayProgress(server->quiet, DISPLAY_GET_PRODUCT_SN_FAILED);
    }

    char folderName[PATH_MAX];
    struct tm *tm_now = localtime(&updateFirmwareOption->startTime);
    if (tm_now == NULL) {
        result->code = UTOOLE_INTERNAL;
        goto FAILURE;
    }
    UtoolWrapSecFmt(folderName, PATH_MAX, PATH_MAX - 1, "%d%02d%02d%02d%02d%02d_%s", tm_now->tm_year + 1900,
                    tm_now->tm_mon + 1,
                    tm_now->tm_mday, tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec,
                    updateFirmwareOption->psn);

    ZF_LOGE("Try to create folder for current updating, folder: %s.", folderName);
#if defined(__MINGW32__)
    int ret = mkdir(folderName);
#else
    int old = umask(S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);
    int ret = mkdir(folderName, 0750);
    umask(old);
#endif
    if (ret != 0) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(FAILED_TO_CREATE_FOLDER),
                                              &(result->desc));
        goto FAILURE;
    }


    char filepath[PATH_MAX] = {0};
    char realFilepath[PATH_MAX] = {0};
    UtoolWrapSecFmt(filepath, PATH_MAX, PATH_MAX - 1, "%s/update-firmware.log", folderName);

    UtoolFileRealpath(filepath, realFilepath, PATH_MAX);
    updateFirmwareOption->logFileFP = fopen(realFilepath, "a");
    if (!updateFirmwareOption->logFileFP) {
        ZF_LOGW("Failed to create log file %s.", filepath);
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(FAILED_TO_CREATE_FILE),
                                              &(result->desc));
        goto FAILURE;
    }

    DisplayProgress(server->quiet, DISPLAY_CREATE_LOG_FILE_DONE);

    /* write log file head content*/
    fprintf(updateFirmwareOption->logFileFP, LOG_HEAD);

    goto DONE;

FAILURE:
    DisplayProgress(server->quiet, DISPLAY_CREATE_LOG_FILE_FAILED);
    result->broken = 1;
    goto DONE;

DONE:
    FREE_CJSON(result->data)
}

static int RebootBMC(char *stage, UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption,
                     UtoolResult *result)
{
    int ret;
    time_t begin, now;
    cJSON *payload = NULL;
    UtoolCurlResponse *resetManagerResp = &(UtoolCurlResponse) {0};

    // when activate, BMC will automate reboot.
    if (!UtoolStringEquals(stage, STAGE_ACTIVATE)) {
        // DisplayProgress(server->quiet, "Try reboot to resolve failures.");
        // ZF_LOGI("Try reboot to resolve failures.");

        // build payload
        char *payloadString = "{\"ResetType\" : \"ForceRestart\" }";
        payload = cJSON_CreateRaw(payloadString);
        ret = UtoolAssetCreatedJsonNotNull(payload);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }

        // we do not care about whether reset manager request is successful,
        // maybe the BMC has automated restart.
        char *url = "/Managers/%s/Actions/Manager.Reset";
        UtoolMakeCurlRequest(server, url, HTTP_POST, payload, NULL, resetManagerResp);
    }

    // from now on
    DisplayProgress(server->quiet, DISPLAY_BMC_REBOOT_START);
    WriteLogEntry(updateFirmwareOption, stage, PROGRESS_REBOOT_BMC, "");
    ZF_LOGI(PROGRESS_REBOOT_BMC);

    DisplayRunningProgress(server->quiet, DISPLAY_WAIT_BMC_SHUTDOWN);
    WriteLogEntry(updateFirmwareOption, stage, PROGRESS_WAITING_SHUTDOWN, "");
    ZF_LOGI(DISPLAY_WAIT_BMC_SHUTDOWN);

    // waiting BMC shutdown, total wait time is 120s = 40*3
    bool hasShutdown = false;
    time(&begin);
    time(&now);
    while (difftime(now, begin) <= TIME_LIMIT_SHUTDOWN) {
        UtoolPrintf(server->quiet, stdout, ".");
        UtoolCurlResponse *getRedfishResp = &(UtoolCurlResponse) {0};
        ret = UtoolMakeCurlRequest(server, "/", HTTP_GET, NULL, NULL, getRedfishResp);
        if (ret != CURLE_OK || getRedfishResp->httpStatusCode > 300) {
            UtoolFreeCurlResponse(getRedfishResp);
            hasShutdown = true;
            // shutdown progress finished
            UtoolPrintf(server->quiet, stdout, "\n");

            DisplayProgress(server->quiet, DISPLAY_BMC_HAS_BEEN_SHUTDOWN);
            WriteLogEntry(updateFirmwareOption, stage, PROGRESS_SHUTDOWN_SUCCEED, "");
            ZF_LOGI(DISPLAY_BMC_HAS_BEEN_SHUTDOWN);

            // waiting BMC power on
            DisplayRunningProgress(server->quiet, DISPLAY_WAIT_BMC_POWER_ON);
            WriteLogEntry(updateFirmwareOption, stage, PROGRESS_WAITING_ALIVE, "");
            ZF_LOGI(DISPLAY_WAIT_BMC_POWER_ON);
            break;
        }

        ZF_LOGI("BMC is still alive now. Next shutdown checking will be %d seconds later.",
                REBOOT_CHECK_INTERVAL);
        sleep(REBOOT_CHECK_INTERVAL);
        UtoolFreeCurlResponse(getRedfishResp);
        time(&now);
    }

    if (!hasShutdown) {
        UtoolPrintf(server->quiet, stdout, "\n");
        DisplayProgress(server->quiet, MSG_SHUTDOWN_TO_EXCEED);
        WriteLogEntry(updateFirmwareOption, stage, PROGRESS_FAILED, MSG_SHUTDOWN_TO_EXCEED);
        UtoolBuildStringOutputResult(STATE_FAILURE, MSG_SHUTDOWN_TO_EXCEED, &(result->desc));
        goto FAILURE;
    }

    // waiting BMC alive, total wait time is 420s = 120 * 3
    time(&begin);
    time(&now);
    bool isAlive = false;
    while (difftime(now, begin) <= TIME_LIMIT_POWER_ON) {
        UtoolPrintf(server->quiet, stdout, ".");
        UtoolCurlResponse *getRedfishResp = &(UtoolCurlResponse) {0};
        ret = UtoolMakeCurlRequest(server, "/redfish/v1/UpdateService/FirmwareInventory/ActiveBMC", HTTP_GET, NULL,
                                   NULL, getRedfishResp);
        if (ret == UTOOLE_OK && getRedfishResp->httpStatusCode < 300) {
            UtoolFreeCurlResponse(getRedfishResp);
            isAlive = true;
            // sleep 15s to make sure BMC has startup completely.
            sleep(15);

            // power on progress finished
            UtoolPrintf(server->quiet, stdout, "\n");
            // reboot progress finished
            DisplayProgress(server->quiet, DISPLAY_BMC_REBOOT_DONE);
            WriteLogEntry(updateFirmwareOption, stage, PROGRESS_REBOOT_DONE, "");
            ZF_LOGI(DISPLAY_BMC_REBOOT_DONE);
            break;
        }

        ZF_LOGI("BMC is still down now. Next alive check will be %d seconds later.", REBOOT_CHECK_INTERVAL);
        sleep(REBOOT_CHECK_INTERVAL);
        UtoolFreeCurlResponse(getRedfishResp);
        time(&now);
    }

    if (!isAlive) {
        UtoolPrintf(server->quiet, stdout, "\n");

        DisplayProgress(server->quiet, MSG_STARTUP_TO_EXCEED);
        WriteLogEntry(updateFirmwareOption, stage, PROGRESS_FAILED, MSG_STARTUP_TO_EXCEED);
        ZF_LOGI(MSG_STARTUP_TO_EXCEED);

        UtoolBuildStringOutputResult(STATE_FAILURE, MSG_STARTUP_TO_EXCEED, &(result->desc));
        goto FAILURE;
    }

    goto DONE;

FAILURE:
    result->broken = 1;
    goto DONE;

DONE:
    FREE_CJSON(payload)
    UtoolFreeCurlResponse(resetManagerResp);
    return ret;
}


/**
* validate user input options for update firmware command
*
* @param updateFirmwareOption
* @param result
* @return
*/
static void ValidateUpdateFirmwareOptions(UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result)
{
    if (updateFirmwareOption->imageURI == NULL) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_IMAGE_URI_REQUIRED),
                                              &(result->desc));
        goto FAILURE;
    }

    if (updateFirmwareOption->activateMode == NULL) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_MODE_REQUIRED),
                                              &(result->desc));
        goto FAILURE;
    }

    if (!UtoolStringInArray(updateFirmwareOption->activateMode, MODE_CHOICES)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_MODE_ILLEGAL),
                                              &(result->desc));
        goto FAILURE;
    }

    if (updateFirmwareOption->firmwareType != NULL) {
        if (!UtoolStringInArray(updateFirmwareOption->firmwareType, TYPE_CHOICES)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_TYPE_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    if (updateFirmwareOption->dualImage != NULL) {
        if (!UtoolStringInArray(updateFirmwareOption->dualImage, DUAL_CHOICES)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_DUAL_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    updateFirmwareOption->isRemoteFile = false;
    updateFirmwareOption->isLocalFile = false;

    /** try to treat imageURI as a local file */
    struct stat fileInfo;
    char *imageUri = updateFirmwareOption->imageURI;
    char realFilepath[PATH_MAX] = {0};
    const char *ok = UtoolFileRealpath(imageUri, realFilepath, PATH_MAX);
    if (ok != NULL) {
        FILE *imageFileFP = fopen(realFilepath, "rb"); /* open file to upload */
        if (imageFileFP) {
            if (fstat(fileno(imageFileFP), &fileInfo) == 0) {
                updateFirmwareOption->isLocalFile = true;
            }
        }

        if (imageFileFP != NULL) {
            fclose(imageFileFP);
        }
    }

    return;

FAILURE:
    result->broken = 1;
    return;
}

static cJSON *BuildPayload(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption,
                           UtoolResult *result)
{
    char *filename = NULL;
    cJSON *payload = NULL;
    UtoolParsedUrl *parsedUrl = NULL;
    char *imageUri = updateFirmwareOption->imageURI;

    ZF_LOGD("Update firmware options:");
    ZF_LOGD(" ");
    ZF_LOGD("\t\tImageURI\t\t : %s", imageUri);
    ZF_LOGD("\t\tActivateMode\t : %s", updateFirmwareOption->activateMode);
    ZF_LOGD("\t\tFirmwareType\t : %s", updateFirmwareOption->firmwareType);
    ZF_LOGD("\t\tDualImage\t : %s", updateFirmwareOption->dualImage);
    ZF_LOGD(" ");

    // build payload
    payload = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    if (updateFirmwareOption->isLocalFile) {
        filename = basename(imageUri);
        char uploadFilePath[MAX_FILE_PATH_LEN];
        UtoolWrapSecFmt(uploadFilePath, MAX_FILE_PATH_LEN, MAX_FILE_PATH_LEN - 1, "/tmp/web/%s", filename);
        cJSON *imageUriNode = cJSON_AddStringToObject(payload, "ImageURI", uploadFilePath);
        result->code = UtoolAssetCreatedJsonNotNull(imageUriNode);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
    } else if (UtoolStringStartsWith(imageUri, "/tmp/")) { /** handle BMC tmp file */
        filename = basename(imageUri);
        cJSON *imageUriNode = cJSON_AddStringToObject(payload, "ImageURI", imageUri);
        result->code = UtoolAssetCreatedJsonNotNull(imageUriNode);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
    } else { /** handle remote file */
        ZF_LOGI("Firmware image uri is not local file, will start update firmware directly now.");

        /** parse url */
        parsedUrl = UtoolParseURL(imageUri);

        if (!parsedUrl || !parsedUrl->scheme) {
            ZF_LOGI("It seems the image uri is illegal.");
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_IMAGE_URI_NO_SCHEMA),
                                                  &(result->desc));
            goto FAILURE;
        }

        if (!UtoolStringCaseInArray(parsedUrl->scheme, IMAGE_PROTOCOL_CHOICES)) {
            char message[MAX_FAILURE_MSG_LEN];
            UtoolWrapSecFmt(message, MAX_FAILURE_MSG_LEN, MAX_FAILURE_MSG_LEN - 1,
                            OPTION_IMAGE_URI_ILLEGAL_SCHEMA,
                            parsedUrl->scheme);
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(message),
                                                  &(result->desc));
            goto FAILURE;
        }

        cJSON *imageUriNode = cJSON_AddStringToObject(payload, "ImageURI", imageUri);
        result->code = UtoolAssetCreatedJsonNotNull(imageUriNode);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }

        UtoolStringToUpper(parsedUrl->scheme);
        cJSON *protocolNode = cJSON_AddStringToObject(payload, "TransferProtocol", parsedUrl->scheme);
        result->code = UtoolAssetCreatedJsonNotNull(protocolNode);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }

        filename = parsedUrl->path;
        updateFirmwareOption->isRemoteFile = true;
    }

    if (updateFirmwareOption->firmwareType == NULL ||
        UtoolStringEquals(updateFirmwareOption->firmwareType, FM_TYPE_BMC)) {
        char *index = strstr(filename, FM_TYPE_BMC);
        if (index != NULL) {
            // detect whether firmware type is BMC from filename
            if (updateFirmwareOption->firmwareType == NULL) {
                updateFirmwareOption->firmwareType = FM_TYPE_BMC;
            }
            parseTargetBmcVersionFromFilepath(updateFirmwareOption, filename);
        }

        // NOTE(qianbiao.ng): if we can not detect target bmc version from filename, just continue upgrade as normal.
        // if (updateFirmwareOption->targetBmcVersion == NULL) {
        //     result->code = UtoolBuildOutputResult(STATE_FAILURE,
        //                                           cJSON_CreateString(OPTION_CANNOT_PARSE_BMC_VERSION_FROM_IMAGE_URI),
        //                                           &(result->desc));
        //     goto FAILURE;
        // }
    }

    goto DONE;

FAILURE:
    result->broken = 1;
    FREE_CJSON(payload)
    payload = NULL;
    goto DONE;

DONE:
    UtoolFreeParsedURL(parsedUrl);
    return payload;
}

static void parseTargetBmcVersionFromFilepath(UpdateFirmwareOption *option, char *path)
{
    // old format: 2288H_V5_2288C_V5_5288_V5-iBMC-V643.hpm
    // new format: 1288HV6-2288HV6-5288V6-32DIMM-iBMC_3.03.10.15.hpm
    char *split = UtoolStringLastSplit(path, '-');
    char *split2 = UtoolStringLastSplit(split, '_'); // V643.hpm or 3.03.10.15.hpm

    // old format
    if (UtoolStringStartsWith(split2, "V") && UtoolStringEndsWith(split2, ".hpm")) {
        int len = strnlen(split2, MAX_FM_VERSION_LEN + 4);
        char *versionString = UtoolStringNDup(split2 + 1, len - 5);
        option->targetBmcVersion = versionString;
        return;
    } else { // new format
        char *findDotCount = split2;
        size_t count = 0;
        while (*findDotCount) if (*findDotCount++ == '.') ++count;
        if (count == 4) { // new format
            int len = strnlen(split2, MAX_FM_VERSION_LEN + 4);
            char *versionString = UtoolStringNDup(split2, len - 4);
            option->targetBmcVersion = versionString;
            return;
        }
    }

    ZF_LOGW("Failed to parse target BMC version from file path: %s", path);
}


static void
WriteFailedLogEntry(UpdateFirmwareOption *option, const char *stage, const char *state, UtoolResult *result)
{
    if (option->disableLogEntry != true) {
        if (result->code != UTOOLE_OK) {
            const char *errorString = (result->code > UTOOLE_OK && result->code < CURL_LAST) ?
                                      curl_easy_strerror(result->code) : UtoolGetStringError(result->code);
            WriteLogEntry(option, stage, state, errorString);
            return;
        } else {
            cJSON *output = cJSON_Parse(result->desc);
            if (output != NULL) {
                cJSON *message = cJSONUtils_GetPointer(output, "/Message/0");
                WriteLogEntry(option, stage, state, message == NULL ? "" : message->valuestring);
            }
            FREE_CJSON(output)
        }
    }
}


static void WriteLogEntry(UpdateFirmwareOption *option, const char *stage, const char *state, const char *note)
{
    if (option->disableLogEntry != true) {
        /* get current timestamp */
        char nowStr[100] = {0};
        time_t now = time(NULL);
        struct tm *tm_now = localtime(&now);
        if (tm_now != NULL) {
            strftime(nowStr, sizeof(nowStr), "%Y-%m-%dT%H%M%S%z", tm_now);
        }

        /* write log file head content*/
        fprintf(option->logFileFP, LOG_ENTRY_FORMAT, nowStr, stage, state, note);
        fflush(option->logFileFP);
    }
}
