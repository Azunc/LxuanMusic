/***************************************************
 *  @file      addtoplaylistpopup.h
 *  @brief     收藏到歌单弹窗：点击 btnFavorite 弹出，选择目标歌单收藏当前歌曲
 ****************************************************/
#ifndef ADDTOPLAYLISTPOPUP_H
#define ADDTOPLAYLISTPOPUP_H

#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QPixmap>

#include "../entity/playlist.h"

class LibraryController; // 前向声明

struct PlaylistInfo
{
  int playlistId = -1;
  QString name;
  int songCount = 0;
  QPixmap cover;
};

class AddToPlaylistPopup : public QWidget
{
  Q_OBJECT
public:
  explicit AddToPlaylistPopup(QWidget *parent = nullptr);

  void setLibraryController(LibraryController *ctrl);
  void refreshData(); // 从LibraryController刷新歌单数据
  void setCurrentSongId(int songId);
  int currentSongId() const;

signals:
  void createNewPlaylistRequested();                                        // 点击“创建新歌单”
  void addToPlaylistRequested(int playlistId, const QString &playlistName); // 选择某个歌单
  void popupClosed();

protected:
  void paintEvent(QPaintEvent *event) override;
  void showEvent(QShowEvent *event) override;

private:
  void setupUI();
  void refreshList();

  LibraryController *m_libraryController = nullptr;
  int m_currentSongId = -1;
  QList<PlaylistInfo> m_playlistInfos;

  QLabel *m_titleLabel = nullptr;
  QPushButton *m_closeBtn = nullptr;
  QListWidget *m_listWidget = nullptr;
};

#endif // ADDTOPLAYLISTPOPUP_H
