#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <utool.h>
#include <constants.h>
#include <cJSON.h>
#include "zf_log.h"
#include "string_utils.h"
#include "url_parser.h"

int run_utool_main()
{
    char *result = NULL;
    char *argv2[] = {
            "utool",
            //"--version",
            "-H", "112.93.129.9",
            "-U", "qianbiao",
            "-P", "*********",
            //"getproduct",
            //"getfw",
            //"getcpu",
            //"getmemory",
            //"getip",
            //"gettemp",
            //"getvolt",
            //"getpsu",
            //"getfan",
            //"getpdisk",
            //"getldisk",
            "getnic",
            //"gettaskstate",
            //"getsysboot",
            //"getsensor",
            //"getbios",
            //"getbiossetting",
            //"gethealthevent",
            //"adduser", "-n", "utool", "-p", "chajian12#$", "-r", "Administrator2",
            //"setpwd", "-n", "utool", "-p", "chajian12#$5",
            //"deluser", "-n", "utool",
            //"upload",
            //"download",
            //"fwupdate",
            //"-e", "Auto", "-t", "BMC",
            //"-u", "/data/nfs/ubuntu-iso.zip",
            //"-u", "nfs://112.93.129.100/data/nfs/cpldimage.hpm",
            NULL
    };

    int argc2 = sizeof(argv2) / sizeof(char *);
    int ret = utool_main(argc2 - 1, (void *) argv2, &result);
    if (result != NULL) {
        fprintf(ret == 0 ? stdout : stderr, "%s", result);
        free(result);
    }
    return ret;
}

int debug()
{
    //char *source = "this is a very long sentance.";
    //char *ori = malloc(100);
    //memset(ori, 0, 100);
    //memccpy(ori, source, sizeof(char), strlen(source));
    //
    //printf("%s\n", ori);
    //char *token = strtok(ori, " ");
    //token = strtok(NULL, " ");
    //printf("token: %s\n", token);
    //
    //free(token);
}


int main(int argc, const char **argv)
{
    int ret = 0;
    //debug();
    ret = run_utool_main();
    return ret;
}


