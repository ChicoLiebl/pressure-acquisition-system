import numpy as np
import apfft


def getSpectrum(data :np.ndarray, fs):
  N = data.size
  Y = np.fft.fft(data)
  Xf = np.fft.fftfreq(N, 1/fs)[:N//2]

  Yf = 2.0/N * np.abs(Y[0:(N//2)])[1:]

  phase = np.angle(Y)[:N//2] / np.pi

  return Xf[1:], Yf, phase

def getSpectrumPhaseCorrected(data :np.ndarray, fs):
  Nsamples = data.size
  N = int((Nsamples+1)/2)

  wc = apfft.make_convoluted_window('han',N)
  dataWin = data*wc

  shifted = np.zeros(N)
  shifted[0] = dataWin[N-1]
  for ix in range(1,N):
    shifted[ix] = dataWin[ix-1] + dataWin[N+ix-1]
  #shifted = shifted/N

  fftShifted = np.fft.fft(shifted)
  # maxix = np.argmax(abs(fftShifted[0:int(N/2)]))
  freq = np.fft.fftfreq(N, 1/fs)[:N//2]
  # freq = (np.arange(N//2) * 1/ fs)/N
  amp = 2.0/N * np.abs(fftShifted[(N//2):])[1:]
  # amp = abs(fftShifted)

  phase=np.arctan2(np.imag(fftShifted),np.real(fftShifted))

  return freq, amp, phase


def getPhase_old(signal :np.ndarray, fs):
  N = len(signal)
  _signal = np.copy(signal)

  for i in range(N//2):
    _signal[i] = (i + 1) * _signal[i]

  for i in range(N//2, N):
    _signal[i] = (N - (i + 1)) * _signal[i]
  
  for i in range(N//2):
    _signal[i+N//2] = _signal[i] + _signal[i + N//2]
    
  for i in range(N//2 + 1):
    _signal[i] = _signal[N//2-1+i]

  Y = np.fft.fft(_signal)

  pf = phase = np.angle(Y)[:N//2] / np.pi

  return pf



if __name__ == "__main__":
  fs = 1000
  f = 60

  wave = 5 * np.sin(np.arange(1000) * 2 * np.pi * f / fs)

  xf, yf, phase = getSpectrum(wave, fs)

  idx = np.argmax(yf)

  print(xf[idx], yf[idx])