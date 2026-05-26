/***************************************************
 *  @file      cachedao.h
 *  @brief     内存缓存：把常用的歌曲列表、歌单列表存在内存里，减少重复数据库查询，提升性能
 *
 *  @author    un
 *  @date      2026/04/13
 *  @history
 ****************************************************/
#ifndef CACHEDAO_H
#define CACHEDAO_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QHash>

#include "./entity/song.h"
#include "./entity/playlist.h"

class CacheDao : public QObject
{
  Q_OBJECT
public:
  static CacheDao *instance();

  // 歌曲缓存
  void cacheAllSongs(const QList<Song> &songs);
  QList<Song> allSongs() const;
  void clearAllSongs();

  // 按ID查找歌曲（基于allSongs缓存）
  Song songById(int songId) const;
  int songIdByFilePath(const QString &filePath) const;

  // 收藏缓存
  void cacheFavoriteSongs(const QList<Song> &songs);
  QList<Song> favoriteSongs() const;
  bool isSongFavorited(int songId) const;
  void clearFavoriteSongs();

  // 播放历史缓存
  void cacheHistorySongs(const QList<Song> &songs);
  QList<Song> historySongs() const;
  void clearHistorySongs();

  // 歌单列表缓存
  void cachePlaylists(const QList<Playlist> &playlists);
  QList<Playlist> playlists() const;
  Playlist playlistById(int playlistId) const;
  void clearPlaylists();

  // 歌单内歌曲缓存
  void cachePlaylistSongs(int playlistId, const QList<Song> &songs);
  QList<Song> playlistSongs(int playlistId) const;
  void clearPlaylistSongs(int playlistId);

  // 全局清空
  void clearAll();

private:
  explicit CacheDao(QObject *parent = nullptr);
  Q_DISABLE_COPY(CacheDao)

  QList<Song> m_allSongs;
  QList<Song> m_favoriteSongs;
  QList<Song> m_historySongs;
  QList<Playlist> m_playlists;
  QMap<int, QList<Song>> m_playlistSongs; // playlistId -> songs
};

#endif // CACHEDAO_H
