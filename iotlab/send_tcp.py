import socket
import time
import numpy as np
import struct
import sys
import threading

HOST = "m3-95.grenoble.iot-lab.info"
PORT = 20000

SEND_RATE = 2000

s = socket.socket()
s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, True)
s.connect((HOST, PORT))

def recv_thread(s):
    while True:
        r = s.recv(1024)
        sys.stdout.buffer.write(r)
        sys.stdout.flush()

r = threading.Thread(target = recv_thread, args = [s])
r.start()

def sine(A, f, t):
    return A * np.sin(2 * np.pi * f * t)

def sensor_sample(t):
    # !!! Change signal here if using python mode
    return sine(1, 10, t)
    #

start = time.perf_counter()
cur_time = 0
while True:
    ##
    value = sensor_sample(time.perf_counter() - start)
    # value = sensor_sample(cur_time)
    ##
    data = struct.pack('f', value)
    
    ## THROUGH TCP OR STDOUT
    s.sendall(data)
    # sys.stdout.buffer.write(data)
    # sys.stdout.flush()
    ##
    time.sleep(1 / SEND_RATE)
    cur_time += (1 / SEND_RATE)