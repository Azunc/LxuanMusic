#include "playlistsongitem.h"
#include "../entity/song.h"
#include "../models/librarymodel.h"
#include "../controllers/playcontroller.h"
#include "../utils/metadataextractor.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QFileInfo>
#include <QMouseEvent>
#include <QTimer>

#include <QCache>

// 全局封面缓存：最大缓存 100 张封面，避免反复解析同一文件
static QCache<QString, QPixmap> g_coverCache(100);

static QPixmap roundedCover(const QPixmap &source, int size, int radius)
{
  if (source.isNull())
    return QPixmap();

  QPixmap target(size, size);
  target.fill(Qt::transparent);

  QPainter painter(&target);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);

  QPainterPath path;
  path.addRoundedRect(target.rect(), radius, radius);
  painter.setClipPath(path);

  QPixmap scaled = source.scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
  int x = (size - scaled.width()) / 2;
  int y = (size - scaled.height()) / 2;
  painter.drawPixmap(x, y, scaled);

  return target;
}

static QPixmap getMusicCover(const QString &filePath, int size)
{
  // 1. 先查缓存
  QPixmap *cached = g_coverCache.object(filePath);
  if (cached)
    return *cached;

  // 2. 使用 MetaDataExtractor 提取封面
  QPixmap cover = MetaDataExtractor::extractCover(filePath);
  if (!cover.isNull())
  {
    QPixmap rounded = roundedCover(cover, size, 4);
    g_coverCache.insert(filePath, new QPixmap(rounded));
    return rounded;
  }

  // 3. 最终 fallback 到默认图
  QPixmap defaultCover(":/resource/images/default_music.png");
  if (!defaultCover.isNull())
  {
    QPixmap rounded = roundedCover(defaultCover, size, 4);
    g_coverCache.insert(filePath, new QPixmap(rounded));
    return rounded;
  }

  return QPixmap();
}

PlaylistSongItem::PlaylistSongItem(QWidget *parent)
    : QWidget(parent)
{
  setupUI();
  setMouseTracking(true);
}

void PlaylistSongItem::setupUI()
{
  setFixedHeight(55);
  setCursor(Qt::PointingHandCursor);

  QHBoxLayout *mainLayout = new QHBoxLayout(this);
  mainLayout->setContentsMargins(16, 4, 16, 4);
  mainLayout->setSpacing(12);

  // 序号 / 播放图标
  m_numberLabel = new QLabel(this);
  m_numberLabel->setFixedWidth(36);
  m_numberLabel->setAlignment(Qt::AlignCenter);
  m_numberLabel->setStyleSheet("color: #999; font-size: 13px;");
  mainLayout->addWidget(m_numberLabel);

  // 封面
  m_coverLabel = new QLabel(this);
  m_coverLabel->setFixedSize(44, 44);
  m_coverLabel->setScaledContents(true);
  m_coverLabel->setStyleSheet("border-radius: 4px;");
  mainLayout->addWidget(m_coverLabel);

  // 歌曲信息容器（标题行 + 歌手行）
  QWidget *infoWidget = new QWidget(this);
  QVBoxLayout *infoLayout = new QVBoxLayout(infoWidget);
  infoLayout->setSpacing(2);
  infoLayout->setContentsMargins(0, 0, 0, 0);

  // 标题行：标题文本 + 悬浮按钮（并排，按钮出现时挤占标题空间）
  QWidget *titleRow = new QWidget(infoWidget);
  QHBoxLayout *titleLayout = new QHBoxLayout(titleRow);
  titleLayout->setContentsMargins(0, 0, 0, 0);
  titleLayout->setSpacing(4);

  m_titleLabel = new QLabel(titleRow);
  m_titleLabel->setStyleSheet("color: #333; font-size: 14px; font-weight: 500;");

  m_hoverActions = new QWidget(titleRow);
  QHBoxLayout *actionLayout = new QHBoxLayout(m_hoverActions);
  actionLayout->setContentsMargins(0, 0, 0, 0);
  actionLayout->setSpacing(8);

  m_loveBtn = new QPushButton("♥", m_hoverActions);
  m_loveBtn->setFixedSize(24, 24);
  m_loveBtn->setStyleSheet("QPushButton { border: none; background: transparent; color: #999; font-size: 14px; }"
                           "QPushButton:hover { color: #c62f2f; }");
  connect(m_loveBtn, &QPushButton::clicked, this, [this]()
          { emit loveClicked(m_songId); });

  m_addBtn = new QPushButton("⊕", m_hoverActions);
  m_addBtn->setFixedSize(24, 24);
  m_addBtn->setStyleSheet("QPushButton { border: none; background: transparent; color: #999; font-size: 14px; }"
                          "QPushButton:hover { color: #333; }");
  connect(m_addBtn, &QPushButton::clicked, this, [this]()
          { emit addToPlaylistClicked(m_songId); });

  m_moreBtn = new QPushButton("⋯", m_hoverActions);
  m_moreBtn->setFixedSize(24, 24);
  m_moreBtn->setStyleSheet("QPushButton { border: none; background: transparent; color: #999; font-size: 14px; }"
                           "QPushButton:hover { color: #333; }");
  connect(m_moreBtn, &QPushButton::clicked, this, [this]()
          { emit moreClicked(m_songId); });

  actionLayout->addWidget(m_loveBtn);
  actionLayout->addWidget(m_addBtn);
  actionLayout->addWidget(m_moreBtn);
  m_hoverActions->setVisible(false);

  titleLayout->addWidget(m_titleLabel, 1);
  titleLayout->addWidget(m_hoverActions);

  m_artistLabel = new QLabel(infoWidget);
  m_artistLabel->setStyleSheet("color: #999; font-size: 12px;");

  infoLayout->addWidget(titleRow);
  infoLayout->addWidget(m_artistLabel);
  mainLayout->addWidget(infoWidget, 1);

  // 专辑
  m_albumLabel = new QLabel(this);
  m_albumLabel->setFixedWidth(210);
  m_albumLabel->setStyleSheet("color: #666; font-size: 13px;");
  mainLayout->addWidget(m_albumLabel);

  // 时长
  m_durationLabel = new QLabel(this);
  m_durationLabel->setFixedWidth(70);
  m_durationLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  m_durationLabel->setStyleSheet("color: #999; font-size: 13px;");
  mainLayout->addWidget(m_durationLabel);

  updateStyle();
}

void PlaylistSongItem::setSongIndex(int index)
{
  m_index = index;
  if (!m_selected)
  {
    m_numberLabel->setText(QString::asprintf("%02d", index));
  }
}

void PlaylistSongItem::setSong(const Song &song)
{
  m_songId = song.songId();
  m_filePath = song.filePath();

  QString displayTitle = song.title().isEmpty() ? QFileInfo(song.filePath()).fileName() : song.title();
  m_titleLabel->setText(displayTitle);
  m_artistLabel->setText(song.artist().isEmpty() ? "未知歌手" : song.artist());
  m_albumLabel->setText(song.album().isEmpty() ? "未知专辑" : song.album());

  // 格式化时长
  qint64 totalSecs = song.duration() / 1000;
  int mins = static_cast<int>(totalSecs / 60);
  int secs = static_cast<int>(totalSecs % 60);
  m_durationLabel->setText(QString::asprintf("%d:%02d", mins, secs));

  // 封面（已裁剪圆角，直接设置）
  QPixmap cover = getMusicCover(song.filePath(), 44);
  if (!cover.isNull())
  {
    m_coverLabel->setPixmap(cover);
  }
  else
  {
    m_coverLabel->setPixmap(QPixmap(":/resource/images/default_music.png").scaled(44, 44, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
  }
}

void PlaylistSongItem::setSelected(bool selected)
{
  m_selected = selected;
  updateStyle();
}

int PlaylistSongItem::songId() const
{
  return m_songId;
}

QString PlaylistSongItem::filePath() const
{
  return m_filePath;
}

QString PlaylistSongItem::songTitle() const
{
  return m_titleLabel ? m_titleLabel->text() : QString();
}

QString PlaylistSongItem::songArtist() const
{
  return m_artistLabel ? m_artistLabel->text() : QString();
}

QString PlaylistSongItem::songAlbum() const
{
  return m_albumLabel ? m_albumLabel->text() : QString();
}

void PlaylistSongItem::setCoverVisible(bool visible)
{
  if (m_coverLabel)
    m_coverLabel->setVisible(visible);
}

void PlaylistSongItem::mousePressEvent(QMouseEvent *event)
{
  QWidget::mousePressEvent(event);
  if (event->button() == Qt::LeftButton)
  {
    emit clicked(m_songId);
  }
}

void PlaylistSongItem::mouseDoubleClickEvent(QMouseEvent *event)
{
  QWidget::mouseDoubleClickEvent(event);
  if (event->button() == Qt::LeftButton)
  {
    emit doubleClicked(m_songId);
  }
}

void PlaylistSongItem::updateStyle()
{
  if (m_selected)
  {
    setStyleSheet("PlaylistSongItem { background: #f5f5f5; border-radius: 6px; }");
    m_numberLabel->setText("▶");
    m_numberLabel->setStyleSheet("color: #c62f2f; font-size: 12px;");
    m_hoverActions->setVisible(true);
  }
  else if (m_hovered)
  {
    setStyleSheet("PlaylistSongItem { background: #fafafa; border-radius: 6px; }");
    m_numberLabel->setText("▶");
    m_numberLabel->setStyleSheet("color: #c62f2f; font-size: 12px;");
    m_hoverActions->setVisible(true);
  }
  else
  {
    setStyleSheet("PlaylistSongItem { background: transparent; }");
    m_numberLabel->setText(QString::asprintf("%02d", m_index));
    m_numberLabel->setStyleSheet("color: #999; font-size: 13px;");
    m_hoverActions->setVisible(false);
  }
}

void PlaylistSongItem::enterEvent(QEnterEvent *event)
{
  Q_UNUSED(event)
  m_hovered = true;
  updateStyle();
}

void PlaylistSongItem::leaveEvent(QEvent *event)
{
  Q_UNUSED(event)
  m_hovered = false;
  updateStyle();
}
