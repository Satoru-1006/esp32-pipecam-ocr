#ifndef MYTHREAD_H
#define MYTHREAD_H

#include <QByteArray>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QThread>
#include <QUrl>

class MyThread : public QThread
{
    Q_OBJECT

public:
    explicit MyThread(QObject *parent = nullptr);
    ~MyThread();

    void run() override;
    void getStreamUrl(const QString &url);

signals:
    void signal_messImage(QImage myImage);
    void signal_status(QString status);

private:
    QString streamUrl;
};

#endif // MYTHREAD_H
