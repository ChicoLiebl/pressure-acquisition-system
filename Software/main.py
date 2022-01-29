import sys
import os

import numpy as np

from PyQt5 import QtWidgets

from PyQt5 import QtCore, QtGui
from PyQt5.QtGui import QColor
from PyQt5.QtWidgets import QApplication, QGridLayout, QPushButton, QTableWidgetItem, QWidget, QMessageBox, QTableWidget
import pyqtgraph as pg

from prefixed import Float

from tcpClient import TcpClient

SERVER_IP = '192.168.0.134'

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
    self.btPausePressed = False
    self.grid.addWidget(self.btPause, 9, 5)

    self.btConnect = QPushButton("Connect")
    self.btConnect.setDefault(True)
    self.btConnect.clicked.connect(self.onConnectClick)
    self.btConnectPressed = False
    self.grid.addWidget(self.btConnect, 9, 2)

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

    """ Serial Related """
    
    self.bufferLen = 20000

    self.receiver = None
    self.timeRaw = None
    self.vdd = None
    self.signal = None
    self.updateLock = None

    """ Graph Itens """

    self.pressure = np.array([1] * self.bufferLen)
    self.xAxis = np.array(range(self.bufferLen))

    self.dataLines = [
      pg.PlotDataItem(
        self.xAxis, self.pressure, name='Pressure', 
        connect='all', pen=pg.mkPen(dict(color=self.linesColours[0], width=2)), 
        antialias=True, autoDownsample=False
      )
    ]
    
    for i in range(self.dataLines.__len__()):
      self.graph.addItem(self.dataLines[i], name=f'Test{i}')
    
    self.graph.setLabel('left', "<span style=\"font-size:14px\">Pressure [N/m²]</span>")
    self.graph.setLabel('bottom', "<span style=\"font-size:14px\">Time [ms]</span>")

    """ Update data timer """

    self.timer = None

  def connectTcp(self):
    ipAddr = self.ipsDropdown.itemText(self.ipsDropdown.currentIndex())
    print(ipAddr)
    try:
      self.stream = TcpClient(ipAddr, self.pressure, 'h', port=3333)
      self.stream.connect('connection_request')
    except Exception as e:
      msg = QMessageBox()
      msg.setIcon(QMessageBox.Critical)
      msg.setText(f'{e}')
      msg.exec_()
      return False
    print('socket connected')
    self.timer = QtCore.QTimer()
    self.timer.setInterval(90)
    self.timer.timeout.connect(self.updateData)
    self.timer.start()
    return True

  def updatePorts(self):
    self.ipsDropdown.clear()
    self.ipsDropdown.addItems(self.portList)

  def pauseClick(self):
    if self.btPausePressed == False:
      self.stream.pause()
      self.btPausePressed = True
      self.btPause.setText('Resume')
    else:
      self.stream.resume()
      self.btPausePressed = False
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

  def connectSocket(self):
    print('Starts connection')
    return True

  def updateData(self):
    self.dataLines[0].setData(self.xAxis, self.pressure)

  def closeEvent(self, a0: QtGui.QCloseEvent) -> None:
    if self.stream != None:
      self.stream.closeConnection()
    return super().closeEvent(a0)

def main():
  app = QApplication(sys.argv)
  os.environ["QT_ENABLE_HIGHDPI_SCALING"] = "1"
  QApplication.setHighDpiScaleFactorRoundingPolicy(QtCore.Qt.HighDpiScaleFactorRoundingPolicy.PassThrough)
  window = MainWindow()
  window.show()

  sys.exit(app.exec_())


if __name__ == '__main__':
  main()