/***************************************************
 *  @file      playlistsongitem.h
 *  @brief     歌单详情页的单曲行控件（未选中/选中两种状态）
 ****************************************************/
#ifndef PLAYLISTSONGITEM_H
#define PLAYLISTSONGITEM_H

#include <QWidget>

class QLabel;
class QPushButton;
class Song;

class PlaylistSongItem : public QWidget
{
  Q_OBJECT
public:
  explicit PlaylistSongItem(QWidget *parent = nullptr);

  void setSongIndex(int index);
  void setSong(const Song &song);
  void setSelected(bool selected);

  int songId() const;
  QString filePath() const;
  QString songTitle() const;
  QString songArtist() const;
  QString songAlbum() const;

signals:
  void clicked(int songId);
  void doubleClicked(int songId);
  void playClicked(int songId);
  void loveClicked(int songId);
  void addToPlaylistClicked(int songId);
  void moreClicked(int songId);

public slots:
  void setCoverVisible(bool visible);

protected:
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
  void setupUI();
  void updateStyle();

  int m_index = 0;
  int m_songId = -1;
  QString m_filePath;
  bool m_selected = false;
  bool m_hovered = false;

  QLabel *m_numberLabel = nullptr;   // 序号或播放图标
  QLabel *m_coverLabel = nullptr;    // 歌曲封面
  QLabel *m_titleLabel = nullptr;    // 歌曲名称
  QLabel *m_artistLabel = nullptr;   // 歌手
  QLabel *m_albumLabel = nullptr;    // 专辑
  QLabel *m_durationLabel = nullptr; // 时长

  // 选中时显示的操作按钮
  QPushButton *m_playBtn = nullptr;
  QPushButton *m_loveBtn = nullptr;
  QPushButton *m_addBtn = nullptr;
  QPushButton *m_moreBtn = nullptr;

  QWidget *m_hoverActions = nullptr; // 悬浮操作按钮容器
};

#endif // PLAYLISTSONGITEM_H
