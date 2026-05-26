#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QFileDialog>
#include <QList>
#include <QFileInfo>
#include <QMessageBox>
#include <QInputDialog>
#include <QTimer>
#include <QDialog>
#include <QFile>
#include <QDesktopServices>
#include <QLineEdit>
#include <QPainterPath>
#include <QPainter>
#include <QScreen>
#include <QGuiApplication>
#include <QShortcut>
#include <QElapsedTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

#include "PlaylistItem.h"
#include "./utils/metadataextractor.h"
#include "./models/lyricmodel.h"

#include "./controllers/librarycontroller.h"
#include "./controllers/playcontroller.h"
#include "./controllers/lyriccontroller.h"
#include "./controllers/settingcontroller.h"
#include "./models/visualizermodel.h"
#include "./utils/hotkeymanager.h"
#include "volumepopup.h"
#include "sidebarwidget.h"
#include "lyricwidget.h"
#include "playlistdetailwidget.h"
#include "localmusicwidget.h"
#include "playlistpopup.h"
#include "addtoplaylistpopup.h"

namespace
{
#ifdef Q_OS_WIN
  quint64 fileTimeToUInt64(const FILETIME &ft)
  {
    return (static_cast<quint64>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
  }

  void logProcessPerformanceMetrics()
  {
    static bool initialized = false;
    static quint64 lastKernel = 0;
    static quint64 lastUser = 0;
    static quint64 lastWallMs = 0;
    static double peakMemoryMb = 0.0;

    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&pmc), sizeof(pmc)))
    {
      const double workingSetMb = static_cast<double>(pmc.WorkingSetSize) / 1024.0 / 1024.0;
      const double privateMb = static_cast<double>(pmc.PrivateUsage) / 1024.0 / 1024.0;
      peakMemoryMb = qMax(peakMemoryMb, workingSetMb);

      FILETIME createTime, exitTime, kernelTime, userTime;
      double cpuPercent = 0.0;
      if (GetProcessTimes(GetCurrentProcess(), &createTime, &exitTime, &kernelTime, &userTime))
      {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        const quint64 nowWallMs = GetTickCount64();
        const quint64 kernel = fileTimeToUInt64(kernelTime);
        const quint64 user = fileTimeToUInt64(userTime);

        if (initialized)
        {
          const quint64 procDelta100ns = (kernel - lastKernel) + (user - lastUser);
          const quint64 wallDeltaMs = nowWallMs - lastWallMs;
          if (wallDeltaMs > 0 && sysInfo.dwNumberOfProcessors > 0)
          {
            const double procDeltaMs = static_cast<double>(procDelta100ns) / 10000.0;
            cpuPercent = procDeltaMs / (static_cast<double>(wallDeltaMs) * sysInfo.dwNumberOfProcessors) * 100.0;
          }
        }

        lastKernel = kernel;
        lastUser = user;
        lastWallMs = nowWallMs;
        initialized = true;
      }

      qInfo() << "[Performance] 进程内存占用: workingSet=" << QString::number(workingSetMb, 'f', 2)
              << "MB, private=" << QString::number(privateMb, 'f', 2)
              << "MB, peakWorkingSet=" << QString::number(peakMemoryMb, 'f', 2)
              << "MB, CPU=" << QString::number(cpuPercent, 'f', 2) << "%";
    }
  }
#else
  void logProcessPerformanceMetrics()
  {
    qInfo() << "[Performance] 当前平台未实现进程内存/CPU采样";
  }
#endif
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
  ui->setupUi(this);
  m_settingController = new SettingController(this);
  m_playController = new PlayController(this);
  m_libraryController = new LibraryController(this);
  m_lyricController = new LyricController(this);
  m_visualizerModel = new VisualizerModel(this);
  m_hotKeyManager = new HotKeyManager(this);
  styleInit();
  connectUi();
  sideBarInit();
  volumePopupInit();
  init_();
  setupPlaylistPopup();
  setupAddToPlaylistPopup();
  setupTrayIcon();
}

MainWindow::~MainWindow()
{
  delete ui;
}

void MainWindow::sendVolumeRequested(int value)
{
  emit volumeRequested(value);
}

void MainWindow::styleInit()
{
  this->setFixedSize(this->geometry().size());
  ui->btnLoop->setToolTip(QStringLiteral("列表循环"));
  setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
  setAttribute(Qt::WA_StyledBackground);
  setAttribute(Qt::WA_TranslucentBackground, true);

  ui->centralwidget->setStyleSheet(R"(
        QWidget#centralwidget {
        background-color: #f7f9fc;
        border-radius: 8px;
        border: 1px solid #bebebe;
        margin: 1px;
        }
    )");
  ui->groupBoxTitle->setStyleSheet(R"(QGroupBox {
    border: none;
    margin: 0px;
    padding: 0px;
    }
    )");
  ui->groupBoxTop->setStyleSheet(ui->groupBoxTitle->styleSheet());
  ui->groupBoxPlay->setStyleSheet(ui->groupBoxTitle->styleSheet());

  {
    QPixmap avatarSource(":/resource/images/pinkwindow.jpg");
    if (!avatarSource.isNull())
    {
      const int avatarSize = 40;
      const int radius = 8;
      QPixmap target(avatarSize, avatarSize);
      target.fill(Qt::transparent);

      QPainter painter(&target);
      painter.setRenderHint(QPainter::Antialiasing);
      painter.setRenderHint(QPainter::SmoothPixmapTransform);

      QPainterPath path;
      path.addRoundedRect(target.rect(), radius, radius);
      painter.setClipPath(path);

      QPixmap scaled = avatarSource.scaled(avatarSize, avatarSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
      int x = (avatarSize - scaled.width()) / 2;
      int y = (avatarSize - scaled.height()) / 2;
      painter.drawPixmap(x, y, scaled);
      painter.end();

      ui->btnAvatar->setIcon(QIcon(target));
      ui->btnAvatar->setIconSize(QSize(avatarSize, avatarSize));
    }
    ui->btnAvatar->setStyleSheet(
        "QPushButton { border: none; background: transparent; padding: 0px; }");
  }

  {
    QPixmap coverSource(":/resource/images/pinkwindow.jpg");
    if (!coverSource.isNull())
    {
      const int coverSize = 64;
      const int radius = 6;
      QPixmap target(coverSize, coverSize);
      target.fill(Qt::transparent);

      QPainter painter(&target);
      painter.setRenderHint(QPainter::Antialiasing);
      painter.setRenderHint(QPainter::SmoothPixmapTransform);

      QPainterPath path;
      path.addRoundedRect(target.rect(), radius, radius);
      painter.setClipPath(path);

      QPixmap scaled = coverSource.scaled(coverSize, coverSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
      int x = (coverSize - scaled.width()) / 2;
      int y = (coverSize - scaled.height()) / 2;
      painter.drawPixmap(x, y, scaled);
      painter.end();

      ui->btnSong->setIcon(QIcon(target));
      ui->btnSong->setIconSize(QSize(coverSize, coverSize));
    }
    ui->btnSong->setStyleSheet(
        "QPushButton { border: none; background: transparent; padding: 0px; margin: 0px; }");
  }

  ui->btnLove->setStyleSheet(
      "QPushButton { border: none; background: transparent; }"
      "QPushButton:hover { background: rgba(255,200,200,80); border-radius: 4px; }");

  ui->sliderPosition->setStyleSheet(
      "QSlider::groove:horizontal {"
      "  height: 4px;"
      "  background: #e0e0e0;"
      "  border-radius: 2px;"
      "}"
      "QSlider::sub-page:horizontal {"
      "  height: 4px;"
      "  background: #ec4141;"
      "  border-radius: 2px;"
      "}"
      "QSlider::add-page:horizontal {"
      "  height: 4px;"
      "  background: #e0e0e0;"
      "  border-radius: 2px;"
      "}"
      "QSlider::handle:horizontal {"
      "  width: 12px;"
      "  height: 12px;"
      "  margin: -4px 0;"
      "  background: #ec4141;"
      "  border-radius: 6px;"
      "  border: 2px solid #ffffff;"
      "}"
      "QSlider::handle:horizontal:hover {"
      "  background: #ff5a5a;"
      "  width: 14px;"
      "  height: 14px;"
      "  margin: -5px 0;"
      "}");
}

void MainWindow::connectUi()
{
  connect(m_playController, &PlayController::sigProgressChanged, this, &MainWindow::do_positionChanged);
  connect(m_playController, &PlayController::sigSourceChanged, this, &MainWindow::do_sourceChanged);
  connect(m_playController, &PlayController::sigPlayPause, this, &MainWindow::do_stateChanged, Qt::UniqueConnection);
  connect(m_playController, &PlayController::sigPlayError, this, [this](const QString &errMsg)
          { QMessageBox::warning(this, "播放错误", errMsg); });
  connect(m_playController, &PlayController::sigMetaDataChanged, this, &MainWindow::do_metaDataChanged);
  connect(this, &MainWindow::volumeRequested, m_playController, &PlayController::setVolume);

  connect(m_playController, &PlayController::sigProgressChanged, m_visualizerModel, &VisualizerModel::updateFromPlayback);
  connect(m_visualizerModel, &VisualizerModel::sigSpectrumDataChanged, this, [this](const QVector<qreal> &levels)
          {
            m_audioLevels = levels;
            updateVisualizationButton(levels);
            ui->groupBoxPlay->update(); });

  connect(m_hotKeyManager, &HotKeyManager::sigTogglePlayPause, m_playController, &PlayController::playPause);
  connect(m_hotKeyManager, &HotKeyManager::sigNextTrack, m_playController, &PlayController::next);
  connect(m_hotKeyManager, &HotKeyManager::sigPreviousTrack, m_playController, &PlayController::previous);
  connect(m_hotKeyManager, &HotKeyManager::sigHotKeyRegistrationChanged, this, [this](bool enabled)
          {
            if (m_settingController) {
              m_settingController->setHotkeyEnabled(enabled);
            } });
}

void MainWindow::volumePopupInit()
{
  popup = new VolumePopup(this);
  popup->hide();
  connect(popup, &VolumePopup::volumeChanged, this, &MainWindow::sendVolumeRequested);
}

void MainWindow::sideBarInit()
{
  QVBoxLayout *lay = qobject_cast<QVBoxLayout *>(ui->scrollAreaWidget->layout());
  if (!lay)
  {
    lay = new QVBoxLayout(ui->scrollAreaWidget);
  }
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(0);

  sideBar = new SideBarWidget(ui->scrollAreaWidget, this);
  sideBar->setLibraryController(m_libraryController);
  lay->addWidget(sideBar);

  connect(sideBar, &SideBarWidget::pageRequested,
          ui->stackedWidget, &QStackedWidget::setCurrentIndex);
  connect(ui->stackedWidget, &QStackedWidget::currentChanged,
          sideBar, &SideBarWidget::setButtonChecked);

  connect(sideBar, &SideBarWidget::customPlaylistAdded,
          this, &MainWindow::onCustomPlaylistAdded);
  connect(sideBar, &SideBarWidget::customPlaylistRemoved,
          this, &MainWindow::onCustomPlaylistRemoved);

  sideBar->loadPlaylistsFromDb();

  connect(sideBar, &SideBarWidget::customPlaylistClicked,
          this, &MainWindow::loadPlaylistDetail);
}

void MainWindow::setupPlaylistDetail()
{
  m_playlistDetail = new PlaylistDetailWidget(ui->pageSongs);
  QVBoxLayout *layout = new QVBoxLayout(ui->pageSongs);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(m_playlistDetail);

  connect(m_playlistDetail, &PlaylistDetailWidget::songDoubleClicked,
          this, &MainWindow::onPlaylistSongDoubleClicked);
  connect(m_playlistDetail, &PlaylistDetailWidget::playAllClicked,
          this, &MainWindow::onPlaylistPlayAllClicked);
  connect(m_playlistDetail, &PlaylistDetailWidget::songLoveClicked,
          this, [this](int songId)
          {
    m_libraryController->toggleFavorite(songId);
    refreshLoveList(); });
  connect(m_playlistDetail, &PlaylistDetailWidget::songAddToPlaylistClicked,
          this, [this](int songId)
          {
    if (!m_addToPlaylistPopup)
      return;
    m_addToPlaylistPopup->setCurrentSongId(songId);
    m_addToPlaylistPopup->refreshData();
    showAddToPlaylistPopup(songId); });

  connect(m_playlistDetail, &PlaylistDetailWidget::playlistInfoEdited,
          this, [this](int playlistId, const QString &name, const QString &description)
          {
    if (m_libraryController->updatePlaylistInfo(playlistId, name, description)) {
      if (sideBar) {
        sideBar->updateCustomPlaylistName(playlistId, name);
      }
    } });

  m_playlistDetailLove = new PlaylistDetailWidget(ui->pageLove);
  QVBoxLayout *loveLayout = new QVBoxLayout(ui->pageLove);
  loveLayout->setContentsMargins(0, 0, 0, 0);
  loveLayout->setSpacing(0);
  loveLayout->addWidget(m_playlistDetailLove);

  connect(m_playlistDetailLove, &PlaylistDetailWidget::songDoubleClicked,
          this, &MainWindow::onPlaylistLoveSongDoubleClicked);
  connect(m_playlistDetailLove, &PlaylistDetailWidget::playAllClicked,
          this, [this]()
          {
    const QList<Song> songs = m_libraryController->favoriteSongs();
    if (!songs.isEmpty()) {
      QList<QUrl> queue;
      for (const Song &s : songs) {
        queue.append(QUrl::fromLocalFile(s.filePath()));
      }
      m_playController->setPlayQueue(queue, 0, true);
    } });
  connect(m_playlistDetailLove, &PlaylistDetailWidget::songLoveClicked,
          this, [this](int songId)
          {
    m_libraryController->toggleFavorite(songId);
    refreshLoveList(); });
  connect(m_playlistDetailLove, &PlaylistDetailWidget::songAddToPlaylistClicked,
          this, [this](int songId)
          {
    if (!m_addToPlaylistPopup)
      return;
    m_addToPlaylistPopup->setCurrentSongId(songId);
    m_addToPlaylistPopup->refreshData();
    showAddToPlaylistPopup(songId); });

  m_playlistDetailHistory = new PlaylistDetailWidget(ui->pageHistory);
  QVBoxLayout *historyLayout = new QVBoxLayout(ui->pageHistory);
  historyLayout->setContentsMargins(0, 0, 0, 0);
  historyLayout->setSpacing(0);
  historyLayout->addWidget(m_playlistDetailHistory);

  connect(m_playlistDetailHistory, &PlaylistDetailWidget::songDoubleClicked,
          this, &MainWindow::onPlaylistHistorySongDoubleClicked);
  connect(m_playlistDetailHistory, &PlaylistDetailWidget::playAllClicked,
          this, &MainWindow::onPlaylistHistoryPlayAllClicked);
  connect(m_playlistDetailHistory, &PlaylistDetailWidget::songLoveClicked,
          this, [this](int songId)
          {
    m_libraryController->toggleFavorite(songId);
    refreshLoveList(); });
  connect(m_playlistDetailHistory, &PlaylistDetailWidget::songAddToPlaylistClicked,
          this, [this](int songId)
          {
    if (!m_addToPlaylistPopup)
      return;
    m_addToPlaylistPopup->setCurrentSongId(songId);
    m_addToPlaylistPopup->refreshData();
    showAddToPlaylistPopup(songId); });

  connect(ui->stackedWidget, &QStackedWidget::currentChanged,
          this, [this](int index)
          {
    if (index == 1) {
      refreshLoveList();
    } else if (index == 2) {
      refreshHistoryList();
    } });
}

void MainWindow::loadPlaylistDetail(int playlistId)
{
  Playlist targetPlaylist = m_libraryController->playlistById(playlistId);
  if (!targetPlaylist.isValid())
  {
    return;
  }

  const QList<Song> songs = m_libraryController->playlistSongs(playlistId);

  if (m_playlistDetail)
  {
    m_playlistDetail->setPlaylist(targetPlaylist);
    m_playlistDetail->setSongs(songs);

    if (!songs.isEmpty())
    {
      QPixmap cover = MetaDataExtractor::extractCover(songs.first().filePath());
      if (!cover.isNull())
      {
        m_playlistDetail->setPlaylistCover(cover);
      }
    }
  }

  ui->stackedWidget->setCurrentWidget(ui->pageSongs);
}

void MainWindow::onPlaylistSongDoubleClicked(int songId, const QString &filePath)
{
  Q_UNUSED(songId)
  if (!filePath.isEmpty())
  {
    m_playController->play(QUrl::fromLocalFile(filePath));
  }
}

void MainWindow::onPlaylistHistorySongDoubleClicked(int songId, const QString &filePath)
{
  Q_UNUSED(songId)
  if (!filePath.isEmpty())
  {
    m_playController->play(QUrl::fromLocalFile(filePath));
  }
}

void MainWindow::onPlaylistHistoryPlayAllClicked()
{
  if (!m_playlistDetailHistory)
    return;

  const QList<Song> songs = m_libraryController->playHistory();
  if (!songs.isEmpty())
  {
    QList<QUrl> queue;
    for (const Song &s : songs)
    {
      queue.append(QUrl::fromLocalFile(s.filePath()));
    }
    m_playController->setPlayQueue(queue, 0, true);
  }
}

void MainWindow::onPlaylistPlayAllClicked()
{
  if (!m_playlistDetail)
    return;

  const QList<Song> songs = m_playlistDetail->songs();
  if (songs.isEmpty())
    return;

  QList<QUrl> queue;
  for (const Song &s : songs)
    queue.append(QUrl::fromLocalFile(s.filePath()));
  m_playController->setPlayQueue(queue, 0, true);
}

void MainWindow::init_()
{
  auto *lyricModel = new LyricModel(this);

  m_lyricWidget = new LyricWidget();
  m_lyricWidget->setLyricModel(lyricModel);
  connect(m_lyricWidget, &LyricWidget::sigLockStateChanged, this, [this](bool /*locked*/)
          { refreshTrayMenu(); });

  m_lyricController->setLyricWidget(m_lyricWidget);
  m_lyricController->setLyricModel(lyricModel);
  m_lyricController->bindPlayController(m_playController);
  ui->btnAudioVisualization->setCheckable(true);
  ui->btnAudioVisualization->setToolTip(QStringLiteral("频谱可视化未开启"));

  ui->groupBoxPlay->installEventFilter(this);
  ui->widgetSong->setStyleSheet("QWidget#widgetSong { background: transparent; }");

  connect(ui->btnLyric, &QPushButton::clicked, this, [this]()
          { m_lyricController->toggleLyricWidget(m_playController->currentSource(), m_playController->playbackState()); });

  // 全局快捷键：启动时强制尝试注册，避免历史配置 hotkey/enabled=false 导致完全无日志、无响应
  if (m_settingController)
  {
    m_settingController->setHotkeyEnabled(true);
  }
  const bool hotKeyEnabled = true;
  const bool hotKeyOk = m_hotKeyManager->setEnabled(hotKeyEnabled);
  qDebug() << "[MainWindow] 全局快捷键启用请求:" << hotKeyEnabled << "注册结果:" << hotKeyOk;

  // 窗口级快捷键兜底：当全局热键被系统/其他软件占用时，至少应用窗口聚焦时仍可使用
  auto *playPauseShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+Space")), this);
  connect(playPauseShortcut, &QShortcut::activated, m_playController, &PlayController::playPause);
  auto *previousShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+Left")), this);
  connect(previousShortcut, &QShortcut::activated, m_playController, &PlayController::previous);
  auto *nextShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+Right")), this);
  connect(nextShortcut, &QShortcut::activated, m_playController, &PlayController::next);

  // 非功能性能测试埋点：普通播放场景下每5秒输出一次内存/CPU占用
  auto *perfTimer = new QTimer(this);
  connect(perfTimer, &QTimer::timeout, this, []()
          { logProcessPerformanceMetrics(); });
  perfTimer->start(5000);
  logProcessPerformanceMetrics();

  updateLoveButtonState();

  setupPlaylistDetail();
  refreshLoveList();
  setupLocalMusicWidget();

  // 默认展示本地音乐库界面
  ui->stackedWidget->setCurrentIndex(0);
  sideBar->setButtonChecked(0);

  // 启动时自动扫描默认音乐文件夹并加载歌曲（异步，不阻塞UI）
  connect(m_libraryController, &LibraryController::importFinished,
          this, [this](const QList<Song> &songs, int)
          {
    if (m_localMusicWidget)
    {
      m_localMusicWidget->loadSongs(songs);
    }
    // 如果有歌曲，自动建立播放队列（不自动播放）
    if (!songs.isEmpty())
    {
      QList<QUrl> queue;
      for (const Song &s : songs)
        queue.append(QUrl::fromLocalFile(s.filePath()));
      m_playController->setPlayQueue(queue, 0, false);
    } });
  m_libraryController->importSongsAsync();
}

void MainWindow::setupPlaylistPopup()
{
  m_playlistPopup = new PlaylistPopup(ui->widget);
  m_playlistPopup->hide();

  connect(m_playlistPopup, &PlaylistPopup::songDoubleClicked, this, &MainWindow::onPlaylistPopupSongDoubleClicked);
  connect(m_playlistPopup, &PlaylistPopup::loveClicked, this, &MainWindow::onPlaylistPopupLoveClicked);
  connect(m_playlistPopup, &PlaylistPopup::removeClicked, this, &MainWindow::onPlaylistPopupRemoveClicked);
  connect(m_playlistPopup, &PlaylistPopup::collectAllClicked, this, &MainWindow::onPlaylistPopupCollectAllClicked);
  connect(m_playlistPopup, &PlaylistPopup::clearClicked, this, &MainWindow::onPlaylistPopupClearClicked);
  connect(m_playlistPopup, &PlaylistPopup::openFileDirClicked, this, &MainWindow::onPlaylistPopupOpenFileDirClicked);
  connect(m_playlistPopup, &PlaylistPopup::playPauseClicked, this, &MainWindow::onPlaylistPopupPlayPauseClicked);
  connect(AudioEngine::instance(), &AudioEngine::sigQueueChanged, this, [this]()
          {
    m_lastPopupQueue.clear();
    refreshPlaylistPopup(); });
  connect(m_playController, &PlayController::sigSourceChanged, this, [this](const QUrl &)
          { refreshPlaylistPopup(); });
}

void MainWindow::showPlaylistPopup()
{
  if (!m_playlistPopup)
    return;

  refreshPlaylistPopup();

  QWidget *targetWidget = ui->widget;
  if (!targetWidget)
    return;

  int popupX = targetWidget->width() - m_playlistPopup->width();
  int popupY = 0;

  m_playlistPopup->move(popupX, popupY);
  m_playlistPopup->raise();
  m_playlistPopup->show();
}

void MainWindow::hidePlaylistPopup()
{
  if (m_playlistPopup)
    m_playlistPopup->hide();
}

void MainWindow::on_btnList_clicked()
{
  if (!m_playlistPopup)
    return;

  if (m_playlistPopup->isVisible())
    hidePlaylistPopup();
  else
    showPlaylistPopup();
}

void MainWindow::refreshPlaylistPopup()
{
  if (!m_playlistPopup)
    return;

  QElapsedTimer timer;
  timer.start();

  QList<QUrl> queue = m_playController->playQueue();
  QList<Song> songs;
  const bool queueChanged = (queue != m_lastPopupQueue);
  if (queueChanged)
  {
    songs = m_libraryController->songsFromUrls(queue);
    m_playlistPopup->setSongs(songs);
    m_lastPopupQueue = queue;
  }
  else
  {
    // 队列未变化时直接复用弹窗内部缓存，避免再次URL转Song和重建UI
    songs = m_playlistPopup->songs();
  }

  QUrl current = m_playController->currentSource();
  if (current.isLocalFile())
  {
    for (int i = 0; i < songs.size(); ++i)
    {
      if (QFileInfo(songs[i].filePath()).absoluteFilePath() == QFileInfo(current.toLocalFile()).absoluteFilePath())
      {
        m_playlistPopup->setCurrentPlayingId(songs[i].songId());
        m_playlistPopup->setSelectedIndex(i);
        break;
      }
    }
  }

  qDebug() << "[MainWindow] refreshPlaylistPopup 耗时:" << timer.elapsed()
           << "ms, queueChanged=" << queueChanged << "queue=" << queue.size();
}

void MainWindow::setupAddToPlaylistPopup()
{
  m_addToPlaylistPopup = new AddToPlaylistPopup(ui->centralwidget);
  m_addToPlaylistPopup->setLibraryController(m_libraryController);
  m_addToPlaylistPopup->hide();

  connect(m_addToPlaylistPopup, &AddToPlaylistPopup::createNewPlaylistRequested,
          this, &MainWindow::onCreateNewPlaylistFromPopup);
  connect(m_addToPlaylistPopup, &AddToPlaylistPopup::addToPlaylistRequested,
          this, &MainWindow::onAddToPlaylist);
}

void MainWindow::showAddToPlaylistPopup(int songId)
{
  if (!m_addToPlaylistPopup)
    return;

  int targetSongId = (songId > 0) ? songId : m_currentSongId;
  m_addToPlaylistPopup->setCurrentSongId(targetSongId);
  m_addToPlaylistPopup->refreshData();

  QWidget *targetWidget = ui->groupBoxTop;
  if (!targetWidget)
    return;

  QRect targetRect = targetWidget->geometry();
  int popupX = targetRect.x() + (targetRect.width() - m_addToPlaylistPopup->width()) / 2;
  int popupY = targetRect.y() + (targetRect.height() - m_addToPlaylistPopup->height()) / 2;

  m_addToPlaylistPopup->move(popupX, popupY);
  m_addToPlaylistPopup->raise();
  m_addToPlaylistPopup->show();
}

void MainWindow::hideAddToPlaylistPopup()
{
  if (m_addToPlaylistPopup)
    m_addToPlaylistPopup->hide();
}

void MainWindow::setupLocalMusicWidget()
{
  m_localMusicWidget = new LocalMusicWidget(ui->pageLocal);
  m_localMusicWidget->setLibraryController(m_libraryController);
  m_localMusicWidget->setGeometry(0, 0, 865, 568);
  m_localMusicWidget->raise();
  m_localMusicWidget->show();

  connect(m_localMusicWidget, &LocalMusicWidget::songDoubleClicked,
          this, &MainWindow::onLocalSongDoubleClicked);
  connect(m_localMusicWidget, &LocalMusicWidget::playAllRequested,
          this, &MainWindow::onLocalPlayAllRequested);
  connect(m_localMusicWidget, &LocalMusicWidget::refreshRequested,
          this, [this]()
          {
    // 重新扫描默认音乐目录（异步，不阻塞UI）
    connect(m_libraryController, &LibraryController::importFinished,
            this, [this](const QList<Song> &songs, int) {
      if (m_localMusicWidget)
      {
        m_localMusicWidget->loadSongs(songs);
      }
      // 重建播放队列（保持当前播放位置，不自动切歌）
      if (!songs.isEmpty())
      {
        QList<QUrl> queue;
        for (const Song &s : songs)
          queue.append(QUrl::fromLocalFile(s.filePath()));
        m_playController->setPlayQueue(queue, m_playController->currentIndex(), false);
      }
    }, Qt::SingleShotConnection);
    m_libraryController->importSongsAsync(); });
  connect(m_localMusicWidget, &LocalMusicWidget::selectDirectoryRequested,
          this, &MainWindow::onSelectDirectoryRequested);
  connect(m_localMusicWidget, &LocalMusicWidget::songLoveToggled,
          this, [this](int songId)
          {
    if (m_libraryController)
    {
      m_libraryController->toggleFavorite(songId);
      refreshLoveList();
    } });
  connect(m_localMusicWidget, &LocalMusicWidget::songAddToPlaylistRequested,
          this, [this](int songId)
          {
    if (!m_addToPlaylistPopup)
      return;

    m_addToPlaylistPopup->setCurrentSongId(songId);
    m_addToPlaylistPopup->refreshData();

    QWidget *targetWidget = ui->groupBoxTop;
    if (!targetWidget)
      return;

    QRect targetRect = targetWidget->geometry();
    int popupX = targetRect.x() + (targetRect.width() - m_addToPlaylistPopup->width()) / 2;
    int popupY = targetRect.y() + (targetRect.height() - m_addToPlaylistPopup->height()) / 2;

    m_addToPlaylistPopup->move(popupX, popupY);
    m_addToPlaylistPopup->raise();
    m_addToPlaylistPopup->show(); });

  m_localMusicWidget->reloadFromDb();
}

void MainWindow::onLocalSongDoubleClicked(int songId, const QString &filePath)
{
  Q_UNUSED(songId)
  if (!filePath.isEmpty())
  {
    m_playController->play(QUrl::fromLocalFile(filePath));
  }
}

void MainWindow::onLocalPlayAllRequested()
{
  if (!m_localMusicWidget)
    return;

  std::vector<Song> songs = m_localMusicWidget->currentVisibleSongs();
  if (songs.empty())
    return;

  QList<QUrl> urls;
  for (const Song &s : songs)
    urls.append(QUrl::fromLocalFile(s.filePath()));
  m_playController->setPlayQueue(urls, 0, true);
}

void MainWindow::onSelectDirectoryRequested()
{
  const QString initialDir = m_settingController
                                 ? m_settingController->lastImportDir().isEmpty()
                                       ? m_libraryController->defaultMusicDirectory()
                                       : m_settingController->lastImportDir()
                                 : m_libraryController->defaultMusicDirectory();

  const QString directory = QFileDialog::getExistingDirectory(
      this,
      QStringLiteral("选择音乐文件夹"),
      initialDir,
      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

  if (directory.isEmpty())
    return;

  if (!m_libraryController->directoryContainsAudio(directory))
  {
    QMessageBox::information(this,
                             QStringLiteral("导入提示"),
                             QStringLiteral("所选文件夹中未找到可导入的音频文件。"));
    return;
  }

  if (m_settingController)
  {
    m_settingController->setLastImportDir(directory);
  }

  // 连接一次性完成信号
  connect(m_libraryController, &LibraryController::importFinished, this, [this, directory](const QList<Song> &songs, int elapsedMs)
          {
    Q_UNUSED(elapsedMs)
    if (songs.isEmpty())
    {
      QMessageBox::warning(this,
                           QStringLiteral("导入失败"),
                           QStringLiteral("扫描完成，但未生成可播放的歌曲列表。"));
      return;
    }

    if (m_localMusicWidget)
      m_localMusicWidget->loadSongs(songs);

    QList<QUrl> queue;
    for (const Song &s : songs)
      queue.append(QUrl::fromLocalFile(s.filePath()));
    if (!queue.isEmpty())
      m_playController->setPlayQueue(queue, 0, true); }, Qt::SingleShotConnection);

  m_libraryController->importSongsAsync(directory);
}

void MainWindow::updateVisualizationButton(const QVector<qreal> &levels)
{
  const QSize iconSize = ui->btnAudioVisualization->iconSize().isValid()
                             ? ui->btnAudioVisualization->iconSize()
                             : QSize(32, 32);
  QPixmap pixmap(iconSize);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  if (levels.isEmpty())
  {
    painter.setPen(QPen(QColor(170, 170, 170), 2));
    painter.drawLine(4, iconSize.height() - 5, iconSize.width() - 4, iconSize.height() - 5);
    painter.end();
    ui->btnAudioVisualization->setIcon(QIcon(":/resource/icons/waveform-custom.png"));
    ui->btnAudioVisualization->setToolTip(QStringLiteral("频谱可视化"));
    return;
  }

  painter.setPen(Qt::NoPen);
  const int barCount = levels.size();
  const qreal step = static_cast<qreal>(iconSize.width()) / qMax(1, barCount);
  const qreal barWidth = qMax<qreal>(0.5, step * 0.7);

  for (int i = 0; i < barCount; ++i)
  {
    const qreal value = qBound<qreal>(0.0, levels.at(i), 1.0);
    const qreal barHeight = qMax<qreal>(1.0, value * (iconSize.height() - 2));
    const QRectF barRect(i * step, iconSize.height() - barHeight, barWidth, barHeight);
    const QColor color = QColor::fromHsv((110 + i * 6) % 360, 180, 230, 220);
    painter.setBrush(color);
    painter.drawRoundedRect(barRect, 0.5, 0.5);
  }

  painter.end();
  ui->btnAudioVisualization->setIcon(QIcon(pixmap));
  ui->btnAudioVisualization->setToolTip(QStringLiteral("频谱可视化已开启，共 %1 个频段").arg(levels.size()));
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton)
  {
    if (m_playlistPopup && m_playlistPopup->isVisible())
    {
      QPoint posInPopup = m_playlistPopup->mapFromGlobal(event->globalPosition().toPoint());
      if (!m_playlistPopup->rect().contains(posInPopup))
      {
        QPoint posInBtnList = ui->btnList->mapFromGlobal(event->globalPosition().toPoint());
        if (!ui->btnList->rect().contains(posInBtnList))
        {
          hidePlaylistPopup();
        }
      }
    }

    if (m_addToPlaylistPopup && m_addToPlaylistPopup->isVisible())
    {
      QPoint posInPopup = m_addToPlaylistPopup->mapFromGlobal(event->globalPosition().toPoint());
      if (!m_addToPlaylistPopup->rect().contains(posInPopup))
      {
        QPoint posInBtnFavorite = ui->btnFavorite->mapFromGlobal(event->globalPosition().toPoint());
        if (!ui->btnFavorite->rect().contains(posInBtnFavorite))
        {
          hideAddToPlaylistPopup();
        }
      }
    }

    if (ui->groupBoxTitle->geometry().contains(event->pos()) && ui->groupBoxTitle->childAt(event->pos() - ui->groupBoxTitle->pos()) == nullptr)
    {
      m_isDragging = true;
      m_dragStartPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
      event->accept();
      return;
    }
  }
  QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
  if (m_isDragging && (event->buttons() & Qt::LeftButton))
  {
    move(event->globalPosition().toPoint() - m_dragStartPos);
    event->accept();
    return;
  }
  QMainWindow::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
  m_isDragging = false;
  QMainWindow::mouseReleaseEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == ui->groupBoxPlay && event->type() == QEvent::Paint)
  {
    bool res = QMainWindow::eventFilter(watched, event);
    QGroupBox *groupBox = qobject_cast<QGroupBox *>(watched);
    if (!groupBox || !groupBox->isVisible())
    {
      return res;
    }

    if (!m_visualizerModel->isEnabled() && m_audioLevels.isEmpty())
    {
      return res;
    }

    QPainter painter(groupBox);
    painter.setRenderHint(QPainter::Antialiasing);

    QRectF spectrumRect = groupBox->contentsRect();
    QMarginsF cutMargins(5, 0, 5, 0);
    spectrumRect = spectrumRect.marginsRemoved(cutMargins);
    if (spectrumRect.width() <= 0 || spectrumRect.height() <= 0)
    {
      return res;
    }

    QLinearGradient bgGradient(spectrumRect.topLeft(), spectrumRect.bottomLeft());
    bgGradient.setColorAt(0.0, QColor(0, 0, 0, 0));
    bgGradient.setColorAt(0.4, QColor(20, 20, 40, 5));
    bgGradient.setColorAt(1.0, QColor(20, 20, 40, 10));
    painter.setBrush(bgGradient);
    painter.setPen(Qt::NoPen);
    painter.drawRect(spectrumRect);

    if (m_audioLevels.isEmpty())
    {
      return res;
    }

    painter.setPen(Qt::NoPen);
    const int barCount = m_audioLevels.size();
    if (barCount == 0)
    {
      return res;
    }
    const qreal step = spectrumRect.width() / qMax(1, barCount);
    const qreal barWidth = qMax<qreal>(1, step * 0.99);
    for (int i = 0; i < barCount; ++i)
    {
      if (i >= m_audioLevels.size())
        break;
      const qreal value = qBound<qreal>(0.0, m_audioLevels.at(i), 1.0);
      if (value <= 0.01)
      {
        continue;
      }
      const qreal barHeight = qMax<qreal>(2.0, value * (spectrumRect.height() - 1));
      const QRectF barRect(
          spectrumRect.left() + i * step,
          spectrumRect.bottom() - barHeight,
          barWidth,
          barHeight);

      const qreal hue = static_cast<qreal>((110 + i * 6) % 360) / 360.0;
      QLinearGradient barGradient(barRect.bottomLeft(), barRect.topLeft());
      barGradient.setColorAt(0.0, QColor::fromHsvF(hue, 0.9, 0.95, 0.85));
      barGradient.setColorAt(0.5, QColor::fromHsvF(hue, 0.75, 0.85, 0.6));
      barGradient.setColorAt(1.0, QColor::fromHsvF(std::fmod(hue + 0.06, 1.0), 0.55, 0.65, 0.35));
      painter.setBrush(barGradient);
      painter.drawRoundedRect(barRect, 0.8, 0.8);
    }
    return res;
  }
  return QMainWindow::eventFilter(watched, event);
}

QPixmap MainWindow::getMusicCoverByTaglib(const QString &filePath)
{
  QPixmap cover = MetaDataExtractor::extractCover(filePath);
  if (cover.isNull())
    return QPixmap(":/resource/images/default_music.png");
  return cover;
}

void MainWindow::do_stateChanged(QMediaPlayer::PlaybackState pb)
{
  if (pb == QMediaPlayer::PlayingState)
  {
    ui->btnPlay->setIcon(QIcon(":/resource/icons/pause.png"));
  }
  else if (pb == QMediaPlayer::PausedState)
  {
    ui->btnPlay->setIcon(QIcon(":/resource/icons/play.png"));
  }
}

void MainWindow::do_sourceChanged(const QUrl &media)
{
  if (m_visualizerModel)
  {
    m_visualizerModel->setSource(media);
  }

  const QUrl source = m_playController->currentSource();
  if (source.isLocalFile())
  {
    m_currentSongId = m_libraryController->songIdByFilePath(source.toLocalFile());
  }
  else
  {
    m_currentSongId = -1;
  }
  updateLoveButtonState();

  if (m_currentSongId > 0)
  {
    m_libraryController->addPlayHistory(m_currentSongId, 0);
  }

  if (source.isLocalFile())
  {
    const QString filePath = source.toLocalFile();
    Song song = MetaDataExtractor().extract(filePath);

    QString title = song.title().isEmpty() ? QFileInfo(filePath).fileName() : song.title();
    ui->labSongName->setText(title);

    QString artist = song.artist().isEmpty() ? QStringLiteral("未知歌手") : song.artist();
    QString album = song.album().isEmpty() ? QStringLiteral("未知专辑") : song.album();
    ui->labSinger->setText(artist + " / " + album);

    QPixmap cover = MetaDataExtractor::extractCover(filePath);
    if (cover.isNull())
      cover = QPixmap(QStringLiteral(":/resource/images/default_music.png"));

    QSize iconSize(64, 64);
    QPixmap scaled = cover.scaled(iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmap rounded(iconSize);
    rounded.fill(Qt::transparent);
    QPainter p(&rounded);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(0, 0, iconSize.width(), iconSize.height(), 6, 6);
    p.setClipPath(path);
    int x = (iconSize.width() - scaled.width()) / 2;
    int y = (iconSize.height() - scaled.height()) / 2;
    p.drawPixmap(x, y, scaled);
    p.end();
    ui->btnSong->setIcon(QIcon(rounded));
  }
  else
  {
    ui->labSongName->setText(QStringLiteral("暂无歌曲"));
    ui->labSinger->setText(QStringLiteral("歌手/专辑"));

    QPixmap defaultCover(QStringLiteral(":/resource/images/default_music.png"));
    QSize iconSize(64, 64);
    QPixmap scaled2 = defaultCover.scaled(iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmap rounded2(iconSize);
    rounded2.fill(Qt::transparent);
    QPainter p2(&rounded2);
    p2.setRenderHint(QPainter::Antialiasing);
    QPainterPath path2;
    path2.addRoundedRect(0, 0, iconSize.width(), iconSize.height(), 6, 6);
    p2.setClipPath(path2);
    int x2 = (iconSize.width() - scaled2.width()) / 2;
    int y2 = (iconSize.height() - scaled2.height()) / 2;
    p2.drawPixmap(x2, y2, scaled2);
    p2.end();
    ui->btnSong->setIcon(QIcon(rounded2));
  }
}

void MainWindow::do_positionChanged(qint64 position, qint64 duration)
{
  if (ui->sliderPosition->isSliderDown())
    return;

  ui->sliderPosition->blockSignals(true);
  ui->sliderPosition->setMaximum(duration);
  ui->sliderPosition->setSliderPosition(position);
  ui->sliderPosition->blockSignals(false);

  int secs = position / 1000, mins = secs / 60;
  secs %= 60;
  QString positionTime = QString::asprintf("%d:%02d", mins, secs);
  secs = duration / 1000;
  mins = secs / 60;
  secs %= 60;
  QString durationTime = QString::asprintf("%d:%02d", mins, secs);
  ui->labRatio->setText(positionTime + "/" + durationTime);
}

void MainWindow::do_metaDataChanged()
{
}

void MainWindow::on_sliderPosition_valueChanged(int value)
{
  m_playController->setPosition(value);
}

void MainWindow::on_btnNext_clicked()
{
  m_playController->next();
}

void MainWindow::on_btnPrevious_clicked()
{
  m_playController->previous();
}

void MainWindow::on_btnPlay_clicked()
{
  m_playController->playPause();
}

void MainWindow::on_btnLoop_clicked()
{
  LoopMode currentMode = m_playController->loopMode();
  LoopMode nextMode;
  QString tip;

  switch (currentMode)
  {
  case LOOP_LIST:
    nextMode = LOOP_SINGLE;
    tip = QStringLiteral("单曲循环");
    break;
  case LOOP_SINGLE:
    nextMode = LOOP_RANDOM;
    tip = QStringLiteral("随机播放");
    break;
  case LOOP_RANDOM:
    nextMode = LOOP_LIST;
    tip = QStringLiteral("列表循环");
    break;
  }

  m_playController->setLoopMode(nextMode);
  ui->btnLoop->setToolTip(tip);
}

void MainWindow::on_btnAudioVisualization_clicked()
{
  const bool enabled = !m_visualizerModel->isEnabled();
  m_visualizerModel->setEnabled(enabled);
  ui->btnAudioVisualization->setChecked(enabled);

  if (enabled)
  {
    m_visualizerModel->updateFromPlayback(m_playController->currentPosition(), m_playController->currentDuration());
  }
  else
  {
    m_audioLevels.clear();
    updateVisualizationButton({});
    ui->groupBoxPlay->update();
  }
}

void MainWindow::on_btnVolume_clicked()
{
  if (popup->isVisible())
  {
    popup->hide();
    return;
  }

  QPoint pos = ui->btnVolume->mapToGlobal(QPoint(ui->btnVolume->width() / 2, 0));
  popup->move(pos.x() - popup->width() / 2, pos.y() - 1.1 * popup->height());
  popup->show();
}

void MainWindow::on_btnMinimize_clicked()
{
  showMinimized();
}

void MainWindow::on_btnClose_clicked()
{
  close();
}

void MainWindow::on_btnFavorite_clicked()
{
  if (m_currentSongId <= 0)
  {
    QMessageBox::information(this, QStringLiteral("提示"),
                             QStringLiteral("当前没有正在播放的歌曲，无法收藏到歌单。"));
    return;
  }

  if (!m_addToPlaylistPopup)
    return;

  if (m_addToPlaylistPopup->isVisible())
    hideAddToPlaylistPopup();
  else
    showAddToPlaylistPopup();
}

void MainWindow::on_btnLove_clicked()
{
  if (m_currentSongId <= 0)
  {
    QMessageBox::information(this, QStringLiteral("提示"),
                             QStringLiteral("当前没有正在播放的歌曲，无法操作收藏。"));
    return;
  }

  if (!m_libraryController->toggleFavorite(m_currentSongId))
  {
    QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("数据库操作失败，请重试。"));
    return;
  }

  updateLoveButtonState();
  refreshLoveList();
}

void MainWindow::updateLoveButtonState()
{
  if (m_currentSongId <= 0)
  {
    ui->btnLove->setStyleSheet(
        "QPushButton { border: none; background: transparent; }"
        "QPushButton:hover { background: rgba(255,200,200,80); border-radius: 4px; }");
    ui->btnLove->setToolTip(QStringLiteral("收藏"));
    ui->btnLove->setIcon(QIcon());
    return;
  }

  const bool isFav = m_libraryController->isSongFavorited(m_currentSongId);

  if (isFav)
  {
    ui->btnLove->setStyleSheet(
        "QPushButton { border: none; background: transparent; }"
        "QPushButton:hover { background: rgba(255,100,100,120); border-radius: 4px; }");
    ui->btnLove->setToolTip(QStringLiteral("取消收藏"));
    QPixmap heartPixmap(24, 24);
    heartPixmap.fill(Qt::transparent);
    QPainter p(&heartPixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(220, 50, 50));
    p.setFont(QFont("Segoe UI Emoji", 14));
    p.drawText(QRect(0, -2, 24, 24), Qt::AlignCenter, QStringLiteral("\u2665"));
    p.end();
    ui->btnLove->setIcon(QIcon(heartPixmap));
  }
  else
  {
    ui->btnLove->setStyleSheet(
        "QPushButton { border: none; background: transparent; }"
        "QPushButton:hover { background: rgba(255,200,200,80); border-radius: 4px; }");
    ui->btnLove->setToolTip(QStringLiteral("收藏"));
    QPixmap heartPixmap(24, 24);
    heartPixmap.fill(Qt::transparent);
    QPainter p(&heartPixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(180, 180, 180));
    p.setFont(QFont("Segoe UI Emoji", 14));
    p.drawText(QRect(0, -2, 24, 24), Qt::AlignCenter, QStringLiteral("\u2661"));
    p.end();
    ui->btnLove->setIcon(QIcon(heartPixmap));
  }
}

void MainWindow::onCustomPlaylistAdded(const QString &name, const QString &description)
{
  int playlistId = m_libraryController->createPlaylist(name, description);
  if (playlistId <= 0)
  {
    QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("创建歌单失败。"));
    return;
  }

  sideBar->addCustomPlaylist(name, playlistId);
  qDebug() << "[UI Info] 创建自定义歌单成功：" << name << "id:" << playlistId;
}

void MainWindow::onCustomPlaylistRemoved(int playlistId, const QString &name)
{
  Q_UNUSED(name)
  if (!m_libraryController->deletePlaylist(playlistId))
  {
    qWarning() << "[DB Error] 删除歌单失败，id:" << playlistId;
  }
  else
  {
    qDebug() << "[DB Info] 删除自定义歌单成功，id:" << playlistId;
  }
}

void MainWindow::refreshLoveList()
{
  if (!m_playlistDetailLove)
    return;

  const QList<Song> favSongs = m_libraryController->favoriteSongs();

  Playlist favPlaylist(Playlist::PL_FAVORITE, QStringLiteral("我喜欢的音乐"), true, QDateTime::currentDateTime());
  m_playlistDetailLove->setPlaylist(favPlaylist);
  m_playlistDetailLove->setSongs(favSongs);

  if (!favSongs.isEmpty())
  {
    QPixmap cover = MetaDataExtractor::extractCover(favSongs.first().filePath());
    if (!cover.isNull())
    {
      m_playlistDetailLove->setPlaylistCover(cover);
    }
  }
}

void MainWindow::refreshHistoryList()
{
  if (!m_playlistDetailHistory)
    return;

  const QList<Song> historySongs = m_libraryController->playHistory();

  Playlist historyPlaylist(Playlist::PL_HISTORY, QStringLiteral("历史播放"), true, QDateTime::currentDateTime());
  historyPlaylist.setDescription(QStringLiteral("最近播放过的歌曲记录"));
  m_playlistDetailHistory->setPlaylist(historyPlaylist);
  m_playlistDetailHistory->setSongs(historySongs);

  if (!historySongs.isEmpty())
  {
    QPixmap cover = MetaDataExtractor::extractCover(historySongs.first().filePath());
    if (!cover.isNull())
    {
      m_playlistDetailHistory->setPlaylistCover(cover);
    }
  }
}

void MainWindow::onPlaylistLoveSongDoubleClicked(int songId, const QString &filePath)
{
  Q_UNUSED(songId)
  if (!filePath.isEmpty())
  {
    m_playController->play(QUrl::fromLocalFile(filePath));
  }
}

void MainWindow::onPlaylistPopupSongDoubleClicked(int index, int songId)
{
  Q_UNUSED(songId)
  QList<QUrl> queue = m_playController->playQueue();
  if (index >= 0 && index < queue.size())
  {
    m_playController->playAt(index);
  }
}

void MainWindow::onPlaylistPopupLoveClicked(int index, int songId)
{
  Q_UNUSED(index)
  m_libraryController->toggleFavorite(songId);
  refreshLoveList();
}

void MainWindow::onPlaylistPopupRemoveClicked(int index, int songId)
{
  Q_UNUSED(songId)
  QList<QUrl> queue = m_playController->playQueue();
  if (index >= 0 && index < queue.size())
  {
    queue.removeAt(index);
    m_playController->setPlayQueue(queue, m_playController->currentIndex(), false);
    refreshPlaylistPopup();
  }
}

void MainWindow::onPlaylistPopupCollectAllClicked()
{
  QList<QUrl> queue = m_playController->playQueue();
  for (const QUrl &url : queue)
  {
    if (!url.isLocalFile())
      continue;
    int songId = m_libraryController->songIdByFilePath(url.toLocalFile());
    if (songId > 0)
    {
      m_libraryController->addSongToPlaylist(Playlist::PL_FAVORITE, songId);
    }
  }
  refreshLoveList();
}

void MainWindow::onPlaylistPopupClearClicked()
{
  m_playController->setPlayQueue(QList<QUrl>(), -1, false);
  refreshPlaylistPopup();
}

void MainWindow::onPlaylistPopupOpenFileDirClicked(const QString &filePath)
{
  QFileInfo fi(filePath);
  if (fi.exists())
  {
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
  }
}

void MainWindow::onPlaylistPopupPlayPauseClicked(int index, int songId)
{
  Q_UNUSED(songId)
  QUrl current = m_playController->currentSource();
  QList<QUrl> queue = m_playController->playQueue();
  if (index >= 0 && index < queue.size())
  {
    if (current == queue[index])
    {
      m_playController->playPause();
    }
    else
    {
      m_playController->playAt(index);
    }
  }
}

void MainWindow::onCreateNewPlaylistFromPopup()
{
  bool ok = false;
  QString name = QInputDialog::getText(this, QStringLiteral("创建歌单"),
                                       QStringLiteral("请输入歌单名称："),
                                       QLineEdit::Normal,
                                       QStringLiteral("新建歌单"), &ok);
  if (!ok || name.trimmed().isEmpty())
    return;

  int playlistId = m_libraryController->createPlaylist(name);
  if (playlistId <= 0)
  {
    QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("创建歌单失败。"));
    return;
  }

  sideBar->addCustomPlaylist(name, playlistId);
  qDebug() << "[UI Info] 从收藏弹窗创建歌单成功：" << name << "id:" << playlistId;

  if (m_currentSongId > 0)
  {
    if (m_libraryController->addSongToPlaylist(playlistId, m_currentSongId))
    {
      QMessageBox::information(this, QStringLiteral("收藏成功"),
                               QStringLiteral("已创建歌单\"%1\"，并收藏当前歌曲。").arg(name));
    }
    else
    {
      QMessageBox::warning(this, QStringLiteral("提示"),
                           QStringLiteral("歌单创建成功，但收藏歌曲失败。"));
    }
  }

  hideAddToPlaylistPopup();
}

void MainWindow::onAddToPlaylist(int playlistId, const QString &playlistName)
{
  int songId = m_addToPlaylistPopup ? m_addToPlaylistPopup->currentSongId() : -1;
  if (songId <= 0)
    songId = m_currentSongId;

  if (songId <= 0)
  {
    QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("当前没有选中的歌曲。"));
    return;
  }

  if (m_libraryController->addSongToPlaylist(playlistId, songId))
  {
    QMessageBox::information(this, QStringLiteral("收藏成功"),
                             QStringLiteral("已收藏到歌单\"%1\"。").arg(playlistName));
  }
  else
  {
    QMessageBox::warning(this, QStringLiteral("提示"),
                         QStringLiteral("收藏失败，该歌曲可能已在歌单中。"));
  }
}

void MainWindow::setupTrayIcon()
{
  m_trayIcon = new QSystemTrayIcon(this);
  m_trayIcon->setIcon(QIcon::fromTheme("media-playback-start"));
  m_trayIcon->setToolTip(QStringLiteral("音乐播放器"));

  m_trayMenu = new QMenu(this);

  m_trayActionShowMain = m_trayMenu->addAction(QStringLiteral("隐藏界面"));
  connect(m_trayActionShowMain, &QAction::triggered, this, &MainWindow::onTrayShowMainWindow);

  m_trayActionToggleLyric = m_trayMenu->addAction(QStringLiteral("关闭桌面歌词"));
  connect(m_trayActionToggleLyric, &QAction::triggered, this, &MainWindow::onTrayToggleLyricVisible);

  m_trayActionToggleLock = m_trayMenu->addAction(QStringLiteral("锁定桌面歌词"));
  connect(m_trayActionToggleLock, &QAction::triggered, this, &MainWindow::onTrayToggleLyricLock);

  m_trayMenu->addSeparator();

  QAction *quitAction = m_trayMenu->addAction(QStringLiteral("关闭"));
  connect(quitAction, &QAction::triggered, this, &MainWindow::onTrayQuitApp);

  connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconActivated);
  m_trayIcon->show();
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
  if (reason == QSystemTrayIcon::DoubleClick)
  {
    onTrayShowMainWindow();
  }
  else if (reason == QSystemTrayIcon::Context)
  {
    QPoint pos = QCursor::pos();
    m_trayMenu->adjustSize();
    QSize menuSize = m_trayMenu->sizeHint();

    QScreen *screen = QGuiApplication::screenAt(pos);
    if (!screen)
      screen = QGuiApplication::primaryScreen();

    QRect screenRect = screen->geometry();

    int x = pos.x();
    int y = pos.y() - menuSize.height();

    if (x + menuSize.width() > screenRect.right())
      x = screenRect.right() - menuSize.width();

    if (y < screenRect.top())
      y = pos.y();

    m_trayMenu->popup(QPoint(x, y));
  }
}

void MainWindow::onTrayShowMainWindow()
{
  if (isVisible())
  {
    hide();
    m_trayActionShowMain->setText(QStringLiteral("显示界面"));
  }
  else
  {
    show();
    raise();
    activateWindow();
    m_trayActionShowMain->setText(QStringLiteral("隐藏界面"));
  }
}

void MainWindow::onTrayToggleLyricVisible()
{
  if (!m_lyricWidget)
    return;

  m_lyricWidget->toggleVisible();
  refreshTrayMenu();
}

void MainWindow::onTrayToggleLyricLock()
{
  if (!m_lyricWidget)
    return;

  if (m_lyricWidget->isLocked())
  {
    m_lyricWidget->unlockLyric();
  }
  else
  {
    QMetaObject::invokeMethod(m_lyricWidget, "on_btnToggleLyricLock_clicked", Qt::QueuedConnection);
  }
  refreshTrayMenu();
}

void MainWindow::onTrayQuitApp()
{
  qApp->quit();
}

void MainWindow::refreshTrayMenu()
{
  if (!m_lyricWidget)
    return;

  if (m_lyricWidget->isVisible())
  {
    m_trayActionToggleLyric->setText(QStringLiteral("关闭桌面歌词"));
  }
  else
  {
    m_trayActionToggleLyric->setText(QStringLiteral("开启桌面歌词"));
  }

  if (m_lyricWidget->isLocked())
  {
    m_trayActionToggleLock->setText(QStringLiteral("解锁桌面歌词"));
  }
  else
  {
    m_trayActionToggleLock->setText(QStringLiteral("锁定桌面歌词"));
  }
}
