#!/usr/bin/env python
# coding=utf-8
from __future__ import print_function
from __future__ import division

import argparse
import zmq
import struct
import time
import datetime
import scipy.signal as signal
import matplotlib.pylab as plt
import numpy as np
import calendar
import dateutil


def get_data_zmq(url, t_start):
    context = zmq.Context()
    ipc = context.socket(zmq.REQ)
    ipc.connect(url)
    now = time.time()
    ipc.send(struct.pack('III', 2, t_start, now))
    js_data = ipc.recv_json()
    lst = map(lambda x: (x['timestamp'], x['temp'], x['humidity']),  js_data['data'])
    return np.array(lst)


def plot_data(data):
    fig, ax1 = plt.subplots()
    t = map(datetime.datetime.fromtimestamp, data[:, 0])
    temperature = data[:, 1]
    humidity = data[:, 2]

    b, a = signal.butter(8, 0.25)
    temperature = signal.filtfilt(b, a, temperature)

    ax1.plot(t, temperature, color='red')
    ax1.set_ylabel('Temperature', color='red')

    ax2 = ax1.twinx()
    ax2.plot(t, humidity, color='dodgerblue', label="Humidity")
    ax2.set_ylabel('Humidity', color='dodgerblue')

    fig.autofmt_xdate()


def parse_date(datestr):
    dt = dateutil.parser.parse(datestr)
    return calendar.timegm(dt.timetuple())


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Noisebox Plotter')
    parser.add_argument('start')
    args = parser.parse_args()

    dt = parse_date(args.start)

    data = get_data_zmq("tcp://noisebox.neuro.bzm:5557", dt)
    print(data)
    plot_data(data)
    plt.show()