/***************************************************
 *  @file      playlist.h
 *  @brief     XXXX Function
 *
 *  @author    un
 *  @date      2026/04/13
 *  @history
 ****************************************************/
#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <QString>
#include <QDateTime>
#include <QList>
#include <QMetaType>
#include "song.h"

class Playlist
{
public:
  // 系统内置歌单固定ID，全局约定不可修改
  enum SystemPlaylistId
  {
    PL_LOCAL = 0,         // 本地音乐
    PL_FAVORITE = 1,      // 我喜欢的音乐
    PL_HISTORY = 2,       // 播放历史
    PL_CUSTOM_START = 100 // 自定义歌单ID从100开始自增
  };

  // 构造函数
  Playlist() = default;
  // 自定义歌单快捷构造
  Playlist(const QString &name, bool isSystem = false);
  // 全参数构造（数据库读取用）
  Playlist(int playlistId, const QString &name, bool isSystem, const QDateTime &createTime, const QList<Song> &songs = {});
  // 全参数构造（含 description，数据库读取用）
  Playlist(int playlistId, const QString &name, bool isSystem, const QDateTime &createTime, const QString &description, const QList<Song> &songs = {});

  // ========================== Getter 接口 ==========================
  int playlistId() const;
  QString name() const;
  bool isSystem() const; // 是否是系统内置歌单（不可删除/改名）
  QDateTime createTime() const;
  QString description() const; // 歌单简介
  QList<Song> songs() const;   // 歌单包含的歌曲列表（可按需缓存）
  int songCount() const;       // 歌单歌曲数量

  // ========================== Setter 接口 ==========================
  void setPlaylistId(int id);
  void setName(const QString &name);
  void setIsSystem(bool isSystem);
  void setCreateTime(const QDateTime &time);
  void setDescription(const QString &description);
  void setSongs(const QList<Song> &songs);
  void addSong(const Song &song); // 追加歌曲到歌单
  void removeSong(int songId);    // 按歌曲ID移除歌曲

  // ========================== 工具方法 ==========================
  bool isValid() const;                         // 合法歌单判断：ID>=0 + 名称不为空
  bool containsSong(int songId) const;          // 判断歌单是否包含指定歌曲
  QString formattedCreateTime() const;          // 格式化创建时间为UI显示格式
  bool operator==(const Playlist &other) const; // 重载==：相同ID视为同一歌单
  bool operator!=(const Playlist &other) const;

private:
  // 成员变量和数据库playlist表字段一一对应
  int m_playlistId = -1;   // 数据库自增主键，默认-1表示未入库
  QString m_name;          // 歌单名称
  bool m_isSystem = false; // 是否系统内置，默认false是用户自定义
  QDateTime m_createTime;  // 歌单创建时间
  QString m_description;   // 歌单简介
  QList<Song> m_songs;     // 歌单包含的歌曲列表（可选缓存，无需则可以去掉）
};

// 注册到Qt元对象系统，支持信号槽、QVariant传递
Q_DECLARE_METATYPE(Playlist)

#endif // PLAYLIST_H
