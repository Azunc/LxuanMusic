/***************************************************
 *  @file      playlistdetailwidget.h
 *  @brief     歌单详情页总控件：顶部信息区 + 表头 + 歌曲列表
 ****************************************************/
#ifndef PLAYLISTDETAILWIDGET_H
#define PLAYLISTDETAILWIDGET_H

#include <QWidget>
#include <QList>
#include "../entity/song.h"

class QLabel;
class QPushButton;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class PlaylistSongItem;
class Playlist;

class PlaylistDetailWidget : public QWidget
{
  Q_OBJECT
public:
  explicit PlaylistDetailWidget(QWidget *parent = nullptr);

  void setPlaylist(const Playlist &playlist);
  void setSongs(const QList<Song> &songs);
  QList<Song> songs() const;
  void setPlaylistCover(const QPixmap &cover);
  void setCreatorInfo(const QPixmap &avatar, const QString &name, const QDateTime &createTime);

  void clearSongs();
  void setCurrentPlayingSongId(int songId);
  void setPlayAllCount(int count);
  void filterSongs(const QString &keyword);

signals:
  void playAllClicked();
  void editPlaylistClicked();
  void playlistInfoEdited(int playlistId, const QString &name, const QString &description);
  void searchTextChanged(const QString &text);
  void songDoubleClicked(int songId, const QString &filePath);
  void songLoveClicked(int songId);
  void songAddToPlaylistClicked(int songId);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void setupUI();
  void setupHeader();
  void setupTableHeader();
  void setupSongList();
  void onItemClicked(QListWidgetItem *item);
  void onItemDoubleClicked(QListWidgetItem *item);
  void updatePlayingState();
  void refreshSongList();
  QPixmap roundedPixmap(const QPixmap &source, int radius, int targetSize = 0) const;
  void showEditPlaylistDialog();

  // 顶部信息区
  QLabel *m_coverLabel = nullptr;
  QLabel *m_nameLabel = nullptr;
  QLabel *m_editIcon = nullptr;
  QLabel *m_descLabel = nullptr;
  QLabel *m_creatorAvatar = nullptr;
  QLabel *m_creatorName = nullptr;
  QLabel *m_createTimeLabel = nullptr;
  QPushButton *m_playAllBtn = nullptr;
  QLineEdit *m_searchEdit = nullptr;

  // 歌曲列表
  QListWidget *m_listWidget = nullptr;
  QWidget *m_tableHeader = nullptr;

  QWidget *m_headerWidget = nullptr;

  int m_currentPlaylistId = -1;
  int m_currentPlayingSongId = -1;
  bool m_isSystem = false;
  QList<Song> m_allSongs;
};

#endif // PLAYLISTDETAILWIDGET_H
