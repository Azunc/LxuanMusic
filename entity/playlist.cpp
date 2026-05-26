#include "playlist.h"
#include <algorithm>

Playlist::Playlist(const QString &name, bool isSystem)
    : m_name(name), m_isSystem(isSystem)
{
  m_createTime = QDateTime::currentDateTime();
}

Playlist::Playlist(int playlistId, const QString &name, bool isSystem, const QDateTime &createTime, const QList<Song> &songs)
    : m_playlistId(playlistId), m_name(name), m_isSystem(isSystem), m_createTime(createTime), m_songs(songs)
{
}

Playlist::Playlist(int playlistId, const QString &name, bool isSystem, const QDateTime &createTime, const QString &description, const QList<Song> &songs)
    : m_playlistId(playlistId), m_name(name), m_isSystem(isSystem), m_createTime(createTime), m_description(description), m_songs(songs)
{
}

// ------------------------------ Getter ------------------------------
int Playlist::playlistId() const { return m_playlistId; }
QString Playlist::name() const { return m_name; }
bool Playlist::isSystem() const { return m_isSystem; }
QDateTime Playlist::createTime() const { return m_createTime; }
QString Playlist::description() const { return m_description; }
QList<Song> Playlist::songs() const { return m_songs; }
int Playlist::songCount() const { return m_songs.size(); }

// ------------------------------ Setter ------------------------------
void Playlist::setPlaylistId(int id) { m_playlistId = id; }
void Playlist::setName(const QString &name) { m_name = name; }
void Playlist::setIsSystem(bool isSystem) { m_isSystem = isSystem; }
void Playlist::setCreateTime(const QDateTime &time) { m_createTime = time; }
void Playlist::setDescription(const QString &description) { m_description = description; }
void Playlist::setSongs(const QList<Song> &songs) { m_songs = songs; }

void Playlist::addSong(const Song &song)
{
  if (!containsSong(song.songId()))
  {
    m_songs.append(song);
  }
}

void Playlist::removeSong(int songId)
{
  auto it = std::remove_if(m_songs.begin(), m_songs.end(), [songId](const Song &s)
                           { return s.songId() == songId; });
  if (it != m_songs.end())
  {
    m_songs.erase(it);
  }
}

// ------------------------------ 工具方法 ------------------------------
bool Playlist::isValid() const
{
  return m_playlistId >= 0 && !m_name.isEmpty();
}

bool Playlist::containsSong(int songId) const
{
  return std::any_of(m_songs.begin(), m_songs.end(), [songId](const Song &s)
                     { return s.songId() == songId; });
}

QString Playlist::formattedCreateTime() const
{
  return m_createTime.toString("yyyy-MM-dd HH:mm");
}

bool Playlist::operator==(const Playlist &other) const
{
  if (m_playlistId != -1 && other.m_playlistId != -1)
  {
    return m_playlistId == other.m_playlistId;
  }
  return m_name == other.m_name && m_isSystem == other.m_isSystem;
}

bool Playlist::operator!=(const Playlist &other) const
{
  return !(*this == other);
}
