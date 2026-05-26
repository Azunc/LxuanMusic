#include "filescanner.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>
#include <QDebug>

FileScanner::FileScanner() {}

QString FileScanner::defaultMusicDirectory() const
{
  QString musicDir = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
  if (musicDir.isEmpty())
  {
    musicDir = QDir::homePath();
  }
  return musicDir;
}

QStringList FileScanner::scanAudioFiles(const QString &rootPath, bool recursive) const
{
  QStringList filePaths;
  const QString scanRoot = rootPath.trimmed().isEmpty() ? defaultMusicDirectory() : rootPath.trimmed();

  QDir rootDir(scanRoot);
  if (!rootDir.exists())
  {
    return filePaths;
  }

  QSet<QString> uniquePaths;
  QDirIterator it(scanRoot,
                  supportedNameFilters(),
                  QDir::Files | QDir::Readable | QDir::NoSymLinks,
                  recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);

  while (it.hasNext())
  {
    const QString filePath = QDir::cleanPath(it.next());
    const QString normalizedPath = QFileInfo(filePath).absoluteFilePath().toLower();
    if (uniquePaths.contains(normalizedPath))
    {
      continue;
    }

    uniquePaths.insert(normalizedPath);
    filePaths.append(filePath);
    qDebug()<< filePath << Qt::endl;
  }

  filePaths.sort(Qt::CaseInsensitive);
  return filePaths;
}

QStringList FileScanner::supportedNameFilters() const
{
  return {
      QStringLiteral("*.mp3"),
      QStringLiteral("*.wav"),
      QStringLiteral("*.wma"),
      QStringLiteral("*.flac"),
      QStringLiteral("*.aac"),
      QStringLiteral("*.m4a"),
      QStringLiteral("*.ogg")};
}
