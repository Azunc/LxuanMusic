#include "song.h"
#include <QFileInfo>
#include <cmath>

Song::Song(const QString& filePath, const QString& title, const QString& artist, const QString& album, qint64 duration)
    : m_filePath(filePath), m_title(title), m_artist(artist), m_album(album), m_duration(duration)
{
    QFileInfo info(filePath);
    m_fileSize = info.size();
    m_addTime = QDateTime::currentDateTime();
}

Song::Song(int songId, const QString& filePath, const QString& title, const QString& artist, const QString& album,
           qint64 duration, qint64 fileSize, int bitRate, const QDateTime& addTime, bool isFavorite, const QString& lrcPath)
    : m_songId(songId), m_filePath(filePath), m_title(title), m_artist(artist), m_album(album),
    m_duration(duration), m_fileSize(fileSize), m_bitRate(bitRate), m_addTime(addTime), m_isFavorite(isFavorite), m_lrcPath(lrcPath)
{}

// ------------------------------ Getter ------------------------------
int Song::songId() const { return m_songId; }
QString Song::filePath() const { return m_filePath; }
QString Song::title() const { return m_title; }
QString Song::artist() const { return m_artist; }
QString Song::album() const { return m_album; }
qint64 Song::duration() const { return m_duration; }
qint64 Song::fileSize() const { return m_fileSize; }
int Song::bitRate() const { return m_bitRate; }
QDateTime Song::addTime() const { return m_addTime; }
bool Song::isFavorite() const { return m_isFavorite; }
QString Song::lrcPath() const { return m_lrcPath; }

// ------------------------------ Setter ------------------------------
void Song::setSongId(int id) { m_songId = id; }
void Song::setFilePath(const QString& path) { m_filePath = path; }
void Song::setTitle(const QString& title) { m_title = title; }
void Song::setArtist(const QString& artist) { m_artist = artist; }
void Song::setAlbum(const QString& album) { m_album = album; }
void Song::setDuration(qint64 duration) { m_duration = duration; }
void Song::setFileSize(qint64 size) { m_fileSize = size; }
void Song::setBitRate(int rate) { m_bitRate = rate; }
void Song::setAddTime(const QDateTime& time) { m_addTime = time; }
void Song::setFavorite(bool favorite) { m_isFavorite = favorite; }
void Song::setLrcPath(const QString& path) { m_lrcPath = path; }

// ------------------------------ 工具方法 ------------------------------
QString Song::formattedDuration() const
{
    if (m_duration <= 0) return "00:00";
    int totalSeconds = m_duration / 1000;
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;
    return QString("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
}

QString Song::formattedFileSize() const
{
    if (m_fileSize < 1024) return QString("%1 B").arg(m_fileSize);
    else if (m_fileSize < 1024 * 1024) return QString("%1 KB").arg(round(m_fileSize / 1024.0 * 100) / 100);
    else return QString("%1 MB").arg(round(m_fileSize / (1024.0 * 1024) * 100) / 100);
}

bool Song::isValid() const
{
    return !m_filePath.isEmpty() && m_duration > 0;
}

bool Song::operator==(const Song& other) const
{
    // 优先用ID判断，没入库的用文件路径判断
    if (m_songId != -1 && other.m_songId != -1) return m_songId == other.m_songId;
    return m_filePath == other.m_filePath;
}

bool Song::operator!=(const Song& other) const
{
    return !(*this == other);
}
