/***************************************************
 *  @file      visualizermodel.h
 *  @brief     频谱数据模型：ffmpeg stdout 实时解码 + kiss_fft 频域分析
 *             支持 PCM 缓存、切歌频谱柱保持
 *
 *  @author    un
 *  @date      2026/04/13
 *  @history
 ****************************************************/
#ifndef VISUALIZERMODEL_H
#define VISUALIZERMODEL_H

#include <QObject>
#include <QVector>
#include <QUrl>
#include <QTimer>
#include <QMap>

extern "C"
{
#include "../utils/kiss_fft.h"
}

class QProcess;

class VisualizerModel : public QObject
{
  Q_OBJECT
public:
  explicit VisualizerModel(QObject *parent = nullptr);
  ~VisualizerModel();

  void setEnabled(bool enabled);
  bool isEnabled() const;

  void setBarCount(int barCount);
  int barCount() const;

  QVector<qreal> spectrumData() const;
  void setSource(const QUrl &source);

public slots:
  void updateFromPlayback(qint64 position, qint64 duration);

signals:
  void sigSpectrumDataChanged(const QVector<qreal> &levels);
  void sigVisualizerEnabledChanged(bool enabled);

private slots:
  void onFfmpegFinished(int exitCode);
  void onFfmpegReadyRead();
  void processSpectrum();

private:
  void startDecoderForSource(const QUrl &source);
  void stopFfmpeg();
  void computeSpectrum();

  QVector<qreal> m_spectrumData;
  bool m_enabled = false;
  int m_barCount = 64;

  // 当前正在解码/播放的 PCM
  QVector<float> m_pcmBuffer;
  int m_sampleRate = 44100;
  bool m_pcmReady = false;

  // PCM 缓存：URL -> 已解码的完整 PCM（float，单声道，44100Hz）
  // 避免同一首歌反复开关可视化时重复解码
  QMap<QUrl, QVector<float>> m_pcmCache;
  static constexpr int MAX_PCM_CACHE_ENTRIES = 3;

  qint64 m_currentPositionMs = 0;
  QUrl m_currentSource;

  QTimer *m_refreshTimer = nullptr;
  QProcess *m_ffmpegProcess = nullptr;

  // 过渡动画：0=正常, 1=淡出中, 2=淡入中
  int m_fadeState = 0;
  qreal m_fadeFactor = 1.0; // 0.0 ~ 1.0
  static constexpr qreal FADE_STEP = 0.08;

  static constexpr int FFT_SIZE = 2048;
  kiss_fft_cfg m_fftConfig = nullptr;
  kiss_fft_cpx *m_fftIn = nullptr;
  kiss_fft_cpx *m_fftOut = nullptr;
};

#endif // VISUALIZERMODEL_H
