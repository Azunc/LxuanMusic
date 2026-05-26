#include "playlistpopup.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QCache>
#include <QDebug>
#include <QElapsedTimer>

#include "../utils/metadataextractor.h"

static QCache<QString, QPixmap> g_playlistPopupCoverCache(200);

static QPixmap cachedPopupCover(const QString &filePath)
{
  const QString key = QFileInfo(filePath).absoluteFilePath().toLower();
  if (QPixmap *cached = g_playlistPopupCoverCache.object(key))
  {
    return *cached;
  }

  QPixmap cover = MetaDataExtractor::extractCover(filePath);
  if (cover.isNull())
  {
    cover = QPixmap(QStringLiteral(":/resource/images/default_music.png"));
  }

  QPixmap scaled = cover.scaled(48, 48, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
  g_playlistPopupCoverCache.insert(key, new QPixmap(scaled));
  return scaled;
}

// ==================== PlaylistPopupItem ====================

PlaylistPopupItem::PlaylistPopupItem(QWidget *parent)
    : QWidget(parent)
{
  setupUI();
  setMouseTracking(true);
}

void PlaylistPopupItem::setupUI()
{
  setFixedHeight(60);
  setCursor(Qt::PointingHandCursor);

  QHBoxLayout *mainLayout = new QHBoxLayout(this);
  mainLayout->setContentsMargins(12, 6, 12, 6);
  mainLayout->setSpacing(10);

  // 封面
  m_coverLabel = new QLabel(this);
  m_coverLabel->setFixedSize(48, 48);
  m_coverLabel->setScaledContents(true);
  m_coverLabel->setStyleSheet("border-radius: 4px;");
  mainLayout->addWidget(m_coverLabel);

  // 歌曲信息（标题 + 歌手）
  QWidget *infoWidget = new QWidget(this);
  QVBoxLayout *infoLayout = new QVBoxLayout(infoWidget);
  infoLayout->setSpacing(2);
  infoLayout->setContentsMargins(0, 0, 0, 0);

  m_titleLabel = new QLabel(infoWidget);
  m_titleLabel->setStyleSheet("color: #333; font-size: 14px; font-weight: 500;");
  m_titleLabel->setMaximumWidth(150);

  m_artistLabel = new QLabel(infoWidget);
  m_artistLabel->setStyleSheet("color: #999; font-size: 12px;");
  m_artistLabel->setMaximumWidth(150);

  infoLayout->addWidget(m_titleLabel);
  infoLayout->addWidget(m_artistLabel);
  mainLayout->addWidget(infoWidget, 1);

  // 悬浮操作按钮
  m_hoverActions = new QWidget(this);
  QHBoxLayout *actionLayout = new QHBoxLayout(m_hoverActions);
  actionLayout->setContentsMargins(0, 0, 0, 0);
  actionLayout->setSpacing(8);

  m_loveBtn = new QPushButton(QStringLiteral("♥"), m_hoverActions);
  m_loveBtn->setFixedSize(24, 24);
  m_loveBtn->setStyleSheet(
      "QPushButton { border: none; background: transparent; color: #999; font-size: 14px; }"
      "QPushButton:hover { color: #c62f2f; }");

  m_addBtn = new QPushButton(QStringLiteral("⊕"), m_hoverActions);
  m_addBtn->setFixedSize(24, 24);
  m_addBtn->setStyleSheet(
      "QPushButton { border: none; background: transparent; color: #999; font-size: 14px; }"
      "QPushButton:hover { color: #333; }");

  m_moreBtn = new QPushButton(QStringLiteral("⋯"), m_hoverActions);
  m_moreBtn->setFixedSize(24, 24);
  m_moreBtn->setStyleSheet(
      "QPushButton { border: none; background: transparent; color: #999; font-size: 14px; }"
      "QPushButton:hover { color: #333; }");

  actionLayout->addWidget(m_loveBtn);
  actionLayout->addWidget(m_addBtn);
  actionLayout->addWidget(m_moreBtn);
  m_hoverActions->setVisible(false);

  mainLayout->addWidget(m_hoverActions);

  // 时长
  m_durationLabel = new QLabel(this);
  m_durationLabel->setFixedWidth(50);
  m_durationLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  m_durationLabel->setStyleSheet("color: #999; font-size: 13px;");
  mainLayout->addWidget(m_durationLabel);

  updateStyle();

  // 连接按钮信号
  connect(m_loveBtn, &QPushButton::clicked, this, [this]()
          { emit loveClicked(m_songId); });
  connect(m_addBtn, &QPushButton::clicked, this, [this]()
          { emit addToPlaylistClicked(m_songId); });
  connect(m_moreBtn, &QPushButton::clicked, this, [this]()
          { showMoreMenu(); });
}

void PlaylistPopupItem::setSong(const Song &song)
{
  m_songId = song.songId();
  m_filePath = song.filePath();

  QString displayTitle = song.title().isEmpty() ? QFileInfo(song.filePath()).fileName() : song.title();
  m_titleLabel->setText(displayTitle);
  m_artistLabel->setText(song.artist().isEmpty() ? QStringLiteral("未知歌手") : song.artist());

  qint64 totalSecs = song.duration() / 1000;
  int mins = static_cast<int>(totalSecs / 60);
  int secs = static_cast<int>(totalSecs % 60);
  m_durationLabel->setText(QString::asprintf("%d:%02d", mins, secs));

  // 封面：使用缓存，避免每次打开播放列表都重新解析音频封面
  m_coverLabel->setPixmap(cachedPopupCover(song.filePath()));
}

void PlaylistPopupItem::setSelected(bool selected)
{
  m_selected = selected;
  updateStyle();
}

void PlaylistPopupItem::setPlaying(bool playing)
{
  m_playing = playing;
  // 播放中的歌曲标题变红
  if (m_playing)
  {
    m_titleLabel->setStyleSheet("color: #ff3a3a; font-size: 14px; font-weight: 500;");
  }
  else
  {
    m_titleLabel->setStyleSheet("color: #333; font-size: 14px; font-weight: 500;");
  }
}

int PlaylistPopupItem::songId() const
{
  return m_songId;
}

QString PlaylistPopupItem::filePath() const
{
  return m_filePath;
}

void PlaylistPopupItem::updateStyle()
{
  if (m_selected)
  {
    setStyleSheet("PlaylistPopupItem { background: #f0f0f5; border-radius: 6px; }");
  }
  else if (m_hovered)
  {
    setStyleSheet("PlaylistPopupItem { background: #f7f7f9; border-radius: 6px; }");
  }
  else
  {
    setStyleSheet("PlaylistPopupItem { background: transparent; }");
  }
}

void PlaylistPopupItem::enterEvent(QEnterEvent *event)
{
  m_hovered = true;
  m_hoverActions->setVisible(true);
  updateStyle();
  QWidget::enterEvent(event);
}

void PlaylistPopupItem::leaveEvent(QEvent *event)
{
  m_hovered = false;
  m_hoverActions->setVisible(false);
  updateStyle();
  QWidget::leaveEvent(event);
}

void PlaylistPopupItem::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton)
  {
    emit clicked(m_songId);
  }
  QWidget::mousePressEvent(event);
}

void PlaylistPopupItem::mouseDoubleClickEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton)
  {
    emit doubleClicked(m_songId);
  }
  QWidget::mouseDoubleClickEvent(event);
}

void PlaylistPopupItem::contextMenuEvent(QContextMenuEvent *event)
{
  QMenu menu(this);

  QAction *playAction = menu.addAction(m_playing ? QStringLiteral("暂停") : QStringLiteral("播放"));
  QAction *loveAction = menu.addAction(QStringLiteral("收藏"));
  QAction *openDirAction = menu.addAction(QStringLiteral("打开文件所在目录"));
  menu.addSeparator();
  QAction *removeAction = menu.addAction(QStringLiteral("移出播放列表"));

  QAction *selected = menu.exec(mapToGlobal(event->pos()));
  if (selected == playAction)
  {
    emit playPauseRequested(m_songId);
  }
  else if (selected == loveAction)
  {
    emit loveClicked(m_songId);
  }
  else if (selected == openDirAction)
  {
    emit openFileDirRequested(m_filePath);
  }
  else if (selected == removeAction)
  {
    emit removeClicked(m_songId);
  }
}

void PlaylistPopupItem::showMoreMenu()
{
  QMenu menu(this);

  QAction *playAction = menu.addAction(m_playing ? QStringLiteral("暂停") : QStringLiteral("播放"));
  QAction *loveAction = menu.addAction(QStringLiteral("收藏"));
  QAction *openDirAction = menu.addAction(QStringLiteral("打开文件所在目录"));
  menu.addSeparator();
  QAction *removeAction = menu.addAction(QStringLiteral("移出播放列表"));

  QAction *selected = menu.exec(QCursor::pos());
  if (selected == playAction)
  {
    emit playPauseRequested(m_songId);
  }
  else if (selected == loveAction)
  {
    emit loveClicked(m_songId);
  }
  else if (selected == openDirAction)
  {
    emit openFileDirRequested(m_filePath);
  }
  else if (selected == removeAction)
  {
    emit removeClicked(m_songId);
  }
}

// ==================== PlaylistPopup ====================

PlaylistPopup::PlaylistPopup(QWidget *parent)
    : QWidget(parent)
{
  setupUI();
}

void PlaylistPopup::setupUI()
{
  // 作为 MainWindow 的子窗口，不使用 Popup/Tool 等独立窗口标志
  // 避免 setWindowFlags 导致子窗口重创建或显示异常
  setFixedSize(350, 550);
  setStyleSheet("PlaylistPopup { background-color: #ffffff; border-radius: 8px; }");

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // 顶部标题栏
  QWidget *headerWidget = new QWidget(this);
  headerWidget->setFixedHeight(60);
  headerWidget->setStyleSheet("background: #ffffff; border-bottom: 1px solid #eee;");
  QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
  headerLayout->setContentsMargins(16, 8, 16, 8);
  headerLayout->setSpacing(4);

  // 标题
  m_titleLabel = new QLabel(QStringLiteral("播放列表"), headerWidget);
  m_titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #333;");
  headerLayout->addWidget(m_titleLabel);

  // 歌曲数
  m_countLabel = new QLabel(QStringLiteral("(0)"), headerWidget);
  m_countLabel->setStyleSheet("font-size: 14px; color: #999;");
  headerLayout->addWidget(m_countLabel);

  headerLayout->addStretch();

  // 收藏全部
  m_collectAllBtn = new QPushButton(QStringLiteral("收藏全部"), headerWidget);
  m_collectAllBtn->setStyleSheet(
      "QPushButton { border: none; background: transparent; color: #666; font-size: 13px; }"
      "QPushButton:hover { color: #333; }");
  headerLayout->addWidget(m_collectAllBtn);

  // 清空
  m_clearBtn = new QPushButton(QStringLiteral("清空"), headerWidget);
  m_clearBtn->setStyleSheet(
      "QPushButton { border: none; background: transparent; color: #666; font-size: 13px; }"
      "QPushButton:hover { color: #c62f2f; }");
  headerLayout->addWidget(m_clearBtn);

  mainLayout->addWidget(headerWidget);

  // 歌曲列表
  m_listWidget = new QListWidget(this);
  m_listWidget->setFrameShape(QFrame::NoFrame);
  m_listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_listWidget->setSpacing(2);
  m_listWidget->setStyleSheet(
      "QListWidget { border: none; background: #ffffff; outline: none; }"
      "QListWidget::item { border: none; padding: 0px; }");
  mainLayout->addWidget(m_listWidget, 1);

  // 连接头部按钮
  connect(m_collectAllBtn, &QPushButton::clicked, this, &PlaylistPopup::collectAllClicked);
  connect(m_clearBtn, &QPushButton::clicked, this, &PlaylistPopup::clearClicked);
}

void PlaylistPopup::setSongs(const QList<Song> &songs)
{
  m_songs = songs;
  refreshList();
}

QList<Song> PlaylistPopup::songs() const
{
  return m_songs;
}

void PlaylistPopup::setCurrentPlayingId(int songId)
{
  if (m_currentPlayingId == songId)
    return;

  m_currentPlayingId = songId;
  for (int i = 0; i < m_listWidget->count(); ++i)
  {
    QListWidgetItem *listItem = m_listWidget->item(i);
    PlaylistPopupItem *widget = qobject_cast<PlaylistPopupItem *>(m_listWidget->itemWidget(listItem));
    if (widget)
    {
      widget->setPlaying(widget->songId() == m_currentPlayingId);
    }
  }
}

void PlaylistPopup::setSelectedIndex(int index)
{
  m_selectedIndex = index;
  for (int i = 0; i < m_listWidget->count(); ++i)
  {
    QListWidgetItem *listItem = m_listWidget->item(i);
    PlaylistPopupItem *widget = qobject_cast<PlaylistPopupItem *>(m_listWidget->itemWidget(listItem));
    if (widget)
    {
      widget->setSelected(i == index);
    }
  }
}

void PlaylistPopup::refreshList()
{
  QElapsedTimer timer;
  timer.start();

  m_listWidget->setUpdatesEnabled(false);
  m_listWidget->clear();
  m_countLabel->setText(QStringLiteral("(%1)").arg(m_songs.size()));

  for (int i = 0; i < m_songs.size(); ++i)
  {
    const Song &song = m_songs[i];
    PlaylistPopupItem *itemWidget = new PlaylistPopupItem(m_listWidget);
    itemWidget->setSong(song);
    itemWidget->setPlaying(song.songId() == m_currentPlayingId);
    itemWidget->setSelected(i == m_selectedIndex);

    connect(itemWidget, &PlaylistPopupItem::clicked, this, [this, i](int songId)
            {
      setSelectedIndex(i);
      emit songClicked(i, songId); });
    connect(itemWidget, &PlaylistPopupItem::doubleClicked, this, [this, i](int songId)
            { emit songDoubleClicked(i, songId); });
    connect(itemWidget, &PlaylistPopupItem::loveClicked, this, [this, i](int songId)
            { emit loveClicked(i, songId); });
    connect(itemWidget, &PlaylistPopupItem::addToPlaylistClicked, this, [this, i](int songId)
            { emit addToPlaylistClicked(i, songId); });
    connect(itemWidget, &PlaylistPopupItem::removeClicked, this, [this, i](int songId)
            { emit removeClicked(i, songId); });
    connect(itemWidget, &PlaylistPopupItem::openFileDirRequested, this, [this](const QString &filePath)
            { emit openFileDirClicked(filePath); });
    connect(itemWidget, &PlaylistPopupItem::playPauseRequested, this, [this, i](int songId)
            { emit playPauseClicked(i, songId); });

    QListWidgetItem *item = new QListWidgetItem(m_listWidget);
    item->setSizeHint(QSize(m_listWidget->width(), 60));
    item->setData(Qt::UserRole, song.songId());
    m_listWidget->addItem(item);
    m_listWidget->setItemWidget(item, itemWidget);
  }

  m_listWidget->setUpdatesEnabled(true);
  qDebug() << "[PlaylistPopup] 刷新播放列表UI耗时:" << timer.elapsed()
           << "ms, count=" << m_songs.size();
}

void PlaylistPopup::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event)
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setBrush(Qt::white);
  p.setPen(QPen(QColor(220, 220, 220), 1));
  p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 8, 8);
}

void PlaylistPopup::showEvent(QShowEvent *event)
{
  QWidget::showEvent(event);
}

void PlaylistPopup::hideEvent(QHideEvent *event)
{
  QWidget::hideEvent(event);
}

bool PlaylistPopup::eventFilter(QObject *watched, QEvent *event)
{
  return QWidget::eventFilter(watched, event);
}
