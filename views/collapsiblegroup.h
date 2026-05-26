#ifndef COLLAPSIBLEGROUP_H
#define COLLAPSIBLEGROUP_H

#include <QWidget>

class QPushButton;
class QVBoxLayout;
class PlaylistItem;

class CollapsibleGroup : public QWidget
{
  Q_OBJECT
public:
  explicit CollapsibleGroup(const QString &title, bool itemsDeletable = false, QWidget *parent = nullptr);

  void addPlaylist(const QString &name, int playlistId = -1, const QPixmap &cover = QPixmap());
  void removePlaylist(PlaylistItem *item);
  void updatePlaylistName(int playlistId, const QString &name);
  bool isExpanded() const;

  int contentHeight() const; // 当前展开的内容高度（0 若折叠）
  int headerHeight() const;  // 标题栏固定高度
  int totalHeight() const;   // 标题 + 内容

signals:
  void sizeChanged();                                                  // 高度发生变化时通知外部重新计算
  void playlistAdded(const QString &name, const QString &description); // 用户点击+号新建歌单时发出
  void playlistRemoved(int playlistId, const QString &name);           // 用户删除歌单时发出，由外部负责从数据库删除
  void playlistClicked(int playlistId);                                // 用户点击歌单项时发出

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
  void toggle();
  void onAddClicked();

private:
  void updateHeight();

  QWidget *m_header;
  QWidget *m_content;
  QVBoxLayout *m_contentLayout;
  QPushButton *m_toggleBtn;
  QPushButton *m_addBtn;
  bool m_expanded;
  bool m_itemsDeletable;
  QList<PlaylistItem *> m_items;

  static constexpr int HEADER_HEIGHT = 40;
  static constexpr int ITEM_HEIGHT = 40;
};

#endif // COLLAPSIBLEGROUP_H
