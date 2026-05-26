#include "visualizermodel.h"
#include <QtMath>
#include <QDebug>
#include <QFile>
#include <QProcess>
#include <QDir>
#include <QCoreApplication>

static constexpr int FFT_SIZE = 2048;

VisualizerModel::VisualizerModel(QObject *parent)
    : QObject(parent)
{
  m_spectrumData.fill(0.0, m_barCount);

  m_fftConfig = kiss_fft_alloc(FFT_SIZE, 0, nullptr, nullptr);
  m_fftIn = (kiss_fft_cpx *)KISS_FFT_MALLOC(sizeof(kiss_fft_cpx) * FFT_SIZE);
  m_fftOut = (kiss_fft_cpx *)KISS_FFT_MALLOC(sizeof(kiss_fft_cpx) * FFT_SIZE);

  m_refreshTimer = new QTimer(this);
  m_refreshTimer->setInterval(33); // ~30fps
  connect(m_refreshTimer, &QTimer::timeout, this, &VisualizerModel::processSpectrum);
}

VisualizerModel::~VisualizerModel()
{
  if (m_refreshTimer && m_refreshTimer->isActive())
  {
    m_refreshTimer->stop();
  }
  if (m_ffmpegProcess)
  {
    m_ffmpegProcess->kill();
    m_ffmpegProcess->waitForFinished(1000);
    delete m_ffmpegProcess;
  }

  if (m_fftConfig)
  {
    kiss_fft_free(m_fftConfig);
    m_fftConfig = nullptr;
  }
  if (m_fftIn)
  {
    KISS_FFT_FREE(m_fftIn);
    m_fftIn = nullptr;
  }
  if (m_fftOut)
  {
    KISS_FFT_FREE(m_fftOut);
    m_fftOut = nullptr;
  }
}

void VisualizerModel::setEnabled(bool enabled)
{
  if (m_enabled == enabled)
  {
    return;
  }

  m_enabled = enabled;
  if (!m_enabled)
  {
    // 关闭可视化：停止刷新，但保留频谱数据和 PCM 缓存，不 kill ffmpeg
    if (m_refreshTimer->isActive())
    {
      m_refreshTimer->stop();
    }
  }
  else
  {
    // 开启可视化：如果有缓存 PCM，直接就绪
    if (!m_currentSource.isEmpty())
    {
      if (m_pcmCache.contains(m_currentSource))
      {
        m_pcmBuffer = m_pcmCache.value(m_currentSource);
        m_pcmReady = true;
        m_sampleRate = 44100;
        qDebug() << "[Visualizer] 从缓存恢复 PCM:" << m_currentSource;
      }
      else if (!m_pcmReady)
      {
        startDecoderForSource(m_currentSource);
      }
    }
    m_refreshTimer->start();
  }
  emit sigVisualizerEnabledChanged(m_enabled);
}

bool VisualizerModel::isEnabled() const
{
  return m_enabled;
}

void VisualizerModel::setBarCount(int barCount)
{
  const int boundedBarCount = qBound(8, barCount, 200);
  if (m_barCount == boundedBarCount)
  {
    return;
  }

  m_barCount = boundedBarCount;
  m_spectrumData.fill(0.0, m_barCount);
  emit sigSpectrumDataChanged(m_spectrumData);
}

int VisualizerModel::barCount() const
{
  return m_barCount;
}

QVector<qreal> VisualizerModel::spectrumData() const
{
  return m_spectrumData;
}

void VisualizerModel::setSource(const QUrl &source)
{
  if (m_currentSource == source)
  {
    return;
  }

  m_currentSource = source;
  m_pcmReady = false;

  // 如果有缓存，直接恢复，无需淡入淡出
  if (m_pcmCache.contains(source))
  {
    m_pcmBuffer = m_pcmCache.value(source);
    m_pcmReady = true;
    m_sampleRate = 44100;
    m_fadeState = 0;
    m_fadeFactor = 1.0;
    qDebug() << "[Visualizer] 切歌，从缓存恢复 PCM:" << source;
  }
  else
  {
    // 当前有频谱数据，先触发淡出
    if (m_spectrumData.size() == m_barCount && !m_spectrumData.isEmpty())
    {
      bool hasNonZero = false;
      for (qreal v : m_spectrumData)
      {
        if (v > 0.01)
        {
          hasNonZero = true;
          break;
        }
      }
      if (hasNonZero)
      {
        m_fadeState = 1; // 开始淡出
      }
    }
    m_pcmBuffer.clear();
    if (m_enabled && !source.isEmpty())
    {
      startDecoderForSource(source);
    }
  }
}

void VisualizerModel::startDecoderForSource(const QUrl &source)
{
  if (!source.isLocalFile())
  {
    qWarning() << "[Visualizer] 非本地文件，跳过解码:" << source;
    return;
  }

  const QString localPath = source.toLocalFile();
  if (!QFile::exists(localPath))
  {
    qWarning() << "[Visualizer] 文件不存在，跳过解码:" << localPath;
    return;
  }

  // 如果已经有解码器在跑同一首歌，不重复启动
  if (m_ffmpegProcess && m_ffmpegProcess->state() == QProcess::Running)
  {
    return;
  }

  stopFfmpeg();

  QString ffmpegPath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("tools/ffmpeg.exe"));
  if (!QFile::exists(ffmpegPath))
  {
    QDir appDir(QCoreApplication::applicationDirPath());
    if (appDir.cdUp() && appDir.cdUp())
    {
      QString altPath = appDir.filePath(QStringLiteral("tools/ffmpeg.exe"));
      if (QFile::exists(altPath))
      {
        ffmpegPath = altPath;
      }
    }
  }
  if (!QFile::exists(ffmpegPath))
  {
    ffmpegPath = QStringLiteral("ffmpeg");
  }

  m_ffmpegProcess = new QProcess(this);
  connect(m_ffmpegProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          this, &VisualizerModel::onFfmpegFinished);
  connect(m_ffmpegProcess, &QProcess::readyReadStandardOutput,
          this, &VisualizerModel::onFfmpegReadyRead);

  QStringList args;
  args << QStringLiteral("-i") << localPath
       << QStringLiteral("-f") << QStringLiteral("s16le")
       << QStringLiteral("-acodec") << QStringLiteral("pcm_s16le")
       << QStringLiteral("-ar") << QStringLiteral("44100")
       << QStringLiteral("-ac") << QStringLiteral("1")
       << QStringLiteral("-loglevel") << QStringLiteral("quiet")
       << QStringLiteral("-");

  qDebug() << "[Visualizer] 启动 ffmpeg stdout 解码:" << localPath;
  m_ffmpegProcess->start(ffmpegPath, args);
}

void VisualizerModel::stopFfmpeg()
{
  if (!m_ffmpegProcess)
  {
    return;
  }
  disconnect(m_ffmpegProcess, nullptr, this, nullptr);
  if (m_ffmpegProcess->state() == QProcess::Running)
  {
    m_ffmpegProcess->kill();
    m_ffmpegProcess->waitForFinished(1000);
  }
  delete m_ffmpegProcess;
  m_ffmpegProcess = nullptr;
}

void VisualizerModel::onFfmpegReadyRead()
{
  if (!m_ffmpegProcess)
  {
    return;
  }

  QByteArray chunk = m_ffmpegProcess->readAllStandardOutput();
  if (chunk.isEmpty())
  {
    return;
  }

  const int sampleCount = chunk.size() / sizeof(qint16);
  const qint16 *samples = reinterpret_cast<const qint16 *>(chunk.constData());

  for (int i = 0; i < sampleCount; ++i)
  {
    m_pcmBuffer.append(static_cast<float>(samples[i]) / 32768.0f);
  }

  if (!m_pcmReady && m_pcmBuffer.size() >= FFT_SIZE * 2)
  {
    m_pcmReady = true;
    m_sampleRate = 44100;
    m_fadeState = 2; // 开始淡入
    m_fadeFactor = 0.0;
    qDebug() << "[Visualizer] PCM 已就绪，样本数:" << m_pcmBuffer.size();
  }
}

void VisualizerModel::onFfmpegFinished(int exitCode)
{
  Q_UNUSED(exitCode)
  if (!m_ffmpegProcess)
  {
    return;
  }

  if (m_ffmpegProcess->exitStatus() != QProcess::NormalExit || exitCode != 0)
  {
    qWarning() << "[Visualizer] ffmpeg 退出异常，退出码:" << exitCode
               << "stderr:" << m_ffmpegProcess->readAllStandardError();
  }
  else
  {
    qDebug() << "[Visualizer] ffmpeg 完成，总 PCM 样本数:" << m_pcmBuffer.size();
  }

  // 解码完成，把当前 PCM 放入缓存
  if (!m_currentSource.isEmpty() && !m_pcmBuffer.isEmpty())
  {
    m_pcmCache.insert(m_currentSource, m_pcmBuffer);
    // 限制缓存条目数
    while (m_pcmCache.size() > MAX_PCM_CACHE_ENTRIES)
    {
      m_pcmCache.erase(m_pcmCache.begin());
    }
  }
}

void VisualizerModel::updateFromPlayback(qint64 position, qint64 duration)
{
  Q_UNUSED(duration)
  m_currentPositionMs = position;
}

void VisualizerModel::processSpectrum()
{
  if (!m_enabled)
  {
    return;
  }

  // 淡出处理：频谱柱逐渐降为 0
  if (m_fadeState == 1)
  {
    m_fadeFactor -= FADE_STEP;
    if (m_fadeFactor <= 0.0)
    {
      m_fadeFactor = 0.0;
      m_fadeState = 0;
      m_spectrumData.fill(0.0, m_barCount);
    }
    QVector<qreal> faded(m_barCount, 0.0);
    for (int i = 0; i < m_barCount; ++i)
    {
      faded[i] = m_spectrumData.value(i, 0.0) * m_fadeFactor;
    }
    emit sigSpectrumDataChanged(faded);
    return;
  }

  if (!m_pcmReady || m_pcmBuffer.isEmpty())
  {
    return;
  }

  computeSpectrum();

  // 淡入处理：频谱柱从 0 逐渐升高
  if (m_fadeState == 2)
  {
    m_fadeFactor += FADE_STEP;
    if (m_fadeFactor >= 1.0)
    {
      m_fadeFactor = 1.0;
      m_fadeState = 0;
    }
  }

  if (m_fadeFactor < 1.0)
  {
    for (int i = 0; i < m_barCount; ++i)
    {
      m_spectrumData[i] *= m_fadeFactor;
    }
  }

  emit sigSpectrumDataChanged(m_spectrumData);
}

void VisualizerModel::computeSpectrum()
{
  if (!m_fftConfig || !m_fftIn || !m_fftOut || m_sampleRate <= 0)
  {
    return;
  }

  const qint64 sampleIndex64 = static_cast<qint64>(m_currentPositionMs) * m_sampleRate / 1000LL;
  if (sampleIndex64 < 0 || sampleIndex64 > INT_MAX - FFT_SIZE)
  {
    return;
  }
  const int sampleIndex = static_cast<int>(sampleIndex64);
  const int halfFft = FFT_SIZE / 2;

  if (sampleIndex + FFT_SIZE > m_pcmBuffer.size())
  {
    return;
  }

  // 加汉宁窗
  for (int i = 0; i < FFT_SIZE; ++i)
  {
    float sample = m_pcmBuffer[sampleIndex + i];
    float window = 0.5f * (1.0f - qCos(2.0f * M_PI * i / (FFT_SIZE - 1)));
    m_fftIn[i].r = sample * window;
    m_fftIn[i].i = 0.0f;
  }

  kiss_fft(m_fftConfig, m_fftIn, m_fftOut);

  QVector<qreal> bandLevels(m_barCount, 0.0);
  const qreal nyquist = m_sampleRate / 2.0;
  const qreal minFreq = 20.0;
  const qreal maxFreq = nyquist;
  const qreal binWidth = static_cast<qreal>(m_sampleRate) / FFT_SIZE;

  for (int i = 0; i < m_barCount; ++i)
  {
    qreal t0 = static_cast<qreal>(i) / m_barCount;
    qreal t1 = static_cast<qreal>(i + 1) / m_barCount;
    qreal fStart = minFreq * qPow(maxFreq / minFreq, t0);
    qreal fEnd = minFreq * qPow(maxFreq / minFreq, t1);

    int binStart = qFloor(fStart / binWidth);
    int binEnd = qCeil(fEnd / binWidth);
    binStart = qBound(1, binStart, halfFft - 1);
    binEnd = qBound(1, binEnd, halfFft - 1);

    qreal sum = 0.0;
    int count = 0;
    for (int b = binStart; b < binEnd; ++b)
    {
      qreal real = static_cast<qreal>(m_fftOut[b].r);
      qreal imag = static_cast<qreal>(m_fftOut[b].i);
      qreal magnitude = qSqrt(real * real + imag * imag);
      sum += magnitude;
      ++count;
    }

    qreal avg = (count > 0) ? (sum / count) : 0.0;
    qreal normalized = avg / FFT_SIZE;
    qreal db = 20.0 * log10(normalized + 1e-6);
    qreal dbMin = -70.0;
    qreal dbMax = -15.0;
    qreal level = (db - dbMin) / (dbMax - dbMin);
    bandLevels[i] = qBound(0.0, level, 1.0);
  }

  for (int i = 0; i < m_barCount; ++i)
  {
    qreal prev = m_spectrumData.value(i, 0.0);
    qreal target = bandLevels[i];
    qreal factor = (target > prev) ? 0.6 : 0.3;
    m_spectrumData[i] = prev * (1.0 - factor) + target * factor;
  }
}
