/*
* Copyright © Huawei Technologies Co., Ltd. 2012-2018. All rights reserved.
* Description: main script
* Author:
* Create: 2019-06-16
* Notes:
*/
#include <stdio.h>
#include <stdlib.h>
#include <utool.h>
#include <curl/curl.h>

int main(int argc, char **argv)
{
    char *result = NULL;
    int ret = utool_main(argc, argv, &result);
    if (result != NULL) {
        fprintf(ret == 0 ? stdout : stderr, "%s\n", result);
        free(result);
    }
    return ret;
}
