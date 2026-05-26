/***************************************************
 *  @file      DbDao.h
 *  @brief     数据库通用操作封装：基于 SQLite，负责歌曲、歌单、播放历史、收藏记录等核心数据读写
 *
 *  @author    un
 *  @date      2026/04/13
 *  @history
 ****************************************************/
#ifndef DBDAO_H
#define DBDAO_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QString>

#include "./entity/playlist.h"
#include "./entity/song.h"

class DbDao : public QObject
{
  Q_OBJECT
public:
  static DbDao *instance();
  ~DbDao() override;

  // 初始化 SQLite 数据库连接
  bool initDb(const QString &dbPath = QString());
  // 初始化表结构（第一次运行自动建表）
  bool createTables();
  bool isDbConnected() const; // 数据库是否连接成功
  QString currentDriverName() const;
  QString sqliteDatabasePath() const;

  // ================== 歌曲相关操作 ==================
  int insertSong(const Song &song);                                // 插入歌曲，返回自增ID
  bool insertSongsBatch(const QList<Song> &songs, int playlistId); // 批量插入歌曲（事务）
  bool updateSong(const Song &song);
  bool deleteSong(int songId);
  QList<Song> queryAllSongs();                      // 查询所有本地歌曲
  QList<Song> queryFavoriteSongs();                 // 查询收藏的歌曲
  bool updateFavorite(int songId, bool isFavorite); // 更新收藏状态
  bool incrementPlayCount(int songId);              // 增加歌曲播放次数

  // ================== 歌单相关操作 ==================
  int insertPlaylist(const Playlist &playlist); // 插入歌单
  bool deletePlaylist(int playlistId);
  bool addSongToPlaylist(int playlistId, int songId); // 歌曲加入歌单
  bool removeSongFromPlaylist(int playlistId, int songId);
  QList<Playlist> queryAllPlaylists();                                                      // 查询所有歌单（包含系统歌单）
  QList<Song> queryPlaylistSongs(int playlistId);                                           // 查询歌单下所有歌曲
  bool updatePlaylistInfo(int playlistId, const QString &name, const QString &description); // 更新歌单名称和简介

  // ================== 播放历史 ==================
  bool addPlayHistory(int songId, int playedDurationMs = 0);
  QList<Song> queryPlayHistory(int limit = 100); // 查询最近播放历史

  // ================== 应用配置 ==================
  bool saveSetting(const QString &key, const QString &value);
  QString querySetting(const QString &key, const QString &defaultValue = QString());

  // ================== 播放队列快照 ==================
  bool savePlayQueueSnapshot(const QList<int> &songIds, int currentIndex);
  QList<int> loadPlayQueueSnapshot(int &currentIndex);
  bool clearPlayQueueSnapshot();

  // ================== 导入文件夹管理 ==================
  bool addImportFolder(const QString &folderPath, bool autoScan = false);
  bool removeImportFolder(int folderId);
  bool updateImportFolderScanTime(int folderId);
  struct ImportFolder
  {
    int folderId;
    QString folderPath;
    bool autoScan;
    QString lastScanTime;
    QString addTime;
  };
  QList<ImportFolder> queryAllImportFolders();

  // ================== 歌词缓存 ==================
  bool saveLyricCache(int songId, const QString &lrcContent, int sourceType = 0);
  QString queryLyricCache(int songId);
  bool deleteLyricCache(int songId);

private:
  explicit DbDao(QObject *parent = nullptr);
  Q_DISABLE_COPY(DbDao)

  // 对象和 SQL 记录互转
  Song songFromRecord(const QSqlRecord &record) const;
  Playlist playlistFromRecord(const QSqlRecord &record) const;

  // 初始化与建表辅助方法
  bool migrateSchema();              // 数据库迁移：修复已有表中可空字段的NOT NULL约束
  bool migrateSQLiteSongTable();     // SQLite song表迁移：lrc_path去掉NOT NULL
  bool migrateSQLitePlaylistTable(); // SQLite playlist表迁移：group_id -> description
  bool addMissingColumn(const QString &tableName, const QString &columnName, const QString &columnDef);
  bool runStatement(QSqlQuery &query, const QString &sql, const QString &context) const;
  bool ensureSystemPlaylists();
  int nextCustomPlaylistId();

  QString defaultSqlitePath() const;

private:
  QSqlDatabase m_db;
  QString m_sqlitePath;
};

#endif // DBDAO_H
