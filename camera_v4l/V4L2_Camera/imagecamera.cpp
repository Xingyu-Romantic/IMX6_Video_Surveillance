#include "imagecamera.h"
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <QDir>
#include <dirent.h>
#include <unistd.h>
#include <asm/types.h>
#include <math.h>
#include <malloc.h>
#include <QImage>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define log_debug qDebug

void yuyv_to_rgb888(unsigned char* yuv,unsigned char* rgb, int imgWidth, int imgHeight)
{
    unsigned int i;
    unsigned char* y0 = yuv + 0;
    unsigned char* u0 = yuv + 1;
    unsigned char* y1 = yuv + 2;
    unsigned char* v0 = yuv + 3;

    unsigned  char* r0 = rgb + 0;
    unsigned  char* g0 = rgb + 1;
    unsigned  char* b0 = rgb + 2;
    unsigned  char* r1 = rgb + 3;
    unsigned  char* g1 = rgb + 4;
    unsigned  char* b1 = rgb + 5;
    //DBG("yuyv_to_rgb start\n");
    float rt0 = 0, gt0 = 0, bt0 = 0, rt1 = 0, gt1 = 0, bt1 = 0;

    for(i = 0; i <= (imgWidth * imgHeight) / 2 ;i++)
    {
        bt0 = 1.164 * (*y0 - 16) + 2.018 * (*u0 - 128);
        gt0 = 1.164 * (*y0 - 16) - 0.813 * (*v0 - 128) - 0.394 * (*u0 - 128);
        rt0 = 1.164 * (*y0 - 16) + 1.596 * (*v0 - 128);

        bt1 = 1.164 * (*y1 - 16) + 2.018 * (*u0 - 128);
        gt1 = 1.164 * (*y1 - 16) - 0.813 * (*v0 - 128) - 0.394 * (*u0 - 128);
        rt1 = 1.164 * (*y1 - 16) + 1.596 * (*v0 - 128);

        if(rt0 > 250)  	rt0 = 255;
        if(rt0 < 0)    	rt0 = 0;

        if(gt0 > 250) 	gt0 = 255;
        if(gt0 < 0)	gt0 = 0;

        if(bt0 > 250)	bt0 = 255;
        if(bt0 < 0)	bt0 = 0;

        if(rt1 > 250)	rt1 = 255;
        if(rt1 < 0)	rt1 = 0;

        if(gt1 > 250)	gt1 = 255;
        if(gt1 < 0)	gt1 = 0;

        if(bt1 > 250)	bt1 = 255;
        if(bt1 < 0)	bt1 = 0;

        *r0 = (unsigned char)rt0;
        *g0 = (unsigned char)gt0;
        *b0 = (unsigned char)bt0;

        *r1 = (unsigned char)rt1;
        *g1 = (unsigned char)gt1;
        *b1 = (unsigned char)bt1;

        yuv = yuv + 4;
        rgb = rgb + 6;
        if(yuv == NULL)
          break;

        y0 = yuv;
        u0 = yuv + 1;
        y1 = yuv + 2;
        v0 = yuv + 3;

        r0 = rgb + 0;
        g0 = rgb + 1;
        b0 = rgb + 2;
        r1 = rgb + 3;
        g1 = rgb + 4;
        b1 = rgb + 5;
    }
}

ImageCamera::ImageCamera(QString c, QObject *parent) : QThread(parent)
{
    camera = c;
    err.clear();
    nWidth = 1280;
    nHeight = 960;
    nSize = nWidth * nHeight * 2;
    bKeep = false;
    memset(pix_format, 0, sizeof(pix_format));

    cam_fd = -1;
    sock = -1;
    buffer_count = 1;   //5;       // count 指定根据图像占用空间大小申请的缓存区个数
    video_buffer_ptr = new u8*[buffer_count];
}

bool ImageCamera::initCamera()
{
    sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));  //每个字节都用0填充
    serv_addr.sin_family = AF_INET;  //使用IPv4地址
    serv_addr.sin_addr.s_addr = inet_addr("192.168.1.102");  //具体的IP地址
    serv_addr.sin_port = htons(1234);  //端口
    if (::connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) != 0){
        err = QString("NetWork Fail!");
        return false;
    }

    int index = 0;
    int ret = 0;

    if(camera.isEmpty())
    {
        err = QString("设备节点位置为空");
        return false;
    }

    if(cam_fd != -1)
    {
        close(cam_fd);
    }

    //  打开相机设备节点
    cam_fd = open(camera.toStdString().c_str(), O_RDWR);
    if(cam_fd == -1)
    {
        err = QString("打开相机节点失败");
        return false;
    }

    //  设置使用的相机通道，此处为0
    if(0 != ioctl(cam_fd, VIDIOC_S_INPUT, &index))
    {
        err = QString("%1").arg(strerror(errno));
        return false;
    }

    //  查询相机的基本信息
    struct v4l2_capability cap;
    ioctl(cam_fd, VIDIOC_QUERYCAP, &cap);
    log_debug("Driver Name:%s, Card Name:%s, Bus info:%s, Driver Version:%u.%u.%u",
           cap.driver,cap.card,cap.bus_info,(cap.version>>16)&0xFF, (cap.version>>8)&0xFF,cap.version&0xFF);

    //  查询相机支持的采图格式
    struct v4l2_fmtdesc fmtdesc;
    fmtdesc.index=0;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    log_debug("Support format:");
    do{
        ret=ioctl(cam_fd, VIDIOC_ENUM_FMT, &fmtdesc);
        log_debug("index %d, format %s", fmtdesc.index,  fmtdesc.description);
        fmtdesc.index++;
    }while(0==ret);

    //  查询相机支持的帧率范围
    struct v4l2_streamparm stream_parm;
    memset(&stream_parm, 0, sizeof(struct v4l2_streamparm));
    stream_parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(cam_fd, VIDIOC_G_PARM, &stream_parm);
    log_debug("FrameRate: numerator = %d, denominator = %d",
              stream_parm.parm.capture.timeperframe.numerator, stream_parm.parm.capture.timeperframe.denominator);

    //  设置相机的输出格式，图片大小等数据
    struct v4l2_format format;
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;    // 设置相机输出为JPEG(V4L2_PIX_FMT_MJPEG)，也可设置为yuyv(V4L2_PIX_FMT_YUYV)，但具体的输出格式不能依靠此处
    format.fmt.pix.width = nWidth;                      // 设置输出图片宽、高
    format.fmt.pix.height = nHeight;

//    format.fmt.pix.bytesperline = IMAGE_WIDTH * 1;
//    format.fmt.pix.sizeimage = IMAGE_WIDTH * IMAGE_HEIGHT * 1;
//    format.fmt.pix.field  = V4L2_FIELD_NONE;

    //  尝试相机是否支持改格式
    ret = ioctl(cam_fd, VIDIOC_TRY_FMT, &format);
    if (ret != 0)
    {
        err = QString::asprintf("ioctl(VIDIOC_TRY_FMT) failed %d(%s)", errno, strerror(errno));
        log_debug(err.toLatin1().constData());
        return false;
    }

    //  真正进行设置改格式
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(cam_fd, VIDIOC_S_FMT, &format);
    if (ret != 0)
    {
        err = QString::asprintf("ioctl(VIDIOC_S_FMT) failed %d(%s)", errno, strerror(errno));
        log_debug(err.toLatin1().constData());
        return false;
    }

    //  获取相机输出的图片参数
    if(-1 == ioctl(cam_fd, VIDIOC_G_FMT, &format)){     // 得到图片格式
        err = QString("set format failed!");
        log_debug(err.toLatin1().constData());
        return false;
    }
    log_debug("fmt.type:\t\t%d",format.type);

    /**************************************************************/
    //  此处的图片格式，决定着采集到的图片数据要怎么解析，一般分为 MJPG 和 YUYV，上面设置的和此处获取的在某些情况下会不一致，因此需要此处决定解析格式
    sprintf(pix_format, "%c%c%c%c", \
             format.fmt.pix.pixelformat & 0xFF,\
            (format.fmt.pix.pixelformat >> 8) & 0xFF, \
            (format.fmt.pix.pixelformat >> 16) & 0xFF,\
            (format.fmt.pix.pixelformat >> 24) & 0xFF);

    log_debug("pix.pixelformat:\t%s", pix_format);
    log_debug("pix.width:\t\t%d",format.fmt.pix.width);
    log_debug("pix.height:\t\t%d",format.fmt.pix.height);
    log_debug("pix.field:\t\t%d",format.fmt.pix.field);

    //  查询相机需要开辟的内存信息
    //  向驱动申请视频流数据的帧缓冲区
    struct v4l2_requestbuffers req;
    req.count = buffer_count;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    ret = ioctl(cam_fd, VIDIOC_REQBUFS, &req);      // 申请若干个帧缓冲区
    if (ret != 0)
    {
        err = QString::asprintf("ioctl(VIDIOC_REQBUFS) failed %d(%s)", errno, strerror(errno));
        log_debug(err.toLatin1().constData());
        return false;
    }
    log_debug("req.count: %d", req.count);
    if (req.count < buffer_count)
    {
        err = QString::asprintf("request buffer failed");
        log_debug(err.toLatin1().constData());
        return false;
    }

    //  逐一将相机开辟的内存进行内存映射
    //  应用程序通过内存映射，将帧缓冲区的地址映射到用户空间
    struct v4l2_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    buffer.type = req.type;
    buffer.memory = V4L2_MEMORY_MMAP;
    for (int i=0; i<req.count; i++)
    {
        buffer.index = i;
        ret = ioctl (cam_fd, VIDIOC_QUERYBUF, &buffer);     // 查询帧缓冲区的内核空间中的长度和偏移量
        if (ret != 0)
        {
            err = QString::asprintf("ioctl(VIDIOC_QUERYBUF) failed %d(%s)", errno, strerror(errno));
            log_debug(err.toLatin1().constData());
            return false;
        }
        log_debug("buffer.length: %d", buffer.length);
        log_debug("buffer.m.offset: %d", buffer.m.offset);

        // 将帧缓冲区的地址映射到用户空间
        video_buffer_ptr[i] = (u8*) mmap(NULL, buffer.length, PROT_READ|PROT_WRITE, MAP_SHARED, cam_fd, buffer.m.offset);
        if (video_buffer_ptr[i] == MAP_FAILED)
        {
            err = QString::asprintf("mmap() failed %d(%s)", errno, strerror(errno));
            log_debug(err.toLatin1().constData());
            return false;
        }

        // 将申请到的帧缓冲全部放入视频采集输出队列
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;
        ret = ioctl(cam_fd, VIDIOC_QBUF, &buffer);
        if (ret != 0)
        {
            err = QString::asprintf("ioctl(VIDIOC_QBUF) failed %d(%s)", errno, strerror(errno));
            log_debug(err.toLatin1().constData());
            return false;
        }
    }

    return true;
}

bool ImageCamera::startGrab()
{
    //  打开视频流，开始视频流数据的采集
    int buffer_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int ret = ioctl(cam_fd, VIDIOC_STREAMON, &buffer_type);
    if (ret != 0)
    {
        err = QString::asprintf("ioctl(VIDIOC_STREAMON) failed %d(%s)\n", errno, strerror(errno));
        log_debug(err.toLatin1().constData());
        return false;
    }

    bKeep = true;
    start();

    return true;
}

void ImageCamera::stopGrab()
{
    bKeep = false;

    wait();

    //  停止视频的采集
    int buffer_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(cam_fd, VIDIOC_STREAMOFF, &buffer_type);
}

void ImageCamera::releaseCamera()
{
    //  关闭相机描述符
    close(cam_fd);
    close(sock);
    cam_fd = -1;

    //  取消内存映射
    struct v4l2_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    for(int i = 0; i < buffer_count; ++i)
    {
        buffer.index = i;
        ioctl (cam_fd, VIDIOC_QUERYBUF, &buffer);
        munmap(video_buffer_ptr[i], buffer.length);
    }
}

//  相机的整个采图过程在子线程中进行，可以避免卡住主线程
void ImageCamera::run()
{
    if(cam_fd == -1)
    {
        err = QString("don't init camera");
        log_debug(err.toLatin1().constData());
        return;
    }

    u8* buf = new u8[nSize];
    u8* rgbBuf = NULL;

    while (bKeep)
    {
        memset(buf, 0, nSize);
        int nImgLen = 0;

        //  抓图
        int ret = grabImage(buf, &nImgLen);
        if ( 0 != ret )
        {
            err = QString::asprintf("cam get image ret = %d\n", ret);
            log_debug(err.toLatin1().constData());
            break;
        }

        //  将抓到的图转为QImage对象，方便后续的处理
        QImage img;
        if(QString(pix_format) == QString("MJPG"))
        {
            img = QImage::fromData(buf, nImgLen, "jpg");
        }
        else if(QString(pix_format) == QString("YUYV"))
        {
            if(rgbBuf == NULL)
            {
                rgbBuf = new u8[nWidth * nHeight * 3];
            }
            memset(rgbBuf, 0, nWidth * nHeight * 3);
            send(sock, "921600          ", 16, 0);
            yuyv_to_rgb888(buf, rgbBuf, nWidth, nHeight);
            send(sock, rgbBuf, nWidth * nHeight * 3, 0);
            img = QImage(rgbBuf, nWidth, nHeight, QImage::Format_RGB888);

            char recvBuf[32];
            if(recv(sock, recvBuf, 32, 0));
//                log_debug("%s\n", recvBuf);
            //		sleep(0.1);
        }
        else
        {
            err = QString("don't suport format: %1").arg(pix_format);
            log_debug(err.toLatin1().constData());
            break;
        }

        //  转为QPixmap对象，通过QT信号发出到外部
        QPixmap pix = QPixmap::fromImage(img);
        emit image(pix);
//        send(sock, "Success !!!     ", 8, 0);
        msleep(33);         // 延时等待抓取下一帧，延时大小根据帧率大小设置
    }

    delete[] buf;
    delete[] rgbBuf;
}

int ImageCamera::grabImage(unsigned char *buf, int *imgLen)
{
    struct v4l2_buffer buffer;

    memset(&buffer, 0, sizeof(buffer));
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = buffer_count;

    //  本次抓图
    //  应用程序从视频采集输出队列中取出已含有采集数据的帧缓冲区
    if(0 != ioctl(cam_fd, VIDIOC_DQBUF, &buffer))
    {
        log_debug("ioctl(VIDIOC_DQBUF) failed %d(%s)", errno, strerror(errno));
        return -1;
    }

    if (buffer.index < 0 || buffer.index >= buffer_count)
    {
        log_debug("invalid buffer index: %d", buffer.index);
        return -100;
    }

    //  将本次抓到的图片拷贝出来
//    log_debug("dequeue done, index: %d", buffer.index);
    memcpy(buf, video_buffer_ptr[buffer.index], buffer.length);
    *imgLen = buffer.length;
//    log_debug("copy done.");

    //  将下次的图片放入队列中
    //  将新的帧缓冲放入视频采集输出队列
    if (0 != ioctl(cam_fd, VIDIOC_QBUF, &buffer))
    {
        log_debug("ioctl(VIDIOC_QBUF) failed %d(%s)", errno, strerror(errno));
        return -1;
    }

    return 0;
}
