#!/usr/bin/env python
# coding=utf-8
from __future__ import print_function
from __future__ import division

import sys

import time
import datetime
import struct
import zmq
from PySide import QtGui, QtCore
import pyqtgraph as pg
import numpy as np


class TimeAxisItem(pg.AxisItem):
    def __init__(self, *args, **kwargs):
        super(TimeAxisItem, self).__init__(*args, **kwargs)

    def tickStrings(self, values, scale, spacing):
        # PySide's QTime() initialiser fails miserably and dismisses args/kwargs
        return [QtCore.QDateTime.fromTime_t(value).toString('d.M - hh:mm:ss') for value in values]


class NBGraph(QtGui.QMainWindow):

    def __init__(self):
        super(NBGraph, self).__init__()

        self.setGeometry(0, 0, 800, 400)
        self.setWindowTitle("CI - Viewer - PC Activation")
        p = self.palette()
        p.setColor(self.backgroundRole(), QtCore.Qt.black)
        self.setAutoFillBackground(True)
        self.setPalette(p)

        context = zmq.Context()
        self.zmq_ctx = context
        socket = context.socket(zmq.SUB)

        socket.connect("tcp://noisebox.neuro.bzm:5556")
        socket.setsockopt_string(zmq.SUBSCRIBE, u"")

        cw = QtGui.QWidget()
        self.setCentralWidget(cw)
        layout = QtGui.QGridLayout()

        self._lbl_temp = QtGui.QLabel('NA')
        self._lbl_hudt = QtGui.QLabel('NA')
        self._lbl_temp.setStyleSheet("QLabel { color: red; font-size: 36pt; }")
        self._lbl_hudt.setStyleSheet("QLabel { color: blue; font-size: 36pt; }")

        layout.addWidget(self._lbl_temp, 0, 0)
        layout.addWidget(self._lbl_hudt, 0, 1)

        pw = pg.PlotWidget(name='Population Code activation',
                           axisItems={'bottom': TimeAxisItem(orientation='bottom')})

        pw.addLegend()

        cw.setLayout(layout)
        layout.addWidget(pw, 1, 0, 1, 2)
        pw.enableAutoRange(axis=pg.ViewBox.YAxis)

        self._temp = pg.PlotCurveItem(name='temperature')
        self._hudt = pg.PlotCurveItem(name='humidity')
        pw.addItem(self._temp)
        pw.addItem(self._hudt)

        self.data = self.get_data()
        self.plot_data(self.data)


        self._sub = socket
        poller = zmq.Poller()
        poller.register(self._sub, zmq.POLLIN)
        self._poller = poller
        t = QtCore.QTimer()
        t.timeout.connect(self.get_live_data)
        t.start(150)
        self._ld_timer = t

        t = QtCore.QTimer()
        t.timeout.connect(self.update_data)
        t.start(1000*5)
        self._ud_timer = t

    def plot_data(self, data):
        self._temp.setData(data[0, :], data[1, :], pen='#ff325b')
        self._hudt.setData(data[0, :], data[2, :], pen='#3182fc')

    def get_live_data(self):
        ready = dict(self._poller.poll(50))
        if self._sub in ready:
            msg = self._sub.recv()
            ut, _, _, val = struct.unpack('HHIf', msg)
            if ut == 1:
                self._lbl_temp.setText(u' %3.2f °C' % val)
            elif ut == 2:
                self._lbl_hudt.setText('%3.2f %%' % val)

    def get_data(self, t_start=None):
        context = self.zmq_ctx
        ipc = context.socket(zmq.REQ)
        ipc.connect("tcp://noisebox.neuro.bzm:5557")
        now = time.time()
        if t_start is None:
            delta = datetime.timedelta(weeks=-1)
            t_start = now + delta.total_seconds()
        ipc.send(struct.pack('III', 2, t_start, now))
        js_data = ipc.recv_json()
        the_data = js_data['data']
        if the_data is None:
            return None
        data = np.array(map(lambda x: (x['timestamp'], x['temp'], x['humidity']), the_data)).T
        return data

    def update_data(self):
        data = self.data
        s = data.shape
        last_update = data[0, s[1]-1]
        new_data = self.get_data(last_update+1)
        if new_data is None or not len(new_data):
            return
        data = np.concatenate((data, new_data), axis=1)
        self.data = data
        self.plot_data(self.data)


if __name__ == '__main__':
    app = QtGui.QApplication(sys.argv)
    ui = NBGraph()
    ui.show()
    sys.exit(app.exec_())