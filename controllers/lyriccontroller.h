/***************************************************
 *  @file      lyriccontroller.h
 *  @brief     歌词业务调度：匹配当前播放歌曲的本地lrc，把解析后的歌词同步给歌词UI，同步播放进度实现滚动
 *
 *  @author    un
 *  @date      2026/04/13
 *  @history
 ****************************************************/
#ifndef LYRICCONTROLLER_H
#define LYRICCONTROLLER_H

#include <QObject>
#include <QUrl>
#include <QtMultimedia>

class LyricWidget;
class PlayController;
class LyricModel;

class LyricController : public QObject
{
  Q_OBJECT
public:
  explicit LyricController(QObject *parent = nullptr);

  void setLyricWidget(LyricWidget *widget);
  void setLyricModel(LyricModel *model);
  void bindPlayController(PlayController *playController);
  void toggleLyricWidget(const QUrl &currentMedia, QMediaPlayer::PlaybackState playbackState);
  void loadLyricsForMedia(const QUrl &mediaUrl);

private:
  QString resolveLyricPath(const QUrl &mediaUrl) const;
  QString loadLrcContent(const QString &filePath) const;

private:
  LyricWidget *m_lyricWidget = nullptr;
  LyricModel *m_lyricModel = nullptr;
  PlayController *m_playController = nullptr;
};

#endif // LYRICCONTROLLER_H
