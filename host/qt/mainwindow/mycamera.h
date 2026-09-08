#ifndef MYCAMERA_H
#define MYCAMERA_H

#include <QWidget>
#include <QMessageBox>
#include "cmvcamera.h"
#include "mythread.h"
#include <opencv2/opencv.hpp>
#include <QStringList>

class MyCamera : public QWidget
{
    Q_OBJECT
public:
    explicit MyCamera(QWidget *parent = nullptr);
    //void display(const Mat* image);
    void FindDevice();
    void OpenDevice();
    void StartGrabBuffer();
    //void DisPlayImage(QImage myImage);

signals:
    void signal_Image(QImage myImage);
private:
    //构造相机对象
    //camera_1 = HKVision()
    //相机ID
    //camera_1_id = "L17067248"
    bool m_bOpenDevice;

    MV_CC_DEVICE_INFO_LIST m_stDevList;
    CMvCamera *m_pcMyCamera = NULL;
    MyThread *myThread = NULL;
    Mat *myImage = NULL;
};

#endif // MYCAMERA_H
