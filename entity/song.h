/***************************************************
 *  @file      song.h
 *  @brief     歌曲实体
 *
 *  @author    un
 *  @date      2026/04/13
 *  @history
 ****************************************************/
#ifndef SONG_H
#define SONG_H

#include <QString>
#include <QDateTime>
#include <QMetaType>

class Song
{
public:
  // 默认构造
  Song() noexcept = default;
  ~Song() noexcept = default;
  // 快捷构造（常用）

  /**
   * @brief Song
   * @param filePath // 文件路径
   * @param title    //歌曲名称
   * @param artist    //作曲家
   * @param album //专辑
   * @param duration  //时长
   */
  Song(const QString &filePath, const QString &title, const QString &artist, const QString &album, qint64 duration);
  // 全参数构造（数据库读取用）
  /**
   * @brief Song
   * @param songId
   * @param filePath // 文件路径
   * @param title    //歌曲名称
   * @param artist    //作曲家
   * @param album //专辑
   * @param duration  //时长
   * @param fileSize  //文件大小
   * @param bitRate   //
   * @param addTime   //
   * @param isFavorite    //是否收藏？
   * @param lrcPath   //歌词路径
   */
  Song(int songId, const QString &filePath, const QString &title, const QString &artist, const QString &album,
       qint64 duration, qint64 fileSize, int bitRate, const QDateTime &addTime, bool isFavorite, const QString &lrcPath);

  // ========================== Getter 接口 ==========================
  int songId() const;
  QString filePath() const;
  QString title() const;
  QString artist() const;
  QString album() const;
  qint64 duration() const; // 单位：毫秒
  qint64 fileSize() const; // 单位：字节
  int bitRate() const;     // 单位：kbps
  QDateTime addTime() const;
  bool isFavorite() const;
  QString lrcPath() const;

  // ========================== Setter 接口 ==========================
  void setSongId(int id);
  void setFilePath(const QString &path);
  void setTitle(const QString &title);
  void setArtist(const QString &artist);
  void setAlbum(const QString &album);
  void setDuration(qint64 duration);
  void setFileSize(qint64 size);
  void setBitRate(int rate);
  void setAddTime(const QDateTime &time);
  void setFavorite(bool favorite);
  void setLrcPath(const QString &path);

  // ========================== 工具方法 ==========================
  QString formattedDuration() const;        // 格式化时长为UI显示的 mm:ss 格式，比如 03:45
  QString formattedFileSize() const;        // 格式化文件大小为UI显示的 MB/KB 格式
  bool isValid() const;                     // 判断是否是有效音频文件（路径不为空+时长大于0）
  bool operator==(const Song &other) const; // 重载==，用于去重：相同ID/相同文件路径视为同一首歌
  bool operator!=(const Song &other) const;

private:
  // 成员变量和数据库song表字段一一对应，前缀统一为m_
  int m_songId = -1;         // 数据库自增主键，全局唯一，默认-1表示未入库
  QString m_filePath = "";   // 本地音频文件绝对路径，去重依据
  QString m_title = "";      // 歌曲名，优先读元数据，读不到则取文件名
  QString m_artist = "";     // 歌手，读不到显示「未知歌手」
  QString m_album = "";      // 专辑，读不到显示「未知专辑」
  qint64 m_duration = 0;     // 音频时长，单位毫秒
  qint64 m_fileSize = 0;     // 文件大小，单位字节
  int m_bitRate = 0;         // 音频比特率，单位kbps
  QDateTime m_addTime;       // 歌曲添加到本地库的时间
  bool m_isFavorite = false; // 是否被收藏
  QString m_lrcPath = "";    // 对应lrc歌词文件的路径，为空表示无歌词；默认空字符串而非null，避免SQL NOT NULL约束插入失败
};

// 注册Song类到Qt元对象系统，支持在信号槽、QVariant中传递
Q_DECLARE_METATYPE(Song)

#endif // SONG_H
