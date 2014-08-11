# -*- coding: utf-8 -*-
from flask import Flask, jsonify, Response, redirect
from flask_sockets import Sockets
import zmq
import json
import struct
import time
import datetime

context = zmq.Context()
app = Flask(__name__)
sockets = Sockets(app)


def get_live_data():
    zmq_ctx = context
    updates = zmq_ctx.socket(zmq.SUB)
    updates.connect("tcp://localhost:5556")
    updates.setsockopt_string(zmq.SUBSCRIBE, u'')

    poller = zmq.Poller()
    poller.register(updates, zmq.POLLIN)

    while True:
        ready = dict(poller.poll())

        if updates in ready:
            update = updates.recv()
            ut, _, _, val = struct.unpack('HHIf', update)

            known_ut = {
                1: 'temperature',
                2: 'humidity'
            }

            if ut not in known_ut:
                continue

            yield json.dumps({known_ut[ut]: val})


@app.route('/')
def index():
    return redirect("/index.htm", code=301)


@app.route('/api/data')
def data():
    ipc = context.socket(zmq.REQ)
    ipc.connect("tcp://localhost:5557")
    now = time.time()
    delta = datetime.timedelta(weeks=-1)
    t_start = now + delta.total_seconds()
    ipc.send(struct.pack('III', 2, t_start, now))
    js_data = ipc.recv_json()
    return jsonify(js_data)


@app.route('/api/subscribe')
def subscribe():
    def sse_data():
        for update in get_live_data():
            sse_data = 'data: %s \n\n' % update
            yield sse_data
    return Response(sse_data(), mimetype="text/event-stream")


@sockets.route('/api/live')
def echo_socket(ws):
    try:
        for update in get_live_data():
            ws.send(update)
            message = ws.receive()
            if message != 'ack':
                break
    except:
        print('ws error')


if __name__ == '__main__':
    app.run()

