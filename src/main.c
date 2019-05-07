#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>
#include <curl/curl.h>
#include <ucw/lib.h>
#include <ucw/opt.h>


static struct opt_section options = {
        OPT_ITEMS {
                // More options can be specified here
                OPT_HELP("Configuration options:"),
                OPT_CONF_OPTIONS,
                OPT_END
        }
};

int main(int argc, char **argv) {
    printf("hello world");

    opt_parse(&options, argv + 1);

    return 0;
}