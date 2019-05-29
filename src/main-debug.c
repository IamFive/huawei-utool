#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <utool.h>
#include <constants.h>
#include <cJSON.h>
#include <typedefs.h>
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
            "-P", "chajian43@1",
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
            //"getnic",
            //"getservice",
            //"geteventsub",
            //"getpwrcap",
            "getmgmtport",
            //"getuser",
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
            //"-u", "/data/nfs/cpldimage.hpm",
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

int debug(UtoolResult *result)
{
    result->data = NULL;
    result->code = 100;
    result->desc = (char *) malloc(100);
    strcpy(result->desc, "this is desc");
}


int main(int argc, const char **argv)
{
    int ret = 0;
    //debug(result);
    ret = run_utool_main();
    return ret;
}


