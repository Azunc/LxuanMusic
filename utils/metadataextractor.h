/***************************************************
 *  @file      metadataextractor.h
 *  @brief     音频元数据提取工具：从 mp3/flac/wav 等文件中提取标题、歌手、专辑、时长、比特率与歌词路径等信息
 *
 *  @author    un
 *  @date      2026/04/13
 *  @history
 ****************************************************/
#ifndef METADATAEXTRACTOR_H
#define METADATAEXTRACTOR_H

#include <QString>
#include <QPixmap>

#include "./entity/song.h"

class MetaDataExtractor
{
public:
  MetaDataExtractor();

  Song extract(const QString &filePath) const;
  static QPixmap extractCover(const QString &filePath);
};

#endif // METADATAEXTRACTOR_H
