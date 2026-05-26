/***************************************************
 *  @file      filescanner.h
 *  @brief     本地音频文件扫描工具：负责从目录中递归扫描可支持的音频文件路径
 *
 *  @author    un
 *  @date      2026/04/13
 *  @history
 ****************************************************/
#ifndef FILESCANNER_H
#define FILESCANNER_H

#include <QString>
#include <QStringList>

class FileScanner
{
public:
  FileScanner();

  QString defaultMusicDirectory() const;
  QStringList scanAudioFiles(const QString &rootPath = QString(), bool recursive = true) const;
  QStringList supportedNameFilters() const;
};

#endif // FILESCANNER_H
