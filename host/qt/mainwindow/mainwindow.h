#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QMessageBox>
#include <QImage>
#include "mythread.h"
#include <QString>
#include <Python.h>
#include <iostream>
#include <QDebug>
#include <QFileDialog>
#include<QSqlDatabase>
#include<QSqlQuery>
#include<QSqlError>
#include<QDateTime>
#include<QVariantList>
#include <QVBoxLayout>
#include <QCoreApplication>
#include <QDir>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QThread>
#include <QTimer>

using namespace std;

QT_BEGIN_NAMESPACE
namespace Ui {class MainWindow;}
QT_END_NAMESPACE

class Paddleocr
{
public:
    Paddleocr();
    ~Paddleocr()
    {
        if(gilReleased)
        {
            PyGILState_Ensure();
        }
        if(pFunc) Py_DECREF(pFunc);
        if(pInstance) Py_DECREF(pInstance);
        if(pClass) Py_DECREF(pClass);
        if(pModule) Py_DECREF(pModule);
        if(Py_IsInitialized()) Py_Finalize();
    }
    QString recognize_text(const QString &img_path);

private:
    PyObject*pModule = NULL;
    PyObject*pClass = NULL;
    PyObject*pInstance = NULL;
    PyObject* pFunc = NULL;

    PyObject* pRet = NULL;
    bool gilReleased = false;

};


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void saveImage(QString format);  //保存图像
    void DataBase(); //创建数据库

    void add_to_database();  //将码号存入数据库yadidatabase

private slots:

    void on_CameraOAC_clicked();  //相机操作

    void DisPlayImage(QImage myImage);  //展示图像

    void on_recognize_clicked();  //test识别

    void on_OCROCV_clicked();  //字符识别
    void finishOcr(const QString &imagePath, const QString &result);

private:
    void startOcrForImage(const QString &imagePath, bool removeAfter);
    void setOcrBusy(bool busy);
    void pollEsp32Status();
    void triggerOcrFromCurrentFrame(const QString &message);
    void sendEsp32ControlAsync(const QString &query);
    void sendEsp32ResultAsync(const QString &state,
                              const QString &text = QString(),
                              const QString &confidence = QString(),
                              const QString &reason = QString());

    Ui::MainWindow *ui;
    bool m_bOpenDevice;

    MyThread *myThread = NULL;  //线程对象
    QThread *ocrThread = NULL;
    QImage currentFrame;
    bool ocrRunning = false;
    QTimer *keyPollTimer = NULL;
    bool key2CounterReady = false;
    unsigned int lastKey2CaptureRequests = 0;
    QNetworkAccessManager *resultNetwork = NULL;

private:
    //字符识别
    QString PhotoPath;
    Paddleocr paddleocr;
    QString saveName;


};

#endif // MAINWINDOW_H
