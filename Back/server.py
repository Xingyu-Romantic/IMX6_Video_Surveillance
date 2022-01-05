import socket
import struct
import cv2
import time
import numpy as np
from infer import DetectorPicoDet, PredictConfig
from visualize import visualize_box_mask
from base_camera import BaseCamera

 
def recv_size(sock, count):
    buf = b''
    while count:
        newbuf = sock.recv(count)
        if not newbuf: return None
        buf += newbuf
        count -= len(newbuf)
    return buf

def recv_all(sock, count):
    buf = b''
    while count:
        # 这里每次只接收一个字节的原因是增强python与C++的兼容性
        # python可以发送任意的字符串，包括乱码，但C++发送的字符中不能包含'\0'，也就是字符串结束标志位
        newbuf = sock.recv(307200)
        if not newbuf: return None
        buf += newbuf
        count -= len(newbuf)
    return buf
def rgb565_to_rgb(rbg565):
    width, height = rgb565.shape[:2]
    rgbArray = np.zeros((width, height, 3), np.uint8)
    rr = rgb565[..., 1] & 0xf8
    gg = (rgb565[..., 1] << 5) | ((rgb565[..., 0] & 0xe0) >> 3)
    bb = rgb565[..., 0] << 3
    rgbArray[..., 2] = rr | ((rr & 0x38) >> 3)
    rgbArray[..., 1] = gg | ((gg & 0x0c) >> 2)
    rgbArray[..., 0] = bb | ((bb & 0x38) >> 3)
    return rgbArray


class Camera(BaseCamera):
    @staticmethod
    def frames():
        # 在这里实现自己视频帧的获取和处理
        pred_config = PredictConfig('./picodet_s_320_coco')
        detector_func = 'DetectorPicoDet'
        detector = eval(detector_func)(pred_config, './picodet_s_320_coco')
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.bind(('0.0.0.0',1234))
        server.listen(5)
        print("waiting...")
        client, addr = server.accept()
        print(addr)
        i = 0
        while True:
            length = recv_size(client, 16).decode()
            # length = 921600
            if isinstance(length, str):
                data = recv_all(client, int(length))
                # rgb565 = np.frombuffer(data, np.uint8).reshape((480, 640, 2))
                img = np.frombuffer(data, np.uint8).reshape((480, 640, 3))
                img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
                # rgbArray = rgb565_to_rgb(rgb565)[:120, :320]
                # img = cv2.resize(rgbArray, (320, 320))
                results = detector.predict([img], 0.5)
                im = visualize_box_mask(
                    img,
                    results,
                    detector.pred_config.labels,
                    threshold=0.5)
                im = np.array(im)
                img_encode = cv2.imencode('.jpg', im)[1]
                img_byte = img_encode.tobytes()
                yield img_byte
                client.send(bytes("Server has recieved messages !", encoding='utf-8'))
                # client.send(im_encode)
                # cv2.imshow("Test", im)
                # if cv2.waitKey(1) == 27:
                #     break
                # # print(recv_size(client, 8).decode())
                
            # img_decoded = cv2.imdecode(img_nparr, cv2.IMREAD_COLOR)
            # print(img_nparr[:-1].reshape((240, 320)))
            # print(img_decoded)
            # if cv2.waitKey(1) == 27:
            #         break
            