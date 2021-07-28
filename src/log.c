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
#include <commons.h>
#include <fcntl.h>
#include <ftw.h>
#include "zf_log.h"
#include <sys/stat.h>
#include <string.h>

#define TEN_M 10 * 1024 * 1024


FILE *g_UtoolLogFileFP = NULL;

static void LogToFileOutputCallback(const zf_log_message *msg, void *arg)
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
        bool pathOk = UtoolIsParentPathExists(log_file_path);
        if (pathOk) {
            // check whether file exists
            int old_umask = umask(S_IXUSR | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);
            UtoolFileRealpath(log_file_path, realFilepath);
            g_UtoolLogFileFP = fopen(realFilepath, "a");
            umask(old_umask);
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
