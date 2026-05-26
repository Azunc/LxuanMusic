/***************************************************
 *  @file      localmusicwidget.h
 *  @brief     本地音乐页Widget：嵌入pageLocal，支持默认/歌手/专辑/文件夹分类
 ****************************************************/
#ifndef LOCALMUSICWIDGET_H
#define LOCALMUSICWIDGET_H

#include <QWidget>
#include <QList>
#include <QMap>
#include <QHash>
#include <vector>
#include "../entity/song.h"

class QLabel;
class QPushButton;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;
class PlaylistSongItem;
class LibraryController;

class LocalMusicWidget : public QWidget
{
  Q_OBJECT
public:
  enum SortMode
  {
    DefaultSort, // 默认排序
    ArtistSort,  // 按歌手
    AlbumSort,   // 按专辑
    FolderSort   // 按文件夹
  };
  Q_ENUM(SortMode)

  explicit LocalMusicWidget(QWidget *parent = nullptr);

  void setLibraryController(LibraryController *ctrl);
  void loadSongs(const QList<Song> &songs);
  void reloadFromDb();

  // 获取当前所有可见歌曲（用于播放全部）
  std::vector<Song> currentVisibleSongs() const;

signals:
  void songDoubleClicked(int songId, const QString &filePath);
  void playAllRequested();
  void refreshRequested();
  void selectDirectoryRequested();
  void songLoveToggled(int songId);            // 收藏/取消收藏歌曲
  void songAddToPlaylistRequested(int songId); // 将歌曲添加到歌单

private slots:
  void onSortModeChanged();
  void onSongItemLoveClicked(int songId);
  void onSongItemAddToPlaylistClicked(int songId);
  void onSearchTextChanged(const QString &text);
  void onCategoryItemClicked(QListWidgetItem *item);
  void onCategoryTreeItemClicked(QTreeWidgetItem *item, int column);
  void onSongItemClicked(int songId);
  void onSongItemDoubleClicked(int songId);
  void onPlayAllClicked();
  void onRefreshClicked();
  void onMoreClicked();
  void onSelectDirectoryClicked();

private:
  LibraryController *m_libraryController = nullptr;

  void setupUI();
  void setupHeader();
  void setupDefaultView();
  void setupCategoryView(); // 歌手/专辑共用
  void setupFolderView();
  void updateHeaderCount(int count);
  void updateSortButtonStyle();

  void refreshDefaultView();
  void refreshArtistView();
  void refreshAlbumView();
  void refreshFolderView();

  void applyFilter(); // 根据搜索文本过滤当前视图

  // 从LibraryController加载数据
  std::vector<Song> fetchAllSongs() const;

  SortMode m_currentSort = DefaultSort;
  QHash<QString, QPixmap> m_coverCache;
  QString m_searchText;
  std::vector<Song> m_allSongs;      // 原始全部歌曲
  std::vector<Song> m_filteredSongs; // 搜索过滤后的歌曲（默认视图用）
  std::vector<Song> m_currentSongs;  // 当前右侧展示的歌曲（分类视图用）

  // 分类数据缓存
  QMap<QString, std::vector<Song>> m_artistMap;
  QMap<QString, std::vector<Song>> m_albumMap;
  QString m_currentArtist; // 当前选中的歌手
  QString m_currentAlbum;  // 当前选中的专辑
  QString m_currentFolder; // 当前选中的文件夹

  // 头部控件
  QLabel *m_titleLabel = nullptr;
  QLabel *m_countLabel = nullptr;
  QPushButton *m_selectDirBtn = nullptr;
  QPushButton *m_playAllBtn = nullptr;
  QPushButton *m_refreshBtn = nullptr;
  QPushButton *m_moreBtn = nullptr;
  QLineEdit *m_searchEdit = nullptr;
  QPushButton *m_defaultSortBtn = nullptr;
  QPushButton *m_artistSortBtn = nullptr;
  QPushButton *m_albumSortBtn = nullptr;
  QPushButton *m_folderSortBtn = nullptr;

  // 视图容器
  QStackedWidget *m_contentStack = nullptr;

  // 默认排序视图
  QWidget *m_defaultView = nullptr;
  QWidget *m_defaultHeader = nullptr;
  QListWidget *m_defaultList = nullptr;

  // 分类视图（歌手/专辑共用）
  QWidget *m_categoryView = nullptr;
  QListWidget *m_categoryList = nullptr;
  QWidget *m_categoryRight = nullptr;
  QWidget *m_categoryTableHeader = nullptr;
  QListWidget *m_categorySongList = nullptr;

  // 文件夹视图
  QWidget *m_folderView = nullptr;
  QTreeWidget *m_folderTree = nullptr;
  QWidget *m_folderRight = nullptr;
  QWidget *m_folderTableHeader = nullptr;
  QListWidget *m_folderSongList = nullptr;

  // 选中状态
  int m_selectedSongId = -1;
  PlaylistSongItem *m_lastSelectedItem = nullptr;
};

#endif // LOCALMUSICWIDGET_H
