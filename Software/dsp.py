import numpy as np


def getSpectrum(data :np.ndarray, fs):
  N = data.size
  Y = np.fft.fft(data)
  Xf = np.fft.fftfreq(N, 1/fs)[:N//2]

  Yf = 2.0/N * np.abs(Y[0:(N//2)])

  phase = np.angle(Y)[:N//2] / np.pi

  return Xf[1:], Yf[1:], phase[1:]


if __name__ == "__main__":
  fs = 1000
  f = 60

  wave = 5 * np.sin(np.arange(1000) * 2 * np.pi * f / fs)

  xf, yf, phase = getSpectrum(wave, fs)

  idx = np.argmax(yf)

  print(xf[idx], yf[idx])