utool
============

# HUAWEI utool

HUAWEI utool is a C99 client library that manges HUAWEI servers using iBMC REST APIs.


## pre-requirements

utool is based on C99 and built with cmake. So, the compile ENV should have:
- git
- CMake>=3.5.0
- C Compiler
- OpenSSL(required by libcurl)
- ipmitool

## Compilation

### Get source code

please make sure git is ready in your ENV. Clone process may cause a long time according to your network situation.

```bash
cd ${your-workspace}
git clone https://github.com/IamFive/huawei-utool.git utool
cd utool
```

### Build curl
The CURL version required is >=7.56.0, the `third-party/curl` provide along with source code is `curl-7_65_0`.
Please make sure `libssl-dev` and `libssh2-dev` is ready before making CURL (the dev package name maybe different in
your ENV).

1. To install libssl-dev

- ubuntu: `sudo apt install libssl-dev`
- RedHat: `sudo yum install openssl-devel`

1. To install libssh2-dev

- ubuntu: `sudo apt install libssh2-*-dev`
- RedHat: `sudo yum install libssh2-devel`


1. Build CURL

```bash
cd ${your-workspace}/utool
cd third-party/curl
./buildconf
./configure --with-ssl --with-libssh2
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


### Install utool

After make done, run `sudo make install` to install utool to current system.

```bash
cd ${workspace}/utool/build
sudo make install
```

Remember, before using `utool`, you should make sure libcurl>=7.56.0 has been installed. 
You can install libcurl through package manager like `apt-get`, `yum` or through:

```bash
cd ${your-workspace}/utool/third-party/curl
./buildconf
./configure --with-ssl --with-libssh2
make 
sudo make install
```

After install,

- `utool` bin script will be install to `/usr/local/bin`
- `utool` bin script will be install to `${your-workspace}/bin`
- `utool` lib will be install to `/usr/local/lib`
- `utool` lib will be install to `${your-workspace}/lib`
- `utool` static lib will be install to `/usr/local/lib`
- `utool` static lib will be install to `${your-workspace}/lib`

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
