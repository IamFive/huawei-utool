#if defined(_WIN32) || defined(_WIN64)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include "zf_log.h"


FILE *utool_g_log_file_fp = NULL;

static void file_output_callback(const zf_log_message *msg, void *arg)
{
    (void) arg;
    *msg->p = '\n';
    fwrite(msg->buf, msg->p - msg->buf + 1, 1, utool_g_log_file_fp);
    fflush(utool_g_log_file_fp);
}

static void file_output_close(void)
{
    ZF_LOGI("Application exit, close log file now.");
    fclose(utool_g_log_file_fp);
}

void utool_set_log_to_file(const char *const log_file_path)
{
    if (!utool_g_log_file_fp) {
        utool_g_log_file_fp = fopen(log_file_path, "a");
        if (!utool_g_log_file_fp) {
            ZF_LOGW("Failed to open log file %s", log_file_path);
            return;
        }
        atexit(file_output_close);
        zf_log_set_output_v(ZF_LOG_PUT_STD, 0, file_output_callback);
        ZF_LOGI("Log to file %s initialize succeed.", log_file_path);
    }
}

//int main(int argc, char *argv[])
//{
//    utool_set_log_to_file("example.log");
//
//    ZF_LOGI("Writing number of arguments to log file: %i", argc);
//    ZF_LOGI_MEM(argv, argc * sizeof(*argv), "argv pointers:");
//
//    return 0;
//}
