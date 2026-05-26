/***************************************************
 *  @file      lyricmodel.h
 *  @brief     歌词实体模型：存解析后的每一行lrc的时间戳、歌词内容，提供二分查找定位
 *
 *  @author    un
 *  @date      2026/04/13
 *  @history
 ****************************************************/
#ifndef LYRICMODEL_H
#define LYRICMODEL_H

#include <QObject>
#include <QList>

struct LyricLine
{
  qint64 timestamp; // 毫秒
  QString text;
};

class LyricModel : public QObject
{
  Q_OBJECT
public:
  explicit LyricModel(QObject *parent = nullptr);

  void parse(const QString &lrcContent);
  void setLines(const QList<LyricLine> &lines);
  int findIndex(qint64 position) const;
  LyricLine getLine(int index) const;
  int count() const;
  void clear();
  const QList<LyricLine> &lines() const { return m_lyrics; }

signals:
  void linesChanged();

private:
  QList<LyricLine> m_lyrics;
};

#endif // LYRICMODEL_H
