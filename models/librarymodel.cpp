#include "librarymodel.h"

#include <QFileInfo>
#include <QSet>
#include <QtConcurrent>
#include <QThread>

#include "./utils/metadataextractor.h"

LibraryModel::LibraryModel() {}

QList<Song> LibraryModel::importFiles(const QStringList &filePaths) const
{
  QList<Song> songs;
  MetaDataExtractor extractor;

  for (const QString &filePath : filePaths)
  {
    const Song song = extractor.extract(filePath);
    if (!song.filePath().trimmed().isEmpty())
    {
      songs.append(song);
    }
  }

  return deduplicate(songs);
}

QList<Song> LibraryModel::importFilesParallel(const QStringList &filePaths) const
{
  const int count = filePaths.size();
  if (count == 0)
  {
    return {};
  }

  // 按 CPU 核心数分块，每块在独立线程中并行提取元数据
  const int threadCount = qMax(1, QThread::idealThreadCount());
  const int chunkSize = qMax(1, (count + threadCount - 1) / threadCount);

  QVector<QList<Song>> results(threadCount);
  QList<QFuture<void>> futures;
  futures.reserve(threadCount);

  for (int t = 0; t < threadCount; ++t)
  {
    const int start = t * chunkSize;
    const int end = qMin(start + chunkSize, count);
    if (start >= end)
    {
      break;
    }

    futures.append(QtConcurrent::run([start, end, t, &filePaths, &results]()
                                     {
      MetaDataExtractor extractor;
      QList<Song> localSongs;
      localSongs.reserve(end - start);
      for (int i = start; i < end; ++i)
      {
        const Song song = extractor.extract(filePaths.at(i));
        if (!song.filePath().trimmed().isEmpty())
        {
          localSongs.append(song);
        }
      }
      results[t] = localSongs; }));
  }

  for (auto &f : futures)
  {
    f.waitForFinished();
  }

  int total = 0;
  for (const auto &list : results)
  {
    total += list.size();
  }

  QList<Song> allSongs;
  allSongs.reserve(total);
  for (const auto &list : results)
  {
    allSongs.append(list);
  }

  return deduplicate(allSongs);
}

QList<Song> LibraryModel::deduplicate(const QList<Song> &songs) const
{
  QList<Song> uniqueSongs;
  QSet<QString> uniquePaths;

  for (const Song &song : songs)
  {
    const QString normalizedPath = QFileInfo(song.filePath()).absoluteFilePath().toLower();
    if (normalizedPath.isEmpty() || uniquePaths.contains(normalizedPath))
    {
      continue;
    }

    uniquePaths.insert(normalizedPath);
    uniqueSongs.append(song);
  }

  return uniqueSongs;
}

QList<Song> LibraryModel::filterByKeyword(const QList<Song> &songs, const QString &keyword) const
{
  const QString trimmedKeyword = keyword.trimmed();
  if (trimmedKeyword.isEmpty())
  {
    return songs;
  }

  QList<Song> filteredSongs;
  for (const Song &song : songs)
  {
    if (song.title().contains(trimmedKeyword, Qt::CaseInsensitive) || song.artist().contains(trimmedKeyword, Qt::CaseInsensitive) || song.album().contains(trimmedKeyword, Qt::CaseInsensitive))
    {
      filteredSongs.append(song);
    }
  }

  return filteredSongs;
}
