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
#include <time.h>
#include "zf_log.h"
#include <sys/stat.h>
#include <string.h>
#include <securec.h>
#include <dirent.h>
#include "string_utils.h"

#define MAX_ROTATION_FILE_COUNT 10
#define MAX_ROTATION_FILE_NAME_LEN 64
#define MAX_ROTATION_FILE_SIZE 50 * 1024 * 1024


FILE *g_UtoolLogFileFP = NULL;

bool RotationLogFile(const char *log_file_path);


bool RemoveOldestLogFiles();

static void LogToFileOutputCallback(const zf_log_message *msg, void *arg)
{
    (void) arg;
    *msg->p = '\n';
    size_t num = fwrite(msg->buf, msg->p - msg->buf + 1, 1, g_UtoolLogFileFP);
    if (num != 1) {
        perror("exception occurs when write log file.");
    }
    fflush(g_UtoolLogFileFP);
}

static void UpdateAllLogBakFilePerm()
{
    int ret;
    DIR *d = opendir("."); // open the path
    if (d == NULL) {
        perror("Failed to open current workspace dir.");
        return;
    }

    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        if (dir->d_type != DT_DIR && UtoolStringStartsWith(dir->d_name, BAK_LOG_FILE_NAME)) {
            ret = chmod(dir->d_name, 0440);
            if (ret == -1) {
                perror("Failed to update rotation log file mod to 0400.");
                goto DONE;
            }
        }
    }

DONE:
    ret = closedir(d);
    if (ret == -1) {
        perror("Failed to call closedir on utool root.");
    }
}

static void CloseLogFileOutput(void)
{
    UpdateAllLogBakFilePerm();
    ZF_LOGI("Application exit, close log file now.");
    fclose(g_UtoolLogFileFP);
    g_UtoolLogFileFP = NULL;
}

static long long GetFileSize(int fd)
{
    struct stat sb;
    int res = fstat(fd, &sb);
    if (res != 0) {
        perror("Failed to get log file size.");
        exit(EXIT_FAILURE);
    }
    return sb.st_size;
}

int UtoolSetLogFilePath(const char *const log_file_path)
{
    if (!g_UtoolLogFileFP) {
        char realFilepath[PATH_MAX] = {0};
        bool pathOk = UtoolIsParentPathExists(log_file_path);
        if (pathOk) {
            // check whether file exists
            UtoolFileRealpath(log_file_path, realFilepath, PATH_MAX);
            g_UtoolLogFileFP = fopen(realFilepath, "a");
            if (!g_UtoolLogFileFP) {
                ZF_LOGW("Failed to open log file %s", log_file_path);
                return 1;
            }

            unsigned long long logFileSize = GetFileSize(fileno(g_UtoolLogFileFP));
            if (logFileSize >= MAX_ROTATION_FILE_SIZE) {
                fclose(g_UtoolLogFileFP);
                bool success = RotationLogFile(log_file_path);
                if (!success) {
                    return 0;
                }

                RemoveOldestLogFiles();
                g_UtoolLogFileFP = fopen(realFilepath, "a");
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


bool RemoveOldestLogFiles()
{
    errno_t ok;
    DIR *d = opendir("."); // open the path
    if (d == NULL) {
        perror("Failed to open current workspace dir.");
        return false;
    }

    char rotationFileNames[64][MAX_ROTATION_FILE_NAME_LEN];
    int rotationFileCount = 0;
    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        if (dir->d_type != DT_DIR && UtoolStringStartsWith(dir->d_name, LOG_FILE_NAME)) {
            size_t rotationFileNameLen = strnlen(dir->d_name, MAX_ROTATION_FILE_NAME_LEN - 1);
            ok = strncpy_s(rotationFileNames[rotationFileCount++], MAX_ROTATION_FILE_NAME_LEN, dir->d_name, rotationFileNameLen);
            if (ok != EOK) {
                perror("Failed to remove rotation log files, reason: strncpy_s failed.");
                closedir(d);
                return false;
            }
        }
    }
    closedir(d);

    if (rotationFileCount > MAX_ROTATION_FILE_COUNT) {
        qsort(rotationFileNames, rotationFileCount, sizeof rotationFileNames[0], strcmp);
        for (int idx = 0; idx < rotationFileCount - MAX_ROTATION_FILE_COUNT; idx++) {
            // remove oldest rotation files
            if (remove(rotationFileNames[idx]) != 0) {
                perror("Failed to remove rotation log files");
                return false;
            }
        }
    }

    return true;
}

bool RotationLogFile(const char *log_file_path)
{
    char rotationFileName[PATH_MAX];
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    if (tm_now == NULL) {
        perror("Can not rotation log file");
        return false;
    }
    UtoolWrapSecFmt(rotationFileName, PATH_MAX, PATH_MAX - 1, "%s.%d%02d%02d%02d%02d%02d",
                    log_file_path, tm_now->tm_year + 1900, tm_now->tm_mon + 1,
                    tm_now->tm_mday, tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
    int ret = rename(log_file_path, rotationFileName);
    if (!ret) {
        perror("Failed to rename rotation log file");
        return false;
    }
    return true;
}
