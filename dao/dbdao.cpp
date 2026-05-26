#include "DbDao.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

DbDao::DbDao(QObject *parent)
    : QObject{parent}
{
}

DbDao::~DbDao()
{
  if (m_db.isOpen())
  {
    m_db.close();
  }
}

// 单例实现
DbDao *DbDao::instance()
{
  static DbDao ins;
  return &ins;
}

// 初始化 SQLite 数据库连接
bool DbDao::initDb(const QString &dbPath)
{
  const QString connectionName = QStringLiteral("LxuanMusicSQLiteConnection");

  if (QSqlDatabase::contains(connectionName))
  {
    m_db = QSqlDatabase::database(connectionName);
  }
  else
  {
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
  }

  QString sqlitePath = dbPath.trimmed();
  if (sqlitePath.isEmpty())
  {
    sqlitePath = defaultSqlitePath();
  }

  QFileInfo dbFileInfo(sqlitePath);
  QDir dbDir = dbFileInfo.dir();
  if (!dbDir.exists() && !dbDir.mkpath(QStringLiteral(".")))
  {
    qWarning() << "[DB Error] SQLite目录创建失败：" << dbDir.absolutePath();
    return false;
  }

  m_sqlitePath = dbFileInfo.absoluteFilePath();
  m_db.setDatabaseName(m_sqlitePath);
  m_db.setConnectOptions(QString());

  if (!m_db.open())
  {
    qWarning() << "[DB Error] 连接失败：" << m_db.lastError().text();
    return false;
  }

  QSqlQuery pragmaQuery(m_db);
  pragmaQuery.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
  pragmaQuery.exec(QStringLiteral("PRAGMA journal_mode = WAL"));

  if (!createTables())
  {
    qWarning() << "[DB Error] 建表失败：" << m_db.lastError().text();
    return false;
  }

  qInfo() << "[DB Info] SQLite数据库连接成功：" << m_sqlitePath;
  return true;
}

// 自动建表（首次启动执行，已有表不会重复创建）
bool DbDao::createTables()
{
  if (!m_db.isOpen())
  {
    qWarning() << "[DB Error] 数据库未连接，无法建表";
    return false;
  }

  // 对于已有旧数据库，先执行字段迁移
  if (!migrateSchema())
  {
    qWarning() << "[DB Warning] 数据库迁移失败，部分功能可能受影响";
  }

  QSqlQuery query(m_db);

  const QStringList statements = {
      QStringLiteral(R"(
            CREATE TABLE IF NOT EXISTS song (
                song_id INTEGER PRIMARY KEY AUTOINCREMENT,
                file_path TEXT NOT NULL UNIQUE,
                title TEXT NOT NULL DEFAULT '',
                artist TEXT NOT NULL DEFAULT '未知歌手',
                album TEXT NOT NULL DEFAULT '未知专辑',
                duration INTEGER NOT NULL DEFAULT 0,
                file_size INTEGER NOT NULL DEFAULT 0,
                bit_rate INTEGER NOT NULL DEFAULT 0,
                add_time TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                is_favorite INTEGER NOT NULL DEFAULT 0,
                lrc_path TEXT DEFAULT '',
                play_count INTEGER NOT NULL DEFAULT 0
            )
        )"),
      QStringLiteral(R"(
            CREATE INDEX IF NOT EXISTS idx_song_title_artist
            ON song(title, artist)
        )"),
      QStringLiteral(R"(
            CREATE TABLE IF NOT EXISTS playlist (
                playlist_id INTEGER NOT NULL PRIMARY KEY,
                name TEXT NOT NULL,
                is_system INTEGER NOT NULL DEFAULT 0,
                description TEXT DEFAULT '',
                create_time TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                cover_path TEXT DEFAULT ''
            )
        )"),
      QStringLiteral(R"(
            CREATE TABLE IF NOT EXISTS playlist_song (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                playlist_id INTEGER NOT NULL,
                song_id INTEGER NOT NULL,
                add_time TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                UNIQUE(playlist_id, song_id),
                FOREIGN KEY(playlist_id) REFERENCES playlist(playlist_id) ON DELETE CASCADE,
                FOREIGN KEY(song_id) REFERENCES song(song_id) ON DELETE CASCADE
            )
        )"),
      QStringLiteral(R"(
            CREATE INDEX IF NOT EXISTS idx_playlist_song_playlist_id
            ON playlist_song(playlist_id)
        )"),
      QStringLiteral(R"(
            CREATE TABLE IF NOT EXISTS play_history (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                song_id INTEGER NOT NULL,
                play_time TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                played_duration_ms INTEGER NOT NULL DEFAULT 0,
                FOREIGN KEY(song_id) REFERENCES song(song_id) ON DELETE CASCADE
            )
        )"),
      QStringLiteral(R"(
            CREATE INDEX IF NOT EXISTS idx_play_history_time
            ON play_history(play_time DESC)
        )"),
      QStringLiteral(R"(
            CREATE TABLE IF NOT EXISTS app_setting (
                setting_key TEXT PRIMARY KEY,
                setting_value TEXT NOT NULL DEFAULT '',
                update_time TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
            )
        )"),
      QStringLiteral(R"(
            CREATE TABLE IF NOT EXISTS play_queue_snapshot (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                song_id INTEGER NOT NULL,
                order_index INTEGER NOT NULL,
                is_current INTEGER NOT NULL DEFAULT 0,
                create_time TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY(song_id) REFERENCES song(song_id) ON DELETE CASCADE
            )
        )"),
      QStringLiteral(R"(
            CREATE INDEX IF NOT EXISTS idx_queue_snapshot_order
            ON play_queue_snapshot(order_index)
        )"),
      QStringLiteral(R"(
            CREATE TABLE IF NOT EXISTS import_folder (
                folder_id INTEGER PRIMARY KEY AUTOINCREMENT,
                folder_path TEXT NOT NULL UNIQUE,
                auto_scan INTEGER NOT NULL DEFAULT 0,
                last_scan_time TEXT DEFAULT '',
                add_time TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
            )
        )"),
      QStringLiteral(R"(
            CREATE TABLE IF NOT EXISTS lyric_cache (
                song_id INTEGER PRIMARY KEY,
                lrc_content TEXT NOT NULL DEFAULT '',
                source_type INTEGER NOT NULL DEFAULT 0,
                update_time TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY(song_id) REFERENCES song(song_id) ON DELETE CASCADE
            )
        )")};

  for (int i = 0; i < statements.size(); ++i)
  {
    if (!runStatement(query, statements.at(i), QStringLiteral("创建表/索引失败[%1]").arg(i)))
    {
      return false;
    }
  }

  return ensureSystemPlaylists();
}

// ========================== 数据库迁移 ==========================
// 修复已有表中可空字段的NOT NULL约束，并补全新增字段
bool DbDao::migrateSchema()
{
  if (!m_db.isOpen())
  {
    return false;
  }

  bool ok = migrateSQLiteSongTable();
  ok = migrateSQLitePlaylistTable() && ok;
  ok = addMissingColumn(QStringLiteral("song"), QStringLiteral("play_count"), QStringLiteral("INTEGER NOT NULL DEFAULT 0")) && ok;
  ok = addMissingColumn(QStringLiteral("playlist"), QStringLiteral("cover_path"), QStringLiteral("TEXT DEFAULT ''")) && ok;
  ok = addMissingColumn(QStringLiteral("play_history"), QStringLiteral("played_duration_ms"), QStringLiteral("INTEGER NOT NULL DEFAULT 0")) && ok;

  // 为新增字段补建索引
  QSqlQuery idxQuery(m_db);
  idxQuery.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_song_play_count ON song(play_count DESC)"));

  return ok;
}

// 通用：为已有表添加缺失列（SQLite支持ALTER TABLE ADD COLUMN）
bool DbDao::addMissingColumn(const QString &tableName, const QString &columnName, const QString &columnDef)
{
  QSqlQuery infoQuery(m_db);
  infoQuery.prepare(QStringLiteral("PRAGMA table_info(%1)").arg(tableName));
  if (!infoQuery.exec())
  {
    qWarning() << "[DB Warning] 获取" << tableName << "表结构失败：" << infoQuery.lastError().text();
    return false;
  }

  while (infoQuery.next())
  {
    if (infoQuery.value(1).toString() == columnName)
    {
      return true; // 列已存在
    }
  }

  QSqlQuery alterQuery(m_db);
  const QString sql = QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3").arg(tableName, columnName, columnDef);
  if (!alterQuery.exec(sql))
  {
    qWarning() << "[DB Error] 为" << tableName << "表添加" << columnName << "列失败：" << alterQuery.lastError().text();
    return false;
  }

  qInfo() << "[DB Info] 已为" << tableName << "表添加" << columnName << "字段";
  return true;
}

// SQLite不支持ALTER COLUMN，需要重建song表来去掉lrc_path的NOT NULL约束
bool DbDao::migrateSQLiteSongTable()
{
  // 检查song表是否存在lrc_path列的NOT NULL约束
  // 方法：尝试插入一条lrc_path为NULL的测试记录（在事务中回滚）
  // 更简单的方式：检查PRAGMA table_info(song)中lrc_path的notnull标志
  QSqlQuery infoQuery(m_db);
  infoQuery.prepare(QStringLiteral("PRAGMA table_info(song)"));
  if (!infoQuery.exec())
  {
    qWarning() << "[DB Warning] 获取song表结构失败：" << infoQuery.lastError().text();
    return false;
  }

  bool lrcPathNotNull = false;
  while (infoQuery.next())
  {
    // PRAGMA table_info返回：cid, name, type, notnull, dflt_value, pk
    if (infoQuery.value(1).toString() == QStringLiteral("lrc_path"))
    {
      lrcPathNotNull = (infoQuery.value(3).toInt() == 1);
      break;
    }
  }

  if (!lrcPathNotNull)
  {
    // lrc_path已经没有NOT NULL约束，无需迁移
    return true;
  }

  qInfo() << "[DB Info] 检测到song表lrc_path字段存在NOT NULL约束，开始迁移...";

  // SQLite不支持ALTER COLUMN，需要重建表
  // 步骤：1.创建新表 2.复制数据 3.删除旧表 4.重命名新表 5.重建索引
  QSqlQuery query(m_db);
  m_db.transaction();

  try
  {
    // 1. 创建临时新表（lrc_path去掉NOT NULL，补充play_count）
    if (!runStatement(query, QStringLiteral(R"(
            CREATE TABLE song_new (
                song_id INTEGER PRIMARY KEY AUTOINCREMENT,
                file_path TEXT NOT NULL UNIQUE,
                title TEXT NOT NULL DEFAULT '',
                artist TEXT NOT NULL DEFAULT '未知歌手',
                album TEXT NOT NULL DEFAULT '未知专辑',
                duration INTEGER NOT NULL DEFAULT 0,
                file_size INTEGER NOT NULL DEFAULT 0,
                bit_rate INTEGER NOT NULL DEFAULT 0,
                add_time TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                is_favorite INTEGER NOT NULL DEFAULT 0,
                lrc_path TEXT DEFAULT '',
                play_count INTEGER NOT NULL DEFAULT 0
            )
        )"),
                      QStringLiteral("迁移：创建临时song表失败")))
    {
      throw query.lastError();
    }

    // 2. 复制数据
    if (!runStatement(query, QStringLiteral(R"(
            INSERT INTO song_new(song_id, file_path, title, artist, album, duration, file_size, bit_rate, add_time, is_favorite, lrc_path, play_count)
            SELECT song_id, file_path, title, artist, album, duration, file_size, bit_rate, add_time, is_favorite, lrc_path, COALESCE(play_count, 0) FROM song
        )"),
                      QStringLiteral("迁移：复制song数据失败")))
    {
      throw query.lastError();
    }

    // 3. 删除旧表
    if (!runStatement(query, QStringLiteral("DROP TABLE song"),
                      QStringLiteral("迁移：删除旧song表失败")))
    {
      throw query.lastError();
    }

    // 4. 重命名新表
    if (!runStatement(query, QStringLiteral("ALTER TABLE song_new RENAME TO song"),
                      QStringLiteral("迁移：重命名song表失败")))
    {
      throw query.lastError();
    }

    // 5. 重建索引
    if (!runStatement(query, QStringLiteral("CREATE INDEX IF NOT EXISTS idx_song_title_artist ON song(title, artist)"),
                      QStringLiteral("迁移：重建索引失败")))
    {
      throw query.lastError();
    }

    m_db.commit();
    qInfo() << "[DB Info] song表迁移完成，lrc_path已去掉NOT NULL约束";
    return true;
  }
  catch (const QSqlError &e)
  {
    m_db.rollback();
    qWarning() << "[DB Error] song表迁移失败，已回滚：" << e.text();
    return false;
  }
}

bool DbDao::migrateSQLitePlaylistTable()
{
  QSqlQuery infoQuery(m_db);
  infoQuery.prepare(QStringLiteral("PRAGMA table_info(playlist)"));
  if (!infoQuery.exec())
  {
    qWarning() << "[DB Warning] 获取playlist表结构失败：" << infoQuery.lastError().text();
    return false;
  }

  bool hasDescription = false;
  while (infoQuery.next())
  {
    if (infoQuery.value(1).toString() == QStringLiteral("description"))
    {
      hasDescription = true;
      break;
    }
  }

  // 如果已有 description 列，说明已经是新结构
  if (hasDescription)
  {
    return true;
  }

  qInfo() << "[DB Info] 检测到playlist表缺少description字段，尝试添加...";

  // 先打印当前表结构便于排查
  QSqlQuery debugQuery(m_db);
  debugQuery.exec(QStringLiteral("PRAGMA table_info(playlist)"));
  QStringList cols;
  while (debugQuery.next())
  {
    cols.append(debugQuery.value(1).toString() + ":" + debugQuery.value(2).toString());
  }
  qInfo() << "[DB Debug] playlist当前列：" << cols.join(", ");

  // 旧表缺少 description，直接 ALTER TABLE 添加（保留可能存在的 group_id）
  QSqlQuery alterQuery(m_db);
  if (!alterQuery.exec(QStringLiteral("ALTER TABLE playlist ADD COLUMN description TEXT DEFAULT ''")))
  {
    qWarning() << "[DB Error] 为playlist表添加description列失败：" << alterQuery.lastError().text();
    return false;
  }

  qInfo() << "[DB Info] 已为playlist表添加description字段";
  return true;
}

bool DbDao::ensureSystemPlaylists()
{
  QSqlQuery query(m_db);
  const QString insertSql = QStringLiteral("INSERT OR IGNORE INTO playlist(playlist_id, name, is_system, description, create_time) VALUES(?, ?, ?, ?, ?)");

  struct SystemPlaylistDef
  {
    int id;
    const char *name;
  };

  const SystemPlaylistDef playlists[] = {
      {Playlist::PL_LOCAL, "本地音乐"},
      {Playlist::PL_FAVORITE, "我喜欢的音乐"},
      {Playlist::PL_HISTORY, "播放历史"}};

  for (const SystemPlaylistDef &playlist : playlists)
  {
    query.prepare(insertSql);
    query.addBindValue(playlist.id);
    query.addBindValue(QString::fromUtf8(playlist.name));
    query.addBindValue(1);
    query.addBindValue(QString());
    query.addBindValue(QDateTime::currentDateTime());
    if (!query.exec())
    {
      qWarning() << "[DB Error] 初始化系统歌单失败：" << query.lastError().text();
      return false;
    }
  }

  return true;
}

bool DbDao::runStatement(QSqlQuery &query, const QString &sql, const QString &context) const
{
  if (!query.exec(sql))
  {
    qWarning() << "[DB Error]" << context << query.lastError().text();
    return false;
  }
  return true;
}

bool DbDao::isDbConnected() const
{
  return m_db.isOpen();
}

QString DbDao::currentDriverName() const
{
  return m_db.driverName();
}

QString DbDao::sqliteDatabasePath() const
{
  return m_sqlitePath;
}

QString DbDao::defaultSqlitePath() const
{
  QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (baseDir.isEmpty())
  {
    baseDir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("data"));
  }

  QDir dir(baseDir);
  if (!dir.exists())
  {
    dir.mkpath(QStringLiteral("."));
  }

  return dir.filePath(QStringLiteral("music_player.db"));
}

int DbDao::nextCustomPlaylistId()
{
  QSqlQuery query(m_db);
  query.prepare(QStringLiteral("SELECT COALESCE(MAX(playlist_id), ?) + 1 FROM playlist WHERE playlist_id >= ?"));
  query.addBindValue(Playlist::PL_CUSTOM_START - 1);
  query.addBindValue(Playlist::PL_CUSTOM_START);

  if (query.exec() && query.next())
  {
    return query.value(0).toInt();
  }

  qWarning() << "[DB Warning] 获取下一个自定义歌单ID失败，使用默认起始ID：" << query.lastError().text();
  return Playlist::PL_CUSTOM_START;
}

// ========================== 工具方法：SQL记录转Song对象 ==========================
Song DbDao::songFromRecord(const QSqlRecord &record) const
{
  const QVariant addTimeValue = record.value(QStringLiteral("add_time"));
  QDateTime addTime = addTimeValue.toDateTime();
  if (!addTime.isValid())
  {
    const QString addTimeText = addTimeValue.toString();
    addTime = QDateTime::fromString(addTimeText, Qt::ISODate);
    if (!addTime.isValid())
    {
      addTime = QDateTime::fromString(addTimeText, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    }
  }
  if (!addTime.isValid())
  {
    addTime = QDateTime::currentDateTime();
  }

  return Song(record.value(QStringLiteral("song_id")).toInt(),
              record.value(QStringLiteral("file_path")).toString(),
              record.value(QStringLiteral("title")).toString(),
              record.value(QStringLiteral("artist")).toString(),
              record.value(QStringLiteral("album")).toString(),
              record.value(QStringLiteral("duration")).toLongLong(),
              record.value(QStringLiteral("file_size")).toLongLong(),
              record.value(QStringLiteral("bit_rate")).toInt(),
              addTime,
              record.value(QStringLiteral("is_favorite")).toInt() == 1,
              record.value(QStringLiteral("lrc_path")).toString());
}

Playlist DbDao::playlistFromRecord(const QSqlRecord &record) const
{
  const QVariant createTimeValue = record.value(QStringLiteral("create_time"));
  QDateTime createTime = createTimeValue.toDateTime();
  if (!createTime.isValid())
  {
    const QString createTimeText = createTimeValue.toString();
    createTime = QDateTime::fromString(createTimeText, Qt::ISODate);
    if (!createTime.isValid())
    {
      createTime = QDateTime::fromString(createTimeText, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    }
  }
  if (!createTime.isValid())
  {
    createTime = QDateTime::currentDateTime();
  }

  QString description;
  if (record.contains(QStringLiteral("description")))
  {
    description = record.value(QStringLiteral("description")).toString();
  }

  return Playlist(record.value(QStringLiteral("playlist_id")).toInt(),
                  record.value(QStringLiteral("name")).toString(),
                  record.value(QStringLiteral("is_system")).toInt() == 1,
                  createTime,
                  description,
                  {});
}

// ========================== 歌曲相关操作 ==========================
// 插入歌曲，返回自增ID，失败返回-1
int DbDao::insertSong(const Song &song)
{
  if (song.filePath().trimmed().isEmpty())
  {
    return -1;
  }

  QSqlQuery existingQuery(m_db);
  existingQuery.prepare(QStringLiteral("SELECT song_id FROM song WHERE file_path = ?"));
  existingQuery.addBindValue(song.filePath());
  if (existingQuery.exec() && existingQuery.next())
  {
    const int songId = existingQuery.value(0).toInt();
    Song updatedSong = song;
    updatedSong.setSongId(songId);
    updateSong(updatedSong);
    addSongToPlaylist(Playlist::PL_LOCAL, songId);
    if (updatedSong.isFavorite())
    {
      addSongToPlaylist(Playlist::PL_FAVORITE, songId);
    }
    return songId;
  }

  QSqlQuery query(m_db);
  query.prepare(QStringLiteral(R"(
        INSERT INTO song(file_path, title, artist, album, duration, file_size, bit_rate, add_time, is_favorite, lrc_path)
        VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )"));
  query.addBindValue(song.filePath());
  query.addBindValue(song.title().isNull() ? QString() : song.title());
  query.addBindValue(song.artist().isNull() ? QString() : song.artist());
  query.addBindValue(song.album().isNull() ? QString() : song.album());
  query.addBindValue(song.duration());
  query.addBindValue(song.fileSize());
  query.addBindValue(song.bitRate());
  query.addBindValue(song.addTime().isValid() ? song.addTime() : QDateTime::currentDateTime());
  query.addBindValue(song.isFavorite() ? 1 : 0);
  query.addBindValue(song.lrcPath().isNull() ? QString() : song.lrcPath());

  if (query.exec())
  {
    const int songId = query.lastInsertId().toInt();
    addSongToPlaylist(Playlist::PL_LOCAL, songId);
    if (song.isFavorite())
    {
      addSongToPlaylist(Playlist::PL_FAVORITE, songId);
    }
    return songId;
  }

  qWarning() << "[DB Error] 插入歌曲失败：" << query.lastError().text() << "路径：" << song.filePath();
  return -1;
}

// 批量插入歌曲，使用事务包裹，写入指定歌单关联
bool DbDao::insertSongsBatch(const QList<Song> &songs, int playlistId)
{
  if (songs.isEmpty())
  {
    return true;
  }

  m_db.transaction();
  bool ok = true;

  QSqlQuery insertQuery(m_db);
  insertQuery.prepare(QStringLiteral(R"(
        INSERT INTO song(file_path, title, artist, album, duration, file_size, bit_rate, add_time, is_favorite, lrc_path)
        VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )"));

  QSqlQuery checkQuery(m_db);
  checkQuery.prepare(QStringLiteral("SELECT song_id FROM song WHERE file_path = ?"));

  QSqlQuery playlistQuery(m_db);
  playlistQuery.prepare(QStringLiteral("INSERT OR IGNORE INTO playlist_song(playlist_id, song_id, add_time) VALUES(?, ?, ?)"));

  int insertedCount = 0;
  for (const Song &song : songs)
  {
    if (song.filePath().trimmed().isEmpty())
    {
      continue;
    }

    int songId = -1;
    bool isExisting = false;
    checkQuery.bindValue(0, song.filePath());
    if (checkQuery.exec() && checkQuery.next())
    {
      songId = checkQuery.value(0).toInt();
      isExisting = true;
    }
    checkQuery.finish();

    if (isExisting)
    {
      // 已存在则更新元数据（可选，若不需要可注释掉以进一步提升速度）
      Song updatedSong = song;
      updatedSong.setSongId(songId);
      QSqlQuery updateQuery(m_db);
      updateQuery.prepare(QStringLiteral(R"(
            UPDATE song
            SET file_path = ?, title = ?, artist = ?, album = ?, duration = ?, file_size = ?, bit_rate = ?, is_favorite = ?, lrc_path = ?
            WHERE song_id = ?
        )"));
      updateQuery.addBindValue(updatedSong.filePath());
      updateQuery.addBindValue(updatedSong.title().isNull() ? QString() : updatedSong.title());
      updateQuery.addBindValue(updatedSong.artist().isNull() ? QString() : updatedSong.artist());
      updateQuery.addBindValue(updatedSong.album().isNull() ? QString() : updatedSong.album());
      updateQuery.addBindValue(updatedSong.duration());
      updateQuery.addBindValue(updatedSong.fileSize());
      updateQuery.addBindValue(updatedSong.bitRate());
      updateQuery.addBindValue(updatedSong.isFavorite() ? 1 : 0);
      updateQuery.addBindValue(updatedSong.lrcPath().isNull() ? QString() : updatedSong.lrcPath());
      updateQuery.addBindValue(songId);
      if (!updateQuery.exec())
      {
        qWarning() << "[DB Error] 批量更新歌曲失败：" << updateQuery.lastError().text();
        ok = false;
      }
    }
    else
    {
      insertQuery.bindValue(0, song.filePath());
      insertQuery.bindValue(1, song.title().isNull() ? QString() : song.title());
      insertQuery.bindValue(2, song.artist().isNull() ? QString() : song.artist());
      insertQuery.bindValue(3, song.album().isNull() ? QString() : song.album());
      insertQuery.bindValue(4, song.duration());
      insertQuery.bindValue(5, song.fileSize());
      insertQuery.bindValue(6, song.bitRate());
      insertQuery.bindValue(7, song.addTime().isValid() ? song.addTime() : QDateTime::currentDateTime());
      insertQuery.bindValue(8, song.isFavorite() ? 1 : 0);
      insertQuery.bindValue(9, song.lrcPath().isNull() ? QString() : song.lrcPath());

      if (insertQuery.exec())
      {
        songId = insertQuery.lastInsertId().toInt();
        ++insertedCount;
      }
      else
      {
        qWarning() << "[DB Error] 批量插入歌曲失败：" << insertQuery.lastError().text() << "路径：" << song.filePath();
        ok = false;
      }
      insertQuery.finish();
    }

    if (songId > 0 && playlistId >= 0)
    {
      playlistQuery.bindValue(0, playlistId);
      playlistQuery.bindValue(1, songId);
      playlistQuery.bindValue(2, QDateTime::currentDateTime());
      if (!playlistQuery.exec())
      {
        qWarning() << "[DB Error] 批量加入歌单失败：" << playlistQuery.lastError().text();
        ok = false;
      }
      playlistQuery.finish();
    }

    if (!ok)
    {
      break;
    }
  }

  if (ok)
  {
    m_db.commit();
    qDebug() << "[DB Info] 批量插入完成，新增/更新歌曲数：" << songs.size() << "，实际插入：" << insertedCount;
    return true;
  }

  m_db.rollback();
  qWarning() << "[DB Error] 批量插入事务已回滚";
  return false;
}

// 更新歌曲元信息
bool DbDao::updateSong(const Song &song)
{
  if (song.songId() <= 0)
  {
    return false;
  }

  QSqlQuery query(m_db);
  query.prepare(QStringLiteral(R"(
        UPDATE song
        SET file_path = ?, title = ?, artist = ?, album = ?, duration = ?, file_size = ?, bit_rate = ?, is_favorite = ?, lrc_path = ?
        WHERE song_id = ?
    )"));
  query.addBindValue(song.filePath());
  query.addBindValue(song.title().isNull() ? QString() : song.title());
  query.addBindValue(song.artist().isNull() ? QString() : song.artist());
  query.addBindValue(song.album().isNull() ? QString() : song.album());
  query.addBindValue(song.duration());
  query.addBindValue(song.fileSize());
  query.addBindValue(song.bitRate());
  query.addBindValue(song.isFavorite() ? 1 : 0);
  query.addBindValue(song.lrcPath().isNull() ? QString() : song.lrcPath());
  query.addBindValue(song.songId());

  const bool res = query.exec();
  if (!res)
  {
    qWarning() << "[DB Error] 更新歌曲失败：" << query.lastError().text();
  }
  return res;
}

// 删除歌曲（同时清理所有关联）
bool DbDao::deleteSong(int songId)
{
  if (songId <= 0)
  {
    return false;
  }

  QSqlQuery query(m_db);
  m_db.transaction();
  try
  {
    query.prepare(QStringLiteral("DELETE FROM playlist_song WHERE song_id = ?"));
    query.addBindValue(songId);
    if (!query.exec())
      throw query.lastError();

    query.prepare(QStringLiteral("DELETE FROM play_history WHERE song_id = ?"));
    query.addBindValue(songId);
    if (!query.exec())
      throw query.lastError();

    query.prepare(QStringLiteral("DELETE FROM song WHERE song_id = ?"));
    query.addBindValue(songId);
    if (!query.exec())
      throw query.lastError();

    m_db.commit();
    return true;
  }
  catch (const QSqlError &e)
  {
    m_db.rollback();
    qWarning() << "[DB Error] 删除歌曲失败：" << e.text();
    return false;
  }
}

// 查询所有本地歌曲
QList<Song> DbDao::queryAllSongs()
{
  QList<Song> res;
  QSqlQuery query(m_db);
  query.prepare(QStringLiteral(R"(
        SELECT song_id, file_path, title, artist, album, duration, file_size, bit_rate, add_time, is_favorite, lrc_path
        FROM song
        ORDER BY add_time DESC, song_id DESC
    )"));

  if (!query.exec())
  {
    qWarning() << "[DB Error] 查询所有歌曲失败：" << query.lastError().text();
    return res;
  }

  while (query.next())
  {
    res.append(songFromRecord(query.record()));
  }
  return res;
}

// 查询收藏的歌曲
QList<Song> DbDao::queryFavoriteSongs()
{
  QList<Song> res;
  QSqlQuery query(m_db);
  query.prepare(QStringLiteral(R"(
        SELECT song_id, file_path, title, artist, album, duration, file_size, bit_rate, add_time, is_favorite, lrc_path
        FROM song
        WHERE is_favorite = 1
        ORDER BY add_time DESC, song_id DESC
    )"));

  if (!query.exec())
  {
    qWarning() << "[DB Error] 查询收藏歌曲失败：" << query.lastError().text();
    return res;
  }

  while (query.next())
  {
    res.append(songFromRecord(query.record()));
  }
  return res;
}

// 更新歌曲收藏状态，同步更新我喜欢的歌单
bool DbDao::updateFavorite(int songId, bool isFavorite)
{
  if (songId <= 0)
  {
    return false;
  }

  QSqlQuery query(m_db);
  query.prepare(QStringLiteral("UPDATE song SET is_favorite = ? WHERE song_id = ?"));
  query.addBindValue(isFavorite ? 1 : 0);
  query.addBindValue(songId);
  if (!query.exec())
  {
    qWarning() << "[DB Error] 更新收藏状态失败：" << query.lastError().text();
    return false;
  }

  if (isFavorite)
  {
    return addSongToPlaylist(Playlist::PL_FAVORITE, songId);
  }
  return removeSongFromPlaylist(Playlist::PL_FAVORITE, songId);
}

// ========================== 歌单相关操作 ==========================
// 插入自定义歌单，返回ID，失败返回-1
int DbDao::insertPlaylist(const Playlist &playlist)
{
  if (playlist.isSystem())
  {
    return -1;
  }

  const int playlistId = nextCustomPlaylistId();
  QSqlQuery query(m_db);
  query.prepare(QStringLiteral(R"(
        INSERT INTO playlist(playlist_id, name, is_system, description, create_time)
        VALUES(?, ?, ?, ?, ?)
    )"));
  query.addBindValue(playlistId);
  query.addBindValue(playlist.name());
  query.addBindValue(0);
  query.addBindValue(playlist.description());
  query.addBindValue(playlist.createTime().isValid() ? playlist.createTime() : QDateTime::currentDateTime());

  if (query.exec())
  {
    return playlistId;
  }

  qWarning() << "[DB Error] 插入歌单失败：" << query.lastError().text()
             << "SQL:" << query.lastQuery()
             << "playlist_id:" << playlistId
             << "name:" << playlist.name();
  return -1;
}

// 删除歌单（系统歌单不可删）
bool DbDao::deletePlaylist(int playlistId)
{
  if (playlistId < 0)
  {
    return false;
  }

  QSqlQuery checkQuery(m_db);
  checkQuery.prepare(QStringLiteral("SELECT is_system FROM playlist WHERE playlist_id = ?"));
  checkQuery.addBindValue(playlistId);
  if (!checkQuery.exec() || !checkQuery.next())
  {
    qWarning() << "[DB Error] 删除歌单前校验失败：" << checkQuery.lastError().text();
    return false;
  }

  if (checkQuery.value(0).toInt() == 1)
  {
    return false;
  }

  QSqlQuery query(m_db);
  m_db.transaction();
  try
  {
    query.prepare(QStringLiteral("DELETE FROM playlist_song WHERE playlist_id = ?"));
    query.addBindValue(playlistId);
    if (!query.exec())
      throw query.lastError();

    query.prepare(QStringLiteral("DELETE FROM playlist WHERE playlist_id = ?"));
    query.addBindValue(playlistId);
    if (!query.exec())
      throw query.lastError();

    m_db.commit();
    return true;
  }
  catch (const QSqlError &e)
  {
    m_db.rollback();
    qWarning() << "[DB Error] 删除歌单失败：" << e.text();
    return false;
  }
}

// 歌曲加入歌单
bool DbDao::addSongToPlaylist(int playlistId, int songId)
{
  if (playlistId < 0 || songId <= 0)
  {
    return false;
  }

  QSqlQuery query(m_db);
  query.prepare(QStringLiteral("INSERT OR IGNORE INTO playlist_song(playlist_id, song_id, add_time) VALUES(?, ?, ?)"));
  query.addBindValue(playlistId);
  query.addBindValue(songId);
  query.addBindValue(QDateTime::currentDateTime());

  const bool res = query.exec();
  if (!res)
  {
    qWarning() << "[DB Error] 歌曲加入歌单失败：" << query.lastError().text();
  }
  return res;
}

// 歌曲从歌单移除
bool DbDao::removeSongFromPlaylist(int playlistId, int songId)
{
  if (playlistId < 0 || songId <= 0)
  {
    return false;
  }

  QSqlQuery query(m_db);
  query.prepare(QStringLiteral("DELETE FROM playlist_song WHERE playlist_id = ? AND song_id = ?"));
  query.addBindValue(playlistId);
  query.addBindValue(songId);
  const bool res = query.exec();
  if (!res)
  {
    qWarning() << "[DB Error] 歌曲从歌单移除失败：" << query.lastError().text();
  }
  return res;
}

// 查询所有歌单（包含系统歌单）
QList<Playlist> DbDao::queryAllPlaylists()
{
  QList<Playlist> res;
  QSqlQuery query(m_db);
  query.prepare(QStringLiteral(R"(
        SELECT playlist_id, name, is_system, description, create_time
        FROM playlist
        ORDER BY is_system DESC, playlist_id ASC
    )"));

  if (!query.exec())
  {
    qWarning() << "[DB Error] 查询歌单失败：" << query.lastError().text();
    return res;
  }

  while (query.next())
  {
    res.append(playlistFromRecord(query.record()));
  }
  return res;
}

// 查询指定歌单下的所有歌曲
QList<Song> DbDao::queryPlaylistSongs(int playlistId)
{
  QList<Song> res;
  if (playlistId < 0)
  {
    return res;
  }

  QSqlQuery query(m_db);
  query.prepare(QStringLiteral(R"(
        SELECT s.song_id, s.file_path, s.title, s.artist, s.album, s.duration, s.file_size, s.bit_rate, s.add_time, s.is_favorite, s.lrc_path
        FROM song s
        INNER JOIN playlist_song ps ON s.song_id = ps.song_id
        WHERE ps.playlist_id = ?
        ORDER BY ps.add_time DESC, ps.id DESC
    )"));
  query.addBindValue(playlistId);

  if (!query.exec())
  {
    qWarning() << "[DB Error] 查询歌单歌曲失败：" << query.lastError().text();
    return res;
  }

  while (query.next())
  {
    res.append(songFromRecord(query.record()));
  }
  return res;
}

// 更新歌单名称和简介（系统歌单不可更新）
bool DbDao::updatePlaylistInfo(int playlistId, const QString &name, const QString &description)
{
  if (playlistId < 0)
  {
    return false;
  }

  QSqlQuery checkQuery(m_db);
  checkQuery.prepare(QStringLiteral("SELECT is_system FROM playlist WHERE playlist_id = ?"));
  checkQuery.addBindValue(playlistId);
  if (!checkQuery.exec() || !checkQuery.next())
  {
    qWarning() << "[DB Error] 更新歌单前校验失败：" << checkQuery.lastError().text();
    return false;
  }

  if (checkQuery.value(0).toInt() == 1)
  {
    return false;
  }

  QSqlQuery query(m_db);
  query.prepare(QStringLiteral(R"(
        UPDATE playlist SET name = ?, description = ? WHERE playlist_id = ?
    )"));
  query.addBindValue(name);
  query.addBindValue(description);
  query.addBindValue(playlistId);

  const bool res = query.exec();
  if (!res)
  {
    qWarning() << "[DB Error] 更新歌单信息失败：" << query.lastError().text();
  }
  return res;
}

// ========================== 播放历史相关操作 ==========================
// 添加播放历史
bool DbDao::addPlayHistory(int songId, int playedDurationMs)
{
  if (songId <= 0)
  {
    return false;
  }

  QSqlQuery query(m_db);
  query.prepare(QStringLiteral("INSERT INTO play_history(song_id, play_time, played_duration_ms) VALUES(?, ?, ?)"));
  query.addBindValue(songId);
  query.addBindValue(QDateTime::currentDateTime());
  query.addBindValue(playedDurationMs);
  const bool res = query.exec();
  if (!res)
  {
    qWarning() << "[DB Error] 添加播放历史失败：" << query.lastError().text();
    return false;
  }

  QSqlQuery cleanupQuery(m_db);
  cleanupQuery.exec(QStringLiteral(R"(
        DELETE FROM play_history
        WHERE id NOT IN (
            SELECT id FROM (
                SELECT id FROM play_history ORDER BY play_time DESC LIMIT 1000
            ) AS latest_history
        )
    )"));

  return true;
}

// 查询最近播放历史，默认取最近100条
QList<Song> DbDao::queryPlayHistory(int limit)
{
  QList<Song> res;
  if (limit <= 0)
  {
    return res;
  }

  QSqlQuery query(m_db);
  query.prepare(QStringLiteral(R"(
        SELECT s.song_id, s.file_path, s.title, s.artist, s.album, s.duration, s.file_size, s.bit_rate, s.add_time, s.is_favorite, s.lrc_path
        FROM song s
        INNER JOIN (
            SELECT song_id, MAX(play_time) AS last_play_time
            FROM play_history
            GROUP BY song_id
        ) ph ON s.song_id = ph.song_id
        ORDER BY ph.last_play_time DESC
        LIMIT ?
    )"));
  query.addBindValue(limit);

  if (!query.exec())
  {
    qWarning() << "[DB Error] 查询播放历史失败：" << query.lastError().text();
    return res;
  }

  while (query.next())
  {
    res.append(songFromRecord(query.record()));
  }
  return res;
}

// ========================== 歌曲播放次数 ==========================
bool DbDao::incrementPlayCount(int songId)
{
  if (songId <= 0)
  {
    return false;
  }

  QSqlQuery query(m_db);
  query.prepare(QStringLiteral("UPDATE song SET play_count = play_count + 1 WHERE song_id = ?"));
  query.addBindValue(songId);
  const bool res = query.exec();
  if (!res)
  {
    qWarning() << "[DB Error] 增加播放次数失败：" << query.lastError().text();
  }
  return res;
}

// ========================== 应用配置 ==========================
bool DbDao::saveSetting(const QString &key, const QString &value)
{
  if (key.isEmpty())
  {
    return false;
  }

  QSqlQuery query(m_db);
  query.prepare(QStringLiteral(R"(
        INSERT INTO app_setting(setting_key, setting_value, update_time)
        VALUES(?, ?, ?)
        ON CONFLICT(setting_key) DO UPDATE SET
            setting_value = excluded.setting_value,
            update_time = excluded.update_time
    )"));
  query.addBindValue(key);
  query.addBindValue(value);
  query.addBindValue(QDateTime::currentDateTime());

  const bool res = query.exec();
  if (!res)
  {
    qWarning() << "[DB Error] 保存配置失败：" << query.lastError().text();
  }
  return res;
}

QString DbDao::querySetting(const QString &key, const QString &defaultValue)
{
  if (key.isEmpty())
  {
    return defaultValue;
  }

  QSqlQuery query(m_db);
  query.prepare(QStringLiteral("SELECT setting_value FROM app_setting WHERE setting_key = ?"));
  query.addBindValue(key);
  if (query.exec() && query.next())
  {
    return query.value(0).toString();
  }
  return defaultValue;
}

// ========================== 播放队列快照 ==========================
bool DbDao::savePlayQueueSnapshot(const QList<int> &songIds, int currentIndex)
{
  if (songIds.isEmpty())
  {
    return false;
  }

  m_db.transaction();
  try
  {
    QSqlQuery delQuery(m_db);
    if (!delQuery.exec(QStringLiteral("DELETE FROM play_queue_snapshot")))
      throw delQuery.lastError();

    QSqlQuery insertQuery(m_db);
    insertQuery.prepare(QStringLiteral("INSERT INTO play_queue_snapshot(song_id, order_index, is_current, create_time) VALUES(?, ?, ?, ?)"));
    for (int i = 0; i < songIds.size(); ++i)
    {
      insertQuery.addBindValue(songIds.at(i));
      insertQuery.addBindValue(i);
      insertQuery.addBindValue((i == currentIndex) ? 1 : 0);
      insertQuery.addBindValue(QDateTime::currentDateTime());
      if (!insertQuery.exec())
        throw insertQuery.lastError();
    }

    m_db.commit();
    return true;
  }
  catch (const QSqlError &e)
  {
    m_db.rollback();
    qWarning() << "[DB Error] 保存播放队列快照失败：" << e.text();
    return false;
  }
}

QList<int> DbDao::loadPlayQueueSnapshot(int &currentIndex)
{
  QList<int> res;
  currentIndex = -1;

  QSqlQuery query(m_db);
  query.prepare(QStringLiteral("SELECT song_id, is_current FROM play_queue_snapshot ORDER BY order_index ASC"));
  if (!query.exec())
  {
    qWarning() << "[DB Error] 加载播放队列快照失败：" << query.lastError().text();
    return res;
  }

  int idx = 0;
  while (query.next())
  {
    res.append(query.value(0).toInt());
    if (query.value(1).toInt() == 1)
    {
      currentIndex = idx;
    }
    ++idx;
  }
  return res;
}

bool DbDao::clearPlayQueueSnapshot()
{
  QSqlQuery query(m_db);
  const bool res = query.exec(QStringLiteral("DELETE FROM play_queue_snapshot"));
  if (!res)
  {
    qWarning() << "[DB Error] 清空播放队列快照失败：" << query.lastError().text();
  }
  return res;
}

// ========================== 导入文件夹管理 ==========================
bool DbDao::addImportFolder(const QString &folderPath, bool autoScan)
{
  if (folderPath.trimmed().isEmpty())
  {
    return false;
  }

  QSqlQuery query(m_db);
  query.prepare(QStringLiteral("INSERT OR IGNORE INTO import_folder(folder_path, auto_scan, add_time) VALUES(?, ?, ?)"));
  query.addBindValue(folderPath);
  query.addBindValue(autoScan ? 1 : 0);
  query.addBindValue(QDateTime::currentDateTime());

  const bool res = query.exec();
  if (!res)
  {
    qWarning() << "[DB Error] 添加导入文件夹失败：" << query.lastError().text();
  }
  return res;
}

bool DbDao::removeImportFolder(int folderId)
{
  if (folderId <= 0)
  {
    return false;
  }

  QSqlQuery query(m_db);
  query.prepare(QStringLiteral("DELETE FROM import_folder WHERE folder_id = ?"));
  query.addBindValue(folderId);

  const bool res = query.exec();
  if (!res)
  {
    qWarning() << "[DB Error] 移除导入文件夹失败：" << query.lastError().text();
  }
  return res;
}

bool DbDao::updateImportFolderScanTime(int folderId)
{
  if (folderId <= 0)
  {
    return false;
  }

  QSqlQuery query(m_db);
  query.prepare(QStringLiteral("UPDATE import_folder SET last_scan_time = ? WHERE folder_id = ?"));
  query.addBindValue(QDateTime::currentDateTime());
  query.addBindValue(folderId);

  const bool res = query.exec();
  if (!res)
  {
    qWarning() << "[DB Error] 更新导入文件夹扫描时间失败：" << query.lastError().text();
  }
  return res;
}

QList<DbDao::ImportFolder> DbDao::queryAllImportFolders()
{
  QList<ImportFolder> res;
  QSqlQuery query(m_db);
  query.prepare(QStringLiteral("SELECT folder_id, folder_path, auto_scan, last_scan_time, add_time FROM import_folder ORDER BY add_time DESC"));

  if (!query.exec())
  {
    qWarning() << "[DB Error] 查询导入文件夹失败：" << query.lastError().text();
    return res;
  }

  while (query.next())
  {
    ImportFolder folder;
    folder.folderId = query.value(0).toInt();
    folder.folderPath = query.value(1).toString();
    folder.autoScan = query.value(2).toInt() == 1;
    folder.lastScanTime = query.value(3).toString();
    folder.addTime = query.value(4).toString();
    res.append(folder);
  }
  return res;
}

// ========================== 歌词缓存 ==========================
bool DbDao::saveLyricCache(int songId, const QString &lrcContent, int sourceType)
{
  if (songId <= 0)
  {
    return false;
  }

  QSqlQuery query(m_db);
  query.prepare(QStringLiteral(R"(
        INSERT INTO lyric_cache(song_id, lrc_content, source_type, update_time)
        VALUES(?, ?, ?, ?)
        ON CONFLICT(song_id) DO UPDATE SET
            lrc_content = excluded.lrc_content,
            source_type = excluded.source_type,
            update_time = excluded.update_time
    )"));
  query.addBindValue(songId);
  query.addBindValue(lrcContent);
  query.addBindValue(sourceType);
  query.addBindValue(QDateTime::currentDateTime());

  const bool res = query.exec();
  if (!res)
  {
    qWarning() << "[DB Error] 保存歌词缓存失败：" << query.lastError().text();
  }
  return res;
}

QString DbDao::queryLyricCache(int songId)
{
  if (songId <= 0)
  {
    return QString();
  }

  QSqlQuery query(m_db);
  query.prepare(QStringLiteral("SELECT lrc_content FROM lyric_cache WHERE song_id = ?"));
  query.addBindValue(songId);
  if (query.exec() && query.next())
  {
    return query.value(0).toString();
  }
  return QString();
}

bool DbDao::deleteLyricCache(int songId)
{
  if (songId <= 0)
  {
    return false;
  }

  QSqlQuery query(m_db);
  query.prepare(QStringLiteral("DELETE FROM lyric_cache WHERE song_id = ?"));
  query.addBindValue(songId);

  const bool res = query.exec();
  if (!res)
  {
    qWarning() << "[DB Error] 删除歌词缓存失败：" << query.lastError().text();
  }
  return res;
}
