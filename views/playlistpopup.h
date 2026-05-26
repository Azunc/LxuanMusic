/***************************************************
 *  @file      playlistpopup.h
 *  @brief     播放列表弹窗：点击 btnList 弹出，展示当前播放队列
 ****************************************************/
#ifndef PLAYLISTPOPUP_H
#define PLAYLISTPOPUP_H

#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QMenu>

#include "../entity/song.h"

// ==================== 播放列表歌曲项 ====================
class PlaylistPopupItem : public QWidget
{
  Q_OBJECT
public:
  explicit PlaylistPopupItem(QWidget *parent = nullptr);
  void setSong(const Song &song);
  void setSelected(bool selected);
  void setPlaying(bool playing);
  int songId() const;
  QString filePath() const;

signals:
  void clicked(int songId);
  void doubleClicked(int songId);
  void loveClicked(int songId);
  void addToPlaylistClicked(int songId);
  void removeClicked(int songId);
  void openFileDirRequested(const QString &filePath);
  void playPauseRequested(int songId);

protected:
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

private:
  void setupUI();
  void updateStyle();
  void showMoreMenu();

  int m_songId = -1;
  QString m_filePath;
  bool m_selected = false;
  bool m_hovered = false;
  bool m_playing = false;

  QLabel *m_coverLabel = nullptr;
  QLabel *m_titleLabel = nullptr;
  QLabel *m_artistLabel = nullptr;
  QLabel *m_durationLabel = nullptr;

  QWidget *m_hoverActions = nullptr;
  QPushButton *m_loveBtn = nullptr;
  QPushButton *m_addBtn = nullptr;
  QPushButton *m_moreBtn = nullptr;
};

// ==================== 播放列表弹窗 ====================
class PlaylistPopup : public QWidget
{
  Q_OBJECT
public:
  explicit PlaylistPopup(QWidget *parent = nullptr);

  void setSongs(const QList<Song> &songs);
  QList<Song> songs() const;
  void setCurrentPlayingId(int songId);
  void setSelectedIndex(int index);

signals:
  void songClicked(int index, int songId);
  void songDoubleClicked(int index, int songId);
  void loveClicked(int index, int songId);
  void addToPlaylistClicked(int index, int songId);
  void removeClicked(int index, int songId);
  void collectAllClicked();
  void clearClicked();
  void openFileDirClicked(const QString &filePath);
  void playPauseClicked(int index, int songId);

protected:
  void paintEvent(QPaintEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void hideEvent(QHideEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void setupUI();
  void refreshList();

  QList<Song> m_songs;
  int m_currentPlayingId = -1;
  int m_selectedIndex = -1;
  bool m_filterInstalled = false;

  QLabel *m_titleLabel = nullptr;
  QLabel *m_countLabel = nullptr;
  QPushButton *m_collectAllBtn = nullptr;
  QPushButton *m_clearBtn = nullptr;
  QListWidget *m_listWidget = nullptr;
};

#endif // PLAYLISTPOPUP_H
