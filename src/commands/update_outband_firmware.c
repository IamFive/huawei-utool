/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
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
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>
#include <securec.h>
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

#define TIME_LIMIT_SHUTDOWN 120
#define TIME_LIMIT_POWER_ON 420
#define REBOOT_CHECK_INTERVAL 3
#define LOG_HEAD "{\"log\":[   \n"
#define LOG_TAIL "\n]}"
#define MAX_LOG_ENTRY_LEN 512
#define LOG_ENTRY_FORMAT "\t{\n\t\t\"Time\": \"%s\",\n\t\t\"Stage\": \"%s\",\n\t\t\"State\": \"%s\",\n\t\t\"Note\": \"%s\"\n\t}, \n"

#define STAGE_CREATE_SESSION "Create session"
#define STAGE_GET_PRODUCT_SN "Get product SN"
#define STAGE_CREATE_LOG_FILE "Create log file"
#define STAGE_UPLOAD_FILE "Upload firmware to BMC"
#define STAGE_DOWNLOAD_FILE "Download remote firmware to BMC"
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

#define DISPLAY_GET_PRODUCT_SN_DONE "Fetch bmc product serial number successfully."
#define DISPLAY_GET_PRODUCT_SN_FAILED "Fetch bmc product serial number failed."
#define DISPLAY_PRODUCT_SN_NULL "Failed to parse bmc product serial number, it's an empty value."

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

#define DISPLAY_UPGRADE_START "Upgrade firmware start"
#define DISPLAY_UPGRADE_DONE "Upgrade firmware successfully"
#define DISPLAY_UPGRADE_FAILED "Upgrade firmware failed."


#define DISPLAY_ACTIVE_START "Activate firmware start."
#define DISPLAY_ACTIVE_IN_PROGRESS "Activate inprogress"
#define DISPLAY_ACTIVE_DONE "Activate successfully"

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
    bool isLocalFile;
    bool isRemoteFile;
    FILE *logFileFP;
    char *uploadFilePath;
    cJSON *payload;
    time_t startTime;
    UtoolCommandOption *commandOption;
} UpdateFirmwareOption;

typedef struct _FirmwareMapping {
    char *firmwareName;
    char *firmwareURL;
} FirmwareMapping;


static const FirmwareMapping firmwareMapping[] = {
        {.firmwareName = FW_BMC, .firmwareURL="/UpdateService/FirmwareInventory/ActiveBMC"},
        {.firmwareName = "Backplane CPLD", .firmwareURL="/UpdateService/FirmwareInventory/MainBoardCPLD"},
        {.firmwareName = "Motherboard CPLD", .firmwareURL="/UpdateService/FirmwareInventory/MainBoardCPLD"},
        {.firmwareName = "PS Firmware", .firmwareURL="/UpdateService/FirmwareInventory/chassisPS1"},
        {.firmwareName = "BIOS", .firmwareURL="/UpdateService/FirmwareInventory/Bios"},
        NULL
};


static const char *MODE_CHOICES[] = {UPGRADE_ACTIVATE_MODE_AUTO, UPGRADE_ACTIVATE_MODE_MANUAL, NULL};
static const char *DUAL_CHOICES[] = {"Single", "Dual", NULL};
static const char *TYPE_CHOICES[] = {"BMC", "BIOS", "CPLD", "PSUFW", NULL};
static const char *IMAGE_PROTOCOL_CHOICES[] = {"HTTPS", "SCP", "SFTP", "CIFS", "TFTP", "NFS", NULL};

static const char *OPTION_IMAGE_URI_REQUIRED = "Error: option `image-uri` is required.";
static const char *OPTION_IMAGE_URI_ILLEGAL = "Error: option `image-uri` is illegal.";
static const char *OPTION_IMAGE_URI_NO_SCHEMA = "Error: URI is not a local file nor a remote network protocol file.";
static const char *OPTION_IMAGE_URI_ILLEGAL_SCHEMA = "Error: Protocol `%s` is not supported.";

static const char *OPTION_MODE_REQUIRED = "Error: option `activate-mode` is required.";
static const char *OPTION_MODE_ILLEGAL = "Error: option `activate-mode` is illegal, available choices: Auto, Manual.";
static const char *OPTION_TYPE_ILLEGAL = "Error: option `firmware-type` is illegal, "
                                         "available choices: BMC, BIOS, CPLD, PSUFW.";
static const char *OPTION_DUAL_ILLEGAL = "Error: option `dual-image` is illegal, "
                                         "available choices: Single, Dual.";

static const char *PRODUCT_SN_IS_NOT_SET = "Error: product SN is not correct.";
static const char *FAILED_TO_CREATE_FOLDER = "Error: failed to create log folder.";
static const char *FAILED_TO_CREATE_FILE = "Error: failed to create log file.";
static const char *LOG_FILE_PATH_ILLEGAL = "Error: log file path is illegal, Please make sure the path exists.";

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
                UpdateFirmwareOption *updateFirmwareOption,
                UtoolResult *result
        ),
        UtoolRedfishServer *server,
        UpdateFirmwareOption *updateFirmwareOption,
        UtoolResult *result,
        bool writeLogEntry);

/**
 *  Auto Retry Steps defination
 */
static int CreateSession(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result);

static int GetProductSn(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result);

static void CreateLogFile(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result);

/**
 * perform upload local file to BMC action
 * @param server
 * @param updateFirmwareOption
 * @param result
 */
static void UploadLocalFile(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption,
                            UtoolResult *result);

/**
 * perform download remote file to BMC action
 * @param server
 * @param updateFirmwareOption
 * @param result
 */
static void DownloadRemoteFile(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption,
                               UtoolResult *result);

/**
 * perform upgrade firmware action.
 *
 * @param server
 * @param updateFirmwareOption
 * @param result
 */
static void
UpgradeFirmware(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result);

/**
 * start whole upgrade firmware workflow
 *
 * @param server
 * @param updateFirmwareOption
 * @param result
 */
static void StartUpgradeFirmwareWorkflow(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption,
                                         UtoolResult *result);

static void DisplayRunningProgress(int quiet, const char *progress);

static void DisplayProgress(int quiet, const char *progress);


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
                UpdateFirmwareOption *updateFirmwareOption,
                UtoolResult *result
        ),
        UtoolRedfishServer *server,
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

        func(server, updateFirmwareOption, result);
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

static int CreateSession(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result)
{
    UtoolGetRedfishServer2(updateFirmwareOption->commandOption, server, result);
    if (result->broken || result->code != UTOOLE_OK) {
        UtoolFreeRedfishServer(server);
    }
}

static int GetProductSn(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result)
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

    cJSON *sn = cJSONUtils_GetPointer(getSystemRespJson, "/SerialNumber");
    if (sn == NULL || sn->valuestring == NULL) {
        ZF_LOGE("Could not parse product SerialNumber from response.");
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

static void CreateLogFile(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result)
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

static void UploadLocalFile(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption,
                            UtoolResult *result)
{
    ZF_LOGI("Firmware image uri is a local file, try upload it to BMC temp now...");
    char *imageUri = updateFirmwareOption->imageURI;

    // upload file to bmc through HTTPS protocol
    UtoolUploadFileToBMC(server, imageUri, result);
    if (result->broken) {
        WriteFailedLogEntry(updateFirmwareOption, STAGE_UPLOAD_FILE, PROGRESS_FAILED, result);
        goto FAILURE;
    }

    goto FAILURE;

FAILURE:
    result->reboot = 1;
    result->broken = 1;
    goto DONE;

DONE:
    return;
}

static void DownloadRemoteFile(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption,
                               UtoolResult *result)
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
            // TODO(turnbig): reboot directly?
            result->reboot = 1;
            goto FAILURE;
        }
    }

FAILURE:
    goto DONE;

DONE:
    return;
}

void UpgradeFirmware(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result)
{
    ZF_LOGI("Start to update outband firmware now");

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

    WriteLogEntry(updateFirmwareOption, STAGE_UPGRADE_FIRMWARE, PROGRESS_RUN, "Start execute update firmware task.");
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

static void StartUpgradeFirmwareWorkflow(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption,
                                         UtoolResult *result)
{
    // clean
    if (updateFirmwareOption->isLocalFile) {
        RetryFunc(STAGE_UPLOAD_FILE, UploadLocalFile, server, updateFirmwareOption, result, true);
        if (result->broken) {
            goto FAILURE;
        }
    }

    // not supported.
    if (updateFirmwareOption->isRemoteFile) {
        RetryFunc(STAGE_DOWNLOAD_FILE, DownloadRemoteFile, server, updateFirmwareOption, result, true);
        if (result->broken) {
            goto FAILURE;
        }
    }

    // upgrade firmware
    RetryFunc(STAGE_UPGRADE_FIRMWARE, UpgradeFirmware, server, updateFirmwareOption, result, true);
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
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('u', "image-uri", &(updateFirmwareOption->imageURI), "firmware image file URI", NULL, 0, 0),
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
    payload = BuildPayload(server, updateFirmwareOption, result);
    updateFirmwareOption->payload = payload;
    if (result->broken) {
        goto FAILURE;
    }

    // clean
    RetryFunc(STAGE_CREATE_SESSION, CreateSession, server, updateFirmwareOption, result, false);
    if (result->broken) {
        goto FAILURE;
    }

    // clean
    RetryFunc(STAGE_GET_PRODUCT_SN, GetProductSn, server, updateFirmwareOption, result, false);
    if (result->broken) {
        // even we can not get product SN, we just create log file without sn, the upgrade progress should continue.
        result->broken = 0;
        result->code = UTOOLE_OK;
    }

    // clean
    RetryFunc(STAGE_CREATE_LOG_FILE, CreateLogFile, server, updateFirmwareOption, result, false);
    if (result->broken) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, result->data, &(result->desc));
        goto FAILURE;
    }

    // upgrade firmware workflow starts,
    // if workflow runs normally, result->data will carry the latest task instance.
    StartUpgradeFirmwareWorkflow(server, updateFirmwareOption, result);

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
        WriteLogEntry(updateFirmwareOption, STAGE_UPGRADE_FIRMWARE, PROGRESS_RUN, DISPLAY_UPGRADE_CURRENT_PLANE_DONE);
        ZF_LOGI(DISPLAY_UPGRADE_CURRENT_PLANE_DONE);

        DisplayProgress(server->quiet, DISPLAY_UPGRADE_BACKUP_PLANE_START);
        WriteLogEntry(updateFirmwareOption, STAGE_UPGRADE_FIRMWARE, PROGRESS_RUN, DISPLAY_UPGRADE_BACKUP_PLANE_START);
        ZF_LOGI(DISPLAY_UPGRADE_BACKUP_PLANE_START);

        // TODO(qianbiao.ng): is logic correct?
        // we need to reboot BMC to update another plane
        RebootBMC(STAGE_UPGRADE_BACKUP_PLANE, server, updateFirmwareOption, result);
        if (result->broken) {
            goto FAILURE;
        }

        // update bakup plane now
        StartUpgradeFirmwareWorkflow(server, updateFirmwareOption, result);

        if (result->broken) {
            FREE_CJSON(result->data)
            DisplayProgress(server->quiet, DISPLAY_UPGRADE_BACKUP_PLANE_FAILED);
            WriteLogEntry(updateFirmwareOption, STAGE_UPGRADE_FIRMWARE, PROGRESS_FAILED, DISPLAY_UPGRADE_BACKUP_PLANE_FAILED);
            ZF_LOGI(DISPLAY_UPGRADE_BACKUP_PLANE_FAILED);
            goto FAILURE;
        }

        DisplayProgress(server->quiet, DISPLAY_UPGRADE_BACKUP_PLANE_DONE);
        WriteLogEntry(updateFirmwareOption, STAGE_UPGRADE_FIRMWARE, PROGRESS_RUN, DISPLAY_UPGRADE_BACKUP_PLANE_DONE);
        ZF_LOGI(DISPLAY_UPGRADE_BACKUP_PLANE_DONE);
    }


    /* step4: wait util firmware updating effect */
    cJSON *firmwareType = cJSONUtils_GetPointer(result->data, "/Messages/MessageArgs/0");
    if (cJSON_IsString(firmwareType)) {
        PrintFirmwareVersion(server, updateFirmwareOption, firmwareType, result);
    }
    FREE_CJSON(result->data)

    /* if upgrade success */
    DisplayProgress(server->quiet, DISPLAY_UPGRADE_DONE);
    WriteLogEntry(updateFirmwareOption, STAGE_UPGRADE_FIRMWARE, PROGRESS_SUCCESS, "");
    ZF_LOGI(DISPLAY_UPGRADE_DONE);

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

    if (updateFirmwareOption->psn != "") {
        FREE_OBJ(updateFirmwareOption->psn)
    }

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

    *outputStr = result->desc;
    return result->code;
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

        WriteLogEntry(updateFirmwareOption, STAGE_UPGRADE_FIRMWARE, PROGRESS_RUN, "Update firmware task completed.");
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
            UtoolWrapSecFmt(displayMessage, MAX_OUTPUT_LEN, MAX_OUTPUT_LEN - 1, DISPLAY_GET_FIRMWARE_VERSION_FAILED,
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

        UtoolWrapSecFmt(displayMessage, MAX_OUTPUT_LEN, MAX_OUTPUT_LEN - 1, "Firmware(%s)'s current version is %s",
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
                    tm_now->tm_mday, tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec, updateFirmwareOption->psn);

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
        DisplayProgress(server->quiet, "Try reboot to resolve failures.");
        ZF_LOGI("Try reboot to resolve failures.");

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
            DisplayRunningProgress(server->quiet, DISPLAY_BMC_HAS_BEEN_SHUTDOWN);
            WriteLogEntry(updateFirmwareOption, stage, PROGRESS_SHUTDOWN_SUCCEED, "");
            ZF_LOGI(DISPLAY_BMC_HAS_BEEN_SHUTDOWN);

            UtoolFreeCurlResponse(getRedfishResp);
            hasShutdown = true;
            // shutdown progress finished
            UtoolPrintf(server->quiet, stdout, "\n");

            // waiting BMC power on
            DisplayRunningProgress(server->quiet, DISPLAY_WAIT_BMC_POWER_ON);
            WriteLogEntry(updateFirmwareOption, stage, PROGRESS_WAITING_ALIVE, "");
            ZF_LOGI(DISPLAY_WAIT_BMC_POWER_ON);
            break;
        }

        ZF_LOGI("BMC is still alive now. Next shutdown checking will be %d seconds later.", REBOOT_CHECK_INTERVAL);
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
        ret = UtoolMakeCurlRequest(server, "/", HTTP_GET, NULL, NULL, getRedfishResp);
        if (ret == UTOOLE_OK && getRedfishResp->httpStatusCode < 300) {
            UtoolFreeCurlResponse(getRedfishResp);
            isAlive = true;
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
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_MODE_REQUIRED), &(result->desc));
        goto FAILURE;
    }

    if (!UtoolStringInArray(updateFirmwareOption->activateMode, MODE_CHOICES)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_MODE_ILLEGAL), &(result->desc));
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
    char *ok = UtoolFileRealpath(imageUri, realFilepath, PATH_MAX);
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
        char *filename = basename(imageUri);
        char uploadFilePath[MAX_FILE_PATH_LEN];
        UtoolWrapSecFmt(uploadFilePath, MAX_FILE_PATH_LEN, MAX_FILE_PATH_LEN - 1, "/tmp/web/%s", filename);
        cJSON *imageUriNode = cJSON_AddStringToObject(payload, "ImageURI", uploadFilePath);
        result->code = UtoolAssetCreatedJsonNotNull(imageUriNode);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
        goto DONE;
    } else if (UtoolStringStartsWith(imageUri, "/tmp/")) { /** handle BMC tmp file */
        cJSON *imageUriNode = cJSON_AddStringToObject(payload, "ImageURI", imageUri);
        result->code = UtoolAssetCreatedJsonNotNull(imageUriNode);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
        goto DONE;
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
            UtoolWrapSecFmt(message, MAX_FAILURE_MSG_LEN, MAX_FAILURE_MSG_LEN - 1, OPTION_IMAGE_URI_ILLEGAL_SCHEMA,
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

        // TODO(qianbiao.ng): 需要和华为方确认，为何新版本文档里多了这个字段。
        /**
        cJSON *activeMode = cJSON_AddStringToObject(payload, "ActiveMode", "Immediately");
        result->code = UtoolAssetCreatedJsonNotNull(activeMode);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        } */

        updateFirmwareOption->isRemoteFile = true;
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


static void
WriteFailedLogEntry(UpdateFirmwareOption *option, const char *stage, const char *state, UtoolResult *result)
{
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


static void WriteLogEntry(UpdateFirmwareOption *option, const char *stage, const char *state, const char *note)
{
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
