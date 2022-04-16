import asyncio
import logging
from enum import Enum

from PyQt5.QtCore import QThread, pyqtSignal

from bleak import BleakScanner, BleakClient
import pydbus
import os

import protobuf.configuration_pb2 as proto
from google.protobuf.json_format import MessageToJson

IP_UUID = "f3641001-00b0-4240-ba50-05ca45bf8abc"
STATUS_UUID = "f3641010-00b0-4240-ba50-05ca45bf8abc"
CMD_UUID = "f3641021-00b0-4240-ba50-05ca45bf8abc"
CONF_UUID = "f3641020-00b0-4240-ba50-05ca45bf8abc"
class BLEOperations(Enum):
  NOP = 0
  SCAN = 1
  CONNECT = 2
  DISCONNECT = 3
  READ_CONF = 4
  SEND_COMMAND = 5

def disconnectConnectedDevices():
  if os.name == 'posix':
    bus = pydbus.SystemBus()
    mngr = bus.get('org.bluez', '/')
    mngd_objs = mngr.GetManagedObjects()
    for path in mngd_objs:
      deviceProxy = mngd_objs[path].get('org.bluez.Device1', {})
      con_state = deviceProxy.get('Connected', False)
      if con_state:
        addr = mngd_objs[path].get('org.bluez.Device1', {}).get('Address')
        name = mngd_objs[path].get('org.bluez.Device1', {}).get('Name')
        if name[:3] == 'PAS':
          print(f'Device {name} [{addr}] is connected')
          os.system(f'bluetoothctl disconnect {addr}')


class BLECLient(QThread):
  scanDoneSignal = pyqtSignal(list)
  bleConnectedSignal = pyqtSignal(list)
  bleDisconnectedSignal = pyqtSignal()
  bleConfigUpdatedSignal = pyqtSignal()
  bleFailedSignal = pyqtSignal()

  def __init__(self):
    super(BLECLient, self).__init__()
    self.quit_flag = False
    self.op = BLEOperations.NOP
    self.client : BleakClient() = None
    self.deviceConnected = False
    self.commandData = None
    self.deviceConfiguration = proto.configuration()
    self.deviceAddr = ''

    disconnectConnectedDevices()

    self.start()

  async def __findDevices(self):
    allDevices = await BleakScanner.discover()
    deviceList = []
    for d in allDevices:
      if d.name[:3] == 'PAS':
        deviceList += [(d.name, d.address)]
    return deviceList

  def resetOperation(self):
    self.op = BLEOperations.NOP

  def handleBleException(self, e):
    logging.error(f'BLE Operation Failed\n{e}')
    self.deviceConnected = False
    self.resetOperation()
    self.bleDisconnectedSignal.emit()


  @asyncio.coroutine
  async def __asyncThread(self):
    while True:
      if self.deviceConnected == True and self.client.is_connected == False:
        self.deviceConnected = False
        self.resetOperation()
        self.bleDisconnectedSignal.emit()
      match self.op:
        case BLEOperations.NOP:
          await asyncio.sleep(0.1)
          continue

        case BLEOperations.SCAN:
          self.resetOperation()
          devices = await asyncio.gather(self.__findDevices())
          print(devices)
          self.scanDoneSignal.emit(devices[0])

        case BLEOperations.CONNECT:
          self.resetOperation()
          try:
            await self.client.connect()
          except Exception as e:
            self.deviceConnected = False
            logging.error(f'Failed connection to client\n{e}')
            self.bleFailedSignal.emit()
            continue
          print('connected')
          self.client._mtu_size = 512
          try:
            ip = await self.client.read_gatt_char(IP_UUID)
            netStatus = await self.client.read_gatt_char(STATUS_UUID)
          except Exception as e:
            self.handleBleException(e)
            continue
          self.deviceConnected = True
          self.bleConnectedSignal.emit([ip, netStatus])

        case BLEOperations.DISCONNECT:
          self.resetOperation()
          try:
            await self.client.disconnect()
          except Exception as e:
            self.handleBleException(e)
            continue
          self.deviceConnected = False
          self.bleDisconnectedSignal.emit()

        case BLEOperations.SEND_COMMAND:
          # self.resetOperation()
          try:
            await self.client.write_gatt_char(CMD_UUID, self.commandData)
          except Exception as e:
            self.handleBleException(e)
            continue
          """ Update conf after command """
          self.op = BLEOperations.READ_CONF

        case BLEOperations.READ_CONF:
          self.resetOperation()
          try:
            rawConf = await self.client.read_gatt_char(CONF_UUID)
          except Exception as e:
            self.handleBleException(e)
            continue
          self.deviceConfiguration.ParseFromString(rawConf)
          self.bleConfigUpdatedSignal.emit()

        

  def scanDevices(self):
    self.op =  BLEOperations.SCAN

  def connectToDevice(self, addr):
    self.deviceAddr = addr
    self.client = BleakClient(addr)
    self.op =  BLEOperations.CONNECT
  
  def connectRetry(self):
    self.op =  BLEOperations.CONNECT

  def closeConnection(self):
    self.op = BLEOperations.DISCONNECT

  def addWifiNet(self, ssid, password):
    cmd = proto.bleCommand()
    cmd.command = proto.ADD_WIFI
    cmd.nwt.ssid = ssid
    cmd.nwt.password = password
    self.commandData = bytearray(cmd.SerializeToString())
    if (self.deviceConnected):
      self.op = BLEOperations.SEND_COMMAND

  def removeWifiNet(self, ssid, password):
    cmd = proto.bleCommand()
    cmd.command = proto.REMOVE_WIFI
    cmd.nwt.ssid = ssid
    cmd.nwt.password = password
    self.commandData = bytearray(cmd.SerializeToString())
    if (self.deviceConnected):
      self.op = BLEOperations.SEND_COMMAND

  def setNickname(self, nickname):
    cmd = proto.bleCommand()
    cmd.command = proto.SET_NICK
    cmd.nickName = nickname
    self.commandData = bytearray(cmd.SerializeToString())
    if (self.deviceConnected):
      self.op = BLEOperations.SEND_COMMAND
  
  def resetSensor(self):
    cmd = proto.bleCommand()
    cmd.command = proto.RESTART
    self.commandData = bytearray(cmd.SerializeToString())
    if (self.deviceConnected):
      self.op = BLEOperations.SEND_COMMAND

  def readConfiguration(self):
    self.op = BLEOperations.READ_CONF


  def run(self):
    asyncio.run(self.__asyncThread())
    self.quit()