#include "playlistmodel.h"

PlaylistModel::PlaylistModel(QObject *parent)
    : QObject(parent)
{
}

void PlaylistModel::setPlaylist(const Playlist &playlist)
{
  m_currentPlaylist = playlist;
  emit playlistChanged(playlist);
}

void PlaylistModel::setSongs(const QList<Song> &songs)
{
  m_currentSongs = songs;
  emit songsChanged(songs);
}

Playlist PlaylistModel::currentPlaylist() const
{
  return m_currentPlaylist;
}

QList<Song> PlaylistModel::currentSongs() const
{
  return m_currentSongs;
}

bool PlaylistModel::hasPlaylist(int playlistId) const
{
  return m_playlistCache.contains(playlistId);
}

Playlist PlaylistModel::playlist(int playlistId) const
{
  return m_playlistCache.value(playlistId);
}

QList<Song> PlaylistModel::songs(int playlistId) const
{
  return m_songCache.value(playlistId);
}

void PlaylistModel::cachePlaylist(int playlistId, const Playlist &playlist, const QList<Song> &songs)
{
  m_playlistCache[playlistId] = playlist;
  m_songCache[playlistId] = songs;
}

void PlaylistModel::clearCache(int playlistId)
{
  m_playlistCache.remove(playlistId);
  m_songCache.remove(playlistId);
}

void PlaylistModel::clearAllCache()
{
  m_playlistCache.clear();
  m_songCache.clear();
}
