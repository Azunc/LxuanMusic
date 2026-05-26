/***************************************************
 *  @file      mainwindow.h
 *  @brief     主窗口容器，所有子UI的父控件，只做全局信号中转、全局UI刷新
 *
 *  @author    un
 *  @date      2026/04/13
 *  @history
 ****************************************************/
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QtMultimedia>

QT_BEGIN_NAMESPACE
namespace Ui
{
  class MainWindow;
}
QT_END_NAMESPACE

class PlayController;
class LibraryController;
class LyricController;
class VisualizerModel;
class HotKeyManager;
class SettingController;
class VolumePopup;
class SideBarWidget;
class LyricWidget;
class PlaylistDetailWidget;
class LocalMusicWidget;
class PlaylistPopup;
class AddToPlaylistPopup;
class QMediaPlayer;
class QMenu;
class QAction;

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

  bool eventFilter(QObject *watched, QEvent *event) override;

signals:
  void volumeRequested(int value);

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

private:
  Ui::MainWindow *ui;

  // Controller 层指针
  PlayController *m_playController = nullptr;
  LibraryController *m_libraryController = nullptr;
  LyricController *m_lyricController = nullptr;
  SettingController *m_settingController = nullptr;

  // Model / Utils 层指针
  VisualizerModel *m_visualizerModel = nullptr;
  HotKeyManager *m_hotKeyManager = nullptr;

  // View 层子控件
  VolumePopup *popup = nullptr;
  SideBarWidget *sideBar = nullptr;
  LyricWidget *m_lyricWidget = nullptr;
  PlaylistDetailWidget *m_playlistDetailLove = nullptr;
  PlaylistDetailWidget *m_playlistDetailHistory = nullptr;
  PlaylistDetailWidget *m_playlistDetail = nullptr;
  LocalMusicWidget *m_localMusicWidget = nullptr;
  PlaylistPopup *m_playlistPopup = nullptr;
  AddToPlaylistPopup *m_addToPlaylistPopup = nullptr;

  // 托盘
  QSystemTrayIcon *m_trayIcon = nullptr;
  QMenu *m_trayMenu = nullptr;
  QAction *m_trayActionShowMain = nullptr;
  QAction *m_trayActionToggleLyric = nullptr;
  QAction *m_trayActionToggleLock = nullptr;

  // 状态
  int m_currentSongId = -1;
  bool m_isDragging = false;
  QPoint m_dragStartPos;
  QVector<qreal> m_audioLevels;
  QList<QUrl> m_lastPopupQueue;
  static constexpr int BAR_GAP = 1;

  // 初始化
  void styleInit();
  void connectUi();
  void volumePopupInit();
  void sideBarInit();
  void init_();
  void setupPlaylistDetail();
  void setupLocalMusicWidget();
  void setupPlaylistPopup();
  void setupAddToPlaylistPopup();
  void setupTrayIcon();

  // UI 更新
  void updateVisualizationButton(const QVector<qreal> &levels);
  void updateLoveButtonState();
  void refreshLoveList();
  void refreshHistoryList();
  void refreshPlaylistPopup();
  void refreshTrayMenu();
  void showPlaylistPopup();
  void hidePlaylistPopup();
  void showAddToPlaylistPopup(int songId = -1);
  void hideAddToPlaylistPopup();
  QPixmap getMusicCoverByTaglib(const QString &filePath);

  // 槽函数
private slots:
  // PlayController 信号响应
  void do_stateChanged(QMediaPlayer::PlaybackState pb);
  void do_sourceChanged(const QUrl &media);
  void do_positionChanged(qint64 position, qint64 duration);
  void do_metaDataChanged();

  // UI 按钮槽
  void on_sliderPosition_valueChanged(int value);
  void on_btnNext_clicked();
  void on_btnPrevious_clicked();
  void on_btnPlay_clicked();
  void on_btnList_clicked();
  void on_btnAudioVisualization_clicked();
  void on_btnVolume_clicked();
  void on_btnMinimize_clicked();
  void on_btnClose_clicked();
  void on_btnLove_clicked();
  void on_btnFavorite_clicked();
  void on_btnLoop_clicked();

  // 歌单管理
  void onCustomPlaylistAdded(const QString &name, const QString &description);
  void onCustomPlaylistRemoved(int playlistId, const QString &name);
  void loadPlaylistDetail(int playlistId);

  // 歌曲双击/播放
  void onPlaylistSongDoubleClicked(int songId, const QString &filePath);
  void onPlaylistPlayAllClicked();
  void onPlaylistLoveSongDoubleClicked(int songId, const QString &filePath);
  void onPlaylistHistorySongDoubleClicked(int songId, const QString &filePath);
  void onPlaylistHistoryPlayAllClicked();
  void onLocalSongDoubleClicked(int songId, const QString &filePath);
  void onLocalPlayAllRequested();
  void onSelectDirectoryRequested();

  // 播放列表弹窗槽
  void onPlaylistPopupSongDoubleClicked(int index, int songId);
  void onPlaylistPopupLoveClicked(int index, int songId);
  void onPlaylistPopupRemoveClicked(int index, int songId);
  void onPlaylistPopupCollectAllClicked();
  void onPlaylistPopupClearClicked();
  void onPlaylistPopupOpenFileDirClicked(const QString &filePath);
  void onPlaylistPopupPlayPauseClicked(int index, int songId);

  // 收藏到歌单弹窗槽
  void onCreateNewPlaylistFromPopup();
  void onAddToPlaylist(int playlistId, const QString &playlistName);

  // 托盘
  void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
  void onTrayShowMainWindow();
  void onTrayToggleLyricVisible();
  void onTrayToggleLyricLock();
  void onTrayQuitApp();

  // 音量中转
  void sendVolumeRequested(int value);
};

#endif // MAINWINDOW_H
