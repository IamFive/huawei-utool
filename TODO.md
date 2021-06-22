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


[] 自定义 OEM 值，涉及到所有返回值的解析，部分请求URL的OEM替换，部分请求提交body的值格式。
[] 编译选项
[] utool -V 输出调整
[?] utool -t 主功能原来就没实现，这个是不是不用做。
[?] utool -q 是要屏蔽所有命令的console输出吗？
[?] utool -M 返回值格式样例
[?] getfw Name mapping，需要提供从华为的 fw Name 到腾讯的要求固定值的映射关系。
[?] getpcie Type mapping 需要提供从华为的 pcie type 到腾讯的要求固定值的映射关系。
[] adduser setpriv roleid 取值调整
[] delvncsession 返回成功在无session时。

- 3rd lib

[?] zf_log 无法使用，是否加入华为可用库列表。这个修改会影响到所有使用日志的地方。
[?] 其他库版本问题，升级后能否兼容未知，升级后依赖库版本能否支持未知。（比如curl库，他需要依赖libssl等，是有版本依赖的，能否在比较旧的centos62下运行未知）
[?] ipmitool 确认要移除吗？ (CMAKE调整，代码路径调整)


[?] CodeDex（377） & cleancode（2700), 多的离谱，看下是否能屏蔽部分，而且我记得上次已经改过CodeDex了，为撒又出来这么多


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



