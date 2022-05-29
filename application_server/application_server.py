# Libs
import base64
import os, sys, logging, time
import paho.mqtt.client as mqtt
import json
import csv
from datetime import datetime
import codecs

import pandas as pd
import numpy as np

# Parameters - CHANGE FOR YOUR APPLICATION
User = "app-tcc-tolerancia-a-falhas@ttn" #Application_id@ttn
Password = "NNSXS.DV5HGU3NIIGDTQB5VP6KLCZ46LPBZ36G2Z446CQ.TWRPLH7ZUH4DWMRNVRURRFHKOHVNEPGSCAROOXHPRHMNOLPTZN4Q" # Token generated in the things stack
Server = "au1.cloud.thethings.network" # Network server
DEVID = "ttgo-app-tolerante-a-falhas" # dev_id of device application

# Global
topic = "v3/"+ User + "/devices/"+DEVID+"/down/push"
message = '{"downlinks":[{"f_port": 1,"frm_payload":"vu8=","priority": "NORMAL"}]}'

list_of_fcnt = []
fcnt_retrans_list = []
fcnt_retrans_request = []
count_next_retrans = 0

# Used to compare algorithms, store json data
list_cr_lorawan = []
list_alr_lorawan = []

# Used to store data received, better use just list_alr_lorawan if want to store data
df_fcnts = pd.DataFrame(columns=['time', 'fcnt', 'fcnt_raw'])
df_fcnt_retrans_request = pd.DataFrame(columns=['retransmission', 'fcnt'])

def on_connect(mqttc, obj, flags, rc):
  global count_retransmission
  global pkt_not_received
  count_retransmission = 0
  pkt_not_received = []
  print("Connection: rc = " + str(rc))

def on_subscribe(mqttc, obj, mid, granted_qos):
	print("Subscribe: " + str(mid) + " " + str(granted_qos))

def on_log(mqttc, obj, level, string):
	print("Log: "+ string)
	logging_level = mqtt.LOGGING_LEVEL[level]
	logging.log(logging_level, string)

def on_publish(client,userdata,result):
  print("client:",client," userdata:",userdata," result:",result)
  print("Published \n")
  pass

def on_message(mqttc, obj, msg):
  print("Message: " + msg.topic + " " + str(msg.qos)))

  payload = json.loads(msg.payload)
  print("Payload: ", payload)

  if(payload['end_device_ids']['device_id'] == 'ttgo-crlorawan'):
    print('-- CR LORAWAN ', payload)
    if ("uplink_message" in payload) and ("f_cnt" in payload["uplink_message"]):
      print('---- CR LORAWAN - SAVE - FCNT: ', payload["uplink_message"]["f_cnt"])
      list_cr_lorawan.append(payload)
    return

  print('-- ALR LORAWAN ', payload)
  if ("uplink_message" in payload) and ("f_cnt" in payload["uplink_message"]):
    
    print('---- ALR LORAWAN - SAVE - FCNT: ', payload["uplink_message"]["f_cnt"])
    list_alr_lorawan.append(payload)

    global count_retransmission
    global pkt_not_received
    global count_next_retrans
    global fcnt_retrans_request

    global df_fcnts # For Analysis
    global df_fcnt_retrans_request # For Analysis

    fcnt = payload["uplink_message"]["f_cnt"]

    list_of_fcnt.append(payload["uplink_message"]["f_cnt"])
    list_of_fcnt.sort()
    
    all_list = list(range((list_of_fcnt[0]), list_of_fcnt[-1]+1))
    fcnt_retrans_list = list(set(all_list) - set(list_of_fcnt))

    print("-- uplink_message received - FCNT: ", fcnt)
    print("uplink_message: ", payload["uplink_message"])
    print("list_of_fcnt: ", list_of_fcnt)
    print("fcnt_retrans_list: ", fcnt_retrans_list)

    if "decoded_payload" in payload["uplink_message"]:
      decoded_payload = payload["uplink_message"]["decoded_payload"]
      fcnt_raw = decoded_payload['fcnt']
      df_fcnts = df_fcnts.append({'time':payload["received_at"], 'fcnt':fcnt, 'fcnt_raw':fcnt_raw},ignore_index=True) # For Analysis

      if (len(fcnt_retrans_list)!=0) and (fcnt_raw in fcnt_retrans_list):
        fcnt_retrans_list.remove(fcnt_raw)
        list_of_fcnt.insert(0, fcnt_raw)

      print("--- decoded_payload: ", decoded_payload, " fcnt_retransmission: ", fcnt_raw)

    msg_downling = ''
    if len(fcnt_retrans_list) > 0 and count_next_retrans == 0:
      fcnt_retrans_list.sort()
      num_package_retrans = 1 # Retransmission factor N
      count_next_retrans = 2 # Dalay for retransmission request

      if len(fcnt_retrans_list) >= num_package_retrans: 
        for retranmite_fcnt in fcnt_retrans_list[0:num_package_retrans]:
          fcnt_retrans_request.append(retranmite_fcnt)
          df_fcnt_retrans_request = df_fcnt_retrans_request.append({'retransmission':0, 'fcnt':retranmite_fcnt},ignore_index=True)
          
          retranmite_fcnt = str(hex(retranmite_fcnt)[2:])
          retranmite_fcnt = retranmite_fcnt.zfill(4)
          msg_downling = msg_downling + retranmite_fcnt
          
      base64_message = codecs.encode(codecs.decode(msg_downling, 'hex'), 'base64').decode().replace("\n", "")
      
      message = '{"downlinks":[{"f_port": 1,"frm_payload":"'+base64_message+'","priority": "NORMAL"}]}'
      print("-- Try pub ", message, ' msg_downling ',msg_downling)
      mqttc.publish(topic,message)

      count_retransmission = count_retransmission + 1
    
    print('----count_retransmission: ', count_retransmission, ' msg_downling: ', msg_downling, ' count_next_retrans: ', count_next_retrans, ' fcnt_retransmited: ', fcnt_retrans_request)
    
    if count_next_retrans != 0:
      count_next_retrans = count_next_retrans-1

def main():
    
  print("Initialization MQTT Client")
  mqttc = mqtt.Client()

  print("Update MQTT Modules callback")
  mqttc.on_connect = on_connect
  mqttc.on_subscribe = on_subscribe
  mqttc.on_message = on_message
  mqttc.on_publish = on_publish

  mqttc.username_pw_set(User, Password) # Setup authentication from settings above
  mqttc.tls_set()	# default certification authority of the system

  mqttc.connect(Server, 8883, 60)

  print("Subscribe")
  mqttc.subscribe("#", 0)	# all device uplinks

  print("And run forever")
  try:    
    while True:
      mqttc.loop(10) 	# seconds timeout / blocking time
      print(".", end="", flush=True)	# feedback to the user that something is actually happening
      
  except KeyboardInterrupt:
    print("Exit")
    sys.exit(0)

if __name__ == '__main__':
  main()