#ifndef MYKND_H
#define MYKND_H

#include <QWidget>
#include <QModbusTcpClient>
#include <QString>
class MyKnd : public QWidget
{
    Q_OBJECT
public:
    explicit MyKnd(QWidget *parent = nullptr);
    QModbusTcpClient *device;
    bool connect_modbus_device(QString ip, int port = 502);
    void close_device();
    void read_discrete_inputs(int start_address,int count,int slave);
    void write_green_light1(bool value, int start_address = 100, int slave = 1);
    void write_red_light1(bool value, int start_address = 101, int slave = 1);
    void write_yellow_light1(bool value, int start_address = 102, int slave = 1);
    void write_green_light2(bool value, int start_address = 104, int slave = 1);
    void write_red_light2(bool value, int start_address = 105, int slave = 1);
    void write_yellow_light2(bool value, int start_address = 106, int slave = 1);
private:
    bool green_light1 = false;  //#DO1
    bool red_light1 = false;    //#DO2
    bool yellow_light1 = false; //#DO3
    bool green_light2 = false;  //#DO4
    bool red_light2 = false;    //#DO5
    bool yellow_light2 = false; //#DO6
    bool isconnect;
    bool state = false;
    QString ip = "192.168.1.1";

signals:

};

#endif // MYKND_H
