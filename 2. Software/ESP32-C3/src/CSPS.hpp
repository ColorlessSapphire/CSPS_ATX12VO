#include <Arduino.h>
#include <Wire.h>

class CSPS
{
public:
    CSPS(byte CSPS_addr);
    CSPS(byte CSPS_addr, byte ROM_addr);
    CSPS(byte CSPS_addr, byte ROM_addr, bool ENABLE_CHECKSUM);

    String getROM(byte addr, byte len);

    // Spare Part No.
    String getSPN()
    {
        return getROM(0x12, 0x0A);
    };

    // FRU File ID:“MM/DD/YY"
    String getMFG()
    {
        return getROM(0x1D, 0x08);
    };

    // 制造商 HUAWE
    String getMFR()
    {
        return getROM(0x2C, 0x05);
    };

    // 设备名称 HUAWE 750 W PLATINUM PS
    String getName()
    {
        return getROM(0x32, 0x1A);
    };

    // Option Kit Number:02310XSD
    String getOKN()
    {
        return getROM(0x4D, 0x0A);
    };

    // Product Part/Model Number - Sequence for Supplier - Year of production - Serial Number
    String getCT()
    {
        return getROM(0x5B, 0x0E);
    };

    // 输入电压
    float getInputVoltage()
    {
        return (float)readCSPSword(0x08) / 32;
    };

    // 输入电流
    float getInputCurrent()
    {
        return (float)getInputPower() / getInputVoltage();
    };

    // 输入功率
    float getInputPower()
    {
        return (float)readCSPSword(0x0C);
    };

    // 输出电压
    float getOutputVoltage()
    {
        return (float)readCSPSword(0x0E) / 256;
    };

    // 输出电流
    float getOutputCurrent()
    {
        return (float)readCSPSword(0x10) / 64;
    };

    // 输出功率
    float getOutputPower()
    {
        return (float)readCSPSword(0x12);
    };

    // 进风温度
    float getTemp1()
    {
        return (float)readCSPSword(0x1A) / 64;
    };

    // 出风温度
    String getTemp2()
    {
       // return (float)readCSPSword(0x1C) / 64;
       return getROM(0xF4, 0x02);
    };

    // 风扇转速
    uint16_t getFanRPM()
    {
        return readCSPSword(0x1E);
    };

    // 运行时间
    uint32_t getRunTime()
    {
        return readCSPSword(0x30);
    };

    // 电源状态
    int getState()
    {
        return readCSPSword(0x04);
    };

    // 设置风扇转速
    void setFanRPM(uint16_t rpm)
    {
        writeCSPSword(0x40, rpm);
    };

private:
    bool _CSPS_READ_CHECKSUM;
    byte _CSPS_addr;
    byte _ROM_addr;
    byte readROM(byte dataAddr);

    uint32_t readCSPSword(byte dataAddr);
    void writeCSPSword(byte dataAddr, unsigned int data);
};
