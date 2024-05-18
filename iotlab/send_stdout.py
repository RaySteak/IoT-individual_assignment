import time
import numpy as np
import struct
import sys

SEND_RATE = 2000

def sine(A, f, t):
    return A * np.sin(2 * np.pi * f * t)

def sensor_sample(t):
    # !!! Change signal here if using python mode
    return sine(1, 10, t)
    #

start = time.perf_counter()
cur_time = 0
while True:
    value = sensor_sample(time.perf_counter() - start)
    # value = sensor_sample(cur_time)
    data = struct.pack('f', value)
    sys.stdout.buffer.write(data)
    sys.stdout.flush()
    time.sleep(1 / SEND_RATE)
    cur_time += (1 / SEND_RATE)
