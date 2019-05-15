utool
============


- build third party dependencies

```bash
cd ${workspace}
bash build-third-party.bash
```

- build utool

```bash
cd ${workspace}
rm -rf build && mkdir -p build
cd build
cmake ..
make && make install
```

- run console script
```sh
cd ${workspace}/build
./utool-bin --help
```


after make install:
> TODO, where to install? directly install to system path?
- a utool-bin console script will be installed under `${workspace}/bin`
- utool lib so files will be installed to `${CMAKE_INSTALL_PREFIX}/lib`


### reference
   
- {permissive} log: [curl](https://github.com/curl/curl) 
- {MIT} log: [cJSON](https://github.com/DaveGamble/cJSON) 
- {MIT} log: [zf_log](https://github.com/wonder-mice/zf_log) 
