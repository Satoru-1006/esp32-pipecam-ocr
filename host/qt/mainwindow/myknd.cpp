#include "myknd.h"
#include <QModbusTcpClient>
#include <QModbusDataUnit>
#include <QModbusReply>
#include <QMessageBox>
#include <QDebug>

MyKnd::MyKnd(QWidget *parent)
    : QWidget{parent}
{

}
bool MyKnd::connect_modbus_device(QString ip, int port)
{
    device = new QModbusTcpClient(this);
    device->setConnectionParameter(QModbusDevice::NetworkPortParameter,port);  //设置端口号
    device->setConnectionParameter(QModbusDevice::NetworkAddressParameter,ip);  //设置Modbus服务器地址
    if (!device->connectDevice())
    {
        qDebug() <<QString::fromLocal8Bit ("连接失败");
    }
    return device->state();
}

void MyKnd::close_device()
{
    if(state)
    {
        bool green_light1 = false;  //#DO1
        bool red_light1 = false;    //#DO2
        bool yellow_light1 = false; //#DO3
        bool green_light2 = false;  //#DO4
        bool red_light2 = false;    //#DO5
        bool yellow_light2 = false; //#DO6
        device->ClosingState;
    }

}

void MyKnd::read_discrete_inputs(int start_address =10200, int count = 24, int slave = 1)
{

}

void MyKnd::write_green_light1(bool value, int start_address, int slave)
{
    if(device->state() == QModbusDevice::ConnectedState)
    {
        QModbusDataUnit writeUnit(QModbusDataUnit::Coils, 100, 1); //
        writeUnit.setValue(0,value);
        if(value)
        {
            green_light1 = true;
        }
        else
        {
            green_light1 = false;
        }
        if (auto *reply = device->sendWriteRequest(writeUnit, 1))
        {
            reply->deleteLater();
        }
    }

}

void MyKnd::write_red_light1(bool value, int start_address, int slave)
{
    if(device->state() == QModbusDevice::ConnectedState)
    {
        QModbusDataUnit writeUnit(QModbusDataUnit::Coils, 101, 1); //
        writeUnit.setValue(0,value);
        if(value)
        {
            red_light1 = true;
        }
        else
        {
            red_light1 = false;
        }
        if (auto *reply = device->sendWriteRequest(writeUnit, 1))
        {
            reply->deleteLater();
        }
    }
}

void MyKnd::write_yellow_light1(bool value, int start_address, int slave)
{
    if(device->state() == QModbusDevice::ConnectedState)
    {
        QModbusDataUnit writeUnit(QModbusDataUnit::Coils, 102, 1); //
        writeUnit.setValue(0,value);
        if(value)
        {
            yellow_light1 = true;
        }
        else
        {
            yellow_light1 = false;
        }
        if (auto *reply = device->sendWriteRequest(writeUnit, 1))
        {
            reply->deleteLater();
        }
    }
}

void MyKnd::write_green_light2(bool value, int start_address, int slave)
{
    if(device->state() == QModbusDevice::ConnectedState)
    {
        QModbusDataUnit writeUnit(QModbusDataUnit::Coils, 104, 1); //
        writeUnit.setValue(0,value);
        if(value)
        {
            green_light2 = true;
        }
        else
        {
            green_light2 = false;
        }
        if (auto *reply = device->sendWriteRequest(writeUnit, 1))
        {
            reply->deleteLater();
        }
    }
}

void MyKnd::write_red_light2(bool value, int start_address , int slave)
{
    if(device->state() == QModbusDevice::ConnectedState)
    {
        QModbusDataUnit writeUnit(QModbusDataUnit::Coils, 105, 1); //
        writeUnit.setValue(0,value);
        if(value)
        {
            red_light2 = true;
        }
        else
        {
            red_light2 = false;
        }
        if (auto *reply = device->sendWriteRequest(writeUnit, 1))
        {
            reply->deleteLater();
        }
    }
}

void MyKnd::write_yellow_light2(bool value, int start_address, int slave)
{
    if(device->state() == QModbusDevice::ConnectedState)
    {
        QModbusDataUnit writeUnit(QModbusDataUnit::Coils, 106, 1); //
        writeUnit.setValue(0,value);
        if(value)
        {
            yellow_light2 = true;
        }
        else
        {
            yellow_light2 = false;
        }
        if (auto *reply = device->sendWriteRequest(writeUnit, 1))
        {
            reply->deleteLater();
        }
    }
}

