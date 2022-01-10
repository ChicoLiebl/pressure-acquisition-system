# from __future__ import print_function
# from __future__ import unicode_literals
import os
import time
import sys
import copy
import struct
import socket
import numpy as np
import logging

from threading import Thread

class TcpClient():
  def __init__(
      self, address: str, data: np.ndarray, dataFormat: str, 
      packetHeader=b'\xFD', maxPacketLen=1024, port=3333
    ):
    self.serverAddr = address
    self.port = port
    self.socket = None
    self.outData = data
    self.dataFormat = dataFormat
    self.bufferLength = data[0].size
    self.header = packetHeader
    self.maxPacketLen = maxPacketLen
    
    self.isRun = False
    self.socketThread = None
    self.isReceiving = False

  def connect(self, startMsg=None):
    for res in socket.getaddrinfo(self.serverAddr, PORT, socket.AF_UNSPEC,
                                  socket.SOCK_STREAM, 0, socket.AI_PASSIVE):
        family_addr, socktype, proto, canonname, addr = res
    try:
        self.socket = socket.socket(family_addr, socket.SOCK_STREAM)
    except socket.error as msg:
      logging.error(f'Could not create socket: {msg[0]} : {msg[1]}')
      raise
    try:
        self.socket.connect(addr)
    except socket.error as msg:
      logging.error(f'Could not open socket: {msg}')
      self.socket.close()
      raise
    if (startMsg != None):
      self.socket.sendall(bytes(startMsg, 'utf-8'))

    self.isRun = True
    if self.socketThread == None:
      self.socketThread = Thread(target=self.__socketTask)
      self.socketThread.start()
      logging.info('Socket thread started')
      # Block till we start receiving values
      while self.isReceiving != True:
        time.sleep(0.1)
  
  def closeConnection(self):
    self.isRun = False
    self.socket.close()
    self.socketThread.join()

  def __socketTask(self):
    packetHeaderLen = self.header.__len__()
    # packetSize = struct.calcsize(self.dataFormat) + packetHeaderLen
    # packets = 10

    totalLen = 0
    packet = bytes([])
    while self.isRun:

      # while totalLen < self.maxPacketLen:

      rawData = self.socket.recv(512)
      if not rawData:
        continue
      self.isReceiving = True
      rawDataLen = rawData.__len__()
      # print(f'totalLen: {totalLen}, paketLen: {packet.__len__()}, dataLen: {rawDataLen}')
      totalLen += rawDataLen
      if (totalLen >= self.maxPacketLen):
        # print(f'totalLen: {totalLen}, dataLen: {rawDataLen}, copyLen: {rawDataLen - (totalLen - self.maxPacketLen)}')
        copyLen = rawDataLen - (totalLen - self.maxPacketLen)
        packet += rawData[:copyLen]
        unpacked = np.array(list(struct.iter_unpack(self.dataFormat, packet)))
        print(f'Received: {packet.__len__()} bytes, byte[0] = {unpacked[0, 0]}')
        totalLen -= self.maxPacketLen
        if totalLen == 0:
          packet = bytes([])
        else:
          packet = rawData[copyLen:]

      else:
        packet += rawData



        # print(rawDataLen)

      
# -----------  Config  ----------
PORT = 3333
# -------------------------------

if __name__ == '__main__':
  if not(sys.argv[2:]):
    print('Usage: example_test.py <server_address> <message_to_send_to_server>')
    exit(0)
  
  data = np.array([0] * 1024)
  print(data)

  addr = sys.argv[1]
  msg = sys.argv[2]
  tcpStream = TcpClient(addr, data, 'h', port=PORT)

  tcpStream.connect(msg)

  while 1:
    try:
      time.sleep(0.1)
    except KeyboardInterrupt:
      tcpStream.closeConnection()
      exit(0)
    