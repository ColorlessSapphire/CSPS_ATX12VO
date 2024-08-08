# CSPS-ATX12VO 转接板 & ATX12VO-DCATX 24Pin 转接板

已验证，带我的 i5-12400 和 2080Ti 可过烤机，满载 400W+  
使用的电源为华为 PAC750D1212-CE（额定功率750W）

---

**<span style="color:red">连接电脑通电前需测试线序正确，脱机测试电压输出正常（炸了不要找我）</span>**

---

## 已知问题

第二次开机需手动断电重置电源，可以试试把 DCATX 板上的 PSON 跳线（8 脚芯片旁一个 0 欧电阻）换一个位置，目前 PSON 信号经过 WT7510 转接，更换跳线位置后将直连 CSPS。

---

## 功能与支持

- 带有物联网功能，可查看电压、电流、温度、风扇转速等
- 支持小爱同学（未测试 [https://diandeng.tech/doc/xiaoai](https://diandeng.tech/doc/xiaoai)）
- 带有 WoL 功能，一键远程开机
- 物联网模块为 ESP32-C3-MINI-1U-N4
- DCATX 板使用的 DC-DC 为华为 NAK12S20-C（PW22ASAB）

## 接口

- 一个 CPU 8Pin 接口
- 两个显卡 8Pin 接口
- 一个 XT30 接口
- 一个 ATX12VO 接口（官方 10Pin 接口找不到卖的，用了一个通用 2\*5Pin 接口）接上 DCATX 板子转为 ATX 24Pin
- 无 SATA 供电

## 工程文件
- `PCB文件`可用立创 EDA 打开（*.epro)  
- `ESP32 程序`可用 VS Code（安装 Platform IO 扩展）打开（用 Arduino IDE 应该也行）


## 组件介绍

- **NAK12S20-C（PW22ASAB）**：华为 DC-DC 电源模块，小黄鱼拆机 4 块钱，额定输入 12V，输出 0.7~5.3V 可调，最大输出电流 20A
- **PAC750D1212-CE（EPW750-12 A）**：华为 750W 80PLUS 铂金认证 服务器 CSPS 电源，某宝 50 块（其他接口一样的也行，但可能要改程序里的 IIC 地址）
- **ESP32-C3-MINI-1U-N4**：乐鑫 ESP32-C3 无线模组
- **WT7510**：ATX 电源监控芯片，提供电压保护功能
- **PSON**：电源启动信号线，默认上拉，低电平有效，板子上有个拨码开关可以脱机启动电源，物联网界面长按开关按钮也可脱机启动电源
- **PSOK(PG)**：电源就绪信号线，电源启动且输出正常时，PSOK 输出高电平，连到板子上一个指示灯


## 物联网

- [下载 Blinker APP](https://diandeng.tech/dev)
- 新建设备，在 ESP32 的程序中填入设备 key，在 APP 设备页面 -> 界面配置中导入配置文件（./2. Software/Blinker/界面配置.txt）即可设置 UI
- **WoL**：支持无线网络唤醒电脑，需在主板 BIOS 内打开网络唤醒（WoL）功能，将电脑用网线连接到路由器（校园网不行）查看电脑 IP 地址和 MAC 地址（最好配置静态），把 IP 和 MAC 填入 ESP32 的程序中，在 Blinker 设备界面点击开关即可使用远程开机功能

---

## 参考工程

- [KCORES-CSPS-to-ATX-Converter](https://github.com/KCORES/KCORES-CSPS-to-ATX-Converter)
- [csps_esphome](https://github.com/hitsword/csps_esphome/tree/master)

## 许可证
<a href="https://github.com/ColorlessSapphire/CSPS_ATX12VO/blob/main/LICENSE">
  <img src="https://img.shields.io/github/license/ColorlessSapphire/CSPS_ATX12VO?color=2b9348" alt="License Badge"/>
</a>