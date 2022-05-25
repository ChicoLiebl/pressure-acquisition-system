
from PyQt5 import QtCore
from PyQt5.QtCore import pyqtSignal
from PyQt5.QtWidgets import \
  QApplication, QGridLayout, QVBoxLayout, QHBoxLayout, \
  QPushButton, QListWidgetItem, QWidget, \
  QMessageBox, QTableWidget, QCheckBox, \
  QDoubleSpinBox, QLineEdit, QLabel, QTabWidget, QScrollArea

from bleManager import BLECLient

def clearLayoutWidgets(layout):
  while layout.count():
    child = layout.takeAt(0)
    if child.widget():
      child.widget().deleteLater()

def clearLayouts(layout):
  while layout.count():
    child = layout.takeAt(0)
    if child.layout():
      clearLayoutWidgets(child.layout())
      child.layout().deleteLater()

class ConfigTab(QWidget):
  updateIpSignal = pyqtSignal(str)

  def __init__(self, *args, **kwargs):
    super(ConfigTab, self).__init__(*args, **kwargs)  
    style = open('style.css').read()
    self.setStyleSheet(style)
    self.grid = QGridLayout()
    self.setLayout(self.grid)

    self.btScan = QPushButton("Scan Devices")
    self.btScan.setDefault(True)
    self.btScan.clicked.connect(self.listDevices)
    self.grid.addWidget(self.btScan, 1, 1)

    """ device List """
    self.grid.addWidget(QLabel("Device List"), 2, 1, alignment=QtCore.Qt.AlignmentFlag.AlignCenter)
    devScroll = QScrollArea()
    devScroll.setMaximumWidth(300)
    devScroll.setWidgetResizable(True)
    devScroll.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOff)
    devWidget = QWidget()
    devScroll.setWidget(devWidget)
    self.devList = QVBoxLayout()
    self.devList.setAlignment(QtCore.Qt.AlignmentFlag.AlignTop)
    devWidget.setLayout(self.devList)

    self.grid.addWidget(devScroll, 3, 1, 8, 1)

    """ Configuration """
    self.grid.addWidget(QLabel("Configuration"), 1, 2, 1, 5, alignment=QtCore.Qt.AlignmentFlag.AlignCenter)
    self.statusLabel = QLabel("BLE: Disconnected")
    self.grid.addWidget(self.statusLabel, 2, 5, 1, 1)
    self.grid.addWidget(QLabel("Sensor Name"), 2, 2, 1, 1)
    self.nickText = QLineEdit("Default name")
    self.nickText.setDisabled(True)
    self.nickText.setMaximumHeight(40)
    self.grid.addWidget(self.nickText, 2, 3, 1, 1)

    self.btSetNick = QPushButton("Set")
    self.btSetNick.setDefault(True)
    self.btSetNick.clicked.connect(self.setSensorNickname)
    self.grid.addWidget(self.btSetNick, 2, 4, 1, 1)


    """ Wifi list """

    self.grid.addWidget(QLabel("Configured Wi-fi List"), 3, 2, 1, 5, alignment=QtCore.Qt.AlignmentFlag.AlignCenter)
    netsScroll = QScrollArea()
    # netsScroll.setMaximumWidth(300)
    netsScroll.setWidgetResizable(True)
    netsScroll.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOff)
    netsWidget = QWidget()
    netsScroll.setWidget(netsWidget)
    self.netsList = QVBoxLayout()
    self.netsList.setAlignment(QtCore.Qt.AlignmentFlag.AlignTop)
    netsWidget.setLayout(self.netsList)

    self.grid.addWidget(netsScroll, 4, 2, 1, 5)

    """ Wifi form """

    self.grid.addWidget(QLabel("SSID"), 5, 2, 1, 1)
    self.ssidText = QLineEdit()
    self.ssidText.setDisabled(True)
    self.ssidText.setMaximumHeight(40)
    self.grid.addWidget(self.ssidText, 5, 3, 1, 1)

    self.grid.addWidget(QLabel("Password"), 5, 4, 1, 1)
    self.pwdText = QLineEdit()
    self.pwdText.setDisabled(True)
    self.pwdText.setMaximumHeight(40)
    self.grid.addWidget(self.pwdText, 5, 5, 1, 1)

    self.btAddNet = QPushButton("Add")
    self.btAddNet.setDefault(True)
    self.btAddNet.clicked.connect(self.addWifi)
    self.grid.addWidget(self.btAddNet, 5, 6, 1, 1)

    self.bleClient = BLECLient()
    self.bleClient.scanDoneSignal.connect(self.updateDeviceList)
    self.bleClient.bleConnectedSignal.connect(self.onBleConnect)
    self.bleClient.bleDisconnectedSignal.connect(self.onBleDisconnect)
    self.bleClient.bleConfigUpdatedSignal.connect(self.onConfigUpdate)
    self.bleClient.bleFailedSignal.connect(self.bleRetryConnect)

    """ Reset Button """
    self.btReset = QPushButton("Reset Sensor")
    self.btReset.setDisabled(True)
    self.btReset.clicked.connect(self.bleClient.resetSensor)
    self.grid.addWidget(self.btReset, 2, 6, 1, 1)

    self.updateNetsList()
    # self.updateDeviceList([('PAS:default', '24:0A:C4:61:92:16')])
    self.listDevices()

  def bleRetryConnect(self):
    mbox = QMessageBox()
    mbox.setIcon(QMessageBox.Critical)
    mbox.setWindowTitle("Error")
    mbox.setText('BLE connection failed')
    btRetry = mbox.addButton('Retry', QMessageBox.YesRole)
    # btRetry.clicked.disconnect()
    btRetry.clicked.connect(self.bleClient.connectRetry)
    mbox.addButton(QMessageBox.Cancel)
    mbox.exec_()

    # self.bleClient.connectToDevice(addr)

  def addWifi(self):
    self.bleClient.addWifiNet(self.ssidText.text(), self.pwdText.text())
    self.ssidText.setText("")
    self.pwdText.setText("")
  
  def setSensorNickname(self):
    self.bleClient.setNickname(self.nickText.text())

  def onConfigUpdate(self):
    self.nickText.setText(self.bleClient.deviceConfiguration.nickName)
    self.updateNetsList()

  def updateNetsList(self):
    conf = self.bleClient.deviceConfiguration
    ssids = [n.ssid for n in conf.networks]
    passwords = [n.password for n in conf.networks]
    
    clearLayouts(self.netsList)
    for i in range(len(ssids)):
      hBox = QHBoxLayout()
      hBox.addWidget(QLabel(f'SSID: {ssids[i]}'))
      hBox.addWidget(QLabel(f'Password: {passwords[i]}'))
      btRm = QPushButton("Remove")
      btRm.clicked.connect(lambda state, s=ssids[i], p=passwords[i]: self.onRemoveNet(s, p))
      hBox.addWidget(btRm)
      self.netsList.addLayout(hBox)

  def onRemoveNet(self, ssid, password):
    self.bleClient.removeWifiNet(ssid, password)

  def onBleConnect(self, status):
    ip = status[0].decode()
    self.statusLabel.setText(f'BLE: Connected\tTCP Server IP: {ip}')
    self.updateIpSignal.emit(ip)
    self.btScan.setText('Disconnect')
    clearLayoutWidgets(self.devList)
    self.ssidText.setDisabled(False)
    self.pwdText.setDisabled(False)
    self.btReset.setDisabled(False)
    self.nickText.setDisabled(False)

    print(status)
    self.bleClient.readConfiguration()

  def onBleDisconnect(self):
    self.btReset.setDisabled(True)
    self.ssidText.setDisabled(True)
    self.pwdText.setDisabled(True)
    self.nickText.setDisabled(True)

    self.btScan.setText('Scan Devices')
    self.statusLabel.setText("BLE: Disconnected")

  def onSelectDevice(self, device, bt):
    for i in range(self.devList.count()):
      self.devList.itemAt(i).widget().setDisabled(True)
    bt.setStyleSheet("\
      background-color: #FCAF50;\
      padding: 8px 10px; \
      font-size: 14px;\
    ")
    self.bleClient.connectToDevice(device[1])

  def updateDeviceList(self, devices):
    clearLayoutWidgets(self.devList)
    for i, d in enumerate(devices):
      b = QPushButton(d[0] + '\nMAC:' + d[1])
      
      b.setStyleSheet("\
        padding: 8px 10px; \
        font-size: 14px;\
      ")
      b.clicked.connect(lambda state, x=d, y=b: self.onSelectDevice(x, y))
      self.devList.addWidget(b)
  
    self.btScan.setText('Scan Devices')

  def listDevices(self):
    if (self.btScan.text() == 'Scan Devices'):
      self.btScan.setText('Scanning...')
      self.bleClient.scanDevices()
    elif (self.btScan.text() == 'Disconnect'):
      self.bleClient.closeConnection()
    
