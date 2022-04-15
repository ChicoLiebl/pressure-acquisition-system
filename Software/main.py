from ctypes import alignment
from decimal import ROUND_DOWN
import sys
import os
import time
from datetime import datetime as dt
from threading import Lock

import numpy as np
import pandas as pd

from PyQt5 import QtWidgets

from PyQt5 import QtCore, QtGui
from PyQt5.QtGui import QColor
from PyQt5.QtWidgets import \
  QMainWindow, QApplication, QGridLayout, \
  QPushButton, QTableWidgetItem, QWidget, \
  QMessageBox, QTableWidget, QCheckBox, \
  QDoubleSpinBox, QLineEdit, QLabel, QTabWidget
import pyqtgraph as pg

from prefixed import Float

from tcpClient import TcpClient
from config import ConfigTab
from trigger import *
from decorators import *

INT16_MAX = 32767
CONVERSION_CONSTANT = 4096 / INT16_MAX * 1.25 / 1000
SAMPLE_FREQUENCY = 100e3

BUFFER_LEN = 50000
MAX_DISPLAY_LEN = 10000

SERVER_IP = '192.168.243.119'

""" Update data Lock """
updateDataLock = Lock()
class App(QMainWindow):

  def __init__(self):
    super().__init__()
    self.setWindowTitle('Pressure Acquisition System')
    self.setGeometry(0, 0, 1280, 720)
    
    self.tab_widget = QTabWidget()
    self.setCentralWidget(self.tab_widget)
    self.configTab = ConfigTab()
    self.plotTab = PlotTab()
    self.tab_widget.addTab(self.configTab, 'Configuration')
    self.tab_widget.addTab(self.plotTab, 'Acquisition')
    
    self.show()

  def closeEvent(self, a0: QtGui.QCloseEvent) -> None:
    if self.plotTab.stream != None:
      self.plotTab.stream.closeConnection()
    self.configTab.bleClient.closeConnection()
    while self.configTab.bleClient.deviceConnected == True:
      time.sleep(0.1)
    return super().closeEvent(a0)

class PlotTab(QWidget):
  def __init__(self, *args, **kwargs):
    super(PlotTab, self).__init__(*args, **kwargs)
    """ Configure colors """
    pg.setConfigOption('background', QColor(200, 200, 200))
    pg.setConfigOption('foreground', QColor(0, 0, 0))
    pg.setConfigOptions(antialias=True)

    style = open('style.css').read()
    
    self.setStyleSheet(style)

    self.grid = QGridLayout()
    
    self.linesColours = ['#DD6677', '#117733', '#88CCEE', '#999933', '#AA4499', '#44AA99', '#332288', '#DDCC77', '#882255']

    self.setLayout(self.grid)
    # self.setWindowTitle("Pressure Acquisition System")

    """ Grid for average values """
    tableSize = 2
    self.summaryTable = QTableWidget(1, tableSize)
    self.summaryTable.setMaximumHeight(60)
    self.summaryTable.setHorizontalHeaderLabels([' Frequency ', ' Peak-Peak '])
    self.summaryTable.setItem(0, 0, QTableWidgetItem())
    self.summaryTable.setItem(0, 1, QTableWidgetItem())
    self.summaryTable.verticalHeader().setVisible(False)
    self.summaryTable.setSizeAdjustPolicy(QtWidgets.QAbstractScrollArea.AdjustToContentsOnFirstShow)
    for i in range(tableSize):
      self.summaryTable.horizontalHeader().setSectionResizeMode(i, QtWidgets.QHeaderView.Stretch)
    self.grid.addWidget(self.summaryTable, 1, 1, 1, 2, alignment=QtCore.Qt.AlignCenter)

    self.triggerBox = QCheckBox("Trigger")
    self.triggerBox.clicked.connect(self.setTrigger)
    self.triggerBox.setCheckState(QtCore.Qt.CheckState.Checked)
    self.grid.addWidget(self.triggerBox, 1, 3)

    self.triggerValueBox = QDoubleSpinBox()
    self.triggerValueBox.setStepType(QtWidgets.QAbstractSpinBox.StepType.AdaptiveDecimalStepType)
    self.triggerValueBox.setValue(1.600)
    self.triggerValueBox.setDecimals(3)
    self.grid.addWidget(self.triggerValueBox, 1, 4)

    self.triggerHistBox = QDoubleSpinBox()
    self.triggerHistBox.setStepType(QtWidgets.QAbstractSpinBox.StepType.AdaptiveDecimalStepType)
    self.triggerHistBox.setValue(0.1)
    self.triggerHistBox.setDecimals(3)
    self.grid.addWidget(self.triggerHistBox, 1, 5)

    self.nWavesBox = QDoubleSpinBox()
    self.nWavesBox.setMinimum(2)
    self.nWavesBox.setMaximum(5)
    self.nWavesBox.setValue(2)
    self.grid.addWidget(self.nWavesBox, 1, 6)

    ipLabel = QLabel('Server IP :')
    ipLabel.setMaximumWidth(120)
    self.grid.addWidget(ipLabel, 9, 1)

    self.ipInput = QLineEdit(SERVER_IP)
    self.ipInput.setSizePolicy(QtWidgets.QSizePolicy.Policy.Ignored, QtWidgets.QSizePolicy.Policy.Preferred)
    self.ipInput.setMaxLength(16)
    self.ipInput.setFixedSize(120, 30)
    self.grid.addWidget(self.ipInput, 9, 2, alignment=QtCore.Qt.AlignLeft)

    self.btConnect = QPushButton("Connect")
    self.btConnect.setDefault(True)
    self.btConnect.clicked.connect(self.onConnectClick)
    self.btConnectPressed = False
    self.grid.addWidget(self.btConnect, 9, 3, alignment=QtCore.Qt.AlignmentFlag.AlignLeft)

    self.btSave = QPushButton("Save Screen Data")
    self.btSave.setDefault(True)
    self.btSave.clicked.connect(self.saveData)
    self.grid.addWidget(self.btSave, 9, 5)

    self.btPause = QPushButton("Pause")
    self.btPause.setDefault(True)
    self.btPause.clicked.connect(self.pauseClick)
    self.paused = False
    self.grid.addWidget(self.btPause, 9, 6)

    """ Stream Related """
    
    self.bufferLen = BUFFER_LEN
    self.stream = None

    """ Trigger """
    self.triggerOn = True

    """ Graph Itens """

    self.graphLayout = pg.GraphicsLayoutWidget()

    """ Subplots """
    self.mainGraph = self.graphLayout.addPlot(0, 0, rowspan=2, colspan=3)
    self.graphFftPower = self.graphLayout.addPlot(0, 4)
    self.graphFftPhase = self.graphLayout.addPlot(1, 4)
    self.graphLayout.addViewBox(row=0, col=0, colspan=2, rowspan=4)
    self.mainGraph.addLegend()
    
    self.mainGraph.setMouseEnabled(x=False, y=False)
    self.mainGraph.vb.setMouseMode(self.mainGraph.vb.RectMode)
    self.graphFftPower.setMouseEnabled(x=False, y=False)
    self.graphFftPower.vb.setMouseMode(self.mainGraph.vb.RectMode)
    self.graphFftPhase.setMouseEnabled(x=False, y=False)
    self.graphFftPhase.vb.setMouseMode(self.mainGraph.vb.RectMode)

    
    self.grid.addWidget(self.graphLayout, 2, 1, 7, 6)

    self.pressure = np.array([1] * self.bufferLen, dtype=np.float64)
    self.xAxis = np.linspace(0, self.bufferLen / SAMPLE_FREQUENCY * 1000, num=self.bufferLen)

    self.dataLines = [
      pg.PlotDataItem(
        self.xAxis, self.pressure, name='Pressure', 
        connect='all', pen=pg.mkPen(dict(color=self.linesColours[0], width=1)), 
        antialias=False, autoDownsample=False
      ),
      pg.PlotDataItem(
        [], [], name='Trigger Level Cross', 
        connect='all', antialias=True, autoDownsample=False, 
        symbol='o', symbolBrush=pg.mkBrush(color=self.linesColours[4])
      )
    ]
    
    for i in range(self.dataLines.__len__()):
      self.mainGraph.addItem(self.dataLines[i], name=f'Test{i}')
    
    """ Add graphs labels """

    self.mainGraph.setLabel('left', "<span style=\"font-size:14px\">Pressure [N/m²]</span>")
    self.mainGraph.setLabel('bottom', "<span style=\"font-size:14px\">Time [ms]</span>")

    self.graphFftPower.setLabel('left', "<span style=\"font-size:14px\">Power [N/m²]</span>")
    self.graphFftPower.setLabel('bottom', "<span style=\"font-size:14px\">Frequency [Hz]</span>")

    self.graphFftPhase.setLabel('left', "<span style=\"font-size:14px\">Phase [Degrees]</span>")
    self.graphFftPhase.setLabel('bottom', "<span style=\"font-size:14px\">Frequency [Hz]</span>")

    """ Update data timer """
    self.timer = None

  def setTrigger(self):
    if (self.triggerBox.checkState() == False):
      self.triggerValueBox.setDisabled(True)
      self.triggerHistBox.setDisabled(True)
      self.nWavesBox.setDisabled(True)
    else:
      self.triggerValueBox.setDisabled(False)
      self.nWavesBox.setDisabled(False)
      self.triggerHistBox.setDisabled(False)

  """ Callback to be called from tcpClient on received data """
  @synchronized(updateDataLock)
  def __onTcpData(self, data, dataLen):
    scaledData = data * CONVERSION_CONSTANT
    self.pressure[:-dataLen] = self.pressure[dataLen:]
    self.pressure[-dataLen:] = scaledData

  def connectTcp(self):
    ipAddr = self.ipInput.text()
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
    self.timer.setInterval(20)
    self.timer.timeout.connect(self.updateData)
    self.timer.start()
    return True

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
      self.dataLines[0].setData(self.xAxis[-MAX_DISPLAY_LEN:], self.pressure[-MAX_DISPLAY_LEN:])
      self.dataLines[1].setData([], [])
      return

    plotData, crossIndexes = findWave(self.pressure, int(self.nWavesBox.value()), self.triggerValueBox.value(), self.triggerHistBox.value())
    if plotData.size == 0:
      return
    xAxis = np.linspace(0, plotData.size / SAMPLE_FREQUENCY * 1000, num=plotData.size)
    self.dataLines[0].setData(xAxis, plotData)
    self.dataLines[1].setData([xAxis[i] for i in crossIndexes], [plotData[i] for i in crossIndexes])
    
    """ Measure values """
    freq = SAMPLE_FREQUENCY / (crossIndexes[1] - crossIndexes[0])
    ppValue = plotData.max() - plotData.min()


    self.summaryTable.item(0, 0).setText(f'{Float(freq):!.2h}\tHz')
    self.summaryTable.item(0, 1).setText(f'{Float(ppValue):!.2h}\tPressure [N/m²]')


  def saveData(self):
    df = pd.DataFrame(self.dataLines[0].getData())
    df = df.transpose()
    timeStr = dt.now().strftime('%Y-%m-%d--%H-%M-%S')
    df.to_csv(f'screen-data/{timeStr}.csv')

def main():
  app = QApplication(sys.argv)
  os.environ["QT_ENABLE_HIGHDPI_SCALING"] = "1"
  QApplication.setHighDpiScaleFactorRoundingPolicy(QtCore.Qt.HighDpiScaleFactorRoundingPolicy.PassThrough)
  ex = App()
  # window = MainWindow()
  # window.show()

  sys.exit(app.exec_())


if __name__ == '__main__':
  main()