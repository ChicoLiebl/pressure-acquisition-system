import asyncio
import logging
from enum import Enum

from PyQt5 import QtWidgets
from PyQt5.QtCore import QThread, pyqtSignal

from bleak import BleakScanner, BleakClient

IP_UUID = "f3641001-00b0-4240-ba50-05ca45bf8abc"
STATUS_UUID = "f3641010-00b0-4240-ba50-05ca45bf8abc"
CMD_UUID = "f3641021-00b0-4240-ba50-05ca45bf8abc"

class BLEOperations(Enum):
  NOP = 0
  SCAN = 1
  CONNECT = 2
  DISCONNECT = 3
  GET_WIFI_LIST = 4

class BLEThread(QThread):
  listUpdateSignal = pyqtSignal(list)
  bleConnectedSignal = pyqtSignal(list)
  bleDisconnectedSignal = pyqtSignal()

  def __init__(self):
    super(BLEThread, self).__init__()
    self.quit_flag = False
    self.op = BLEOperations.NOP
    self.callback = None
    self.client : BleakClient() = None

  async def __findDevices(self):
    allDevices = await BleakScanner.discover()
    deviceList = []
    for d in allDevices:
      if d.name[:3] == 'PAS':
        deviceList += [(d.name, d.address)]
    return deviceList

  def resetOperation(self):
    self.op = BLEOperations.NOP

  @asyncio.coroutine
  async def __asyncThread(self):
    while True:
      match self.op:
        case BLEOperations.NOP:
          await asyncio.sleep(0.1)
          continue
        case BLEOperations.SCAN:
          self.resetOperation()
          devices = await asyncio.gather(self.__findDevices())
          print(devices)
          if (self.callback != None):
            self.callback(devices[0])
          self.callback == None
        case BLEOperations.CONNECT:
          self.resetOperation()
          try:
            await self.client.connect()
          except Exception as e:
            logging.error(f'Failed connection to client\n{e}')
            continue
          print('connected')
          self.client._mtu_size = 512
          ip = await self.client.read_gatt_char(IP_UUID)
          netStatus = await self.client.read_gatt_char(STATUS_UUID)
          if (self.callback != None):
            self.callback(ip, netStatus)
        case BLEOperations.DISCONNECT:
          self.resetOperation()
          await self.client.disconnect()
          self.callback()
          

  def run(self):
    asyncio.run(self.__asyncThread())
    self.quit()

class BLEManager():
  
  def __init__(self) -> None:
    self.thread = BLEThread()
    self.listUpdateSignal = self.thread.listUpdateSignal
    self.bleConnectedSignal = self.thread.bleConnectedSignal
    self.bleDisconnectedSignal = self.thread.bleDisconnectedSignal

    self.deviceConnected = False
    self.thread.start()
    pass

  def __onDeviceList(self, devices):
    self.listUpdateSignal.emit(devices)

  def scanDevices(self):
    self.thread.callback = self.__onDeviceList
    self.thread.op =  BLEOperations.SCAN

  def __onConnection(self, ip, status):
    self.deviceConnected = True
    self.bleConnectedSignal.emit([ip, status])


  def connectToDevice(self, addr):
    self.thread.client = BleakClient(addr)
    self.thread.callback = self.__onConnection
    self.thread.op =  BLEOperations.CONNECT

  def __onDisconnection(self):
    self.deviceConnected = False
    self.bleDisconnectedSignal.emit()

  def closeConnection(self):
    self.thread.callback = self.__onDisconnection
    self.thread.op = BLEOperations.DISCONNECT


