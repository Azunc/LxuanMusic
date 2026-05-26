#include "librarycontroller.h"

#include "./dao/dbdao.h"
#include "./utils/metadataextractor.h"
#include <QDebug>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QHash>
#include <QtConcurrent>

LibraryController::LibraryController(QObject *parent)
    : QObject(parent)
{
  m_playlistModel = new PlaylistModel(this);
  m_cacheDao = CacheDao::instance();
}

QString LibraryController::defaultMusicDirectory() const
{
  return m_fileScanner.defaultMusicDirectory();
}

bool LibraryController::directoryContainsAudio(const QString &directoryPath) const
{
  const QString rootPath = directoryPath.trimmed().isEmpty() ? defaultMusicDirectory() : directoryPath.trimmed();
  return !m_fileScanner.scanAudioFiles(rootPath, true).isEmpty();
}

QList<Song> LibraryController::loadSongsFromDirectory(const QString &directoryPath)
{
  QElapsedTimer timer;
  timer.start();

  const QString rootPath = directoryPath.trimmed().isEmpty() ? defaultMusicDirectory() : directoryPath.trimmed();
  qDebug() << "[LibraryController] 开始扫描目录:" << rootPath;

  const QStringList filePaths = m_fileScanner.scanAudioFiles(rootPath, true);
  qDebug() << "[LibraryController] 扫描到音频文件数:" << filePaths.size();

  const QList<Song> songs = m_libraryModel.importFiles(filePaths);
  qDebug() << "[LibraryController] 提取元数据完成，有效歌曲数:" << songs.size();

  DbDao *dbDao = DbDao::instance();
  if (dbDao->isDbConnected())
  {
    for (const Song &song : songs)
    {
      dbDao->insertSong(song);
    }
    QList<Song> all = dbDao->queryAllSongs();
    m_cacheDao->cacheAllSongs(all);
    qDebug() << "[LibraryController] 扫描完成，总计耗时:" << timer.elapsed() << "ms，数据库歌曲总数:" << all.size();
    return all;
  }

  m_cacheDao->cacheAllSongs(songs);
  qDebug() << "[LibraryController] 扫描完成，总计耗时:" << timer.elapsed() << "ms，歌曲数:" << songs.size();
  return songs;
}

bool LibraryController::isImporting() const
{
  return m_isImporting;
}

void LibraryController::importSongsAsync()
{
  importSongsAsync(defaultMusicDirectory());
}

void LibraryController::importSongsAsync(const QString &directoryPath)
{
  const QString rootPath = directoryPath.trimmed().isEmpty() ? defaultMusicDirectory() : directoryPath.trimmed();
  const QStringList filePaths = m_fileScanner.scanAudioFiles(rootPath, true);
  importSongsAsync(filePaths);
}

void LibraryController::importSongsAsync(const QStringList &filePaths)
{
  if (filePaths.isEmpty())
  {
    // 即使没有新文件，也从数据库加载已有歌曲，避免UI空白
    DbDao *dbDao = DbDao::instance();
    QList<Song> allSongs;
    if (dbDao && dbDao->isDbConnected())
    {
      allSongs = dbDao->queryAllSongs();
    }
    m_cacheDao->cacheAllSongs(allSongs);
    emit importFinished(allSongs, 0);
    return;
  }

  if (m_isImporting)
  {
    qWarning() << "[LibraryController] 已有导入任务正在运行，忽略本次请求";
    return;
  }

  m_isImporting = true;
  emit importStarted(filePaths.size());

  // 后台线程只执行文件扫描与元数据提取（数据库操作不可跨线程）
  auto future = QtConcurrent::run([this, filePaths]()
                                  {
    QElapsedTimer timer;
    timer.start();

    const QList<Song> songs = m_libraryModel.importFilesParallel(filePaths);

    const int elapsed = timer.elapsed();
    qDebug() << "[LibraryController] 元数据提取完成，耗时:" << elapsed
             << "ms，文件数:" << filePaths.size() << "，有效歌曲:" << songs.size();

    // 切回主线程执行数据库写入与UI刷新（QSqlDatabase不可跨线程）
    QMetaObject::invokeMethod(this, [this, songs, filePaths, elapsed]() {
      DbDao *dbDao = DbDao::instance();
      if (dbDao && dbDao->isDbConnected() && !songs.isEmpty())
      {
        dbDao->insertSongsBatch(songs, Playlist::PL_LOCAL);
      }

      QList<Song> allSongs;
      if (dbDao && dbDao->isDbConnected())
      {
        allSongs = dbDao->queryAllSongs();
      }
      else
      {
        allSongs = songs;
      }
      m_cacheDao->cacheAllSongs(allSongs);

      m_isImporting = false;
      emit songsChanged();
      emit importFinished(allSongs, elapsed);
    }, Qt::QueuedConnection); });
  Q_UNUSED(future)
}

QList<QUrl> LibraryController::buildQueue(const QList<Song> &songs) const
{
  QList<QUrl> queue;
  queue.reserve(songs.size());
  for (const Song &song : songs)
  {
    if (!song.filePath().trimmed().isEmpty())
    {
      queue.append(QUrl::fromLocalFile(song.filePath()));
    }
  }
  return queue;
}

// ===== 歌曲查询 =====
QList<Song> LibraryController::allSongs()
{
  QList<Song> cached = m_cacheDao->allSongs();
  if (!cached.isEmpty())
    return cached;

  DbDao *dbDao = DbDao::instance();
  if (dbDao && dbDao->isDbConnected())
  {
    cached = dbDao->queryAllSongs();
    m_cacheDao->cacheAllSongs(cached);
  }
  return cached;
}

Song LibraryController::songById(int songId)
{
  Song s = m_cacheDao->songById(songId);
  if (s.isValid())
    return s;

  QList<Song> all = allSongs();
  for (const Song &song : all)
  {
    if (song.songId() == songId)
      return song;
  }
  return Song();
}

int LibraryController::songIdByFilePath(const QString &filePath)
{
  int id = m_cacheDao->songIdByFilePath(filePath);
  if (id > 0)
    return id;

  QList<Song> all = allSongs();
  const QString normalizedPath = QFileInfo(filePath).absoluteFilePath();
  for (const Song &song : all)
  {
    if (QFileInfo(song.filePath()).absoluteFilePath() == normalizedPath)
      return song.songId();
  }
  return -1;
}

// ===== 收藏管理 =====
bool LibraryController::isSongFavorited(int songId)
{
  if (m_cacheDao->favoriteSongs().isEmpty())
  {
    DbDao *dbDao = DbDao::instance();
    if (dbDao && dbDao->isDbConnected())
      m_cacheDao->cacheFavoriteSongs(dbDao->queryPlaylistSongs(Playlist::PL_FAVORITE));
  }
  return m_cacheDao->isSongFavorited(songId);
}

bool LibraryController::toggleFavorite(int songId)
{
  DbDao *dbDao = DbDao::instance();
  if (!dbDao || !dbDao->isDbConnected())
    return false;

  bool isFav = isSongFavorited(songId);
  bool ok;
  if (isFav)
    ok = dbDao->removeSongFromPlaylist(Playlist::PL_FAVORITE, songId);
  else
    ok = dbDao->addSongToPlaylist(Playlist::PL_FAVORITE, songId);

  if (ok)
  {
    // 刷新缓存
    m_cacheDao->cacheFavoriteSongs(dbDao->queryPlaylistSongs(Playlist::PL_FAVORITE));
    emit favoriteChanged(songId, !isFav);
  }
  return ok;
}

QList<Song> LibraryController::favoriteSongs()
{
  QList<Song> cached = m_cacheDao->favoriteSongs();
  if (!cached.isEmpty())
    return cached;

  DbDao *dbDao = DbDao::instance();
  if (dbDao && dbDao->isDbConnected())
  {
    cached = dbDao->queryPlaylistSongs(Playlist::PL_FAVORITE);
    m_cacheDao->cacheFavoriteSongs(cached);
  }
  return cached;
}

// ===== 播放历史 =====
QList<Song> LibraryController::playHistory()
{
  QList<Song> cached = m_cacheDao->historySongs();
  if (!cached.isEmpty())
    return cached;

  DbDao *dbDao = DbDao::instance();
  if (dbDao && dbDao->isDbConnected())
  {
    cached = dbDao->queryPlayHistory();
    m_cacheDao->cacheHistorySongs(cached);
  }
  return cached;
}

bool LibraryController::addPlayHistory(int songId, int playedDurationMs)
{
  DbDao *dbDao = DbDao::instance();
  if (!dbDao || !dbDao->isDbConnected())
    return false;

  bool ok = dbDao->addPlayHistory(songId, playedDurationMs);
  if (ok)
    m_cacheDao->clearHistorySongs(); // 下次访问时重新加载
  return ok;
}

// ===== 歌单CRUD =====
QList<Playlist> LibraryController::allPlaylists()
{
  QList<Playlist> cached = m_cacheDao->playlists();
  if (!cached.isEmpty())
    return cached;

  DbDao *dbDao = DbDao::instance();
  if (dbDao && dbDao->isDbConnected())
  {
    cached = dbDao->queryAllPlaylists();
    m_cacheDao->cachePlaylists(cached);
  }
  return cached;
}

Playlist LibraryController::playlistById(int playlistId)
{
  Playlist pl = m_cacheDao->playlistById(playlistId);
  if (pl.isValid())
    return pl;

  DbDao *dbDao = DbDao::instance();
  if (dbDao && dbDao->isDbConnected())
  {
    for (const Playlist &p : dbDao->queryAllPlaylists())
    {
      if (p.playlistId() == playlistId)
        return p;
    }
  }
  return Playlist();
}

QList<Song> LibraryController::playlistSongs(int playlistId)
{
  QList<Song> cached = m_cacheDao->playlistSongs(playlistId);
  if (!cached.isEmpty())
    return cached;

  DbDao *dbDao = DbDao::instance();
  if (dbDao && dbDao->isDbConnected())
  {
    cached = dbDao->queryPlaylistSongs(playlistId);
    m_cacheDao->cachePlaylistSongs(playlistId, cached);
  }
  return cached;
}

int LibraryController::createPlaylist(const QString &name, const QString &description)
{
  DbDao *dbDao = DbDao::instance();
  if (!dbDao || !dbDao->isDbConnected())
    return -1;

  Playlist pl(name, false);
  pl.setDescription(description);
  int id = dbDao->insertPlaylist(pl);
  if (id > 0)
  {
    m_cacheDao->clearPlaylists();
    emit playlistsChanged();
  }
  return id;
}

bool LibraryController::deletePlaylist(int playlistId)
{
  DbDao *dbDao = DbDao::instance();
  if (!dbDao || !dbDao->isDbConnected())
    return false;

  bool ok = dbDao->deletePlaylist(playlistId);
  if (ok)
  {
    m_cacheDao->clearPlaylists();
    m_cacheDao->clearPlaylistSongs(playlistId);
    emit playlistsChanged();
  }
  return ok;
}

bool LibraryController::updatePlaylistInfo(int playlistId, const QString &name, const QString &description)
{
  DbDao *dbDao = DbDao::instance();
  if (!dbDao || !dbDao->isDbConnected())
    return false;

  bool ok = dbDao->updatePlaylistInfo(playlistId, name, description);
  if (ok)
  {
    m_cacheDao->clearPlaylists();
    emit playlistsChanged();
  }
  return ok;
}

bool LibraryController::addSongToPlaylist(int playlistId, int songId)
{
  DbDao *dbDao = DbDao::instance();
  if (!dbDao || !dbDao->isDbConnected())
    return false;

  bool ok = dbDao->addSongToPlaylist(playlistId, songId);
  if (ok)
  {
    m_cacheDao->clearPlaylistSongs(playlistId);
    emit playlistDetailChanged(playlistId);
  }
  return ok;
}

bool LibraryController::removeSongFromPlaylist(int playlistId, int songId)
{
  DbDao *dbDao = DbDao::instance();
  if (!dbDao || !dbDao->isDbConnected())
    return false;

  bool ok = dbDao->removeSongFromPlaylist(playlistId, songId);
  if (ok)
  {
    m_cacheDao->clearPlaylistSongs(playlistId);
    emit playlistDetailChanged(playlistId);
  }
  return ok;
}

// ===== 队列URL转歌曲 =====
QList<Song> LibraryController::songsFromUrls(const QList<QUrl> &urls)
{
  QElapsedTimer timer;
  timer.start();

  QList<Song> result;
  result.reserve(urls.size());

  const QList<Song> all = allSongs();
  QHash<QString, Song> songMap;
  songMap.reserve(all.size());
  for (const Song &s : all)
  {
    songMap.insert(QFileInfo(s.filePath()).absoluteFilePath().toLower(), s);
  }

  for (const QUrl &url : urls)
  {
    if (!url.isLocalFile())
      continue;

    const QString path = url.toLocalFile();
    const QString key = QFileInfo(path).absoluteFilePath().toLower();
    if (songMap.contains(key))
    {
      result.append(songMap.value(key));
    }
    else
    {
      result.append(MetaDataExtractor().extract(path));
    }
  }

  qDebug() << "[LibraryController] 播放队列URL转歌曲耗时:" << timer.elapsed()
           << "ms, queue=" << urls.size() << "result=" << result.size();
  return result;
}

// ===== 导入文件夹 =====
bool LibraryController::addImportFolder(const QString &folderPath, bool autoScan)
{
  DbDao *dbDao = DbDao::instance();
  if (!dbDao || !dbDao->isDbConnected())
    return false;
  return dbDao->addImportFolder(folderPath, autoScan);
}

// ===== 歌词缓存 =====
bool LibraryController::saveLyricCache(int songId, const QString &lrcContent, int sourceType)
{
  DbDao *dbDao = DbDao::instance();
  if (!dbDao || !dbDao->isDbConnected())
    return false;
  return dbDao->saveLyricCache(songId, lrcContent, sourceType);
}

QString LibraryController::queryLyricCache(int songId)
{
  DbDao *dbDao = DbDao::instance();
  if (!dbDao || !dbDao->isDbConnected())
    return QString();
  return dbDao->queryLyricCache(songId);
}

// ===== 播放队列快照 =====
bool LibraryController::savePlayQueueSnapshot(const QList<int> &songIds, int currentIndex)
{
  DbDao *dbDao = DbDao::instance();
  if (!dbDao || !dbDao->isDbConnected())
    return false;
  return dbDao->savePlayQueueSnapshot(songIds, currentIndex);
}

QList<int> LibraryController::loadPlayQueueSnapshot(int &currentIndex)
{
  DbDao *dbDao = DbDao::instance();
  if (!dbDao || !dbDao->isDbConnected())
    return QList<int>();
  return dbDao->loadPlayQueueSnapshot(currentIndex);
}
