#ifndef IMAGECAMERA_H
#define IMAGECAMERA_H

#include <QObject>
#include <QPixmap>
#include <QThread>

#define u8      unsigned char
#define uint8_t unsigned char

//#define IMAGE_WIDTH 1280
//#define IMAGE_HEIGHT 960
//#define IMAGE_SIZE (IMAGE_WIDTH * IMAGE_HEIGHT * 2)
//#define BUFFER_COUNT 5


class ImageCamera : public QThread
{
    Q_OBJECT
public:
    explicit ImageCamera(QString c, QObject *parent = 0);

//    void setCameraPath(QString c){
//        camera = c;
//    }

    void setGrabSize(int w, int h)
    {
        nWidth = w;
        nHeight = h;
        nSize = w * h * 2;
    }

    bool initCamera();              //  初始化相机
    bool startGrab();               //  开始抓图
    void stopGrab();                //  停止抓图
    void releaseCamera();           //  释放相机

    QString errorString(){
        QString s = err;
        err.clear();
        return s;
    }

protected:
    void run();
    int grabImage(u8* buf, int* imgLen);        //  实际的抓图函数

signals:
    void image(QPixmap &pix);                   //  相机抓到图后，会将图片通过此信号发出

public:
    QString camera;             //  相机的设备节点
    QString err;
    int nWidth;                 //  图片宽、高
    int nHeight;
    int nSize;
    bool bKeep;

    char pix_format[64];        //  当前相机的图片格式
    int cam_fd;                 //  相机描述符
    int sock;                   // Sock NetWork
    int buffer_count;           //  相机抓图容器数量
    u8** video_buffer_ptr;      //  相机抓图容器队列指针
};

#endif  // IMAGECAMERA_H
