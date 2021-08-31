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

#define LOG_HEAD "{\"log\":[   \n"
#define LOG_TAIL "\n]}"
#define MAX_LOG_ENTRY_LEN 512
#define LOG_ENTRY_FORMAT "\t{\n\t\t\"Time\": \"%s\",\n\t\t\"Stage\": \"%s\",\n\t\t\"State\": \"%s\",\n\t\t\"Note\": \"%s\"\n\t}, \n"

#define STAGE_UPDATE "Update firmware"
#define STAGE_UPLOAD_FILE "Upload File"
#define STAGE_DOWNLOAD_FILE "Download File"
#define STAGE_ACTIVATE "Activate"

#define PROGRESS_START "Start"
#define PROGRESS_RUN "In Progress"
#define PROGRESS_SUCCESS "Success"
#define PROGRESS_FAILED "Failed"
#define PROGRESS_INVAILD_URI "Invalid URI"
#define PROGRESS_GET_CURRENT_VERSION "Get current version"
#define PROGRESS_WAITING_SHUTDOWN "Waiting for BMC shutdown"
#define PROGRESS_WAITING_ALIVE "Waiting for BMC startup"
#define PROGRESS_REBOOT_DONE "BMC reboot succeed"
#define PROGRESS_GET_NEW_VERSION "Get new version"

#define DISPLAY_CREATE_SESSION_DONE "Create session successfully"
#define DISPLAY_CREATE_SESSION_FAILED "Create session failed"

#define DISPLAY_GET_PRODUCT_SN_DONE "Fetch bmc product serial number successfully"
#define DISPLAY_GET_PRODUCT_SN_FAILED "Fetch bmc product serial number failed"

#define DISPLAY_CREATE_LOG_FILE_DONE "Create log file successfully"
#define DISPLAY_CREATE_LOG_FILE_FAILED "Create log file failed"

#define DISPLAY_UPLOAD_FILE_START "Upload file start"
#define DISPLAY_UPLOAD_FILE_DONE "Upload file successfully"
#define DISPLAY_UPLOAD_FILE_FAILED "Upload file failed"

#define DISPLAY_DOWNLOAD_FILE_START "Download remote file start"
#define DISPLAY_DOWNLOAD_FILE_DONE "Download remote file successfully"
#define DISPLAY_DOWNLOAD_FILE_FAILED "Download remote file failed"

#define DISPLAY_UPGRADE_CURRENT_PLANE_DONE "Upgrade firmware for current plane successfully"
#define DISPLAY_UPGRADE_BAKUP_PLANE_START "Upgrade firmware for bakup plane start."
#define DISPLAY_UPGRADE_BAKUP_PLANE_DONE "Upgrade firmware for bakup plane successfully."
#define DISPLAY_UPGRADE_BAKUP_PLANE_FAILED "Upgrade firmware for bakup plane failed."

#define DISPLAY_UPGRADE_START "Upgrade firmware start"
#define DISPLAY_UPGRADE_FAILED "Upgrade firmware failed"
#define DISPLAY_UPGRADE_DONE "Upgrade firmware successfully"


#define DISPLAY_ACTIVE_START "Activate firmware start"
#define DISPLAY_ACTIVE_IN_PROGRESS "Activate inprogress"
#define DISPLAY_ACTIVE_DONE "Activate successfully"

#define DISPLAY_BMC_REBOOT_START "BMC reboot start"
#define DISPLAY_WAIT_BMC_SHUTDOWN "Waiting BMC shutdown..."
#define DISPLAY_WAIT_BMC_POWER_ON "Waiting BMC power on..."
#define DISPLAY_BMC_REBOOT_DONE "BMC reboot complete"
#define DISPLAY_GET_FIRMWARE_VERSION_FAILED "Get %s's current version failed"
#define DISPLAY_VERIFY_DONE "Version verify ok"
#define DISPLAY_UPDATE_FAILED_RETRY_NOW "Update failed, will retry now."

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
    FILE *logFileFP;
    time_t startTime;
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

static void UpdateFirmware(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result);

static int ResetBMCAndWaitingAlive(UtoolRedfishServer *server);

static void createUpdateLogFile(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption,
                                UtoolResult *result);

static void WriteLogEntry(UpdateFirmwareOption *option, const char *stage, const char *state, const char *note);

static void
WriteFailedLogEntry(UpdateFirmwareOption *option, const char *stage, const char *state, UtoolResult *result);

void PrintFirmwareVersion(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption, cJSON *firmwareType,
                          UtoolResult *result);

static int RebootBMC(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result);

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
    cJSON *output = NULL,                   /** output result json*/
    *payload = NULL,                /** payload json */
    *lastRedfishUpdateTask = NULL;

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

    // get redfish system id
    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        DisplayProgress(server->quiet, DISPLAY_CREATE_SESSION_FAILED);
        goto FAILURE;
    } else {
        DisplayProgress(server->quiet, DISPLAY_CREATE_SESSION_DONE);
    }

    ZF_LOGI("Start update outband firmware progress now");

    /* create log file */
    createUpdateLogFile(server, updateFirmwareOption, result);
    if (result->broken) {
        goto FAILURE;
    }

    // TODO dual image update
    UpdateFirmware(server, updateFirmwareOption, result);

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
        WriteLogEntry(updateFirmwareOption, STAGE_UPDATE, PROGRESS_RUN, DISPLAY_UPGRADE_CURRENT_PLANE_DONE);

        DisplayProgress(server->quiet, DISPLAY_UPGRADE_BAKUP_PLANE_START);
        WriteLogEntry(updateFirmwareOption, STAGE_UPDATE, PROGRESS_RUN, DISPLAY_UPGRADE_BAKUP_PLANE_START);

        // we need to reboot BMC to update another plane
        RebootBMC(server, updateFirmwareOption, result);
        if (result->broken) {
            goto FAILURE;
        }

        // update bakup plane now
        UpdateFirmware(server, updateFirmwareOption, result);
        if (result->broken) {
            FREE_CJSON(result->data)
            ZF_LOGI("Updating firmware for bakup plane failed.");
            DisplayProgress(server->quiet, DISPLAY_UPGRADE_BAKUP_PLANE_FAILED);
            WriteLogEntry(updateFirmwareOption, STAGE_UPDATE, PROGRESS_FAILED, DISPLAY_UPGRADE_BAKUP_PLANE_FAILED);
            goto FAILURE;
        }

        ZF_LOGI("Updating firmware for bakup plane succeed.");
        DisplayProgress(server->quiet, DISPLAY_UPGRADE_BAKUP_PLANE_DONE);
        WriteLogEntry(updateFirmwareOption, STAGE_UPDATE, PROGRESS_RUN, DISPLAY_UPGRADE_BAKUP_PLANE_DONE);
    }

    lastRedfishUpdateTask = result->data;

    /* step4: wait util firmware updating effect */
    cJSON *firmwareType = cJSONUtils_GetPointer(result->data, "/Messages/MessageArgs/0");
    if (cJSON_IsString(firmwareType)) {
        PrintFirmwareVersion(server, updateFirmwareOption, firmwareType, result);
    }
    FREE_CJSON(result->data)

    /* if update success */
    ZF_LOGI("Updating firmware task succeed.");
    DisplayProgress(server->quiet, DISPLAY_UPGRADE_DONE);
    WriteLogEntry(updateFirmwareOption, STAGE_UPDATE, PROGRESS_SUCCESS, "");

    /* everything finished */
    UtoolBuildDefaultSuccessResult(&(result->desc));
    goto DONE;

FAILURE:
    FREE_CJSON(output)
    goto DONE;

DONE:
    FREE_CJSON(payload)
    FREE_CJSON(lastRedfishUpdateTask)
    UtoolFreeRedfishServer(server);
    UtoolFreeCurlResponse(response);

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


void UpdateFirmware(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result)
{
    cJSON *payload = NULL;
    int retryTimes = 0; /* current retry round */

    while (retryTimes++ < UPGRADE_FIRMWARE_RETRY_TIMES) {
        ZF_LOGI("Start to update outband firmware now, round: %d.", retryTimes);

        /* if (retryTimes > 1) {
             sleep(5);
         }*/

        char round[16];
        UtoolWrapSnprintf(round, sizeof(round), sizeof(round) - 1, "Round %d", retryTimes);

        /* step1: build payload - upload local file if necessary */
        payload = BuildPayload(server, updateFirmwareOption, result);
        if (payload == NULL || result->broken) {
            goto RETRY;
        }

        DisplayProgress(server->quiet, DISPLAY_UPGRADE_START);
        WriteLogEntry(updateFirmwareOption, STAGE_UPDATE, PROGRESS_START, round);

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
        WriteLogEntry(updateFirmwareOption, STAGE_UPDATE, PROGRESS_RUN, "Start execute update firmware task.");
        UtoolRedfishWaitUtilTaskFinished(server, result->data, result);
        if (result->broken) {
            ZF_LOGI("Updating firmware task failed.");
            DisplayProgress(server->quiet, DISPLAY_UPGRADE_FAILED);
            WriteFailedLogEntry(updateFirmwareOption, STAGE_UPDATE, PROGRESS_FAILED, result);
            goto RETRY;
        }

        WriteLogEntry(updateFirmwareOption, STAGE_UPDATE, PROGRESS_RUN, "Update firmware task completed.");
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
                RebootBMC(server, updateFirmwareOption, result);
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
    if (mapping) {
        /* get current firmware version */
        ZF_LOGI("Try to get new firmware version ...");
        UtoolRedfishGet(server, mapping->firmwareURL, NULL, NULL, result);
        if (result->broken) {
            UtoolWrapSnprintf(displayMessage, MAX_OUTPUT_LEN, MAX_OUTPUT_LEN - 1, DISPLAY_GET_FIRMWARE_VERSION_FAILED,
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

        UtoolWrapSnprintf(displayMessage, MAX_OUTPUT_LEN, MAX_OUTPUT_LEN - 1, "%s firmware's current version is %s",
                          mapping->firmwareName, versionNode->valuestring);
        DisplayProgress(server->quiet, displayMessage);
        WriteLogEntry(updateFirmwareOption, STAGE_ACTIVATE, PROGRESS_GET_CURRENT_VERSION, displayMessage);
        FREE_CJSON(result->data)

        /* we need to get new firmware version if auto restart */
        if (UtoolStringEquals(mapping->firmwareName, FW_BMC)) {
            DisplayProgress(server->quiet, DISPLAY_BMC_REBOOT_START);
            DisplayRunningProgress(server->quiet, DISPLAY_WAIT_BMC_SHUTDOWN);
            ZF_LOGI("Waiting BMC shutdown now...");
            WriteLogEntry(updateFirmwareOption, STAGE_ACTIVATE, PROGRESS_WAITING_SHUTDOWN, "");
            // waiting BMC shutdown, total wait time is 136s = (16+1)*16/2
            bool hasShutdown = false;
            int shutdownInterval = 16;
            while (shutdownInterval > 0) {
                UtoolPrintf(server->quiet, stdout, ".");
                UtoolCurlResponse *getRedfishResp = &(UtoolCurlResponse) {0};
                int ret = UtoolMakeCurlRequest(server, "/", HTTP_GET, NULL, NULL, getRedfishResp);
                if (ret != CURLE_OK || getRedfishResp->httpStatusCode > 300) {
                    ZF_LOGI("BMC is down now.");
                    UtoolFreeCurlResponse(getRedfishResp);
                    hasShutdown = true;
                    // shutdown progress finished
                    UtoolPrintf(server->quiet, stdout, "\n");

                    // waiting BMC power on
                    DisplayRunningProgress(server->quiet, DISPLAY_WAIT_BMC_POWER_ON);
                    WriteLogEntry(updateFirmwareOption, STAGE_ACTIVATE, PROGRESS_WAITING_ALIVE, "");
                    break;
                }

                ZF_LOGI("BMC is still alive now. Next shutdown checking will be %d seconds later.", shutdownInterval);
                sleep(shutdownInterval);
                shutdownInterval--;
                UtoolFreeCurlResponse(getRedfishResp);
            }

            if (!hasShutdown) {
                UtoolPrintf(server->quiet, stdout, "\n");
                DisplayProgress(server->quiet, MSG_SHUTDOWN_TO_EXCEED);
                WriteLogEntry(updateFirmwareOption, STAGE_ACTIVATE, PROGRESS_FAILED, MSG_SHUTDOWN_TO_EXCEED);
                UtoolBuildStringOutputResult(STATE_FAILURE, MSG_SHUTDOWN_TO_EXCEED, &(result->desc));
                goto FAILURE;
            }

            // waiting BMC alive, total wait time is 136s = (30+1)*30/2
            ZF_LOGI("Waiting BMC alive now...");
            int aliveInterval = 30;
            bool hasReboot = false;
            while (aliveInterval > 0) {
                UtoolPrintf(server->quiet, stdout, ".");
                UtoolCurlResponse *getRedfishResp = &(UtoolCurlResponse) {0};
                int ret = UtoolMakeCurlRequest(server, "/", HTTP_GET, NULL, NULL, getRedfishResp);
                if (ret == UTOOLE_OK && getRedfishResp->httpStatusCode < 300) {
                    ZF_LOGI("BMC is alive now.");
                    UtoolFreeCurlResponse(getRedfishResp);
                    hasReboot = true;
                    // power on progress finished
                    UtoolPrintf(server->quiet, stdout, "\n");
                    // reboot progress finished
                    DisplayProgress(server->quiet, DISPLAY_BMC_REBOOT_DONE);
                    WriteLogEntry(updateFirmwareOption, STAGE_ACTIVATE, PROGRESS_REBOOT_DONE, "");
                    break;
                }

                ZF_LOGI("BMC is still down now. Next alive check will be %d seconds later.", aliveInterval);
                sleep(aliveInterval);
                aliveInterval--;
                UtoolFreeCurlResponse(getRedfishResp);
            }

            if (!hasReboot) {
                UtoolPrintf(server->quiet, stdout, "\n");
                DisplayProgress(server->quiet, MSG_STARTUP_TO_EXCEED);
                WriteLogEntry(updateFirmwareOption, STAGE_ACTIVATE, PROGRESS_FAILED, MSG_STARTUP_TO_EXCEED);
                UtoolBuildStringOutputResult(STATE_FAILURE, MSG_STARTUP_TO_EXCEED, &(result->desc));
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
            UtoolWrapSnprintf(message, MAX_OUTPUT_LEN, MAX_OUTPUT_LEN - 1, "%s firmware's new version is %s",
                              mapping->firmwareName, versionNode->valuestring);
            DisplayProgress(server->quiet, message);
            WriteLogEntry(updateFirmwareOption, STAGE_ACTIVATE, PROGRESS_GET_NEW_VERSION, message);
            FREE_CJSON(result->data)
        }
    }

    DisplayProgress(server->quiet, DISPLAY_ACTIVE_DONE);
    WriteLogEntry(updateFirmwareOption, STAGE_ACTIVATE, PROGRESS_SUCCESS, "");
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
    UtoolWrapSnprintf(folderName, PATH_MAX, PATH_MAX - 1, "%d%02d%02d%02d%02d%02d_%s", tm_now->tm_year + 1900,
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
    UtoolWrapSnprintf(filepath, PATH_MAX, PATH_MAX - 1, "%s/update-firmware.log", folderName);

    UtoolFileRealpath(filepath, realFilepath);
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


/**
 *
 * @param server
 * @return
 */
static int ResetBMCAndWaitingAlive(UtoolRedfishServer *server)
{
    int ret;
    cJSON *payload;
    UtoolCurlResponse *resetManagerResp = &(UtoolCurlResponse) {0};

    DisplayProgress(server->quiet, DISPLAY_BMC_REBOOT_START);
    ZF_LOGI("Restart BMC and waiting alive");

    // build payload
    char *payloadString = "{\"ResetType\" : \"ForceRestart\" }";
    payload = cJSON_CreateRaw(payloadString);
    ret = UtoolAssetCreatedJsonNotNull(payload);
    if (ret != UTOOLE_OK) {
        goto DONE;
    }

    DisplayProgress(server->quiet, DISPLAY_WAIT_BMC_SHUTDOWN);
    // we do not care about whether reset manager is successful
    char *url = "/Managers/%s/Actions/Manager.Reset";
    UtoolMakeCurlRequest(server, url, HTTP_POST, payload, NULL, resetManagerResp);
    sleep(5);  // wait 5 seconds to let manager resetting takes effect

    DisplayRunningProgress(server->quiet, DISPLAY_WAIT_BMC_POWER_ON);
    ZF_LOGI("Waiting BMC alive now...");
    // waiting BMC up, total wait time is (30 + 1) * 30/2
    int interval = 30;
    while (interval > 0) {
        UtoolCurlResponse *getRedfishResp = &(UtoolCurlResponse) {0};
        ret = UtoolMakeCurlRequest(server, "/", HTTP_GET, NULL, NULL, getRedfishResp);
        if (ret == UTOOLE_OK && getRedfishResp->httpStatusCode < 300) {
            ZF_LOGI("BMC is alive now.");
            UtoolPrintf(server->quiet, stdout, "\n");
            // reboot progress finished
            DisplayProgress(server->quiet, DISPLAY_BMC_REBOOT_DONE);
            UtoolFreeCurlResponse(getRedfishResp);
            break;
        }

        UtoolPrintf(server->quiet, stdout, ".");
        ZF_LOGI("BMC is down now. Next alive check will be %d seconds later.", interval);
        sleep(interval);
        interval--;
        UtoolFreeCurlResponse(getRedfishResp);
    }

DONE:
    FREE_CJSON(payload)
    UtoolFreeCurlResponse(resetManagerResp);
    return ret;
}

static int RebootBMC(UtoolRedfishServer *server, UpdateFirmwareOption *updateFirmwareOption, UtoolResult *result)
{

    int ret;

    cJSON *payload;
    UtoolCurlResponse *resetManagerResp = &(UtoolCurlResponse) {0};

    DisplayProgress(server->quiet, DISPLAY_BMC_REBOOT_START);
    ZF_LOGI("Restart BMC and waiting alive");

    // build payload
    char *payloadString = "{\"ResetType\" : \"ForceRestart\" }";
    payload = cJSON_CreateRaw(payloadString);
    ret = UtoolAssetCreatedJsonNotNull(payload);
    if (ret != UTOOLE_OK) {
        goto DONE;
    }

    DisplayRunningProgress(server->quiet, DISPLAY_WAIT_BMC_SHUTDOWN);
    ZF_LOGI("Waiting BMC shutdown now...");
    WriteLogEntry(updateFirmwareOption, STAGE_ACTIVATE, PROGRESS_WAITING_SHUTDOWN, "");

    // we do not care about whether reset manager is successful
    char *url = "/Managers/%s/Actions/Manager.Reset";
    UtoolMakeCurlRequest(server, url, HTTP_POST, payload, NULL, resetManagerResp);

    // waiting BMC shutdown, total wait time is 136s = (16+1)*16/2
    bool hasShutdown = false;
    int shutdownInterval = 16;
    while (shutdownInterval > 0) {
        UtoolPrintf(server->quiet, stdout, ".");
        UtoolCurlResponse *getRedfishResp = &(UtoolCurlResponse) {0};
        ret = UtoolMakeCurlRequest(server, "/", HTTP_GET, NULL, NULL, getRedfishResp);
        if (ret != CURLE_OK || getRedfishResp->httpStatusCode > 300) {
            ZF_LOGI("BMC is down now.");
            UtoolFreeCurlResponse(getRedfishResp);
            hasShutdown = true;
            // shutdown progress finished
            UtoolPrintf(server->quiet, stdout, "\n");

            // waiting BMC power on
            DisplayRunningProgress(server->quiet, DISPLAY_WAIT_BMC_POWER_ON);
            WriteLogEntry(updateFirmwareOption, STAGE_ACTIVATE, PROGRESS_WAITING_ALIVE, "");
            break;
        }

        ZF_LOGI("BMC is still alive now. Next shutdown checking will be %d seconds later.", shutdownInterval);
        sleep(shutdownInterval);
        shutdownInterval--;
        UtoolFreeCurlResponse(getRedfishResp);
    }

    if (!hasShutdown) {
        UtoolPrintf(server->quiet, stdout, "\n");
        DisplayProgress(server->quiet, MSG_SHUTDOWN_TO_EXCEED);
        WriteLogEntry(updateFirmwareOption, STAGE_ACTIVATE, PROGRESS_FAILED, MSG_SHUTDOWN_TO_EXCEED);
        UtoolBuildStringOutputResult(STATE_FAILURE, MSG_SHUTDOWN_TO_EXCEED, &(result->desc));
        goto FAILURE;
    }

    // waiting BMC alive, total wait time is 136s = (30+1)*30/2
    ZF_LOGI("Waiting BMC alive now...");
    int aliveInterval = 30;
    bool hasReboot = false;
    while (aliveInterval > 0) {
        UtoolPrintf(server->quiet, stdout, ".");
        UtoolCurlResponse *getRedfishResp = &(UtoolCurlResponse) {0};
        ret = UtoolMakeCurlRequest(server, "/", HTTP_GET, NULL, NULL, getRedfishResp);
        if (ret == UTOOLE_OK && getRedfishResp->httpStatusCode < 300) {
            ZF_LOGI("BMC is alive now.");
            UtoolFreeCurlResponse(getRedfishResp);
            hasReboot = true;
            // power on progress finished
            UtoolPrintf(server->quiet, stdout, "\n");
            // reboot progress finished
            DisplayProgress(server->quiet, DISPLAY_BMC_REBOOT_DONE);
            WriteLogEntry(updateFirmwareOption, STAGE_ACTIVATE, PROGRESS_REBOOT_DONE, "");
            break;
        }

        ZF_LOGI("BMC is still down now. Next alive check will be %d seconds later.", aliveInterval);
        sleep(aliveInterval);
        aliveInterval--;
        UtoolFreeCurlResponse(getRedfishResp);
    }

    if (!hasReboot) {
        UtoolPrintf(server->quiet, stdout, "\n");
        DisplayProgress(server->quiet, MSG_STARTUP_TO_EXCEED);
        WriteLogEntry(updateFirmwareOption, STAGE_ACTIVATE, PROGRESS_FAILED, MSG_STARTUP_TO_EXCEED);
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

    updateFirmwareOption->isLocalFile = false;

    /** try to treat imageURI as a local file */
    struct stat fileInfo;
    char *imageUri = updateFirmwareOption->imageURI;
    char realFilepath[PATH_MAX] = {0};
    char *ok = UtoolFileRealpath(imageUri, realFilepath);
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
    FILE *imageFileFP = NULL;
    char *imageUri = updateFirmwareOption->imageURI;

    // only output if not command help action is requested.
    ZF_LOGD("Update firmware options:");
    ZF_LOGD(" ");
    ZF_LOGD("\t\tImageURI\t\t : %s", imageUri);
    ZF_LOGD("\t\tActivateMode\t : %s", updateFirmwareOption->activateMode);
    ZF_LOGD("\t\tFirmwareType\t : %s", updateFirmwareOption->firmwareType);
    ZF_LOGD("\t\tDualImage\t : %s", updateFirmwareOption->dualImage);
    ZF_LOGD(" ");

    struct stat fileInfo;
    UtoolParsedUrl *parsedUrl = NULL;

    // build payload
    payload = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    bool isLocalFile = false;
    char realFilePath[PATH_MAX] = {0};
    char *ok = UtoolFileRealpath(imageUri, realFilePath);
    if (ok != NULL) { /** try to treat imageURI as a local file */
        imageFileFP = fopen(realFilePath, "rb"); /* open file to upload */
        if (imageFileFP) {
            if (fstat(fileno(imageFileFP), &fileInfo) == 0) {
                isLocalFile = true;
            }
        }
    }

    if (isLocalFile) {  /** if file exists in local machine, try to upload it to BMC */
        ZF_LOGI("Firmware image uri is a local file.");

        DisplayProgress(server->quiet, DISPLAY_UPLOAD_FILE_START);
        WriteLogEntry(updateFirmwareOption, STAGE_UPLOAD_FILE, PROGRESS_START, "");

        // upload file to bmc through HTTP protocol
        UtoolUploadFileToBMC(server, imageUri, result);

        if (result->broken) {
            WriteFailedLogEntry(updateFirmwareOption, STAGE_UPLOAD_FILE, PROGRESS_FAILED, result);
            goto FAILURE;
        }

        DisplayProgress(server->quiet, DISPLAY_UPLOAD_FILE_DONE);
        WriteLogEntry(updateFirmwareOption, STAGE_UPLOAD_FILE, PROGRESS_SUCCESS, "");

        char *filename = basename(imageUri);
        char uploadFilePath[MAX_FILE_PATH_LEN];
        UtoolWrapSnprintf(uploadFilePath, MAX_FILE_PATH_LEN, MAX_FILE_PATH_LEN - 1, "/tmp/web/%s", filename);

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
            WriteLogEntry(updateFirmwareOption, STAGE_UPLOAD_FILE, PROGRESS_INVAILD_URI, OPTION_IMAGE_URI_NO_SCHEMA);
            goto FAILURE;
        }

        if (!UtoolStringCaseInArray(parsedUrl->scheme, IMAGE_PROTOCOL_CHOICES)) {
            char message[MAX_FAILURE_MSG_LEN];
            UtoolWrapSnprintf(message, MAX_FAILURE_MSG_LEN, MAX_FAILURE_MSG_LEN - 1, OPTION_IMAGE_URI_ILLEGAL_SCHEMA,
                              parsedUrl->scheme);
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(message),
                                                  &(result->desc));
            WriteLogEntry(updateFirmwareOption, STAGE_UPLOAD_FILE, PROGRESS_INVAILD_URI, message);
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
    }

    goto DONE;

FAILURE:
    DisplayProgress(server->quiet, DISPLAY_UPLOAD_FILE_FAILED);
    result->broken = 1;
    FREE_CJSON(payload)
    payload = NULL;
    goto DONE;

DONE:
    if (imageFileFP) {                  /* close FP */
        fclose(imageFileFP);
    }
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
