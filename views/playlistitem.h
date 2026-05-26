#ifndef PLAYLISTITEM_H
#define PLAYLISTITEM_H

#include <QWidget>

class QLabel;

class PlaylistItem : public QWidget
{
  Q_OBJECT
public:
  explicit PlaylistItem(const QString &name, bool deletable = false, QWidget *parent = nullptr);

  QString playlistName() const;
  void setPlaylistName(const QString &name);
  int playlistId() const;
  void setPlaylistId(int id);
  void setCover(const QPixmap &cover);

signals:
  void deleted();
  void clicked();

protected:
  void contextMenuEvent(QContextMenuEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;

private:
  QLabel *m_coverLabel = nullptr;
  QLabel *m_label = nullptr;
  bool m_deletable;
  int m_playlistId = -1; // 对应数据库playlist_id，-1表示未入库
};

#endif // PLAYLISTITEM_H
