#include "cachedao.h"

#include <QFileInfo>

CacheDao::CacheDao(QObject *parent)
    : QObject(parent)
{
}

CacheDao *CacheDao::instance()
{
  static CacheDao inst;
  return &inst;
}

// ================== 歌曲缓存 ==================
void CacheDao::cacheAllSongs(const QList<Song> &songs)
{
  m_allSongs = songs;
}

QList<Song> CacheDao::allSongs() const
{
  return m_allSongs;
}

void CacheDao::clearAllSongs()
{
  m_allSongs.clear();
}

Song CacheDao::songById(int songId) const
{
  for (const Song &s : m_allSongs)
  {
    if (s.songId() == songId)
      return s;
  }
  return Song();
}

int CacheDao::songIdByFilePath(const QString &filePath) const
{
  const QString normalizedPath = QFileInfo(filePath).absoluteFilePath();
  for (const Song &s : m_allSongs)
  {
    if (QFileInfo(s.filePath()).absoluteFilePath() == normalizedPath)
      return s.songId();
  }
  return -1;
}

// ================== 收藏缓存 ==================
void CacheDao::cacheFavoriteSongs(const QList<Song> &songs)
{
  m_favoriteSongs = songs;
}

QList<Song> CacheDao::favoriteSongs() const
{
  return m_favoriteSongs;
}

bool CacheDao::isSongFavorited(int songId) const
{
  for (const Song &s : m_favoriteSongs)
  {
    if (s.songId() == songId)
      return true;
  }
  return false;
}

void CacheDao::clearFavoriteSongs()
{
  m_favoriteSongs.clear();
}

// ================== 播放历史缓存 ==================
void CacheDao::cacheHistorySongs(const QList<Song> &songs)
{
  m_historySongs = songs;
}

QList<Song> CacheDao::historySongs() const
{
  return m_historySongs;
}

void CacheDao::clearHistorySongs()
{
  m_historySongs.clear();
}

// ================== 歌单列表缓存 ==================
void CacheDao::cachePlaylists(const QList<Playlist> &playlists)
{
  m_playlists = playlists;
}

QList<Playlist> CacheDao::playlists() const
{
  return m_playlists;
}

Playlist CacheDao::playlistById(int playlistId) const
{
  for (const Playlist &pl : m_playlists)
  {
    if (pl.playlistId() == playlistId)
      return pl;
  }
  return Playlist();
}

void CacheDao::clearPlaylists()
{
  m_playlists.clear();
}

// ================== 歌单内歌曲缓存 ==================
void CacheDao::cachePlaylistSongs(int playlistId, const QList<Song> &songs)
{
  m_playlistSongs[playlistId] = songs;
}

QList<Song> CacheDao::playlistSongs(int playlistId) const
{
  return m_playlistSongs.value(playlistId);
}

void CacheDao::clearPlaylistSongs(int playlistId)
{
  m_playlistSongs.remove(playlistId);
}

// ================== 全局清空 ==================
void CacheDao::clearAll()
{
  m_allSongs.clear();
  m_favoriteSongs.clear();
  m_historySongs.clear();
  m_playlists.clear();
  m_playlistSongs.clear();
}
