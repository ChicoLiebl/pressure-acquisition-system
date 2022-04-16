import sys
import os
import time

from PyQt5 import QtCore, QtGui
from PyQt5.QtWidgets import \
  QMainWindow, QApplication, QTabWidget

from config import ConfigTab
from realTime import PlotTab
from trigger import *
from decorators import *

class App(QMainWindow):

  def __init__(self):
    super().__init__()
    self.setWindowTitle('Pressure Acquisition System')
    self.setGeometry(0, 0, 1280, 720)
    
    self.tab_widget = QTabWidget()
    self.setCentralWidget(self.tab_widget)
    self.configTab = ConfigTab()
    self.plotTab = PlotTab()
    self.tab_widget.addTab(self.plotTab, 'Acquisition')
    self.tab_widget.addTab(self.configTab, 'Configuration')
    self.configTab.updateIpSignal.connect(self.plotTab.updateIp)
    
    self.show()

  def closeEvent(self, a0: QtGui.QCloseEvent) -> None:
    if self.plotTab.stream != None:
      self.plotTab.stream.closeConnection()
    self.configTab.bleClient.closeConnection()
    while self.configTab.bleClient.deviceConnected == True:
      time.sleep(0.1)
    return super().closeEvent(a0)

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