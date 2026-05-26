/***************************************************
 *  @file      playlistmodel.h
 *  @brief     歌单实体模型：存每个歌单的ID、名称、包含的歌曲ID列表、是否是收藏/系统内置歌单等，提供歌单数据组织与缓存能力
 *
 *  @author    un
 *  @date      2026/04/13
 *  @history
 ****************************************************/
#ifndef PLAYLISTMODEL_H
#define PLAYLISTMODEL_H

#include <QObject>
#include <QList>
#include <QMap>

#include "./entity/song.h"
#include "./entity/playlist.h"

class PlaylistModel : public QObject
{
  Q_OBJECT
public:
  explicit PlaylistModel(QObject *parent = nullptr);

  // 设置当前歌单及歌曲
  void setPlaylist(const Playlist &playlist);
  void setSongs(const QList<Song> &songs);

  Playlist currentPlaylist() const;
  QList<Song> currentSongs() const;

  // 查询缓存
  bool hasPlaylist(int playlistId) const;
  Playlist playlist(int playlistId) const;
  QList<Song> songs(int playlistId) const;

  // 缓存管理
  void cachePlaylist(int playlistId, const Playlist &playlist, const QList<Song> &songs);
  void clearCache(int playlistId);
  void clearAllCache();

signals:
  void playlistChanged(const Playlist &playlist);
  void songsChanged(const QList<Song> &songs);

private:
  Playlist m_currentPlaylist;
  QList<Song> m_currentSongs;

  QMap<int, Playlist> m_playlistCache;
  QMap<int, QList<Song>> m_songCache;
};

#endif // PLAYLISTMODEL_H
