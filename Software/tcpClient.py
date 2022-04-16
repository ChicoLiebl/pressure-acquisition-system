import time
import sys
import struct
import socket
import numpy as np
import logging

from threading import Thread

from numpy.core.multiarray import array

class TcpClient():
  def __init__(
      self, address: str, dataFormat: str, onDataCb, 
      packetHeader=b'\xFD', maxPacketLen=2048, port=3333
    ):
    self.serverAddr = address
    self.port = port
    self.socket = None
    self.dataFormat = dataFormat
    self.header = packetHeader
    self.maxPacketLen = maxPacketLen
    self.onDataCb = onDataCb

    self.currException = None
    
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
      tries = 0
      while self.isReceiving != True and self.isRun == True:
        time.sleep(0.1)
        tries += 1
        if tries > 10:
          logging.error('TCP timeout on receive')
          raise TimeoutError
      if not self.isRun:
        raise self.currException
  
  def closeConnection(self):
    self.isRun = False
    self.socket.close()
    if (self.socketThread != None):
      self.socketThread.join()

  def __socketTask(self):
    totalLen = 0
    packet = bytes([])
    while self.isRun:
      
      try:
        rawData = self.socket.recv(2048)
      except Exception as e:
        self.currException = e
        self.isRun = False
        self.socket.close()
        continue

      if not rawData:
        continue
      self.isReceiving = True
      rawDataLen = rawData.__len__()
      totalLen += rawDataLen
      if (totalLen >= self.maxPacketLen):
        copyLen = rawDataLen - (totalLen - self.maxPacketLen)
        packet += rawData[:copyLen]
        unpacked = np.array(list(struct.iter_unpack(self.dataFormat, packet))).transpose()[0]
        unpackedLen = unpacked.size
        logging.debug(f'Received {totalLen} bytes')
        self.onDataCb(unpacked, unpackedLen)
        
        totalLen -= self.maxPacketLen
        if totalLen == 0:
          packet = bytes([])
        else:
          packet = rawData[copyLen:]

      else:
        packet += rawData
    
      
# -----------  Config  ----------
PORT = 3333
# -------------------------------

""" TODO: Update example """
if __name__ == '__main__':
  if not(sys.argv[2:]):
    print('Usage: example_test.py <server_address> <message_to_send_to_server>')
    exit(0)
  
  data = np.array([0] * 4096)
  # print(data)

  addr = sys.argv[1]
  msg = sys.argv[2]
  tcpStream = TcpClient(addr, data, 'H', port=PORT)

  tcpStream.connect(msg)

  while 1:
    try:
      print(data)
      time.sleep(0.1)
    except KeyboardInterrupt:
      tcpStream.closeConnection()
      exit(0)
    