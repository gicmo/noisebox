# -*- coding: utf-8 -*-
from flask import Flask, jsonify, Response
import zmq
import json
import struct
import time
import datetime

context = zmq.Context()
app = Flask(__name__)


@app.route('/data')
def data():
    ipc = context.socket(zmq.REQ)
    ipc.connect("tcp://localhost:5557")
    now = time.time()
    delta = datetime.timedelta(weeks=-1)
    t_start = now + delta.total_seconds()
    ipc.send(struct.pack('III', 2, t_start, now))
    js_data = ipc.recv_json()
    return jsonify(js_data)


@app.route('/subscribe')
def subscribe():
    zmq_ctx = context
    updates = zmq_ctx.socket(zmq.SUB)
    updates.connect("tcp://localhost:5556")
    updates.setsockopt_string(zmq.SUBSCRIBE, u'')

    poller = zmq.Poller()
    poller.register(updates, zmq.POLLIN)

    def loop():
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

                js = json.dumps({known_ut[ut]: val})
                sse_data = 'data: %s \n\n' % js
                yield sse_data

    return Response(loop(), mimetype="text/event-stream")


if __name__ == '__main__':
    app.run()

