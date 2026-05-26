#include "metadataextractor.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QPixmap>

#include <taglib/audioproperties.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacfile.h>

MetaDataExtractor::MetaDataExtractor() {}

Song MetaDataExtractor::extract(const QString &filePath) const
{
  QFileInfo fileInfo(filePath);
  Song song(filePath,
            fileInfo.completeBaseName(),
            QStringLiteral("未知歌手"),
            QStringLiteral("未知专辑"),
            0);

  if (!fileInfo.exists() || !fileInfo.isFile())
  {
    return song;
  }

  song.setFileSize(fileInfo.size());
  song.setAddTime(QDateTime::currentDateTime());

  const QString lrcPath = fileInfo.absolutePath() + QDir::separator() + fileInfo.completeBaseName() + QStringLiteral(".lrc");
  if (QFileInfo::exists(lrcPath))
  {
    song.setLrcPath(lrcPath);
  }

  TagLib::FileRef fileRef(filePath.toStdWString().c_str());
  if (fileRef.isNull())
  {
    return song;
  }

  if (TagLib::Tag *tag = fileRef.tag())
  {
    const QString title = QString::fromUtf8(tag->title().toCString(true));
    const QString artist = QString::fromUtf8(tag->artist().toCString(true));
    const QString album = QString::fromUtf8(tag->album().toCString(true));

    if (!title.trimmed().isEmpty())
    {
      song.setTitle(title.trimmed());
    }
    if (!artist.trimmed().isEmpty())
    {
      song.setArtist(artist.trimmed());
    }
    if (!album.trimmed().isEmpty())
    {
      song.setAlbum(album.trimmed());
    }
  }

  if (TagLib::AudioProperties *audioProperties = fileRef.audioProperties())
  {
    song.setDuration(static_cast<qint64>(audioProperties->lengthInSeconds()) * 1000);
    song.setBitRate(audioProperties->bitrate());
  }

  return song;
}

QPixmap MetaDataExtractor::extractCover(const QString &filePath)
{
  QFileInfo fi(filePath);
  if (!fi.exists() || !fi.isFile())
    return QPixmap();

  const QString suffix = fi.suffix().toLower();
  QPixmap cover;

  // 1. MP3：读取 ID3v2 APIC（优先 FrontCover，找不到则取第一个 APIC）
  if (suffix == QStringLiteral("mp3"))
  {
    TagLib::MPEG::File mp3File(filePath.toStdWString().c_str());
    if (mp3File.isValid() && mp3File.ID3v2Tag())
    {
      TagLib::ID3v2::FrameList picFrames = mp3File.ID3v2Tag()->frameList("APIC");
      TagLib::ID3v2::AttachedPictureFrame *targetFrame = nullptr;
      for (auto frame : picFrames)
      {
        auto *picFrame = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(frame);
        if (!picFrame)
          continue;
        if (picFrame->type() == TagLib::ID3v2::AttachedPictureFrame::FrontCover)
        {
          targetFrame = picFrame;
          break;
        }
        if (!targetFrame)
          targetFrame = picFrame;
      }
      if (targetFrame)
      {
        QByteArray data(targetFrame->picture().data(), static_cast<int>(targetFrame->picture().size()));
        if (cover.loadFromData(data))
          return cover;
      }
    }
  }

  // 2. FLAC：读取 pictureList，fallback 到 ID3v2
  if (suffix == QStringLiteral("flac"))
  {
    TagLib::FLAC::File flacFile(filePath.toStdWString().c_str());
    if (flacFile.isValid())
    {
      const auto &picList = flacFile.pictureList();
      if (!picList.isEmpty())
      {
        QByteArray data(picList.front()->data().data(), static_cast<int>(picList.front()->data().size()));
        if (cover.loadFromData(data))
          return cover;
      }
      if (flacFile.ID3v2Tag())
      {
        TagLib::ID3v2::FrameList picFrames = flacFile.ID3v2Tag()->frameList("APIC");
        for (auto frame : picFrames)
        {
          auto *picFrame = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(frame);
          if (!picFrame)
            continue;
          QByteArray data(picFrame->picture().data(), static_cast<int>(picFrame->picture().size()));
          if (cover.loadFromData(data))
            return cover;
        }
      }
    }
  }

  // 3. 同目录常见外部封面文件
  const QStringList coverNames = {
      QStringLiteral("cover.jpg"), QStringLiteral("cover.png"),
      QStringLiteral("folder.jpg"), QStringLiteral("folder.png"),
      QStringLiteral("albumart.jpg"), QStringLiteral("albumart.png"),
      QStringLiteral("front.jpg"), QStringLiteral("front.png"),
      fi.baseName() + QStringLiteral(".jpg"),
      fi.baseName() + QStringLiteral(".png")};
  QDir dir = fi.dir();
  for (const QString &name : coverNames)
  {
    QString coverPath = dir.filePath(name);
    if (QFile::exists(coverPath))
    {
      if (cover.load(coverPath))
        return cover;
    }
  }

  return QPixmap();
}
