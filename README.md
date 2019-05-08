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

after make install:
- a utool-bin console script will be installed under `${workspace}/bin`
- a utool lib so will be generated under `${workspace}/lib`



### reference
   
- {permissive} log: [curl](https://github.com/curl/curl) 
- {MIT} log: [cJSON](https://github.com/DaveGamble/cJSON) 
- {MIT} log: [zf_log](https://github.com/wonder-mice/zf_log) 
