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




### 

固件升级自动重启 BMC 会有未知问题，不稳定。比如当前BMC正在升级另外一个固件，强行重启。

{
    "@odata.context": "/redfish/v1/$metadata#TaskService/Tasks/Members/$entity",
    "@odata.type": "#Task.v1_0_2.Task",
    "@odata.id": "/redfish/v1/TaskService/Tasks/1",
    "Id": "1",
    "Name": "Upgarde Task",
    "TaskState": "Running",
    "StartTime": "2019-06-03T12:07:36+08:00",
    "Messages": {
        "@odata.type": "/redfish/v1/$metadata#MessageRegistry.1.0.0.MessageRegistry",
        "MessageId": "iBMC.1.0.FirmwareUpgradeComponent",
        "RelatedProperties": [],
        "Message": "Upgrading the BMC.",
        "MessageArgs": [
            "BMC"
        ],
        "Severity": "OK",
        "Resolution": "Wait until the upgrade is complete."
    },
    "Oem": {
        "Huawei": {
            "TaskPercentage": "7%"
        }
    }
}

{
    "@odata.context": "/redfish/v1/$metadata#TaskService/Tasks/Members/$entity",
    "@odata.type": "#Task.v1_0_2.Task",
    "@odata.id": "/redfish/v1/TaskService/Tasks/1",
    "Id": "1",
    "Name": "Upgarde Task",
    "TaskState": "Running",
    "StartTime": "2019-06-03T12:07:36+08:00",
    "Messages": {
        "@odata.type": "/redfish/v1/$metadata#MessageRegistry.1.0.0.MessageRegistry",
        "MessageId": "iBMC.1.0.FirmwareUpgradeComponent",
        "RelatedProperties": [],
        "Message": "Upgrading the BMC.",
        "MessageArgs": [
            "BMC"
        ],
        "Severity": "OK",
        "Resolution": "Wait until the upgrade is complete."
    },
    "Oem": {
        "Huawei": {
            "TaskPercentage": "75%"
        }
    }
}

{
    "@odata.context": "/redfish/v1/$metadata#TaskService/Tasks/Members/$entity",
    "@odata.type": "#Task.v1_0_2.Task",
    "@odata.id": "/redfish/v1/TaskService/Tasks/1",
    "Id": "1",
    "Name": "Upgarde Task",
    "TaskState": "Running",
    "StartTime": "1970-01-01T08:03:54+08:00",
    "Messages": {
        "@odata.type": "/redfish/v1/$metadata#MessageRegistry.1.0.0.MessageRegistry",
        "MessageId": "iBMC.1.0.FileTransferProgress",
        "RelatedProperties": [],
        "Message": "Downloading the upgrade package ... Progress: 4% complete.",
        "MessageArgs": [
            "4%"
        ],
        "Severity": "OK",
        "Resolution": "Wait until the upgrade package is downloaded. The upgrade starts after the upgrade package is downloaded."
    },
    "Oem": {
        "Huawei": {
            "TaskPercentage": null
        }
    }
}

{
    "@odata.context": "/redfish/v1/$metadata#TaskService/Tasks/Members/$entity",
    "@odata.type": "#Task.v1_0_2.Task",
    "@odata.id": "/redfish/v1/TaskService/Tasks/1",
    "Id": "1",
    "Name": "Upgarde Task",
    "TaskState": "Running",
    "StartTime": "1970-01-01T08:03:54+08:00",
    "Messages": {
        "@odata.type": "/redfish/v1/$metadata#MessageRegistry.1.0.0.MessageRegistry",
        "MessageId": "iBMC.1.0.FirmwareUpgradeComponent",
        "RelatedProperties": [],
        "Message": "Upgrading the BMC.",
        "MessageArgs": [
            "BMC"
        ],
        "Severity": "OK",
        "Resolution": "Wait until the upgrade is complete."
    },
    "Oem": {
        "Huawei": {
            "TaskPercentage": "18%"
        }
    }
}


{
    "@odata.context": "/redfish/v1/$metadata#TaskService/Tasks/Members/$entity",
    "@odata.type": "#Task.v1_0_2.Task",
    "@odata.id": "/redfish/v1/TaskService/Tasks/1",
    "Id": "1",
    "Name": "Upgarde Task",
    "TaskState": "Running",
    "StartTime": "2019-06-03T14:52:32+08:00",
    "Messages": {
        "@odata.type": "/redfish/v1/$metadata#MessageRegistry.1.0.0.MessageRegistry",
        "MessageId": "iBMC.1.0.FirmwareUpgradeComponent",
        "RelatedProperties": [],
        "Message": "Upgrading the Motherboard CPLD.",
        "MessageArgs": [
            "Motherboard CPLD"
        ],
        "Severity": "OK",
        "Resolution": "Wait until the upgrade is complete."
    },
    "Oem": {
        "Huawei": {
            "TaskPercentage": "3%"
        }
    }
}

###
不在位状态的过滤
重新封装支撑 getraid 那种模式？自动多层加载。


### getip

- IpMode 也是必填项？

"Message":	["[Warning] A file transfer task is being performed or an upgrade operation is in progress. Resolution: Wait until the upgrade is complete."]

/tmp/web/xxx

// http upload file

0. // 打印正在重启 (重启限时?)
1. 已启动
1. 前后的版本号
2. 等到重启后再返回.


BIOS：dcpowercycle
CPLD：dcpowercycle
iBMC：automatic
PSU：automatic
FAN：automatic
FW：none
Driver：none


3次失败 => 重启 => 再试3次
2次失败 => 重启 => 再试1次

1. mapping  message args => firmware url (task message arg -> firmware) 需要永波提供映射表

2. 失败 自动重启? 什么条件?  => 不管什么原因, 失败了就重启
3. 升级已"完成", 但是获取版本号接口失败.  => 继续往下执行,版本号显示无法获取. // 如果升级BMC自动重启之后还获取不到,当做失败,
4. 升级完自动重启? 什么条件?              => 除BMC外不自动重启, 除BMC不获取生效后版本. (或者改成获取全部固件版本).
5. 自动重试包含哪些步骤,                  => 固件升级 Task 那一块, 其他步骤不包含在自动重试流程中.
6. 什么时候去获取升级前版本号?            => 在Task完成后, 去获取升级前版本号.



Motherboard CPLD
Backplane CPLD


ipmitool
===========

## window

由用户自行配置

## centos 

在 Utool 中打包携带 ipmitool 的二进制文件

## 集成功能

- 获取redfish 端口号
- send ipmi raw command ,  IPMI 参数
- getbiosresult 需要确认结果
- locatedisk -> 有接口


CPLD --- 版本

升级前 固件版本都打印
升级后 


configure: error: --with-ssl was given but OpenSSL could not be detected
mingw64-x86_64-gcc-core
-DCMAKE_SH="CMAKE_SH-NOTFOUND"
--host=x86_64-w64-mingw32

C:\Program Files\mingw-w64\x86_64-8.1.0-posix-seh-rt_v6-rev0\mingw64\opt\bin

 ./buildconf
mingw32-make.exe
 ./configure  --with-ssl --host=x86_64-w64-mingw32

 ./configure --host=x86_64-w64-mingw32 --with-zlib --with-winssl --without-ssl  --without-nghttp2 --without-libssh2 --without-gssapi --without-libidn2 --without-librtmp  --without-libpsl



