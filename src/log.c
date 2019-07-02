/*
* Copyright © Huawei Technologies Co., Ltd. 2012-2018. All rights reserved.
* Description: zf-log initialize
* Author:
* Create: 2019-06-16
* Notes:
*/
#if defined(_WIN32) || defined(_WIN64)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include "zf_log.h"


FILE *g_UtoolLogFileFP = NULL;

static void LogToFileOutputCallback(const zf_log_message *msg, const void *arg)
{
    (void) arg;
    *msg->p = '\n';
    fwrite(msg->buf, msg->p - msg->buf + 1, 1, g_UtoolLogFileFP);
    fflush(g_UtoolLogFileFP);
}

static void CloseLogFileOutput(void)
{
    ZF_LOGI("Application exit, close log file now.");
    fclose(g_UtoolLogFileFP);
}

int UtoolSetLogFilePath(const char *const log_file_path)
{
    if (!g_UtoolLogFileFP) {
        char realFilepath[PATH_MAX] = {0};
        realpath(log_file_path, realFilepath);
        if (realFilepath != NULL) {
            g_UtoolLogFileFP = fopen(realFilepath, "a");
            if (!g_UtoolLogFileFP) {
                ZF_LOGW("Failed to open log file %s", log_file_path);
                return 1;
            }
            atexit(CloseLogFileOutput);
            zf_log_set_output_v(ZF_LOG_PUT_STD, 0, LogToFileOutputCallback);
            ZF_LOGI("Log to file %s initialize succeed.", log_file_path);
            return 0;
        } else {
            perror("failed to create log file");
            return 1;
        }
    }

    return 0;
}
