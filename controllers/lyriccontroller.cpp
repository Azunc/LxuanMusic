#include "lyriccontroller.h"

#include <QDir>
#include <QFileInfo>

#include "./dao/dbdao.h"
#include "playcontroller.h"
#include "./views/lyricwidget.h"
#include "./models/lyricmodel.h"

LyricController::LyricController(QObject *parent)
    : QObject(parent)
{
}

void LyricController::setLyricWidget(LyricWidget *widget)
{
  m_lyricWidget = widget;
}

void LyricController::setLyricModel(LyricModel *model)
{
  m_lyricModel = model;
  if (m_lyricWidget)
  {
    m_lyricWidget->setLyricModel(m_lyricModel);
  }
}

void LyricController::bindPlayController(PlayController *playController)
{
  if (m_playController == playController)
  {
    return;
  }

  if (m_playController)
  {
    disconnect(m_playController, nullptr, this, nullptr);
  }

  m_playController = playController;
  if (!m_playController)
  {
    return;
  }

  // 切歌即加载歌词（无论歌词窗口是否可见）
  connect(m_playController, &PlayController::sigSourceChanged, this, [this](const QUrl &mediaUrl)
          { loadLyricsForMedia(mediaUrl); });

  // 播放进度驱动歌词滚动
  connect(m_playController, &PlayController::sigProgressChanged, this, [this](qint64 position, qint64 /*total*/)
          {
            if (m_lyricWidget) {
              m_lyricWidget->onProgressChanged(position);
            } });
}

void LyricController::toggleLyricWidget(const QUrl &currentMedia, QMediaPlayer::PlaybackState playbackState)
{
  if (!m_lyricWidget)
  {
    return;
  }

  m_lyricWidget->toggleVisible();
  if (m_lyricWidget->isVisible() && playbackState != QMediaPlayer::StoppedState)
  {
    loadLyricsForMedia(currentMedia);
  }
}

void LyricController::loadLyricsForMedia(const QUrl &mediaUrl)
{
  if (!m_lyricWidget && !m_lyricModel)
  {
    return;
  }

  const QString lyricPath = resolveLyricPath(mediaUrl);
  if (!lyricPath.isEmpty())
  {
    if (m_lyricWidget)
      m_lyricWidget->loadLyricFile(lyricPath);
    else if (m_lyricModel)
      m_lyricModel->parse(loadLrcContent(lyricPath));
    return;
  }

  if (m_lyricWidget)
    m_lyricWidget->loadLyricForSong(mediaUrl);
}

QString LyricController::resolveLyricPath(const QUrl &mediaUrl) const
{
  if (!mediaUrl.isLocalFile())
  {
    return QString();
  }

  const QString mediaPath = QFileInfo(mediaUrl.toLocalFile()).absoluteFilePath();

  DbDao *dbDao = DbDao::instance();
  if (dbDao->isDbConnected())
  {
    const QList<Song> songs = dbDao->queryAllSongs();
    for (const Song &song : songs)
    {
      if (QFileInfo(song.filePath()).absoluteFilePath().compare(mediaPath, Qt::CaseInsensitive) != 0)
      {
        continue;
      }

      const QString lrcPath = song.lrcPath().trimmed();
      if (!lrcPath.isEmpty() && QFileInfo::exists(lrcPath))
      {
        return lrcPath;
      }
      break;
    }
  }

  const QFileInfo fileInfo(mediaPath);
  const QString baseName = fileInfo.completeBaseName();
  QDir dir = fileInfo.absoluteDir();
  const QStringList filters = {baseName + QStringLiteral(".lrc"), baseName + QStringLiteral(".LRC")};

  QStringList foundFiles = dir.entryList(filters, QDir::Files);
  if (!foundFiles.isEmpty())
  {
    return dir.absoluteFilePath(foundFiles.first());
  }

  QDir lyricDir(QDir::currentPath() + QStringLiteral("/Lyrics"));
  foundFiles = lyricDir.entryList(filters, QDir::Files);
  if (!foundFiles.isEmpty())
  {
    return lyricDir.absoluteFilePath(foundFiles.first());
  }

  return QString();
}

QString LyricController::loadLrcContent(const QString &filePath) const
{
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return QString();
  QString content = QString::fromUtf8(file.readAll());
  file.close();
  return content;
}
