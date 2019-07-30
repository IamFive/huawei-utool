Build utool with MinGW64 under window10
====

## Install compile toolchain

### Install MSYS2

- install MSYS2-x86_64 following http://www.msys2.org/
- install location should be `d:/tools/msys2`
- update pacman package source mirror if neccessary (https://wiki.archlinux.org/index.php/Mirrors_(%E7%AE%80%E4%BD%93%E4%B8%AD%E6%96%87).

### Install MinGW toolchain

- Update package database and core system packages with

```bash
pacman -Sy pacman
pacman -Syu
pacman -Su
```

- install auto tools and cmake

```bash
pacman -S cmake automake autoconf libtool git
```


- install MinGW64 toolchain

```
pacman -S  mingw-w64-i686-toolchain
pacman -S  mingw-w64-x86_64-toolchain
```

> `1`,`3`,`13` is required


## Compile

### ENV

Open `D:\tools\msys2\mingw64.exe` as administrator.
The whole compile progress is under MinGW64 ENV, and please make sure it's run as administrator.

### download source code from github

```bash
cd ${your-workspace}
git clone https://github.com/IamFive/huawei-utool.git huawei-utool
cd huawei-utool
```

### build CURL

```bash
cd ${your-workspace}/huawei-utool
cd third-party/curl
./buildconf
./configure --host=x86_64-w64-mingw32 --with-zlib --with-winssl --without-ssl --without-axtls --without-nghttp2 --without-libssh2 --without-gssapi --without-libidn2 --without-librtmp  --without-libpsl
mingw32-make
 ```


### build Utool

Using cmake to build `utool`, after `make install`, `utool` bin script will be install to `${your-workspace}/bin`.

```bash
cd ${workspace}/huawei-utool
rm -rf build && mkdir -p build && cd build
cmake -G "MinGW Makefiles" -DCMAKE_SH=CMAKE_SH-NOTFOUND ..
mingw32-make
mingw32-make install
```

