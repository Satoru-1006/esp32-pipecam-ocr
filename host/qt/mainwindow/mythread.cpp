#include "mythread.h"

#include <QEventLoop>
#include <QNetworkRequest>
#include <QTimer>

MyThread::MyThread(QObject *parent)
    : QThread{parent}
{
}

MyThread::~MyThread()
{
    requestInterruption();
    wait(2000);
}

void MyThread::getStreamUrl(const QString &url)
{
    streamUrl = url;
}

void MyThread::run()
{
    if(streamUrl.isEmpty())
    {
        emit signal_status("ESP32 视频地址为空。");
        return;
    }

    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl(streamUrl)};
    request.setRawHeader("User-Agent", "ScanHost/ESP32");

    QNetworkReply *reply = manager.get(request);
    QByteArray buffer;
    QEventLoop loop;
    QTimer timer;

    connect(reply, &QNetworkReply::readyRead, [&]() {
        buffer.append(reply->readAll());

        QByteArray latestJpeg;
        auto emitLatestFrame = [&]() {
            if(latestJpeg.isEmpty())
            {
                return;
            }

            QImage image;
            if(image.loadFromData(latestJpeg, "JPG"))
            {
                emit signal_messImage(image);
            }
        };

        while(true)
        {
            int start = buffer.indexOf("\xff\xd8", 0);
            if(start < 0)
            {
                if(buffer.size() > 4096)
                {
                    buffer = buffer.right(4096);
                }
                emitLatestFrame();
                return;
            }

            int end = buffer.indexOf("\xff\xd9", start + 2);
            if(end < 0)
            {
                if(start > 0)
                {
                    buffer.remove(0, start);
                }
                emitLatestFrame();
                return;
            }

            QByteArray jpeg = buffer.mid(start, end - start + 2);
            buffer.remove(0, end + 2);

            latestJpeg = jpeg;
        }
    });

    connect(reply, &QNetworkReply::finished, [&]() {
        if(reply->error() != QNetworkReply::NoError)
        {
            emit signal_status("ESP32 视频连接失败：" + reply->errorString());
        }
        loop.quit();
    });

    connect(&timer, &QTimer::timeout, [&]() {
        if(isInterruptionRequested())
        {
            reply->abort();
            loop.quit();
        }
    });

    timer.start(100);
    emit signal_status("正在接收 ESP32 视频流：" + streamUrl);
    loop.exec();
    reply->deleteLater();
}
