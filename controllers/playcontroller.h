/***************************************************
 *  @file      playcontroller.h
 *  @brief     播放控制核心调度：接收用户的播放/暂停/切歌/调倍速/调音量操作，调用底层音频引擎播放，再把播放状态同步给UI
 *
 *  @author    un
 *  @date      2026/04/13
 *  @history
 ****************************************************/
#ifndef PLAYCONTROLLER_H
#define PLAYCONTROLLER_H

#include <QObject>
#include <QList>
#include <QUrl>
#include <QtMultimedia>

#include "./models/audioengine.h"

class PlayController : public QObject
{
  Q_OBJECT
public:
  explicit PlayController(QObject *parent = nullptr);

  void setPlayQueue(const QList<QUrl> &queue, int startIndex = 0, bool autoPlay = true);
  void playAt(int index);
  void play(const QUrl &mediaUrl);
  void playPause();
  void next();
  void previous();
  void setPosition(qint64 position);
  void setVolume(int volume);

  QList<QUrl> playQueue() const;
  int currentIndex() const;
  qint64 currentPosition() const;
  qint64 currentDuration() const;
  QUrl currentSource() const;
  QMediaPlayer::PlaybackState playbackState() const;
  void setLoopMode(LoopMode mode);
  LoopMode loopMode() const;

signals:
  void sigProgressChanged(qint64 current, qint64 total);
  void sigSourceChanged(const QUrl &mediaUrl);
  void sigPlayError(const QString &errMsg);
  void sigMetaDataChanged();
  void sigPlayPause(QMediaPlayer::PlaybackState state);

private:
  AudioEngine *m_audioEngine = nullptr;
};

#endif // PLAYCONTROLLER_H
