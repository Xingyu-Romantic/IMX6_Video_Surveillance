#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "imagecamera.h"
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    qRegisterMetaType<QPixmap>("QPixmap&");

    c = new ImageCamera("/dev/video2", this);   // 实例化封装的对V4L2操作的ImageCamera类
    c->setGrabSize(640, 480);                   // 设置视频图像的显示分辨率

    // 创建ImageCamera获取到视频图像的信号与槽连接
    connect(c, SIGNAL(image(QPixmap&)), this, SLOT(showImg(QPixmap&)), Qt::QueuedConnection);
    if(!c->initCamera())    // 初始化摄像头设备节点
    {
        QMessageBox::information(this, QString(), c->errorString());
        return;
    }

    if(!c->startGrab())     // 启动ImageCamera线程并开始视频流数据的采集
    {
        QMessageBox::information(this, QString(), c->errorString());
        return;
    }


}

MainWindow::~MainWindow()
{
    c->stopGrab();          // 停止图像采集
    c->releaseCamera();     // 关闭设备节点

    delete ui;
}

void MainWindow::showImg(QPixmap &img)
{
    ui->lb->setPixmap(img);
}
