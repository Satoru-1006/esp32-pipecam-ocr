/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.8.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTextBrowser>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QVBoxLayout *rootLayout;
    QFrame *headerFrame;
    QHBoxLayout *headerLayout;
    QVBoxLayout *titleLayout;
    QLabel *appTitle;
    QLabel *appSubtitle;
    QSpacerItem *headerSpacer;
    QLabel *wifiStatus;
    QLabel *deviceStatus;
    QLabel *videoStatus;
    QLabel *ocrStatus;
    QHBoxLayout *contentLayout;
    QFrame *videoPanel;
    QVBoxLayout *videoLayout;
    QHBoxLayout *videoTitleLayout;
    QLabel *videoTitle;
    QSpacerItem *videoTitleSpacer;
    QLabel *streamInfo;
    QLabel *lb_show;
    QHBoxLayout *logTitleLayout;
    QLabel *logTitle;
    QSpacerItem *logTitleSpacer;
    QLabel *logTip;
    QTextBrowser *textBrowser;
    QFrame *controlPanel;
    QVBoxLayout *controlLayout;
    QLabel *controlTitle;
    QLabel *controlSubtitle;
    QLabel *resultTitle;
    QLineEdit *lineEdit;
    QLabel *resultHint;
    QPushButton *OCROCV;
    QPushButton *CameraOAC;
    QHBoxLayout *secondaryButtons;
    QPushButton *recognize;
    QPushButton *copyButton;
    QPushButton *clearButton;
    QFrame *helpCard;
    QVBoxLayout *helpLayout;
    QLabel *helpTitle;
    QLabel *helpText;
    QSpacerItem *controlSpacer;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(1280, 760);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        rootLayout = new QVBoxLayout(centralwidget);
        rootLayout->setSpacing(16);
        rootLayout->setObjectName("rootLayout");
        rootLayout->setContentsMargins(24, 20, 24, 20);
        headerFrame = new QFrame(centralwidget);
        headerFrame->setObjectName("headerFrame");
        headerFrame->setMinimumSize(QSize(0, 72));
        headerLayout = new QHBoxLayout(headerFrame);
        headerLayout->setObjectName("headerLayout");
        headerLayout->setContentsMargins(20, -1, 20, -1);
        titleLayout = new QVBoxLayout();
        titleLayout->setObjectName("titleLayout");
        appTitle = new QLabel(headerFrame);
        appTitle->setObjectName("appTitle");

        titleLayout->addWidget(appTitle);

        appSubtitle = new QLabel(headerFrame);
        appSubtitle->setObjectName("appSubtitle");

        titleLayout->addWidget(appSubtitle);


        headerLayout->addLayout(titleLayout);

        headerSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        headerLayout->addItem(headerSpacer);

        wifiStatus = new QLabel(headerFrame);
        wifiStatus->setObjectName("wifiStatus");

        headerLayout->addWidget(wifiStatus);

        deviceStatus = new QLabel(headerFrame);
        deviceStatus->setObjectName("deviceStatus");

        headerLayout->addWidget(deviceStatus);

        videoStatus = new QLabel(headerFrame);
        videoStatus->setObjectName("videoStatus");

        headerLayout->addWidget(videoStatus);

        ocrStatus = new QLabel(headerFrame);
        ocrStatus->setObjectName("ocrStatus");

        headerLayout->addWidget(ocrStatus);


        rootLayout->addWidget(headerFrame);

        contentLayout = new QHBoxLayout();
        contentLayout->setSpacing(16);
        contentLayout->setObjectName("contentLayout");
        videoPanel = new QFrame(centralwidget);
        videoPanel->setObjectName("videoPanel");
        videoLayout = new QVBoxLayout(videoPanel);
        videoLayout->setSpacing(12);
        videoLayout->setObjectName("videoLayout");
        videoLayout->setContentsMargins(18, 16, 18, 16);
        videoTitleLayout = new QHBoxLayout();
        videoTitleLayout->setObjectName("videoTitleLayout");
        videoTitle = new QLabel(videoPanel);
        videoTitle->setObjectName("videoTitle");

        videoTitleLayout->addWidget(videoTitle);

        videoTitleSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        videoTitleLayout->addItem(videoTitleSpacer);

        streamInfo = new QLabel(videoPanel);
        streamInfo->setObjectName("streamInfo");

        videoTitleLayout->addWidget(streamInfo);


        videoLayout->addLayout(videoTitleLayout);

        lb_show = new QLabel(videoPanel);
        lb_show->setObjectName("lb_show");
        lb_show->setMinimumSize(QSize(720, 430));
        lb_show->setAlignment(Qt::AlignCenter);
        lb_show->setScaledContents(false);

        videoLayout->addWidget(lb_show);

        logTitleLayout = new QHBoxLayout();
        logTitleLayout->setObjectName("logTitleLayout");
        logTitle = new QLabel(videoPanel);
        logTitle->setObjectName("logTitle");

        logTitleLayout->addWidget(logTitle);

        logTitleSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        logTitleLayout->addItem(logTitleSpacer);

        logTip = new QLabel(videoPanel);
        logTip->setObjectName("logTip");

        logTitleLayout->addWidget(logTip);


        videoLayout->addLayout(logTitleLayout);

        textBrowser = new QTextBrowser(videoPanel);
        textBrowser->setObjectName("textBrowser");
        textBrowser->setMaximumSize(QSize(16777215, 112));

        videoLayout->addWidget(textBrowser);


        contentLayout->addWidget(videoPanel);

        controlPanel = new QFrame(centralwidget);
        controlPanel->setObjectName("controlPanel");
        controlPanel->setMinimumSize(QSize(360, 0));
        controlPanel->setMaximumSize(QSize(390, 16777215));
        controlLayout = new QVBoxLayout(controlPanel);
        controlLayout->setSpacing(14);
        controlLayout->setObjectName("controlLayout");
        controlLayout->setContentsMargins(24, 22, 24, 22);
        controlTitle = new QLabel(controlPanel);
        controlTitle->setObjectName("controlTitle");

        controlLayout->addWidget(controlTitle);

        controlSubtitle = new QLabel(controlPanel);
        controlSubtitle->setObjectName("controlSubtitle");

        controlLayout->addWidget(controlSubtitle);

        resultTitle = new QLabel(controlPanel);
        resultTitle->setObjectName("resultTitle");

        controlLayout->addWidget(resultTitle);

        lineEdit = new QLineEdit(controlPanel);
        lineEdit->setObjectName("lineEdit");
        lineEdit->setMinimumSize(QSize(0, 78));
        lineEdit->setAlignment(Qt::AlignCenter);

        controlLayout->addWidget(lineEdit);

        resultHint = new QLabel(controlPanel);
        resultHint->setObjectName("resultHint");
        resultHint->setAlignment(Qt::AlignCenter);

        controlLayout->addWidget(resultHint);

        OCROCV = new QPushButton(controlPanel);
        OCROCV->setObjectName("OCROCV");
        OCROCV->setMinimumSize(QSize(0, 54));

        controlLayout->addWidget(OCROCV);

        CameraOAC = new QPushButton(controlPanel);
        CameraOAC->setObjectName("CameraOAC");
        CameraOAC->setMinimumSize(QSize(0, 48));

        controlLayout->addWidget(CameraOAC);

        secondaryButtons = new QHBoxLayout();
        secondaryButtons->setSpacing(10);
        secondaryButtons->setObjectName("secondaryButtons");
        recognize = new QPushButton(controlPanel);
        recognize->setObjectName("recognize");
        recognize->setMinimumSize(QSize(0, 44));

        secondaryButtons->addWidget(recognize);

        copyButton = new QPushButton(controlPanel);
        copyButton->setObjectName("copyButton");
        copyButton->setMinimumSize(QSize(0, 44));

        secondaryButtons->addWidget(copyButton);


        controlLayout->addLayout(secondaryButtons);

        clearButton = new QPushButton(controlPanel);
        clearButton->setObjectName("clearButton");
        clearButton->setMinimumSize(QSize(0, 40));

        controlLayout->addWidget(clearButton);

        helpCard = new QFrame(controlPanel);
        helpCard->setObjectName("helpCard");
        helpLayout = new QVBoxLayout(helpCard);
        helpLayout->setObjectName("helpLayout");
        helpLayout->setContentsMargins(14, 12, 14, 12);
        helpTitle = new QLabel(helpCard);
        helpTitle->setObjectName("helpTitle");

        helpLayout->addWidget(helpTitle);

        helpText = new QLabel(helpCard);
        helpText->setObjectName("helpText");
        helpText->setWordWrap(true);

        helpLayout->addWidget(helpText);


        controlLayout->addWidget(helpCard);

        controlSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        controlLayout->addItem(controlSpacer);


        contentLayout->addWidget(controlPanel);


        rootLayout->addLayout(contentLayout);

        MainWindow->setCentralWidget(centralwidget);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "OCR \346\211\253\347\240\201\346\236\252\344\270\212\344\275\215\346\234\272", nullptr));
        appTitle->setText(QCoreApplication::translate("MainWindow", "OCR \346\211\253\347\240\201\346\236\252\344\270\212\344\275\215\346\234\272", nullptr));
        appSubtitle->setText(QCoreApplication::translate("MainWindow", "\345\256\236\346\227\266\347\224\273\351\235\242\351\207\207\351\233\206 \302\267 15\344\275\215\346\225\260\345\255\227\350\257\206\345\210\253 \302\267 \350\207\252\345\212\250\345\233\236\344\274\240\350\256\276\345\244\207", nullptr));
        wifiStatus->setText(QCoreApplication::translate("MainWindow", "\342\227\217 Wi-Fi \345\276\205\350\277\236\346\216\245", nullptr));
        deviceStatus->setText(QCoreApplication::translate("MainWindow", "\342\227\217 \350\256\276\345\244\207\347\246\273\347\272\277", nullptr));
        videoStatus->setText(QCoreApplication::translate("MainWindow", "\342\227\217 \350\247\206\351\242\221\347\255\211\345\276\205", nullptr));
        ocrStatus->setText(QCoreApplication::translate("MainWindow", "\342\227\217 OCR \345\260\261\347\273\252", nullptr));
        videoTitle->setText(QCoreApplication::translate("MainWindow", "\345\256\236\346\227\266\347\224\273\351\235\242", nullptr));
        streamInfo->setText(QCoreApplication::translate("MainWindow", "192.168.4.1 \302\267 MJPEG", nullptr));
        lb_show->setText(QCoreApplication::translate("MainWindow", "\347\255\211\345\276\205\350\256\276\345\244\207\350\277\236\346\216\245\n"
"\n"
"\350\257\267\345\205\210\350\277\236\346\216\245 Wi-Fi\357\274\232PipeCam-2D55", nullptr));
        logTitle->setText(QCoreApplication::translate("MainWindow", "\350\277\220\350\241\214\346\227\245\345\277\227", nullptr));
        logTip->setText(QCoreApplication::translate("MainWindow", "\350\256\276\345\244\207\351\200\232\344\277\241\344\270\216\350\257\206\345\210\253\350\256\260\345\275\225", nullptr));
        controlTitle->setText(QCoreApplication::translate("MainWindow", "\350\257\206\345\210\253\346\216\247\345\210\266", nullptr));
        controlSubtitle->setText(QCoreApplication::translate("MainWindow", "\350\256\276\345\244\207 K2 \346\214\211\351\224\256\346\210\226\344\270\212\344\275\215\346\234\272\345\235\207\345\217\257\350\247\246\345\217\221 OCR", nullptr));
        resultTitle->setText(QCoreApplication::translate("MainWindow", "\346\234\254\346\254\241\350\257\206\345\210\253\347\273\223\346\236\234", nullptr));
        lineEdit->setPlaceholderText(QCoreApplication::translate("MainWindow", "\347\255\211\345\276\205\350\257\206\345\210\25315\344\275\215\346\225\260\345\255\227", nullptr));
        resultHint->setText(QCoreApplication::translate("MainWindow", "\345\260\232\346\227\240\350\257\206\345\210\253\347\273\223\346\236\234", nullptr));
        OCROCV->setText(QCoreApplication::translate("MainWindow", "\345\274\200\345\247\213\350\257\206\345\210\253", nullptr));
        CameraOAC->setText(QCoreApplication::translate("MainWindow", "\350\277\236\346\216\245\350\256\276\345\244\207", nullptr));
        recognize->setText(QCoreApplication::translate("MainWindow", "\346\211\223\345\274\200\345\233\276\347\211\207", nullptr));
        copyButton->setText(QCoreApplication::translate("MainWindow", "\345\244\215\345\210\266\347\273\223\346\236\234", nullptr));
        clearButton->setText(QCoreApplication::translate("MainWindow", "\346\270\205\347\251\272\347\273\223\346\236\234", nullptr));
        helpTitle->setText(QCoreApplication::translate("MainWindow", "\344\275\277\347\224\250\346\217\220\347\244\272", nullptr));
        helpText->setText(QCoreApplication::translate("MainWindow", "1. \347\224\265\350\204\221\350\277\236\346\216\245 PipeCam-2D55\n"
"2. \347\202\271\345\207\273\342\200\234\350\277\236\346\216\245\350\256\276\345\244\207\342\200\235\346\237\245\347\234\213\345\256\236\346\227\266\347\224\273\351\235\242\n"
"3. \346\214\211\346\211\253\347\240\201\346\236\252 K2 \346\210\226\347\202\271\345\207\273\342\200\234\345\274\200\345\247\213\350\257\206\345\210\253\342\200\235", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
