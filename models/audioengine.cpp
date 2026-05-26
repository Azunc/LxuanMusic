#include "audioengine.h"
#include <QRandomGenerator>
#include <QFile>
#include <QUrl>
#include <QDebug>
#include <QtGlobal>

AudioEngine::AudioEngine(QObject *parent)
    : QObject{parent}
{
  // 初始化Qt6多媒体组件
  m_player = new QMediaPlayer(this);
  m_audioOutput = new QAudioOutput(this);
  m_player->setAudioOutput(m_audioOutput);

  // 默认音量9
  m_audioOutput->setVolume(0.09);

  // 播放状态变化时自动发sigPlayStateChanged，不用在每个play/pause/stop里手动发
  connect(m_player, &QMediaPlayer::playbackStateChanged, this, [=]()
          { emit sigPlayPause(m_player->playbackState()); });
  // 播放进度条修改position和后面的时间
  connect(m_player, &QMediaPlayer::positionChanged, this, [this](qint64 pos)
          {
        if (m_waitingSeekMetric && m_seekTarget >= 0 && qAbs(pos - m_seekTarget) <= 1000) {
            qInfo() << "[Performance] 进度条跳转响应时间:" << m_seekTimer.elapsed()
                    << "ms, target=" << m_seekTarget << "actual=" << pos;
            m_waitingSeekMetric = false;
            m_seekTarget = -1;
        }
        emit sigProgressChanged(pos, m_player->duration()); });
  connect(m_player, &QMediaPlayer::durationChanged, this, [this](qint64 dur)
          { emit sigProgressChanged(m_player->position(), dur); });

  // 发信号
  connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &AudioEngine::onMediaStatusChanged);
  connect(m_player, &QMediaPlayer::errorOccurred, this, &AudioEngine::onPlaybackError);
  connect(m_player, &QMediaPlayer::metaDataChanged, this, &AudioEngine::sigMetaDataChanged);
}

AudioEngine::~AudioEngine()
{
  // 销毁前先停止播放，避免资源泄漏
  m_player->stop();
}

// 单例实现（C++11静态局部变量线程安全，逻辑完全保留）
AudioEngine *AudioEngine::instance()
{
  static AudioEngine ins;
  return &ins;
}

// ==================== 核心播放接口实现 ====================
/**
 * @brief 播放指定媒体源（支持本地/网络URL）
 * @param mediaUrl 媒体地址，必须是合法QUrl
 */
void AudioEngine::play(const QUrl &mediaUrl)
{
  // 1. 有效性校验
  if (!mediaUrl.isValid())
  {
    emit sigPlayError("无效的媒体源地址");
    return;
  }
  // 额外校验本地文件是否存在（网络流跳过）
  if (mediaUrl.isLocalFile() && !QFile::exists(mediaUrl.toLocalFile()))
  {
    emit sigPlayError("本地音频文件不存在：" + mediaUrl.toLocalFile());
    return;
  }

  // 2. 查找媒体在队列中的索引，不存在则追加到队列尾部
  int index = m_playQueue.indexOf(mediaUrl);
  if (index == -1)
  {
    m_playQueue.append(mediaUrl);
    index = m_playQueue.size() - 1;
    emit sigQueueChanged();
  }

  // 3. 更新当前索引、设置源、播放
  m_currentIndex = index;
  m_metricSource = mediaUrl;
  m_decodeTimer.restart();
  m_waitingDecodeMetric = true;
  qInfo() << "[Performance] 开始音频解码计时:" << mediaUrl;

  m_player->setSource(mediaUrl);
  m_player->play();

  // 4. 通知上层媒体源切换
  emit sigSourceChanged(mediaUrl);
}

/**
 * @brief 重载：直接传本地文件路径播放，内部自动转QUrl
 * @param filePath 本地音频文件绝对路径
 */
void AudioEngine::play(const QString &filePath)
{
  if (filePath.trimmed().isEmpty())
  {
    emit sigPlayError("文件路径不能为空");
    return;
  }
  // 本地路径转标准QUrl，自动处理中文/特殊字符
  play(QUrl::fromLocalFile(filePath));
}

// 播放/暂停切换（逻辑完全保留，仅替换Song为QUrl）
void AudioEngine::playPause()
{
  if (m_player->playbackState() == QMediaPlayer::PlayingState)
  {
    m_player->pause();
  }
  else if (m_player->playbackState() == QMediaPlayer::PausedState)
  {
    m_player->play();
  }
  else if (m_currentIndex >= 0 && m_currentIndex < m_playQueue.size())
  {
    play(m_playQueue[m_currentIndex]);
  }
}

// 下一首（逻辑100%保留原有循环规则，仅替换Song为QUrl）
void AudioEngine::next()
{
  if (m_playQueue.isEmpty())
    return;
  int nextIndex = m_currentIndex;
  switch (m_loopMode)
  {
  case LOOP_SINGLE:
    break; // 单曲循环下一首还是当前歌曲
  case LOOP_LIST:
    nextIndex = (m_currentIndex + 1) % m_playQueue.size();
    break;
  case LOOP_RANDOM:
    // 随机到不同歌曲，避免连续重复
    do
    {
      nextIndex = QRandomGenerator::global()->bounded(m_playQueue.size());
    } while (nextIndex == m_currentIndex && m_playQueue.size() > 1);
    break;
  }
  play(m_playQueue[nextIndex]);
}

// 上一首（逻辑100%保留原有循环规则，仅替换Song为QUrl）
void AudioEngine::previous()
{
  if (m_playQueue.isEmpty())
    return;
  int prevIndex = m_currentIndex;
  switch (m_loopMode)
  {
  case LOOP_SINGLE:
    break;
  case LOOP_LIST:
    prevIndex = m_currentIndex == 0 ? m_playQueue.size() - 1 : m_currentIndex - 1;
    break;
  case LOOP_RANDOM:
    do
    {
      prevIndex = QRandomGenerator::global()->bounded(m_playQueue.size());
    } while (prevIndex == m_currentIndex && m_playQueue.size() > 1);
    break;
  }
  play(m_playQueue[prevIndex]);
}

// 跳转进度（逻辑完全保留）
void AudioEngine::setPosition(qint64 pos)
{
  if (m_player->isSeekable())
  {
    m_seekTarget = pos;
    m_seekTimer.restart();
    m_waitingSeekMetric = true;
    qInfo() << "[Performance] 开始进度跳转计时, target=" << pos;
    m_player->setPosition(pos);
  }
}

// 设置音量0-100（逻辑完全保留）
void AudioEngine::setVolume(int volume)
{
  volume = qBound(0, volume, 100);
  m_audioOutput->setVolume(volume / 100.0);
}

// 设置倍速0.5-2.0（逻辑完全保留）
void AudioEngine::setSpeed(qreal speed)
{
  speed = qBound(0.5, speed, 2.0);
  m_player->setPlaybackRate(speed);
}

// 设置循环模式（逻辑完全保留）
void AudioEngine::setLoopMode(LoopMode mode)
{
  m_loopMode = mode;
}

// 设置播放队列（仅替换Song为QUrl，逻辑完全保留）
void AudioEngine::setPlayQueue(const QList<QUrl> &queue, int startIndex, bool autoPlay)
{
  m_playQueue = queue;
  m_currentIndex = queue.isEmpty() ? -1 : qBound(0, startIndex, queue.size() - 1);
  emit sigQueueChanged();
  if (autoPlay && !queue.isEmpty() && m_currentIndex >= 0)
  {
    play(queue[m_currentIndex]);
  }
}

// ==================== 状态获取接口实现 ====================
QMediaPlayer::PlaybackState AudioEngine::isPlaying() const
{
  return m_player->playbackState();
}

qint64 AudioEngine::currentPosition() const
{
  return m_player->position();
}

qint64 AudioEngine::currentDuration() const
{
  return m_player->duration();
}

int AudioEngine::volume() const
{
  return qRound(m_audioOutput->volume() * 100);
}

qreal AudioEngine::speed() const
{
  return m_player->playbackRate();
}

LoopMode AudioEngine::loopMode() const
{
  return m_loopMode;
}

QUrl AudioEngine::currentSource() const
{
  return (m_currentIndex >= 0 && m_currentIndex < m_playQueue.size()) ? m_playQueue[m_currentIndex] : QUrl();
}

QList<QUrl> AudioEngine::playQueue() const
{
  return m_playQueue;
}

// ==================== 内部槽函数实现（逻辑100%保留，无改动） ====================
// 处理播放结束自动切歌
void AudioEngine::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
  if (m_waitingDecodeMetric &&
      (status == QMediaPlayer::LoadedMedia ||
       status == QMediaPlayer::BufferedMedia ||
       status == QMediaPlayer::BufferingMedia))
  {
    qInfo() << "[Performance] 音频解码/加载延迟:" << m_decodeTimer.elapsed()
            << "ms, status=" << status << "source=" << m_metricSource;
    m_waitingDecodeMetric = false;
  }

  if (status == QMediaPlayer::EndOfMedia)
  {
    if (m_loopMode == LOOP_SINGLE)
    {
      m_player->setPosition(0);
      m_player->play();
    }
    else
    {
      next();
    }
  }
}

// 处理播放错误
void AudioEngine::onPlaybackError(QMediaPlayer::Error error, const QString &errorString)
{
  Q_UNUSED(error)
  emit sigPlayError("播放失败：" + errorString);
  // 错误自动跳下一首，避免卡住
  next();
}
