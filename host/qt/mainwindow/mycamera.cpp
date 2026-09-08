#include "mycamera.h"
#include "mythread.h"

MyCamera::MyCamera(QWidget *parent)
    : QWidget{parent}
{

    //图像指针对象
    myImage = new Mat();
    m_bOpenDevice = false;
    myThread = new MyThread();


    // connect(myThread,&MyThread::signal_messImage,this,[=](QImage myImage)
    //         {
    //     emit signal_Image(myImage);

    // });
}


void MyCamera::FindDevice()
{
    memset(&m_stDevList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));

    //枚举子网内所有设备
    int nRet = CMvCamera::EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE,&m_stDevList);
    if (MV_OK != nRet)
    {
        return;
    }
    for(unsigned int i = 0; i < m_stDevList.nDeviceNum; i++)
    {
        MV_CC_DEVICE_INFO* pDeviceInfo = m_stDevList.pDeviceInfo[i];
        QString strModelName = "";
        if(pDeviceInfo->nTLayerType == MV_USB_DEVICE)
        {
            strModelName = (char*)pDeviceInfo->SpecialInfo.stUsb3VInfo.chModelName;
        }
        else if(pDeviceInfo->nTLayerType == MV_GIGE_DEVICE)
        {
            strModelName = (char*)pDeviceInfo->SpecialInfo.stGigEInfo.chModelName;
        }
        else
        {
            //QMessageBox::warning(this,"警告","未知设备枚举！");
            return;
        }
        qDebug()<<"strModelName:"<<strModelName;
    }
}

void MyCamera::OpenDevice()
{
    if(m_bOpenDevice)
    {
        return;
    }
    m_pcMyCamera = new CMvCamera;
    if(NULL == m_pcMyCamera)
    {
        return;
    }
    qDebug()<<"okshuchu3333333333333";
    //打开设备
    int nRet = m_pcMyCamera->Open(m_stDevList.pDeviceInfo[0]);
    m_bOpenDevice = true;
        qDebug()<<"okshuchu444444444444";
    qDebug()<<"Connect:"<<nRet;
    if(MV_OK != nRet)
    {
        delete m_pcMyCamera;
        m_pcMyCamera = NULL;
        //QMessageBox::warning(this,"警告","打开设备失败！");
        return;
    }
    //设置为触发模式
    qDebug()<<"TriggerMode:"<<m_pcMyCamera->SetEnumValue("TriggerMode",1);
    //设置触发源为软触发
    qDebug()<<"TriggerSource:"<<m_pcMyCamera->SetEnumValue("TriggerSource",7);
    //设置曝光时间
    qDebug()<<"SetExposureTime:"<<m_pcMyCamera->SetFloatValue("ExposureTime",4000);
    //设置增益
    //qDebug()<<"SetGain:"<<m_pcMyCamera->SetFloatValue("Gain",50);
    //开启相机采集
    qDebug()<<"StartCamera:"<<m_pcMyCamera->StartGrabbing();

    myThread = new MyThread;
    myThread->getCameraPtr(m_pcMyCamera);
    myThread->getImagePtr(myImage);
}

void MyCamera::StartGrabBuffer()
{
    if(!m_bOpenDevice)
    {
        //QMessageBox::warning(this,"警告","采集失败,请打开设备！");
        return;
    }

    if(!myThread->isRunning())
    {
        myThread->start();
    }

}

