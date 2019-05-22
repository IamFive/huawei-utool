Things to do
------------

- [x] multiple thread support
    - zf log file init
    - curl global init
- [x] CURL make request (post/patch/etc)
- [x] cJSON output build mapping strategy
- [] atexit clean up resources when program exit


### getproduct
- TotalPowerWatts
PowerControl.PowerConsumedWatts

### getfw
- SupportActivateType  -- ["automatic"]
- Type          -- 通过name匹配

### adduser
- privilege  // 不支持暂时

### getip
all  -- 待确认

### getfan
- Model                     null    
- RatedSpeedRPM             MaxReadingRange
- SpeedRPM                  Reading
- LowerThresholdRPM         MinReadingRange

### getpdisk
- Location  Oem/Huawei/Position
- Volumes       提取
- SASAddress    返回数组
- SpareforLogicalDrives 原样返回
- CapacityGiB   557.861328125,

### fwupdate
- 异步方式实现

