utool
============

# HUAWEI utool

HUAWEI utool is a C99 client library that manges HUAWEI servers using iBMC REST APIs.


## pre-requirements

utool is based on C99 and built with cmake. So, the compile ENV should have:
- git
- CMake>=3.5.0
- C Compiler
- OpenSSL(used by libcurl)

## Compilation

### Get source code

please make sure git is ready in your ENV. Clone process may cause a long time according to your network situation.

```bash
cd ${your-workspace}
git clone https://github.com/IamFive/huawei-utool.git utool
cd utool
git submodule update --init
```

### Build curl
The CURL version required is >=curl-7_65_0.
Please make sure `libssl-dev` is ready before making CURL. 

- ubuntu: `sudo apt install libssl-dev`
- RedHat: `sudo yum install openssl-dev`

```bash
cd ${your-workspace}/utool
cd third-party/curl
./buildconf
./configure --with-ssl
make
```


### Build Utool

Using cmake to build `utool`, after make `utool-bin` script will be generated under build folder.

```bash
cd ${workspace}/utool
rm -rf build && mkdir -p build
cd build
cmake .. && make
```

### Try Utool

```sh
$ cd ${workspace}/utool/build
$ ./utool-bin --version
{
	"State":	"Success",
	"Message":	["HUAWEI server management command-line tool version v1.0.2"]
}
```


## C99 Example

```C
#include <utool.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    char *result = NULL;
    char *argv2[] = {
            "utool",
            "-H", "your-ibmc-host",
            "-U", "your-ibmc-username",
            "-P", "your-ibmc-password",
            "getproduct",
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
```

or 

```C
#include <stdio.h>

extern int utool_main(int argc, char *argv[], char **result);

int main(int argc, char **argv)
{
    char *result = NULL;
    char *argv2[] = {
            "utool",
            "-H", "your-ibmc-host",
            "-U", "your-ibmc-username",
            "-P", "your-ibmc-password",
            "getproduct",
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
```

## reference
   
- {permissive} curl: [curl](https://github.com/curl/curl) 
- {MIT} cJSON: [cJSON](https://github.com/DaveGamble/cJSON) 
- {MIT} zf_log: [zf_log](https://github.com/wonder-mice/zf_log) 
- {MIT} argparse: [argparse](https://github.com/cofyc/argparse.git) 
