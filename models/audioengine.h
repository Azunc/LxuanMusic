/***************************************************
 *  @file      audioengine.h
 *  @brief     底层音频播放引擎，封装Qt的QMediaPlayer，直接控制音频的播放、进度、倍速、音量，只有playcontroller能调用它
 *  @note      本层仅负责媒体播放逻辑，不持有歌曲元信息（歌名/歌手等），元信息统一由PlayController层维护
 *
 *  @author    un
 *  @date      2026/04/13
 *  @history
 ****************************************************/
#ifndef AUDIOENGINE_H
#define AUDIOENGINE_H

#include <QObject>
#include <QMediaPlayer>
#include <QtMultimedia>
#include <QAudioOutput>
#include <QUrl>
#include <QList>
#include <QElapsedTimer>

// 循环模式枚举
enum LoopMode
{
  LOOP_LIST,   // 列表循环
  LOOP_SINGLE, // 单曲循环
  LOOP_RANDOM  // 随机播放
};

class AudioEngine : public QObject
{
  Q_OBJECT
public:
  // 单例接口
  static AudioEngine *instance();
  ~AudioEngine() override;

  // ==================== 核心控制接口 ====================
  /**
   * @brief 播放指定媒体源
   * @param mediaUrl 媒体地址，支持本地文件（需用QUrl::fromLocalFile转换）、网络HTTP/HTTPS流
   */
  void play(const QUrl &mediaUrl);

  /**
   * @brief 重载：直接传本地文件路径播放，内部自动转QUrl
   * @param filePath 本地音频文件绝对路径
   * QUrl::fromLocalFile(绝对路径)
   */
  void play(const QString &filePath);

  void playPause();                // 播放/暂停切换
  void next();                     // 下一首
  void previous();                 // 上一首
  void setPosition(qint64 pos);    // 跳转进度（单位毫秒）
  void setVolume(int volume);      // 音量 0-100
  void setSpeed(qreal speed);      // 倍速 0.5-2.0
  void setLoopMode(LoopMode mode); // 设置循环模式

  int currentIndex() const { return m_currentIndex; } // 当前索引获取，后面解决第二个错误要用

  /**
   * @brief 设置播放队列
   * @param queue 媒体URL队列
   * @param startIndex 起始播放索引，默认从第一个开始
   * @param autoPlay 是否在设置队列后立即播放
   */
  void setPlayQueue(const QList<QUrl> &queue, int startIndex = 0, bool autoPlay = true);

  // ==================== 获取状态接口 ====================
  QMediaPlayer::PlaybackState isPlaying() const;
  qint64 currentPosition() const;
  qint64 currentDuration() const;
  int volume() const;
  qreal speed() const;
  LoopMode loopMode() const;
  QUrl currentSource() const;
  QList<QUrl> playQueue() const;
  QMediaMetaData currentMetaData() const { return m_player->metaData(); }
  void setMute(bool mute) { m_audioOutput->setMuted(mute); }

signals:
  void sigProgressChanged(qint64 current, qint64 total); // 播放进度变化
  void sigSourceChanged(const QUrl &mediaUrl);           // 当前播放媒体源切换（替换原有sigSongChanged）
  void sigPlayError(const QString &errMsg);              // 播放错误
  void sigQueueChanged();                                // 播放队列变化
  void sigMetaDataChanged();                             // 新增元数据变化信号
  void sigPlayPause(QMediaPlayer::PlaybackState);        // 播放暂停信号。

private slots:
  void onMediaStatusChanged(QMediaPlayer::MediaStatus status);                 // 处理播放结束自动切歌
  void onPlaybackError(QMediaPlayer::Error error, const QString &errorString); // 处理播放错误

private:
  explicit AudioEngine(QObject *parent = nullptr);
  Q_DISABLE_COPY(AudioEngine) // 禁止拷贝

  QMediaPlayer *m_player;
  QAudioOutput *m_audioOutput;
  QList<QUrl> m_playQueue;         // 播放队列（替换原有QList<Song>）
  int m_currentIndex = -1;         // 当前播放的队列索引
  LoopMode m_loopMode = LOOP_LIST; // 当前循环模式

  // 性能测试埋点：解码延迟与进度跳转响应
  QElapsedTimer m_decodeTimer;
  bool m_waitingDecodeMetric = false;
  QUrl m_metricSource;

  QElapsedTimer m_seekTimer;
  bool m_waitingSeekMetric = false;
  qint64 m_seekTarget = -1;
};

#endif // AUDIOENGINE_H
