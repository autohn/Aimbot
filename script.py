'''
import sys
from time import time,ctime
print('Today is ',ctime(time()))
def MyFunc(a,b):
    result = a + b
    print(type(result))
    return result
'''

import sys
import time
import numpy as np
import PIL.Image, PIL.ImageTk
import tkinter
import matplotlib.pyplot as plt
import matplotlib.animation as animation 
import cv2
import threading

import os
import pathlib
import tensorflow as tf
import numpy as np
from PIL import Image
import warnings
warnings.filterwarnings('ignore')   # Suppress Matplotlib warnings



# включение динамического выделения памяти gpu
gpus = tf.config.experimental.list_physical_devices('GPU')
for gpu in gpus:
    tf.config.experimental.set_memory_growth(gpu, True)


# Load the model
import time
from object_detection.utils import label_map_util
from object_detection.utils import visualization_utils as viz_utils

PATH_TO_SAVED_MODEL = "C:/Users/autoh/source/repos/tensorflow_test/my_cs_test/CSGO_inference_graph/saved_model"

print('Loading model...', end='')
start_time = time.time()

# Load saved model and build the detection function
detect_fn = tf.saved_model.load(PATH_TO_SAVED_MODEL)

end_time = time.time()
elapsed_time = end_time - start_time
print('Done! Took {} seconds'.format(elapsed_time))

category_index = label_map_util.create_category_index_from_labelmap("C:/Users/autoh/source/repos/tensorflow_test/my_cs_test/CSGO_training/labelmap.pbtxt", use_display_name=True)


def run_inference_for_single_image(model, image):

    input_tensor = tf.convert_to_tensor(image)
    input_tensor = input_tensor[tf.newaxis, ...]
    detections = detect_fn(input_tensor)
    num_detections = int(detections.pop('num_detections'))
    detections = {key: value[0, :num_detections].numpy()
                   for key, value in detections.items()}
    detections['num_detections'] = num_detections
    detections['detection_classes'] = detections['detection_classes'].astype(np.int64)
        
    return detections



















width = 1920
height = 1200
window = tkinter.Tk()
window.wm_geometry("+%d+%d" % (1920, 0))
canvas = tkinter.Canvas(window, width = width, height = height)
canvas.pack()
photo = tkinter.PhotoImage(file="C:/Users/autoh/Desktop/Мультимедиа/_картинки/woodland_wanderer_dribbble.gif")
image_id = canvas.create_image(0, 0, image=photo, anchor=tkinter.NW)
window.update()

def MainPyFunc(a):
    #start_time = time.time()

    image_np = a.reshape(height, width, 4)[400:800,690:1210,[0,1,2]]
    #image_np = a.reshape(height, width, 4)[height,width,[0,1,2]]
    #image_np = a


    im_height = image_np.shape[0]
    im_width = image_np.shape[1]





    #print(im_height, '   ', im_width)

    #with open('c:/Users/autoh/source/repos/AutoAIMOver2/log_py.txt', 'w') as logf:
    #    logf.write("--- {0} seconds ---\n".format(str((time.time() - start_time))))
    #    logf.close() 

    # Actual detection.
    detections = run_inference_for_single_image(detect_fn, image_np)
    #with open('c:/Users/autoh/source/repos/AutoAIMOver2/log_py.txt', 'a') as logf:
    #    logf.write("--- {0} seconds ---\n".format(str((time.time() - start_time))))
    #    logf.close() 

    # Visualization of the results of a detection.
    viz_utils.visualize_boxes_and_labels_on_image_array(
          image_np,
          detections['detection_boxes'],
          detections['detection_classes'],
          detections['detection_scores'],
          category_index,
          use_normalized_coordinates=True,
          max_boxes_to_draw=200,
          min_score_thresh=.30,
          agnostic_mode=False)




    # This is the way I'm getting my coordinates
    boxes = detections['detection_boxes']
    # get all boxes from an array
    max_boxes_to_draw = boxes.shape[0]
    # get scores to get a threshold
    scores = detections['detection_scores']
    # this is set as a default but feel free to adjust it to your needs
    min_score_thresh=.5
    # iterate over all objects found
    targets = np.empty((0,3))
    best_target = np.array([im_height/2, im_width/2, 3])
    for i in range(min(max_boxes_to_draw, boxes.shape[0])):
        if scores is None or scores[i] > min_score_thresh:
            # boxes[i] is the box which will be drawn
            class_name = category_index[detections['detection_classes'][i]]['name']
            print ("Левый верхний", "y:", boxes[i][0]*im_height, "x:", boxes[i][1]*im_width, "Правый нижний", "y:", boxes[i][2]*im_height, "x:", boxes[i][3]*im_width, " ", class_name, " ", detections['detection_classes'][i])
            if (detections['detection_classes'][i] == 1 or detections['detection_classes'][i] == 3): #если нашли корпус целимся в его верхнюю часть
                targets = np.append(targets, np.array([[(boxes[i][0]*im_height + 5), (boxes[i][1]*im_width + boxes[i][3]*im_width)/2, detections['detection_classes'][i]]]), axis=0)
            else: #иначе целимся в голову
                targets = np.append(targets, np.array([[(boxes[i][0]*im_height + boxes[i][2]*im_height)/2, (boxes[i][1]*im_width + boxes[i][3]*im_width)/2, detections['detection_classes'][i]]]), axis=0)

    #print(targets)
    if targets.size != 0:
        best_target = targets[0]
        for target in targets:
            if (target[2] == 2 or target[2] == 4) and (best_target[2] == 1 or best_target[2] == 3): #голова важнее корпуса
                best_target = target
            elif not(target[2] == 1 or target[2] == 3) and (best_target[2] == 2 or best_target[2] == 4):
                if ( np.sqrt(((target[0] - im_height/2)**2 + (target[1] - im_width/2)**2)) < np.sqrt(((best_target[0] - im_height/2)**2 + (best_target[1] - im_width/2)**2)) ):
                    best_target = target
        #print(best_target)



    #левый верхний высота  ширина  правый нижний высота ширина
    #1 сонтр 2 голова контра 3 контр 4 голова террора
    #print(detections['detection_boxes'])
    #print(detections['detection_scores'])



    #b = a.reshape(height, width, 4)
    #size = 960, 600
    #image.thumbnail(size)
    photo2 = PIL.ImageTk.PhotoImage(image = PIL.Image.fromarray(image_np, mode='RGB')) 
    #window.mainloop() 
    canvas.itemconfig(image_id, image=photo2)
    #window.update_idletasks()
    #with open('c:/Users/autoh/source/repos/AutoAIMOver2/log_py.txt', 'w') as logf:
    #    logf.write("--- {0} seconds ---\n".format(str((time.time() - start_time))))
    #    logf.close()   
    window.update()
    #with open('c:/Users/autoh/source/repos/AutoAIMOver2/log_py.txt', 'a') as logf:
    #    logf.write("--- {0} seconds ---\n".format(str((time.time() - start_time))))
    #    logf.close() 

    target_coords = np.array([best_target[0] - im_height/2, best_target[1] - im_width/2, best_target[2]], dtype=np.int32)
    return target_coords



#MainPyFunc(np.random.randint(255, size=(9216000), dtype=np.uint8))
#MainPyFunc(np.array(Image.open("C:/Users/autoh/.keras/datasets/image1.jpg")))




'''
def TestUpdate():
    global window
    window.update()
    window.after(50, TestUpdate)

def WindowF():
    global width
    width = 1920
    global height
    height = 1200
    global window
    window = tkinter.Tk()
    window.wm_geometry("+%d+%d" % (1920, 0))
    global canvas
    canvas = tkinter.Canvas(window, width = width, height = height)
    canvas.pack()
    photo = tkinter.PhotoImage(file="C:/Users/autoh/Desktop/Мультимедиа/_картинки/woodland_wanderer_dribbble.gif")
    global image_id
    image_id = canvas.create_image(0, 0, image=photo, anchor=tkinter.NW)
    window.update_idletasks()
    global WindowF_ready
    WindowF_ready = True
    window.after(50, TestUpdate)
    window.mainloop()

if __name__ == "__main__":
    WindowF_ready = False
    Thread1 = threading.Thread(target=WindowF)
    Thread1.start()
    time.sleep(3)
    if(WindowF_ready):
        print('ready')

def MainPyFunc(a):
    global window
    global canvas
    #start_time = time.time()
    b = a.reshape(height, width, 4)
    photo2 = PIL.ImageTk.PhotoImage(image = PIL.Image.fromarray(b, mode='RGBA')) 
    #window.mainloop()
    canvas.itemconfig(image_id, image=photo2) 
    #window.update_idletasks()
    #with open('c:/Users/autoh/source/repos/AutoAIMOver2/log_py.txt', 'w') as logf:
    #    logf.write("--- {0} seconds ---\n".format(str((time.time() - start_time))))
    #    logf.close()   
    #window.update()
    return 0
'''











'''
def MainPyFunc(a):
    b = a.reshape(1200, 1920, 4)
    with open('c:/Users/autoh/source/repos/AutoAIMOver2/errors_py.txt', 'w') as logf:
        logf.write("-- {0}, {1}, {2}--\n".format(str(b.shape),str(b),str(a[2])))
        logf.close() 
    image = Image.fromarray(b, mode='RGBA').show()
    return 0
'''
'''
i = 0
def MainPyFunc(a):
    global i
    i += 1
    with open('c:/Users/autoh/source/repos/AutoAIMOver2/errors_py.txt', 'w') as logf:
        logf.write("-- {0}, {1}, {2}--\n".format(str(type(a[0])),str(type(np.sum(a))),str(a[2])))
        logf.close()
    return np.sum([a[0], a[1], a[2], a[3]])
'''  
'''
def MainPyFunc(a):
    with open('c:/Users/autoh/source/repos/AutoAIMOver2/errors_py.txt', 'w') as logf:
        logf.write("-- {0} --\n".format(str(a.ndim)))
        logf.close()
    np.reshape(a, (4, 1920, 1200))
    b = np.array([[1,2,3], [4,5,6], [4,5,6], [4,5,6], [4,5,6], [4,5,6], [4,5,6], [4,5,6]])
    b.tofile('c:/Users/autoh/source/repos/AutoAIMOver2/errors_py.txt', sep=",", format="%d")
    return 0
'''
'''
a = np.array([[1,2,3], [4,5,6], [4,5,6], [4,5,6], [4,5,6], [4,5,6], [4,5,6], [4,5,6]])
MainPyFunc(a)
'''
'''
    a = 1 / 0
    try:
        a = 1 / 0
    except Exception as e:
        exc_type, exc_value, exc_tb = sys.exc_info()
        error = traceback.format_exception(exc_type, exc_value, exc_tb)
    print(error)
    return 0
try:
    MainPyFunc(1)
except Exception:
    print(sys.exc_info()[1])
'''
'''
print(sys.last_value)
try:
    window=Tk()
    window.title(type(a))
    window.geometry("300x200+10+20")
    window.after(3000, window.destroy)
    window.mainloop()
except Exception as e:
    with open('c:/Users/autoh/source/repos/AutoAIMOver2/errors_py.txt', 'w') as logf:
        logf.write("-- {0} --\n".format(str(e)))
        logf.close()
finally:
    pass'''



