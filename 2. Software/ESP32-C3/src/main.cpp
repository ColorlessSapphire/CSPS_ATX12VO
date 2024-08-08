/*
物联网CSPS电源监控
微控制器使用ESP32-C3
CSPS电源使用 HUAWEI PAC750D1212-CE 80PLUS铂金 750W 服务器电源（EPW750-12 A）
*/

#define BLINKER_WIFI
#define BLINKER_MIOT_OUTLET

#include <Blinker.h>
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "CSPS.hpp"

#define APP_VERSION "R 1.0.3" // 固件版本
#define EEPROM_ADDR 0x50      // 电源ROM i2c地址
#define MCU_ADDR 0x58         // 电源PMBus(i2c)地址
#define CSPS_PSOK GPIO_NUM_7  // PSOK信号线
#define CSPS_PSON GPIO_NUM_10 // PSON信号线
#define ADD0 GPIO_NUM_4       // i2c地址选择信号线
#define ADD1 GPIO_NUM_5       // i2c地址选择信号线
#define ADD2 GPIO_NUM_6       // i2c地址选择信号线
#define LED GPIO_NUM_3        // ESP32工作指示灯

const char *AUTH = ""; // Blinker 设备密钥
const char *SSID = ""; // Wi-Fi SSID
const char *PSWD = ""; // Wi-Fi 密码

const char *OTA_HOSTNAME = "PAC750"; // Arduino OTA主机名
// const char *OTA_PASSWORD = "";    // OTA密码
const char *OTA_PASSWORD_MD5 = ""; // OTA密码MD5

#define BROADCAST_ADD "192.168.1.255" // 广播地址
#define REMOTEPORT 9                  // 网卡唤醒端口
#define LOCALPORT 9                   // 本地UDP端口
#define timezone 8                    // 时区

const IPAddress remote_ip(192, 168, 1, 2);                  // PC端的IP地址
char NETCARD_MAC[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // PC网卡的MAC地址
char WOL_Buffer[102];                                       // WOL包
unsigned long long mJoule = 0;                              // 累计能量（mJ）
float kWh = 0;                                              // 累计能量（kWh）
unsigned long currentRoutineMillis = 0;                     // 当前时间（ms）
unsigned long prevRoutineMillis = 0;                        // 之前时间（ms）
int millisRoutineInterval = 100;                            // 刷新间隔（ms）用于耗电累计
unsigned long long runtime = 0;                             // 运行时间（ms）
float runhours = 0;                                         // 运行时间（h）
unsigned long prevRuntimeMillis = 0;                        // 当前时间（ms）
unsigned long currentRuntimeMillis = 0;                     // 之前时间（ms）用于运行时间累计

// 新建Blinker组件对象
BlinkerButton SWITCH("SWITCH"); // 电源开关

BlinkerNumber OutputVoltage("OutputVoltage"); // 输出电压
BlinkerNumber OutputCurrent("OutputCurrent"); // 输出电流
BlinkerNumber OutputPower("OutputPower");     // 输出功率
BlinkerNumber InputVoltage("InputVoltage");   // 输入电压
BlinkerNumber InputCurrent("InputCurrent");   // 输入电流
BlinkerNumber InputPower("InputPower");       // 输入功率
BlinkerNumber Efficiency("Efficiency");       // 转换效率
BlinkerNumber Temperature1("Temperature1");   // 进风温度
BlinkerNumber Temperature2("Temperature2");   // 出风温度
BlinkerNumber RunTime("RunTime");             // 运行时间
BlinkerNumber Energy("Energy");               // 累计电量

BlinkerSlider FanRPM("FanRPM"); // 风扇转速

BlinkerText APPVersion("APPVersion"); // APP版本
BlinkerText SPN("SPN");               // 备件编号
BlinkerText DeviceName("DeviceName"); // 设备名称
BlinkerText MFG("MFG");               // 生产日期
BlinkerText State("State");           // 状态
BlinkerText MFR("MFR");               // 生产厂家
BlinkerText CT("CT");                 // Product Part/Model Number - Sequence for Supplier - Year of production - Serial Number

CSPS EPW750(MCU_ADDR, EEPROM_ADDR, true); // 电源对象
Preferences preferences;                  // 电源参数存储对象
WiFiUDP Udp;                              // UDP对象

bool oState = false; // 开关状态（米家）
bool Alarm = true;   // 报警状态

// 网络唤醒
void WoL()
{
  int i = 0;
  // 填充唤醒魔包
  for (i = 0; i < 6; i++)
  {
    WOL_Buffer[i] = 0xFF;
  }
  for (i = 6; i < 102; i += 6)
  {
    memcpy(WOL_Buffer + i, NETCARD_MAC, 6);
  }
  Udp.beginPacket(BROADCAST_ADD, 3333);
  Udp.write((byte *)WOL_Buffer, 102);
  Udp.endPacket();
  return;
}

// 计算效率
float effi()
{
  if (EPW750.getInputPower() == 0)
    return 0;
  float effi = (EPW750.getOutputPower() / EPW750.getInputPower()) * 100.0;
  if (effi > 100)
    effi = (float)100.0;
  return effi;
}

// 心跳包
void Heartbeat()
{
  APPVersion.print(APP_VERSION);
  SPN.print(EPW750.getSPN());
  DeviceName.print(EPW750.getName());
  MFG.print(EPW750.getMFG());
  MFR.print(EPW750.getMFR());
  CT.print(EPW750.getCT());

  if (digitalRead(CSPS_PSON) == HIGH)
  {
    State.color("#faa510");         // 黄色
    State.icon("fad fa-power-off"); // 待机
    State.print("待机");
    SWITCH.color("#faa510"); // 黄色
    SWITCH.print();
    return;
  }

  switch (EPW750.getState())
  {
  case 0b00000000:
    if (Alarm)
    {
      State.color("#00a90b");            // 绿色
      State.icon("fad fa-shield-check"); // 勾号
      State.print("正常运行");
      SWITCH.color("#00a90b"); // 绿色
      SWITCH.print();

      Alarm = false;
    }
    break;
  case 0b00000001:
    if (!Alarm)
    {
      State.color("#9b0604");   // 红色
      State.icon("fad fa-ban"); // 禁止
      State.print("电源失效");
      Blinker.notify("电源失效!");                                                                  // 电源失效
      Blinker.wechat("Title: 电源状态告警", "State: 已失效", "Message: 电源发生故障，12V输出关闭"); // 微信推送
      Alarm = true;
    }
    break;
  case 0b00000010:
    if (!Alarm)
    {
      State.color("#9b0604");                    // 红色
      State.icon("fad fa-exclamation-triangle"); // 三角感叹号
      State.print("过流保护");
      Blinker.notify("过流保护!");                                                                        // 过流保护
      Blinker.wechat("Title: 电源状态告警", "State: 过流保护", "Message: 电流过大触发保护，12V输出关闭"); // 微信推送
      Alarm = true;
    }
    break;
  case 0b00000100:
    if (!Alarm)
    {
      State.color("#9b0604");                    // 红色
      State.icon("fad fa-exclamation-triangle"); // 三角感叹号
      State.print("过压保护");
      Blinker.notify("过压保护!");                                                                        // 过压保护
      Blinker.wechat("Title: 电源状态告警", "State: 过压保护", "Message: 电压过大触发保护，12V输出关闭"); // 微信推送
      Alarm = true;
    }
    break;
  case 0b00001000:
    if (!Alarm)
    {
      State.color("#9b0604");                    // 红色
      State.icon("fad fa-exclamation-triangle"); // 三角感叹号
      State.print("过温保护");
      Blinker.notify("过温保护!");                                                                        // 过温保护
      Blinker.wechat("Title: 电源状态告警", "State: 过温保护", "Message: 温度过高触发保护，12V输出关闭"); // 微信推送
      Alarm = true;
    }
    break;
  case 0b01000000:
    if (!Alarm)
    {
      State.color("#9b0604");                    // 红色
      State.icon("fad fa-exclamation-triangle"); // 三角感叹号
      State.print("风扇故障");
      Blinker.notify("风扇故障!");                                                                // 风扇故障
      Blinker.wechat("Title: 电源状态告警", "State: 风扇故障", "Message: 风扇故障，12V输出关闭"); // 微信推送
      Alarm = true;
    }
    break;
  case 0b00010000:
    if (!Alarm)
    {
      State.color("#9b0604");      // 红色
      State.icon("fad fa-unlink"); // 断开连接
      State.print("无输入");
      Blinker.notify("无交流输入!"); // 无输入
      Alarm = true;
    }
    break;
  default:
    if (!Alarm)
    {
      State.color("#9b0604");               // 红色
      State.icon("fad fa-question-circle"); // 问号
      State.print("未知故障");
      Blinker.notify("未知故障!");                                                                        // 未知故障
      Blinker.wechat("Title: 电源状态告警", "State: 未知故障", "Message: 电源发生未知故障，12V输出关闭"); // 微信推送
      Alarm = true;
    }
    break;
  }
}

// 实时数据
void rtData()
{
  // 电压电流
  Blinker.sendRtData("OutputVoltage", EPW750.getOutputVoltage());
  Blinker.sendRtData("OutputCurrent", EPW750.getOutputCurrent());
  Blinker.sendRtData("InputVoltage", EPW750.getInputVoltage());
  Blinker.sendRtData("InputCurrent", EPW750.getInputCurrent());

  // 功率
  Blinker.sendRtData("OutputPower", EPW750.getOutputPower());
  Blinker.sendRtData("InputPower", EPW750.getInputPower());
  Blinker.printRtData();

  // 转换效率
  Blinker.sendRtData("Efficiency", effi());

  // 温度
  Blinker.sendRtData("Temperature1", EPW750.getTemp1());

  Blinker.sendRtData("RunTime", runhours);
  Blinker.sendRtData("FanRPM", EPW750.getFanRPM());
  Blinker.sendRtData("Energy", kWh);

  Blinker.printRtData();
}

// 数据存储
void dataStorage()
{
  Blinker.dataStorage("OutputPower", EPW750.getOutputPower());
  Blinker.dataStorage("Efficiency", effi());
  Blinker.dataStorage("Temperature1", EPW750.getTemp1());
}

// 如果未绑定的组件被触发，则会执行其中内容
void dataRead(const String &data)
{
  if (data == "reboot")
  {
    ESP.restart();
  }
  else if (data == "clear")
  {
    preferences.begin("EPW750", false);
    preferences.putULong64("Joule", 0);
    preferences.end();
    mJoule = 0;
    kWh = 0;
  }
  else if (data == "reset")
  {
    preferences.begin("EPW750", false);
    preferences.putULong64("runtime", 0);
    preferences.end();
    runtime = 0;
    runhours = 0;
  }
  else if (data == "resetmiot")
  {
    oState = false;
  }
}

// 电源开关
void PowerSwitch(const String &state)
{
  if (state == BLINKER_CMD_BUTTON_TAP)
  {
    Blinker.notify("唤醒中..."); // 开启中请稍等
    WoL();                       // 网络唤醒电脑
  }
  else if (state == BLINKER_CMD_BUTTON_PRESSED)
  {

    if (digitalRead(CSPS_PSON) == LOW)
    {
      Blinker.notify("关闭中...");   // 关闭中请稍等
      digitalWrite(CSPS_PSON, HIGH); // 电源待机
      SWITCH.color("#faa510");       // 黄色
      SWITCH.print();
      State.color("#faa510");         // 黄色
      State.icon("fad fa-power-off"); // 待机
      State.print("待机");
    }
    else
    {
      Blinker.notify("开启中...");  // 开启中请稍等
      digitalWrite(CSPS_PSON, LOW); // 开启电源
      SWITCH.color("#00a90b");      // 绿色
      SWITCH.print();
      State.color("#00a90b");            // 绿色
      State.icon("fad fa-shield-check"); // 勾号
      State.print("正常运行");
    }
  }
  else
  {
    BLINKER_LOG("invalid command");
  }
}

// 风扇转速
void SetFanRPM(int32_t value)
{
  EPW750.setFanRPM(value);
  return;
}

// 米家IoT电源开关
void miotPowerState(const String &state)
{
  if (state == BLINKER_CMD_ON)
  {
    BlinkerMIOT.powerState("on");
    BlinkerMIOT.print();

    Blinker.notify("Opening... Plase wait."); // 开启中请稍等
    WoL();
    oState = false;
  }
  else if (state == BLINKER_CMD_OFF)
  {

    BlinkerMIOT.powerState("off");
    BlinkerMIOT.print();
    oState = false;
  }
  return;
}

void miotQuery(int32_t queryCode)
{
  BLINKER_LOG("MIOT Query codes: ", queryCode);

  switch (queryCode)
  {
  case BLINKER_CMD_QUERY_ALL_NUMBER:
    BLINKER_LOG("MIOT Query All");
    BlinkerMIOT.powerState(oState ? "on" : "off");
    BlinkerMIOT.print();
    break;
  case BLINKER_CMD_QUERY_POWERSTATE_NUMBER:
    BLINKER_LOG("MIOT Query Power State");
    BlinkerMIOT.powerState(oState ? "on" : "off");
    BlinkerMIOT.print();
    break;
  default:
    BlinkerMIOT.powerState(oState ? "on" : "off");
    BlinkerMIOT.print();
    break;
  }
}

// 设置i2c地址
void SetI2CADDR(uint8_t addr)
{
  // A2/A1/A0 0/0/0 0/0/1 0/1/0 0/1/1 1/0/0 1/0/1 1/1/0 1/1/1
  //  EEPROM  0xA0  0xA2  0xA4  0xA6  0xA8  0xAA  0xAC  0xAE
  //    MCU   0xB0  0xB2  0xB4  0xB6  0xB8  0xBA  0xBC  0xBE

  digitalWrite(ADD0, bitRead(addr, 0));
  digitalWrite(ADD1, bitRead(addr, 1));
  digitalWrite(ADD2, bitRead(addr, 2));
  return;
}

// 累计电量 - 此部分有bug 累计电量会偏大
void sumEnergy()
{
  currentRoutineMillis = millis(); // 获取当前时间
  if (currentRoutineMillis < prevRoutineMillis)
    prevRoutineMillis = currentRoutineMillis; // 存储当前时间
  if ((currentRoutineMillis - prevRoutineMillis) >= millisRoutineInterval)
  {                                                                                               // 每隔一段时间刷新一次
    mJoule = mJoule + (int)(EPW750.getInputPower() * (currentRoutineMillis - prevRoutineMillis)); // 计算能量
    kWh = mJoule / (float)3600000000;                                                             // 计算kWh
    prevRoutineMillis = currentRoutineMillis;                                                     // 存储当前时间

    preferences.begin("EPW750", false);
    preferences.putULong64("Joule", mJoule);
    preferences.end();
  }
  return;
}

// 累计运行时间
void sumRunTime()
{
  currentRuntimeMillis = millis(); // 获取当前时间
  if ((currentRuntimeMillis - prevRuntimeMillis) >= 5000)
  {
    if (digitalRead(CSPS_PSON) == LOW)
      runtime = runtime + (currentRuntimeMillis - prevRuntimeMillis); // 计算运行时间
    runhours = runtime / (float)3600000;                              // 计算运行时间
    prevRuntimeMillis = currentRuntimeMillis;                         // 存储当前时间

    preferences.begin("EPW750", false);
    preferences.putULong64("runtime", runtime);
    preferences.end();
  }
  return;
}

// 初始化
void setup()
{
  pinMode(ADD0, OUTPUT);
  pinMode(ADD1, OUTPUT);
  pinMode(ADD2, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(CSPS_PSON, GPIO_MODE_INPUT_OUTPUT_OD); // 设置PSON为开漏输入输出
  digitalWrite(CSPS_PSON, HIGH);
  digitalWrite(LED, HIGH);
  // pinMode(CSPS_PSOK, INPUT);

  // 设置i2c地址
  SetI2CADDR(EEPROM_ADDR);

  // 初始化串口
  // Serial.begin(115200);
  // BLINKER_DEBUG.stream(Serial);
  // BLINKER_DEBUG.debugAll();
  // Serial.println(APP_VERSION);

  // 初始化Preferences
  preferences.begin("EPW750", false);
  mJoule = preferences.getULong64("Joule", 0);
  runtime = preferences.getULong64("runtime", 0);
  kWh = mJoule / 3600000000.0;    // 计算kWh
  runhours = runtime / 3600000.0; // 计算运行时间
  preferences.end();

  Blinker.begin(AUTH, SSID, PSWD);

  Blinker.attachData(dataRead);
  Blinker.attachRTData(rtData);
  Blinker.attachHeartbeat(Heartbeat);
  Blinker.attachDataStorage(dataStorage);
  SWITCH.attach(PowerSwitch);
  FanRPM.attach(SetFanRPM);

  // 用户自定义电源类操作的回调函数: 务必在回调函数中反馈该控制状态
  BlinkerMIOT.attachPowerState(miotPowerState);
  // 用户自定义设备查询的回调函数:
  BlinkerMIOT.attachQuery(miotQuery);

  // 初始化OTA
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PSWD);
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    delay(500);
    ESP.restart();
  }
  ArduinoOTA.setHostname(OTA_HOSTNAME);         // 设置OTA主机名
  ArduinoOTA.setPasswordHash(OTA_PASSWORD_MD5); // 设置OTA密码
  ArduinoOTA.begin();

  // 设置时间格式以及时间服务器的网址
  configTime(timezone * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("\nWaiting for time");
  while (!time(nullptr))
  {
    Serial.print(".");
    delay(1000);
  }
}

void loop()
{
  Blinker.run();
  ArduinoOTA.handle();
  sumEnergy();
  sumRunTime();
}
