#ifndef SIDEBARWIDGET_H
#define SIDEBARWIDGET_H

#include <QWidget>

class QPushButton;
class CollapsibleGroup;
class LibraryController;

class SideBarWidget : public QWidget
{
  Q_OBJECT
public:
  explicit SideBarWidget(QWidget *scrollAreaWidget = nullptr, QWidget *parent = nullptr);
  void setScrollAreaWidget(QWidget *widget);

  void setLibraryController(LibraryController *ctrl);

  // 外部调用：将数据库中已插入的歌单添加到UI（由MainWindow在DB操作后调用）
  void addCustomPlaylist(const QString &name, int playlistId); // 添加到"创建的歌单"
  // 启动时从LibraryController加载已有自定义歌单
  void loadPlaylistsFromDb();
  // 更新歌单名称
  void updateCustomPlaylistName(int playlistId, const QString &name);

public slots:
  void setButtonChecked(int index); // 0=本地, 1=喜欢, 2=历史播放
private slots:
  void updateSideBarHeight();
signals:
  void pageRequested(int index);                                             // 0=本地, 1=喜欢, 2=历史播放
  void customPlaylistAdded(const QString &name, const QString &description); // 用户点击+号新建歌单
  void customPlaylistRemoved(int playlistId, const QString &name);           // 用户删除歌单
  void customPlaylistClicked(int playlistId);                                // 用户点击自定义歌单
private:
  LibraryController *m_libraryController = nullptr;

  void setupUI();
  int calculateHeight() const;
  void updateButtonStates(QPushButton *activeBtn);

  QPushButton *m_favBtn;
  QPushButton *m_localBtn;
  QPushButton *m_historyBtn;
  CollapsibleGroup *m_createList;
  QWidget *m_scrollAreaWidget;

  static constexpr int BTN_HEIGHT = 40;
};

#endif // SIDEBARWIDGET_H
