#include "addtoplaylistpopup.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QListWidgetItem>
#include <QDebug>
#include <functional>

#include "controllers/librarycontroller.h"
#include "utils/metadataextractor.h"

// ==================== 内部歌单项 Widget ====================
class PlaylistSelectItem : public QWidget
{
public:
  explicit PlaylistSelectItem(QWidget *parent = nullptr)
      : QWidget(parent)
  {
    setAttribute(Qt::WA_StyledBackground, true);
    setCursor(Qt::PointingHandCursor);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 8, 16, 8);
    layout->setSpacing(12);

    // 封面/图标
    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(48, 48);
    m_iconLabel->setScaledContents(false);
    layout->addWidget(m_iconLabel);

    // 文字信息
    QVBoxLayout *textLayout = new QVBoxLayout();
    textLayout->setSpacing(4);
    textLayout->setContentsMargins(0, 0, 0, 0);

    m_nameLabel = new QLabel(this);
    m_nameLabel->setStyleSheet("color: #333333; font-size: 14px;");
    textLayout->addWidget(m_nameLabel);

    m_countLabel = new QLabel(this);
    m_countLabel->setStyleSheet("color: #999999; font-size: 12px;");
    textLayout->addWidget(m_countLabel);

    layout->addLayout(textLayout, 1);
    setFixedHeight(64);
    updateStyle(false);
  }

  void setCreateNewMode()
  {
    m_isCreateNew = true;

    // 绘制灰色圆角背景 + 加号
    QPixmap plusPixmap(48, 48);
    plusPixmap.fill(Qt::transparent);
    QPainter p(&plusPixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(240, 240, 240));
    p.drawRoundedRect(0, 0, 48, 48, 6, 6);
    p.setPen(QPen(QColor(150, 150, 150), 2));
    p.drawLine(16, 24, 32, 24);
    p.drawLine(24, 16, 24, 32);
    p.end();
    m_iconLabel->setPixmap(plusPixmap);

    m_nameLabel->setText(QStringLiteral("创建新歌单"));
    m_countLabel->setText(QStringLiteral(""));
  }

  void setPlaylistInfo(const PlaylistInfo &info)
  {
    m_isCreateNew = false;

    QPixmap cover;
    if (!info.cover.isNull())
      cover = info.cover;
    else
      cover = QPixmap(QStringLiteral(":/resource/images/default_music.png"));

    // 绘制圆角封面
    QPixmap rounded(48, 48);
    rounded.fill(Qt::transparent);
    QPainter p(&rounded);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(0, 0, 48, 48, 6, 6);
    p.setClipPath(path);
    p.drawPixmap(0, 0, 48, 48, cover.scaled(48, 48, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    p.end();
    m_iconLabel->setPixmap(rounded);

    m_nameLabel->setText(info.name);
    m_countLabel->setText(QStringLiteral("%1首").arg(info.songCount));
  }

  std::function<void()> onClicked;

protected:
  void mousePressEvent(QMouseEvent *event) override
  {
    if (event->button() == Qt::LeftButton && onClicked)
    {
      onClicked();
    }
    QWidget::mousePressEvent(event);
  }

  void enterEvent(QEnterEvent *event) override
  {
    updateStyle(true);
    QWidget::enterEvent(event);
  }

  void leaveEvent(QEvent *event) override
  {
    updateStyle(false);
    QWidget::leaveEvent(event);
  }

private:
  void updateStyle(bool hovered)
  {
    if (hovered)
    {
      setStyleSheet("background: #f5f5f7; border-radius: 6px;");
    }
    else
    {
      setStyleSheet("background: transparent;");
    }
  }

  QLabel *m_iconLabel = nullptr;
  QLabel *m_nameLabel = nullptr;
  QLabel *m_countLabel = nullptr;
  bool m_isCreateNew = false;
};

// ==================== AddToPlaylistPopup ====================

AddToPlaylistPopup::AddToPlaylistPopup(QWidget *parent)
    : QWidget(parent)
{
  setupUI();
}

void AddToPlaylistPopup::setupUI()
{
  setFixedSize(400, 500);
  setStyleSheet("AddToPlaylistPopup { background-color: #ffffff; border-radius: 8px; }");

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // 标题栏
  QWidget *headerWidget = new QWidget(this);
  headerWidget->setFixedHeight(56);
  headerWidget->setStyleSheet("background: #ffffff; border-bottom: 1px solid #eeeeee;");
  QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
  headerLayout->setContentsMargins(16, 0, 16, 0);
  headerLayout->setSpacing(0);

  m_titleLabel = new QLabel(QStringLiteral("收藏到歌单"), headerWidget);
  m_titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #333333;");
  m_titleLabel->setAlignment(Qt::AlignCenter);
  headerLayout->addStretch();
  headerLayout->addWidget(m_titleLabel);
  headerLayout->addStretch();

  m_closeBtn = new QPushButton(QStringLiteral("×"), headerWidget);
  m_closeBtn->setFixedSize(28, 28);
  m_closeBtn->setStyleSheet(
      "QPushButton { border: none; background: transparent; color: #999999; font-size: 20px; }"
      "QPushButton:hover { color: #333333; }");
  connect(m_closeBtn, &QPushButton::clicked, this, [this]()
          {
        hide();
        emit popupClosed(); });
  headerLayout->addWidget(m_closeBtn);

  mainLayout->addWidget(headerWidget);

  // 列表
  m_listWidget = new QListWidget(this);
  m_listWidget->setFrameShape(QFrame::NoFrame);
  m_listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_listWidget->setSpacing(4);
  m_listWidget->setStyleSheet(
      "QListWidget { border: none; background: #ffffff; outline: none; padding: 8px; }"
      "QListWidget::item { border: none; padding: 0px; }");
  mainLayout->addWidget(m_listWidget, 1);
}

void AddToPlaylistPopup::setLibraryController(LibraryController *ctrl)
{
  m_libraryController = ctrl;
}

void AddToPlaylistPopup::refreshData()
{
  m_playlistInfos.clear();

  if (!m_libraryController)
  {
    qWarning() << "[AddToPlaylistPopup] LibraryController 未设置，无法加载歌单";
    refreshList();
    return;
  }

  // 查询所有歌单（排除本地音乐/播放历史，保留"我喜欢的音乐"）
  const QList<Playlist> playlists = m_libraryController->allPlaylists();
  for (const Playlist &pl : playlists)
  {
    // 排除本地音乐(0)和播放历史(2)，保留"我喜欢的音乐"(1)和自定义歌单
    if (pl.playlistId() == Playlist::PL_LOCAL || pl.playlistId() == Playlist::PL_HISTORY)
      continue;

    PlaylistInfo info;
    info.playlistId = pl.playlistId();
    info.name = pl.name();

    // 查询歌曲数量
    const QList<Song> songs = m_libraryController->playlistSongs(pl.playlistId());
    info.songCount = songs.size();

    // 取第一首歌的封面作为歌单封面
    if (!songs.isEmpty())
    {
      info.cover = MetaDataExtractor::extractCover(songs.first().filePath());
    }

    m_playlistInfos.append(info);
  }

  refreshList();
}

void AddToPlaylistPopup::setCurrentSongId(int songId)
{
  m_currentSongId = songId;
}

int AddToPlaylistPopup::currentSongId() const
{
  return m_currentSongId;
}

void AddToPlaylistPopup::refreshList()
{
  m_listWidget->clear();

  // 第一项：创建新歌单
  QListWidgetItem *createItem = new QListWidgetItem(m_listWidget);
  createItem->setSizeHint(QSize(m_listWidget->width() - 16, 64));
  PlaylistSelectItem *createWidget = new PlaylistSelectItem(m_listWidget);
  createWidget->setCreateNewMode();
  createWidget->onClicked = [this]()
  {
    emit createNewPlaylistRequested();
  };
  m_listWidget->addItem(createItem);
  m_listWidget->setItemWidget(createItem, createWidget);

  // 歌单项
  for (int i = 0; i < m_playlistInfos.size(); ++i)
  {
    const PlaylistInfo &info = m_playlistInfos[i];
    QListWidgetItem *item = new QListWidgetItem(m_listWidget);
    item->setSizeHint(QSize(m_listWidget->width() - 16, 64));

    PlaylistSelectItem *widget = new PlaylistSelectItem(m_listWidget);
    widget->setPlaylistInfo(info);

    widget->onClicked = [this, info]()
    {
      emit addToPlaylistRequested(info.playlistId, info.name);
      hide();
    };

    m_listWidget->addItem(item);
    m_listWidget->setItemWidget(item, widget);
  }

  // 如果没有歌单，显示提示
  if (m_playlistInfos.isEmpty())
  {
    QLabel *emptyLabel = new QLabel(QStringLiteral("暂无自定义歌单，请先创建"), m_listWidget);
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setStyleSheet("color: #999999; font-size: 13px; padding: 20px;");
    QListWidgetItem *emptyItem = new QListWidgetItem(m_listWidget);
    emptyItem->setSizeHint(QSize(m_listWidget->width() - 16, 80));
    m_listWidget->addItem(emptyItem);
    m_listWidget->setItemWidget(emptyItem, emptyLabel);
  }
}

void AddToPlaylistPopup::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event)
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setBrush(Qt::white);
  p.setPen(QPen(QColor(220, 220, 220), 1));
  p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 8, 8);
}

void AddToPlaylistPopup::showEvent(QShowEvent *event)
{
  refreshData();
  QWidget::showEvent(event);
}
