#!/usr/bin/env python

from time import sleep
import sys
import zmq
import json
from Adafruit_CharLCDPlate import Adafruit_CharLCDPlate

lcd = Adafruit_CharLCDPlate()

lcd.begin(16, 2)
lcd.clear()
lcd.backlight(lcd.ON | lcd.RED)
lcd.message("Noisebox Monitor")
sleep(1)

context = zmq.Context()
socket = context.socket(zmq.SUB)

print("Noisebox monitor - LCD")
sys.stdout.write("Waiting for monitor daemon...")
sys.stdout.flush()
socket.connect("tcp://localhost:5556")
socket.setsockopt_string(zmq.SUBSCRIBE, u"")

while True:
    msg = socket.recv_string()
    js = json.loads(msg)
    lcd.clear()
    temp = js["temperature"]
    sys.stdout.write("\r T: %3.5f%s" % (temp, " "*17))
    sys.stdout.flush()
    lcd.message("T: %3.2f" % temp)

