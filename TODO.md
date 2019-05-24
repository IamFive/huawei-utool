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

似乎搞错了？最新需求提到的，似乎是所有输入之和。

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


### gettaskstatus

"TaskId": "0",
"TaskType": "upgrade",          ??
"TaskDesc": "BMC upgrade",
"State": "notstart",            ??
"EstimatedTimeSenconds": 300,   ??
"Trigger": ["automatic"]        ??

"TaskId": "0",
"TaskType": "upgrade",
"TaskDesc": "BMC upgrade",
"State": "notstart",
"EstimatedTimeSenconds": 300,
"Trigger": ["automatic"]


### get fru
哪个接口 -- system? 

### cpu view/memory view

使用新接口，版本配套由华为和腾讯协商
/redfish/v1/Systems/1/ProcessorView
/redfish/v1/Systems/1/MemoryView


### get sensor

Maximum": 180  ==> count sensor 
SensorNumber  ===> 返回index

https://112.93.129.9/redfish/v1/Chassis/1/ThresholdSensors
https://112.93.129.9/redfish/v1/Chassis/1/DiscreteSensors

unr ==> UpperThresholdFatal



划掉的是不做吗
addeventsub
deleventsub


### getbiosresult ?

没找到对应的接口。

### gethealthevent
- Entity ==> EventSubject
- Id  ==> 是否使用递增


### getcpu
"TotalPowerWatts":356, mapping
https://112.93.129.9/redfish/v1/Chassis/1/Power ?? PowerControl/Oem/Huawei/PowerMetricsExtended/CurrentCPUPowerWatts


### getldisk
OverallHealth 
Maximum ==> count ?
RedundantType

### getnic
- Serialnumber

- Controller  重复属性？
"Id": 0,
"Manufacturer": "intel",
"Model": "X710",
"Serialnumber": "xxxxxx",
"FirmwareVersion": "1.5",
"PortCount": 2,