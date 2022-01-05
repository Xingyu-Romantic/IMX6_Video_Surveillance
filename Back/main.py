from flask import Flask, render_template, Response
from flask_bootstrap import Bootstrap
import cv2
from server import Camera
from flask_nav import Nav
from flask_nav.elements import *
import matplotlib
import matplotlib.pyplot as plt
import numpy as np
import random
import base64
import time
from io import BytesIO
app = Flask(__name__, template_folder='./static/templates')
Bootstrap(app)
matplotlib.use('Agg')
# nav=Nav()
# nav.register_element('top',Navbar(u'视频监控系统',
#                                     View(u'主页','index'),
#                                     Subgroup(u'监控',
#                                              View(u'项目一','index'),
#                                              Separator(),
#                                              View(u'项目二', 'index'),
#                                     ),
# ))

# nav.init_app(app)

@app.route('/')
def index():
    """Video streaming home page."""
    return render_template('index.html')
 
 
def gen(camera):
    """Video streaming generator function."""
    while True:
        frame = camera.get_frame()
        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')

def gen_plot():
    
    pervious = list(np.random.randint(5,10,size=10))
    time.sleep(5)
    while True:
        plt.clf()
        buffer = BytesIO()
        new = random.randint(5, 10)
        pervious.append(new)
        plt.plot(pervious)
        plt.savefig(buffer)
        plot_data = buffer.getvalue()
        img = np.array(plot_data).tobytes()
        yield (b'--frame\r\n'
               b'Content-Type: data:image/png;base64, \r\n\r\n' + img + b'\r\n')
        pervious = pervious[1:]
        time.sleep(1)

 
@app.route('/video_feed')
def video_feed():
    """Video streaming route. Put this in the src attribute of an img tag."""
    return Response(gen(Camera()),
                    mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/matplot_feed')
def matplot_feed():
    return Response(gen_plot(),
                    mimetype='multipart/x-mixed-replace; boundary=frame')

if __name__ == '__main__':
    app.run(host='0.0.0.0', threaded=True)