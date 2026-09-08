#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFile>
#include <QApplication>
#include <QClipboard>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMetaObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QTextBrowser>
#include <QTimer>
#include <QTransform>
#include <QUrlQuery>

static const QString kEsp32StreamUrl = "http://192.168.4.1:8080/stream";
static const QString kSourceDir = "C:/Users/86198/Desktop/scan/mainwindow";

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_bOpenDevice(false)
{
    ui->setupUi(this);

    setWindowTitle("OCR 扫码枪上位机");
    setStyleSheet(R"(
        QMainWindow, QWidget#centralwidget { background: #f4f7fb; color: #172033; font-family: "Microsoft YaHei UI"; font-size: 14px; }
        QFrame#headerFrame, QFrame#videoPanel, QFrame#controlPanel { background: #ffffff; border: 1px solid #dce3ed; border-radius: 12px; }
        QLabel#appTitle { font-size: 22px; font-weight: 700; color: #172033; }
        QLabel#appSubtitle, QLabel#controlSubtitle, QLabel#streamInfo, QLabel#logTip { color: #718096; font-size: 12px; }
        QLabel#wifiStatus, QLabel#deviceStatus, QLabel#videoStatus, QLabel#ocrStatus { background: #f7f9fc; border: 1px solid #dce3ed; border-radius: 12px; padding: 6px 10px; color: #718096; }
        QLabel#videoTitle, QLabel#controlTitle { font-size: 18px; font-weight: 700; color: #172033; }
        QLabel#resultTitle, QLabel#logTitle, QLabel#helpTitle { font-weight: 600; color: #344054; }
        QLabel#lb_show { background: #f8fafc; border: 1px solid #cfd8e5; border-radius: 8px; color: #718096; font-size: 18px; }
        QLineEdit#lineEdit { background: #ffffff; border: 2px solid #cfd8e5; border-radius: 8px; color: #111827; font-family: Consolas; font-size: 25px; font-weight: 700; letter-spacing: 1px; padding: 8px; }
        QLineEdit#lineEdit:focus { border-color: #2563eb; }
        QLabel#resultHint { color: #718096; font-size: 12px; }
        QPushButton { background: #f7f9fc; border: 1px solid #cfd8e5; border-radius: 8px; color: #344054; font-weight: 600; padding: 8px 14px; }
        QPushButton:hover { background: #eef3f9; border-color: #94a3b8; }
        QPushButton:pressed { background: #e4eaf2; }
        QPushButton:disabled { color: #98a2b3; background: #f2f4f7; }
        QPushButton#OCROCV { background: #2563eb; border-color: #3b82f6; color: white; font-size: 16px; }
        QPushButton#OCROCV:hover { background: #1d4ed8; }
        QPushButton#clearButton { background: transparent; color: #667085; }
        QFrame#helpCard { background: #f7f9fc; border: 1px solid #dce3ed; border-radius: 8px; }
        QLabel#helpText { color: #667085; line-height: 1.5; }
        QTextBrowser { background: #f8fafc; border: 1px solid #dce3ed; border-radius: 7px; color: #475467; padding: 8px; font-family: Consolas, "Microsoft YaHei UI"; font-size: 12px; }
        QStatusBar { background: #f4f7fb; color: #718096; }
    )");

    ui->CameraOAC->setText("连接设备");
    ui->OCROCV->setText("开始识别");
    ui->recognize->setText("打开图片");
    ui->lb_show->setText("等待设备连接\n\n请先连接 Wi-Fi：PipeCam-2D55");
    ui->lineEdit->setReadOnly(true);
    ui->textBrowser->append("系统已就绪，请连接 Wi-Fi 热点 PipeCam-2D55。");

    myThread = new MyThread();
    resultNetwork = new QNetworkAccessManager(this);
    keyPollTimer = new QTimer(this);
    keyPollTimer->setInterval(300);

    DataBase();

    connect(myThread, &MyThread::signal_messImage, this, &MainWindow::DisPlayImage);
    connect(myThread, &MyThread::signal_status, ui->textBrowser, &QTextBrowser::append);
    connect(ui->lineEdit, &QLineEdit::textChanged, this, &MainWindow::add_to_database);
    connect(keyPollTimer, &QTimer::timeout, this, &MainWindow::pollEsp32Status);
    connect(ui->copyButton, &QPushButton::clicked, this, [this]() {
        const QString result = ui->lineEdit->text().trimmed();
        if(!result.isEmpty()) {
            QApplication::clipboard()->setText(result);
            ui->resultHint->setText("结果已复制到剪贴板");
            ui->resultHint->setStyleSheet("color:#2563eb;");
        }
    });
    connect(ui->clearButton, &QPushButton::clicked, this, [this]() {
        ui->lineEdit->clear();
        ui->resultHint->setText("尚无识别结果");
        ui->resultHint->setStyleSheet("color:#718096;");
    });
}

MainWindow::~MainWindow()
{
    if(myThread)
    {
        if(myThread->isRunning())
        {
            myThread->requestInterruption();
            myThread->wait();
        }
        delete myThread;
    }

    if(ocrThread && ocrThread->isRunning())
    {
        ocrThread->wait();
    }

    delete ui;
}

void MainWindow::on_CameraOAC_clicked()
{
    if(ui->CameraOAC->text() == "连接设备")
    {
        if(myThread->isRunning())
        {
            return;
        }

        myThread->getStreamUrl(kEsp32StreamUrl);
        myThread->start();

        m_bOpenDevice = true;
        ui->CameraOAC->setText("断开设备");
        ui->wifiStatus->setText("● Wi-Fi 检测中");
        ui->wifiStatus->setStyleSheet("color:#d97706;");
        ui->deviceStatus->setText("● 设备连接中");
        ui->deviceStatus->setStyleSheet("color:#d97706;");
        ui->videoStatus->setText("● 视频连接中");
        ui->videoStatus->setStyleSheet("color:#d97706;");
        ui->textBrowser->append("正在连接 ESP32 视频流：" + kEsp32StreamUrl);
        ui->lb_show->setPixmap(QPixmap());
        ui->lb_show->setText("正在连接 ESP32 视频流...");
        sendEsp32ControlAsync("realtime=1");
        key2CounterReady = false;
        lastKey2CaptureRequests = 0;
        keyPollTimer->start();
        ui->textBrowser->append("已启用实时预览：按 ESP32 KEY2 后，上位机自动识别当前画面并回传 LCD。");
        return;
    }

    if(myThread->isRunning())
    {
        myThread->requestInterruption();
        myThread->wait();
    }

    m_bOpenDevice = false;
    if(keyPollTimer)
    {
        keyPollTimer->stop();
    }
    ui->CameraOAC->setText("连接设备");
    ui->deviceStatus->setText("● 设备离线");
    ui->deviceStatus->setStyleSheet("color:#718096;");
    ui->videoStatus->setText("● 视频等待");
    ui->videoStatus->setStyleSheet("color:#718096;");
    ui->lb_show->setPixmap(QPixmap());
    ui->lb_show->setText("视频已断开");
    ui->textBrowser->append("ESP32 视频已断开。");
}

void MainWindow::DisPlayImage(QImage image)
{
    currentFrame = image.copy();
    ui->wifiStatus->setText("● Wi-Fi 已连接");
    ui->wifiStatus->setStyleSheet("color:#16a34a;");
    ui->deviceStatus->setText("● 设备在线");
    ui->deviceStatus->setStyleSheet("color:#16a34a;");
    ui->videoStatus->setText("● 视频正常");
    ui->videoStatus->setStyleSheet("color:#16a34a;");
    image = image.scaled(ui->lb_show->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    ui->lb_show->setPixmap(QPixmap::fromImage(image));
}

Paddleocr::Paddleocr()
{
    QDir appDir(QCoreApplication::applicationDirPath());
    QString pythonHomePath = appDir.path();
    if(!QDir(appDir.filePath("Lib")).exists())
    {
        if(QDir(appDir.filePath("hunhe")).exists())
        {
            pythonHomePath = appDir.filePath("hunhe");
        }
        else if(QDir(appDir.filePath("../hunhe")).exists())
        {
            pythonHomePath = appDir.filePath("../hunhe");
        }
        else if(QDir(QDir(kSourceDir).filePath("hunhe")).exists())
        {
            pythonHomePath = QDir(kSourceDir).filePath("hunhe");
        }
    }

    static std::wstring pythonHome = QDir::toNativeSeparators(pythonHomePath).toStdWString();
    Py_SetPythonHome((wchar_t*)pythonHome.c_str());

    Py_Initialize();
    if(!Py_IsInitialized())
    {
        std::cout << "python init fail" << endl;
        return;
    }

    const QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    const QString sourcePath = QDir::toNativeSeparators(QDir(QCoreApplication::applicationDirPath()).filePath(".."));
    const QString fixedSourcePath = QDir::toNativeSeparators(kSourceDir);
    const QByteArray command = QString(
        "import sys\n"
        "sys.path.insert(0, r'%1')\n"
        "sys.path.insert(0, r'%1/Lib/site-packages')\n"
        "sys.path.insert(0, r'%1/hunhe/Lib/site-packages')\n"
        "sys.path.insert(0, r'%2')\n"
        "sys.path.insert(0, r'%2/Lib/site-packages')\n"
        "sys.path.insert(0, r'%2/hunhe/Lib/site-packages')\n"
        "sys.path.insert(0, r'%3')\n"
        "sys.path.insert(0, r'%3/Lib/site-packages')\n"
        "sys.path.insert(0, r'%3/hunhe/Lib/site-packages')\n"
    ).arg(appPath, sourcePath, fixedSourcePath).toLocal8Bit();
    PyRun_SimpleString(command.constData());

    pModule = PyImport_ImportModule("Py_Module");
    if(pModule == NULL)
    {
        PyErr_Print();
        std::cout << "module not found" << endl;
        return;
    }

    pClass = PyObject_GetAttrString(pModule, "Rec_Text");
    if(pClass == NULL || !PyCallable_Check(pClass))
    {
        PyErr_Print();
        std::cout << "Rec_Text class not found" << endl;
        return;
    }

    pInstance = PyObject_CallObject(pClass, nullptr);
    if(pInstance == nullptr)
    {
        PyErr_Print();
        std::cout << "Failed to create Rec_Text instance" << endl;
        return;
    }

    pFunc = PyObject_GetAttrString(pInstance, "recognize_text");
    if(pFunc == NULL || !PyCallable_Check(pFunc))
    {
        PyErr_Print();
        std::cout << "recognize_text function not found" << endl;
        return;
    }

    PyEval_SaveThread();
    gilReleased = true;
}

QString Paddleocr::recognize_text(const QString &img_path)
{
    if(pFunc == NULL)
    {
        return "";
    }

    PyGILState_STATE gilState = PyGILState_Ensure();
    const QByteArray pathBytes = QDir::toNativeSeparators(img_path).toLocal8Bit();
    PyObject *args = Py_BuildValue("(s)", pathBytes.constData());
    PyObject *ret = PyObject_CallObject(pFunc, args);
    Py_DECREF(args);

    if(ret == NULL)
    {
        PyErr_Print();
        PyGILState_Release(gilState);
        return "";
    }

    const char *result = "";
    PyArg_Parse(ret, "s", &result);
    QString text = QString::fromUtf8(result);
    Py_DECREF(ret);
    PyGILState_Release(gilState);
    return text;
}

void MainWindow::on_recognize_clicked()
{
    ui->lineEdit->clear();
    PhotoPath = QFileDialog::getOpenFileName(this, "选择要识别的图片", ".", "图片(*.png *.jpg *.bmp *.tif)");
    if(PhotoPath.isEmpty())
    {
        return;
    }

    QImage img;
    if(!img.load(PhotoPath))
    {
        QMessageBox::information(this, "打开图片失败", "打开图片失败。");
        return;
    }
    ui->lb_show->setPixmap(QPixmap::fromImage(img.scaled(ui->lb_show->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));

    startOcrForImage(PhotoPath, false);
}

void MainWindow::saveImage(QString format)
{
    saveName.clear();
    QString savePath = QDir(QCoreApplication::applicationDirPath()).filePath("myImage");
    QDir().mkpath(savePath);
    QString curDate = QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss.zzz");
    saveName = QDir(savePath).filePath(curDate + "." + format);

    if(currentFrame.isNull() || !currentFrame.save(saveName))
    {
        ui->textBrowser->append("临时图片保存失败：" + saveName);
    }
}

void MainWindow::DataBase()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(QDir(QCoreApplication::applicationDirPath()).filePath("scan_records.db"));

    if(!db.open())
    {
        ui->textBrowser->append("本地数据库打开失败：" + db.lastError().text());
        return;
    }

    QSqlQuery query;
    bool success = query.exec("CREATE TABLE IF NOT EXISTS YaDiDataBase (coding VARCHAR(20) UNIQUE, date DATETIME NOT NULL)");
    ui->textBrowser->append(success ? "本地数据库已就绪。" : "本地数据库创建失败：" + query.lastError().text());
}

void MainWindow::add_to_database()
{
    QString coding = ui->lineEdit->text().trimmed();
    if(coding.isEmpty())
    {
        return;
    }

    QSqlQuery query;
    query.prepare("SELECT date FROM YaDiDataBase WHERE coding = :coding");
    query.bindValue(":coding", coding);
    query.exec();

    if(query.next())
    {
        ui->textBrowser->append("码号 " + coding + " 重复，首次记录时间：" + query.value(0).toString());
        return;
    }

    QString date = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    query.prepare("INSERT INTO YaDiDataBase (coding, date) VALUES (:coding, :date)");
    query.bindValue(":coding", coding);
    query.bindValue(":date", date);
    if(query.exec())
    {
        ui->textBrowser->append("码号已保存：" + coding);
    }
}

void MainWindow::on_OCROCV_clicked()
{
    if(currentFrame.isNull())
    {
        QMessageBox::warning(this, "警告", "当前没有可识别画面，请先连接ESP32视频。");
        return;
    }

    saveImage("png");
    if(!QFile::exists(saveName))
    {
        setOcrBusy(false);
        return;
    }
    startOcrForImage(saveName, true);
}

void MainWindow::startOcrForImage(const QString &imagePath, bool removeAfter)
{
    if(ocrRunning)
    {
        ui->textBrowser->append("OCR 正在运行，请等待当前识别完成。");
        return;
    }

    setOcrBusy(true);
    ui->lineEdit->clear();
    ui->textBrowser->append("正在识别当前最新画面...");
    sendEsp32ResultAsync("processing");

    QThread *worker = QThread::create([this, imagePath, removeAfter]() {
        QString result = paddleocr.recognize_text(imagePath);
        QMetaObject::invokeMethod(this, [this, imagePath, result, removeAfter]() {
            finishOcr(imagePath, result);
            if(removeAfter)
            {
                QFile::remove(imagePath);
            }
        }, Qt::QueuedConnection);
    });

    ocrThread = worker;
    connect(worker, &QThread::finished, this, [this, worker]() {
        if(ocrThread == worker)
        {
            ocrThread = NULL;
        }
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void MainWindow::finishOcr(const QString &imagePath, const QString &result)
{
    Q_UNUSED(imagePath);

    ui->lineEdit->setText(result);
    if(result.length() == 15)
    {
        ui->resultHint->setText("✓ 15位数字校验通过");
        ui->resultHint->setStyleSheet("color:#16a34a; font-weight:600;");
        ui->textBrowser->append("识别成功：" + result);
        sendEsp32ResultAsync("ok", result);
    }
    else
    {
        ui->resultHint->setText("未检测到完整的15位数字");
        ui->resultHint->setStyleSheet("color:#dc2626; font-weight:600;");
        ui->textBrowser->append("未识别到15位码号，当前结果：" + result);
        sendEsp32ResultAsync("ng", result, QString(), "NO VALID TEXT");
    }

    setOcrBusy(false);
}

void MainWindow::setOcrBusy(bool busy)
{
    ocrRunning = busy;
    ui->OCROCV->setEnabled(!busy);
    ui->recognize->setEnabled(!busy);
    ui->ocrStatus->setText(busy ? "● OCR 识别中" : "● OCR 就绪");
    ui->ocrStatus->setStyleSheet(busy ? "color:#d97706;" : "color:#16a34a;");
    ui->OCROCV->setText(busy ? "正在识别…" : "开始识别");
}

void MainWindow::pollEsp32Status()
{
    if(!resultNetwork || !m_bOpenDevice)
    {
        return;
    }

    QNetworkRequest request{QUrl("http://192.168.4.1:8081/status")};
    QNetworkReply *reply = resultNetwork->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const bool ok = reply->error() == QNetworkReply::NoError;
        const QByteArray body = reply->readAll();
        reply->deleteLater();

        if(!ok)
        {
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
        if(parseError.error != QJsonParseError::NoError || !doc.isObject())
        {
            return;
        }

        const QJsonObject obj = doc.object();
        const int requests = obj.value("capture_requests").toInt(-1);
        if(requests < 0)
        {
            return;
        }

        if(!key2CounterReady)
        {
            lastKey2CaptureRequests = static_cast<unsigned int>(requests);
            key2CounterReady = true;
            return;
        }

        if(static_cast<unsigned int>(requests) != lastKey2CaptureRequests)
        {
            lastKey2CaptureRequests = static_cast<unsigned int>(requests);
            triggerOcrFromCurrentFrame("收到 ESP32 KEY2 触发，自动识别当前实时画面。");
        }
    });
}

void MainWindow::triggerOcrFromCurrentFrame(const QString &message)
{
    if(ocrRunning)
    {
        ui->textBrowser->append("收到 ESP32 KEY2 触发，但 OCR 正在运行，已忽略本次触发。");
        return;
    }

    if(currentFrame.isNull())
    {
        ui->textBrowser->append("收到 ESP32 KEY2 触发，但当前还没有可识别的视频画面。");
        sendEsp32ResultAsync("ng", QString(), QString(), "NO FRAME");
        return;
    }

    saveImage("png");
    if(!QFile::exists(saveName))
    {
        sendEsp32ResultAsync("ng", QString(), QString(), "SAVE FAILED");
        return;
    }

    ui->textBrowser->append(message);
    startOcrForImage(saveName, true);
}

void MainWindow::sendEsp32ControlAsync(const QString &query)
{
    if(!resultNetwork)
    {
        return;
    }

    QNetworkRequest request{QUrl("http://192.168.4.1:8081/control?" + query)};
    QNetworkReply *reply = resultNetwork->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, query]() {
        if(reply->error() != QNetworkReply::NoError)
        {
            ui->textBrowser->append("ESP32 控制失败：" + query + "，" + reply->errorString());
        }
        reply->deleteLater();
    });
}

void MainWindow::sendEsp32ResultAsync(const QString &state,
                                      const QString &text,
                                      const QString &confidence,
                                      const QString &reason)
{
    if(!resultNetwork)
    {
        return;
    }

    QUrlQuery form;
    form.addQueryItem("state", state);
    if(!text.isEmpty())
    {
        form.addQueryItem("text", text);
    }
    if(!confidence.isEmpty())
    {
        form.addQueryItem("confidence", confidence);
    }
    if(!reason.isEmpty())
    {
        form.addQueryItem("reason", reason);
    }

    QNetworkRequest request{QUrl("http://192.168.4.1:8081/result")};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QNetworkReply *reply = resultNetwork->post(request, form.query(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if(reply->error() != QNetworkReply::NoError)
        {
            ui->textBrowser->append("ESP32 结果回传失败：" + reply->errorString());
        }
        reply->deleteLater();
    });
}
