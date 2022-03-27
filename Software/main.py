import sys
import os
import datetime
from threading import Lock

import numpy as np
import pandas as pd

from PyQt5 import QtWidgets

from PyQt5 import QtCore, QtGui
from PyQt5.QtGui import QColor
from PyQt5.QtWidgets import QApplication, QGridLayout, QPushButton, QTableWidgetItem, QWidget, QMessageBox, QTableWidget, QCheckBox
import pyqtgraph as pg

from prefixed import Float

from tcpClient import TcpClient
from trigger import *
from decorators import *

INT16_MAX = 32767
CONVERSION_CONSTANT = 4096 / INT16_MAX * 1.25 / 1000
SAMPLE_FREQUENCY = 100e3

BUFFER_LEN = 50000

SERVER_IP = '192.168.0.209'

""" Update data Lock """
updateDataLock = Lock()

class ComboBox(QtWidgets.QComboBox):
    onShowPopup = QtCore.pyqtSignal()

    def showPopup(self):
        self.onShowPopup.emit()
        super(ComboBox, self).showPopup()

class MainWindow(QWidget):
  def __init__(self, *args, **kwargs):
    super(MainWindow, self).__init__(*args, **kwargs)
    """ Configure colors """
    pg.setConfigOption('background', QColor(200, 200, 200))
    pg.setConfigOption('foreground', QColor(0, 0, 0))
    pg.setConfigOptions(antialias=True)

    style = open('style.css').read()
    
    self.setStyleSheet(style)

    self.grid = QGridLayout()
    self.graph = pg.PlotWidget()
    self.graph.addLegend()
    self.grid.addWidget(self.graph, 2, 1, 7, 5)
    
    self.linesColours = ['#DD6677', '#117733', '#88CCEE', '#999933', '#AA4499', '#44AA99', '#332288', '#DDCC77', '#882255']

    self.linePen = pg.mkPen(dict(color='#FF0909', width=3))

    self.setLayout(self.grid)
    self.setWindowTitle("Pressure Acquisition System")

    self.portList = [SERVER_IP]

    self.ipsDropdown = ComboBox()
    self.ipsDropdown.addItems(self.portList)
    self.ipsDropdown.onShowPopup.connect(self.updatePorts)
    self.grid.addWidget(self.ipsDropdown, 9, 1)

    self.btPause = QPushButton("Pause")
    self.btPause.setDefault(True)
    self.btPause.clicked.connect(self.pauseClick)
    self.paused = False
    self.grid.addWidget(self.btPause, 9, 5)

    self.btSave = QPushButton("Save Screen Data")
    self.btSave.setDefault(True)
    self.btSave.clicked.connect(self.saveData)
    self.grid.addWidget(self.btSave, 9, 4)

    self.btConnect = QPushButton("Connect")
    self.btConnect.setDefault(True)
    self.btConnect.clicked.connect(self.onConnectClick)
    self.btConnectPressed = False
    self.grid.addWidget(self.btConnect, 9, 2)

    self.triggerBox = QCheckBox("Trigger")
    self.triggerBox.setCheckState(QtCore.Qt.CheckState.Checked)
    self.grid.addWidget(self.triggerBox, 9, 3)


    """ Grid for average values """
    self.summaryTable = QTableWidget(1, 3)
    self.summaryTable.setMaximumHeight(60)
    self.summaryTable.setHorizontalHeaderLabels(['Current value', 'Average on screen', 'Sample Frequency'])
    self.summaryTable.setItem(0, 0, QTableWidgetItem())
    self.summaryTable.setItem(0, 1, QTableWidgetItem())
    self.summaryTable.setItem(0, 2, QTableWidgetItem())
    self.summaryTable.verticalHeader().setVisible(False)
    self.summaryTable.setSizeAdjustPolicy(QtWidgets.QAbstractScrollArea.AdjustToContents)
    self.summaryTable.horizontalHeader().setSectionResizeMode(QtWidgets.QHeaderView.Stretch)
    for i in range(3):
      self.summaryTable.horizontalHeader().setSectionResizeMode(i, QtWidgets.QHeaderView.ResizeToContents)
    # self.grid.addWidget(self.summaryTable, 1, 1, 1, 5, alignment=QtCore.Qt.AlignLeft)

    """ Stream Related """
    
    self.bufferLen = BUFFER_LEN

    self.receiver = None
    self.timeRaw = None
    self.vdd = None
    self.signal = None
    self.updateLock = None
    
    self.triggerOn = True

    """ Graph Itens """

    self.pressure = np.array([1] * self.bufferLen, dtype=np.float64)
    self.xAxis = np.linspace(0, self.bufferLen / SAMPLE_FREQUENCY * 1000, num=self.bufferLen)

    self.dataLines = [
      pg.PlotDataItem(
        self.xAxis, self.pressure, name='Pressure', 
        connect='all', pen=pg.mkPen(dict(color=self.linesColours[0], width=2)), 
        antialias=True, autoDownsample=False
      ),
      pg.PlotDataItem(
        [], [], name='Max/Min', 
        connect='all', antialias=True, autoDownsample=False, 
        symbol='o', symbolBrush=pg.mkBrush(color=self.linesColours[3])
      ),
      pg.PlotDataItem(
        [], [], name='Trigger Level Cross', 
        connect='all', antialias=True, autoDownsample=False, 
        symbol='o', symbolBrush=pg.mkBrush(color=self.linesColours[4])
      )
    ]
    
    for i in range(self.dataLines.__len__()):
      self.graph.addItem(self.dataLines[i], name=f'Test{i}')
    
    self.graph.setLabel('left', "<span style=\"font-size:14px\">Pressure [N/m²]</span>")
    self.graph.setLabel('bottom', "<span style=\"font-size:14px\">Time [ms]</span>")

    """ Update data timer """
    self.timer = None


  """ Callback to be called from tcoClient on received data """
  @synchronized(updateDataLock)
  def __onTcpData(self, data, dataLen):
    scaledData = data * CONVERSION_CONSTANT
    self.pressure[:-dataLen] = self.pressure[dataLen:]
    self.pressure[-dataLen:] = scaledData

  def connectTcp(self):
    ipAddr = self.ipsDropdown.itemText(self.ipsDropdown.currentIndex())
    print(ipAddr)
    try:
      self.stream = TcpClient(ipAddr, 'h', self.__onTcpData, port=3333)
      self.stream.connect('connection_request')
    except Exception as e:
      msg = QMessageBox()
      msg.setIcon(QMessageBox.Critical)
      msg.setText(f'{e}')
      msg.exec_()
      return False
    print('socket connected')
    self.timer = QtCore.QTimer()
    self.timer.setInterval(33)
    self.timer.timeout.connect(self.updateData)
    self.timer.start()
    return True

  def updatePorts(self):
    self.ipsDropdown.clear()
    self.ipsDropdown.addItems(self.portList)

  def pauseClick(self):
    if self.paused == False:
      self.paused = True
      self.btPause.setText('Resume')
    else:
      self.paused = False
      self.btPause.setText('Pause')

  def onConnectClick(self):
    if self.btConnectPressed == False:
      if self.connectTcp() == True:
        self.btConnectPressed = True
        self.btConnect.setText('Disconnect')
    else:
      self.stream.closeConnection()
      del self.stream
      self.stream = None
      self.timer.stop()
      del self.timer
      self.stream = None
      self.btConnectPressed = False
      self.btConnect.setText('Connect')

  @synchronized(updateDataLock)
  def updateData(self):
    if self.paused:
      return
    if not self.triggerBox.checkState():
      self.dataLines[0].setData(self.xAxis, self.pressure)
      self.dataLines[1].setData([], [])
      self.dataLines[2].setData([], [])
      return

    triggerVal = 0
    plotData, centerIndex, crossIndexes, lowIndex = findWave(self.pressure, 2, triggerVal)
    if plotData.size == 0:
      return
    xAxis = np.linspace(0, plotData.size / SAMPLE_FREQUENCY * 1000, num=plotData.size)
    self.dataLines[0].setData(xAxis, plotData)
    self.dataLines[1].setData([xAxis[centerIndex], xAxis[lowIndex]], [plotData[centerIndex], plotData[lowIndex]])
    self.dataLines[2].setData([xAxis[i] for i in crossIndexes], [plotData[i] for i in crossIndexes])

  def closeEvent(self, a0: QtGui.QCloseEvent) -> None:
    if self.stream != None:
      self.stream.closeConnection()
    return super().closeEvent(a0)

  def saveData(self):
    df = pd.DataFrame(self.dataLines[0].getData())
    df = df.transpose()
    df.to_csv('test.csv')

def main():
  app = QApplication(sys.argv)
  os.environ["QT_ENABLE_HIGHDPI_SCALING"] = "1"
  QApplication.setHighDpiScaleFactorRoundingPolicy(QtCore.Qt.HighDpiScaleFactorRoundingPolicy.PassThrough)
  window = MainWindow()
  window.show()

  sys.exit(app.exec_())


if __name__ == '__main__':
  main()