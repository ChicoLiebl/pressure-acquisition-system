from weakref import ref
import numpy as np



def findWave(signal: np.ndarray, nWaves: int, triggerLevel: float):
  # if signal.size < 100:
  #   print('small signal')
  #   return np.array([])


  """ Finds trigger level crossing with hysteresis """
  hysteresis = 1
  mask1 = (signal[:-1] < (triggerLevel + hysteresis)) & (signal[1:] > (triggerLevel + hysteresis))
  mask2 = (signal[:-1] > (triggerLevel - hysteresis)) & (signal[1:] < (triggerLevel - hysteresis))
  cross = np.flatnonzero(mask1 | mask2)

  """ if few crossing points are found ignore trigger """
  if cross.size < 5:
    # print(f'cross = {cross}')
    return np.array([]), 0, [], 0

  """ Select a center crossing point """
  centerCross = cross.size - (nWaves * 2) - 2

  meanWave = int(np.mean(cross[1:] - cross[:-1]))

  wave = signal[cross[centerCross - 1] : cross[centerCross + 1]]
  if wave.size == 0:
    return np.array([]), 0, [], 0

  centerIndex = wave.argmax() + cross[centerCross - 1]
  lowIndex = wave.argmin() + cross[centerCross - 1]
  waveLength = abs(centerIndex - lowIndex)

  triggered = np.copy(signal[centerIndex - (nWaves * waveLength):centerIndex + (nWaves * waveLength) + 1])
  # print(f'centerIndex = {signal[centerIndex]}, triggered = {triggered[(nWaves * waveLength)]}')
  # print(triggered.size)

  refCross = cross[centerCross - 1: centerCross + 2] - (centerIndex - (nWaves * waveLength))
  
  return triggered, int(nWaves * waveLength), refCross, lowIndex - (centerIndex - (nWaves * waveLength))

def main():
  # s = np.sin(np.array(range(100)) / 10)
  s = np.array([
    0.0,
    0.09983341664682817,
    0.19866933079506124,
    0.2955202066613396,
    0.3894183423086505,
    0.47942553860420295,
    0.5646424733950353,
    0.644217687237691,
    0.7173560908995229,
    0.7833269096274835,
    0.8414709848078965,
    0.8912073600614353,
    0.9320390859672263,
    0.9635581854171931,
    0.9854497299884601,
    0.9974949866040544,
    0.9995736030415054,
    0.9916648104524686,
    0.9738476308781954,
    0.9463000876874146,
    0.9092974268256816,
    0.8632093666488737,
    0.8084964038195902,
    0.7457052121767204,
    0.675463180551151,
    0.5984721441039564,
    0.515501371821464,
    0.4273798802338298,
    0.3349881501559051,
    0.23924932921398243,
    0.1411200080598672,
    -0.04158066243329049,
    0.058374143427580086,
    -0.15774569414324824,
    -0.2555411020268313,
    -0.3507832276896199,
    -0.44252044329485246,
    -0.5298361409084933,
    -0.6118578909427188,
    -0.6877661591839738,
    -0.7568024953079284,
    -0.8182771110644104,
    -0.8715757724135881,
    -0.9161659367494548,
    -0.9516020738895161,
    -0.9775301176650971,
    -0.9936910036334642,
    -0.9999232575641009,
    -0.9961646088358407,
    -0.9824526126243325,
    -0.9589242746631386,
    -0.9258146823277323,
    -0.883454655720153,
    -0.8322674422239013,
    -0.7727644875559873,
    -0.7055403255703919,
    -0.6312666378723215,
    -0.5506855425976375,
    -0.4646021794137573,
    -0.373876664830236,
    -0.2794154981989259,
    -0.1821625042720959,
    -0.08308940281749641,
    0.016813900484349713,
    0.11654920485049364,
    0.21511998808781554,
    0.3115413635133779,
    0.40484992061659836,
    0.49411335113860816,
    0.5784397643882001,
    0.6569865987187891,
    0.728969040125876,
    0.7936678638491533,
    0.8504366206285644,
    0.8987080958116268,
    0.9379999767747389,
    0.9679196720314865,
    0.9881682338770004,
    0.9985433453746051,
    0.9989413418397722,
    0.9893582466233817,
    0.9698898108450865,
    0.9407305566797732,
    0.9021718337562933,
    0.8545989080882804,
    0.7984871126234904,
    0.7343970978741134,
    0.6629692300821833,
    0.5849171928917616,
    0.5010208564578846,
    0.4121184852417566,
    0.3190983623493522,
    0.22288991410024767,
    0.1244544235070617,
    0.024775425453357765,
    -0.07515112046180931,
    -0.17432678122297965,
    -0.27176062641094245,
    -0.36647912925192844,
    -0.45753589377532133
  ])
  # [print(f'{v}') for v in s]

  findWave(s, 2, 0)


if __name__ == "__main__":
  main()
