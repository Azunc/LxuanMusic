/***************************************************
 *  @file      librarymodel.h
 *  @brief     音乐库模型：负责把文件路径集合转换为歌曲实体，并提供去重与关键字筛选等基础能力
 *
 *  @author    un
 *  @date      2026/04/13
 *  @history
 ****************************************************/
#ifndef LIBRARYMODEL_H
#define LIBRARYMODEL_H

#include <QList>
#include <QString>
#include <QStringList>
#include <QFutureWatcher>

#include "./entity/song.h"

class LibraryModel
{
public:
  LibraryModel();

  QList<Song> importFiles(const QStringList &filePaths) const;
  QList<Song> importFilesParallel(const QStringList &filePaths) const;
  QList<Song> deduplicate(const QList<Song> &songs) const;
  QList<Song> filterByKeyword(const QList<Song> &songs, const QString &keyword) const;
};

#endif // LIBRARYMODEL_H
