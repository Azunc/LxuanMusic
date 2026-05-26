#include "lyricmodel.h"
#include <QRegularExpression>
#include <algorithm>

LyricModel::LyricModel(QObject *parent) : QObject(parent) {}

void LyricModel::parse(const QString &lrcContent)
{
  QList<LyricLine> lines;
  QRegularExpression regex("\\[(\\d{2}):(\\d{2})\\.(\\d{2,3})\\](.*)");
  const QStringList rawLines = lrcContent.split('\n');

  for (const QString &line : rawLines)
  {
    QRegularExpressionMatch match = regex.match(line);
    if (match.hasMatch())
    {
      int min = match.captured(1).toInt();
      int sec = match.captured(2).toInt();
      int ms = match.captured(3).toInt();
      if (match.captured(3).length() == 2)
        ms *= 10;

      qint64 timestamp = min * 60000 + sec * 1000 + ms;
      QString text = match.captured(4).trimmed();
      if (!text.isEmpty())
      {
        lines.append({timestamp, text});
      }
    }
  }

  std::sort(lines.begin(), lines.end(), [](const LyricLine &a, const LyricLine &b)
            { return a.timestamp < b.timestamp; });

  setLines(lines);
}

void LyricModel::setLines(const QList<LyricLine> &lines)
{
  m_lyrics = lines;
  emit linesChanged();
}

int LyricModel::findIndex(qint64 position) const
{
  if (m_lyrics.isEmpty())
    return -1;
  if (position < m_lyrics.first().timestamp)
    return -1;
  if (position >= m_lyrics.last().timestamp)
    return m_lyrics.size() - 1;

  int left = 0, right = static_cast<int>(m_lyrics.size()) - 1;
  while (left < right)
  {
    int mid = left + (right - left + 1) / 2;
    if (m_lyrics[mid].timestamp <= position)
    {
      left = mid;
    }
    else
    {
      right = mid - 1;
    }
  }
  return left;
}

LyricLine LyricModel::getLine(int index) const
{
  if (index >= 0 && index < m_lyrics.size())
    return m_lyrics[index];
  return {-1, ""};
}

int LyricModel::count() const { return m_lyrics.size(); }

void LyricModel::clear() { m_lyrics.clear(); }
