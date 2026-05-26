/***************************************************
 *  @file      librarycontroller.h
 *  @brief     音乐库业务调度：接收用户导入文件/文件夹、搜索、按歌手/专辑筛选的操作，
 *             调用扫描工具扫文件，调用存储层存数据，管理歌单/收藏/历史等业务，再把歌曲列表同步给UI
 *
 *  @author    un
 *  @date      2026/04/13
 *  @history
 ****************************************************/
#ifndef LIBRARYCONTROLLER_H
#define LIBRARYCONTROLLER_H

#include <QObject>
#include <QList>
#include <QString>
#include <QUrl>

#include "./entity/song.h"
#include "./entity/playlist.h"
#include "./models/librarymodel.h"
#include "./models/playlistmodel.h"
#include "./dao/cachedao.h"
#include "./utils/filescanner.h"

class LibraryController : public QObject
{
  Q_OBJECT
public:
  explicit LibraryController(QObject *parent = nullptr);

  // ===== 基础导入 =====
  QString defaultMusicDirectory() const;
  bool directoryContainsAudio(const QString &directoryPath) const;
  QList<Song> loadSongsFromDirectory(const QString &directoryPath = QString());

  // 异步批量导入（不阻塞UI）
  void importSongsAsync(); // 使用默认音乐目录
  void importSongsAsync(const QString &directoryPath);
  void importSongsAsync(const QStringList &filePaths);
  bool isImporting() const;

  QList<QUrl> buildQueue(const QList<Song> &songs) const;

  // ===== 歌曲查询 =====
  QList<Song> allSongs();
  Song songById(int songId);
  int songIdByFilePath(const QString &filePath);

  // ===== 收藏管理 =====
  bool isSongFavorited(int songId);
  bool toggleFavorite(int songId);
  QList<Song> favoriteSongs();

  // ===== 播放历史 =====
  QList<Song> playHistory();
  bool addPlayHistory(int songId, int playedDurationMs = 0);

  // ===== 歌单CRUD =====
  QList<Playlist> allPlaylists();
  Playlist playlistById(int playlistId);
  QList<Song> playlistSongs(int playlistId);
  int createPlaylist(const QString &name, const QString &description = QString());
  bool deletePlaylist(int playlistId);
  bool updatePlaylistInfo(int playlistId, const QString &name, const QString &description);
  bool addSongToPlaylist(int playlistId, int songId);
  bool removeSongFromPlaylist(int playlistId, int songId);

  // ===== 队列URL转歌曲（供播放列表弹窗使用）=====
  QList<Song> songsFromUrls(const QList<QUrl> &urls);

  // ===== 导入文件夹 =====
  bool addImportFolder(const QString &folderPath, bool autoScan = false);

  // ===== 歌词缓存 =====
  bool saveLyricCache(int songId, const QString &lrcContent, int sourceType = 0);
  QString queryLyricCache(int songId);

  // ===== 播放队列快照 =====
  bool savePlayQueueSnapshot(const QList<int> &songIds, int currentIndex);
  QList<int> loadPlayQueueSnapshot(int &currentIndex);

signals:
  void songsChanged();
  void playlistsChanged();
  void favoriteChanged(int songId, bool isFavorite);
  void historyChanged();
  void playlistDetailChanged(int playlistId);
  void errorOccurred(const QString &errMsg);

  // 异步导入进度/完成信号
  void importStarted(int totalFiles);
  void importProgress(int processedFiles, int totalFiles);
  void importFinished(const QList<Song> &songs, int elapsedMs);

private:
  void refreshCache();

  FileScanner m_fileScanner;
  LibraryModel m_libraryModel;
  PlaylistModel *m_playlistModel = nullptr;
  CacheDao *m_cacheDao = nullptr;
  bool m_isImporting = false;
};

#endif // LIBRARYCONTROLLER_H
