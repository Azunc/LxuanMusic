#include "playlistdetailwidget.h"
#include "playlistsongitem.h"
#include "../entity/playlist.h"
#include "../entity/song.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QFileInfo>
#include <QPixmap>
#include <QPainter>
#include <QWheelEvent>
#include <QScrollBar>
#include <QEvent>
#include <QPainterPath>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTextEdit>

PlaylistDetailWidget::PlaylistDetailWidget(QWidget *parent)
    : QWidget(parent)
{
  setupUI();
}

void PlaylistDetailWidget::setupUI()
{
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  setupHeader();
  setupTableHeader();
  setupSongList();

  // 把 header 和 tableHeader 作为 listWidget 的前两项，
  // 这样整个区域统一滚动，header 会随歌曲一起滚走
  QListWidgetItem *headerItem = new QListWidgetItem(m_listWidget);
  headerItem->setSizeHint(QSize(865, 195));
  headerItem->setFlags(Qt::NoItemFlags); // 不可选中、不获取焦点
  m_listWidget->addItem(headerItem);
  m_listWidget->setItemWidget(headerItem, m_headerWidget);

  QListWidgetItem *thItem = new QListWidgetItem(m_listWidget);
  thItem->setSizeHint(QSize(865, 36));
  thItem->setFlags(Qt::NoItemFlags);
  m_listWidget->addItem(thItem);
  m_listWidget->setItemWidget(thItem, m_tableHeader);

  mainLayout->addWidget(m_listWidget, 1);
}

QPixmap PlaylistDetailWidget::roundedPixmap(const QPixmap &source, int radius, int targetSize) const
{
  if (source.isNull())
    return QPixmap();

  const int size = targetSize > 0 ? targetSize : qMax(source.width(), source.height());
  QPixmap target(size, size);
  target.fill(Qt::transparent);

  QPainter painter(&target);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);

  QPainterPath path;
  path.addRoundedRect(target.rect(), radius, radius);
  painter.setClipPath(path);

  // 按原始比例缩放并居中裁剪为正方形
  QPixmap scaled = source.scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
  int x = (size - scaled.width()) / 2;
  int y = (size - scaled.height()) / 2;
  painter.drawPixmap(x, y, scaled);

  return target;
}

void PlaylistDetailWidget::setupHeader()
{
  m_headerWidget = new QWidget(this);
  m_headerWidget->setFixedHeight(195);
  QHBoxLayout *headerLayout = new QHBoxLayout(m_headerWidget);
  headerLayout->setContentsMargins(24, 16, 24, 16);
  headerLayout->setSpacing(20);

  // 左侧封面
  m_coverLabel = new QLabel(m_headerWidget);
  m_coverLabel->setFixedSize(160, 160);
  m_coverLabel->setScaledContents(false);
  QPixmap defaultCover(":/resource/images/default_music.png");
  if (!defaultCover.isNull())
  {
    m_coverLabel->setPixmap(roundedPixmap(defaultCover, 12, 160));
  }
  headerLayout->addWidget(m_coverLabel);

  // 右侧信息区
  QVBoxLayout *infoLayout = new QVBoxLayout();
  infoLayout->setSpacing(8);
  infoLayout->setContentsMargins(0, 0, 0, 0);

  // 歌单名称 + 编辑图标
  QHBoxLayout *nameLayout = new QHBoxLayout();
  m_nameLabel = new QLabel(m_headerWidget);
  m_nameLabel->setStyleSheet("color: #333; font-size: 22px; font-weight: bold;");
  m_editIcon = new QLabel("✎", m_headerWidget);
  m_editIcon->setStyleSheet("color: #999; font-size: 14px; cursor: pointer;");
  m_editIcon->setToolTip(QStringLiteral("编辑歌单信息"));
  m_editIcon->installEventFilter(this);
  nameLayout->addWidget(m_nameLabel);
  nameLayout->addWidget(m_editIcon);
  nameLayout->addStretch();
  infoLayout->addLayout(nameLayout);

  // 简介
  m_descLabel = new QLabel(m_headerWidget);
  m_descLabel->setStyleSheet("color: #666; font-size: 13px;");
  m_descLabel->setWordWrap(true);
  infoLayout->addWidget(m_descLabel);

  // 创建者信息行
  QHBoxLayout *creatorLayout = new QHBoxLayout();
  creatorLayout->setSpacing(8);

  m_creatorAvatar = new QLabel(m_headerWidget);
  m_creatorAvatar->setFixedSize(28, 28);
  m_creatorAvatar->setScaledContents(true);
  m_creatorAvatar->setStyleSheet("border-radius: 14px;");
  creatorLayout->addWidget(m_creatorAvatar);

  m_creatorName = new QLabel(m_headerWidget);
  m_creatorName->setStyleSheet("color: #507daf; font-size: 13px;");
  creatorLayout->addWidget(m_creatorName);

  creatorLayout->addStretch();

  m_createTimeLabel = new QLabel(m_headerWidget);
  m_createTimeLabel->setStyleSheet("color: #999; font-size: 12px;");
  creatorLayout->addWidget(m_createTimeLabel);

  infoLayout->addLayout(creatorLayout);

  // 操作按钮行
  QHBoxLayout *btnLayout = new QHBoxLayout();
  btnLayout->setSpacing(12);

  m_playAllBtn = new QPushButton(QStringLiteral("▶ 播放全部"), m_headerWidget);
  m_playAllBtn->setFixedSize(140, 34);
  m_playAllBtn->setStyleSheet(
      "QPushButton { background: #ec4141; color: white; border: none; border-radius: 17px; font-size: 14px; }"
      "QPushButton:hover { background: #d13b3b; }");
  connect(m_playAllBtn, &QPushButton::clicked, this, &PlaylistDetailWidget::playAllClicked);

  btnLayout->addWidget(m_playAllBtn);
  btnLayout->addStretch();
  infoLayout->addLayout(btnLayout);

  infoLayout->addStretch();
  headerLayout->addLayout(infoLayout, 1);

  // 右侧搜索框
  m_searchEdit = new QLineEdit(m_headerWidget);
  m_searchEdit->setPlaceholderText("🔍 搜索");
  m_searchEdit->setFixedSize(160, 32);
  m_searchEdit->setStyleSheet(
      "QLineEdit { border: 1px solid #e0e0e0; border-radius: 16px; padding-left: 12px; font-size: 13px; background: #f5f5f5; }"
      "QLineEdit:focus { border-color: #c62f2f; background: white; }");
  connect(m_searchEdit, &QLineEdit::textChanged, this, &PlaylistDetailWidget::searchTextChanged);
  headerLayout->addWidget(m_searchEdit, 0, Qt::AlignTop);
}

void PlaylistDetailWidget::setupTableHeader()
{
  m_tableHeader = new QWidget(this);
  m_tableHeader->setFixedHeight(36);
  QHBoxLayout *layout = new QHBoxLayout(m_tableHeader);
  layout->setContentsMargins(16, 0, 16, 0);
  layout->setSpacing(12);

  QLabel *numLabel = new QLabel("#", m_tableHeader);
  numLabel->setFixedWidth(36);
  numLabel->setStyleSheet("color: #999; font-size: 12px;");
  layout->addWidget(numLabel);

  QLabel *titleLabel = new QLabel("标题", m_tableHeader);
  titleLabel->setStyleSheet("color: #999; font-size: 12px;");
  layout->addWidget(titleLabel, 1);

  QLabel *albumLabel = new QLabel("专辑", m_tableHeader);
  albumLabel->setFixedWidth(200);
  albumLabel->setStyleSheet("color: #999; font-size: 12px;");
  layout->addWidget(albumLabel);

  QLabel *timeLabel = new QLabel("时长", m_tableHeader);
  timeLabel->setFixedWidth(50);
  timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  timeLabel->setStyleSheet("color: #999; font-size: 12px;");
  layout->addWidget(timeLabel);
}

void PlaylistDetailWidget::setupSongList()
{
  m_listWidget = new QListWidget(this);
  m_listWidget->setFrameShape(QFrame::NoFrame);
  m_listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_listWidget->setSpacing(2);
  m_listWidget->setStyleSheet(
      "QListWidget { background: transparent; border: none; outline: none; }"
      "QListWidget::item { background: transparent; border: none; padding: 0px; }");

  // 降低滚动速度：拦截滚轮事件，每次只滚动 1/3 行
  m_listWidget->viewport()->installEventFilter(this);

  connect(m_listWidget, &QListWidget::itemClicked, this, &PlaylistDetailWidget::onItemClicked);
  connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &PlaylistDetailWidget::onItemDoubleClicked);
}

void PlaylistDetailWidget::setPlaylist(const Playlist &playlist)
{
  m_currentPlaylistId = playlist.playlistId();
  m_isSystem = playlist.isSystem();
  m_nameLabel->setText(playlist.name());

  // 默认封面用占位图
  QPixmap defaultCover(":/resource/images/default_music.png");
  if (!defaultCover.isNull())
  {
    m_coverLabel->setPixmap(roundedPixmap(defaultCover, 12, 160));
  }

  // 简介显示实际描述，空则显示空
  m_descLabel->setText(playlist.description());

  // 创建时间，只保留年月日
  m_createTimeLabel->setText(playlist.createTime().toString("yyyy-MM-dd") + " 创建");

  // 系统歌单隐藏编辑按钮
  m_editIcon->setVisible(!m_isSystem);
}

void PlaylistDetailWidget::setPlaylistCover(const QPixmap &cover)
{
  if (!cover.isNull())
  {
    m_coverLabel->setPixmap(roundedPixmap(cover, 12, 160));
  }
}

void PlaylistDetailWidget::setCreatorInfo(const QPixmap &avatar, const QString &name, const QDateTime &createTime)
{
  if (!avatar.isNull())
  {
    m_creatorAvatar->setPixmap(avatar.scaled(28, 28, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
  }
  m_creatorName->setText(name);
  m_createTimeLabel->setText(createTime.toString("yyyy-MM-dd") + " 创建");
}

void PlaylistDetailWidget::setSongs(const QList<Song> &songs)
{
  m_allSongs = songs;
  refreshSongList();
}

QList<Song> PlaylistDetailWidget::songs() const
{
  return m_allSongs;
}

void PlaylistDetailWidget::refreshSongList()
{
  // 保留前两项（header 和 tableHeader），只删除歌曲项（索引 >= 2）
  while (m_listWidget->count() > 2)
  {
    QListWidgetItem *item = m_listWidget->takeItem(m_listWidget->count() - 1);
    QWidget *widget = m_listWidget->itemWidget(item);
    if (widget)
    {
      m_listWidget->removeItemWidget(item);
      widget->deleteLater();
    }
    delete item;
  }

  for (int i = 0; i < m_allSongs.size(); ++i)
  {
    const Song &song = m_allSongs[i];
    PlaylistSongItem *itemWidget = new PlaylistSongItem();
    itemWidget->setSongIndex(i + 1);
    itemWidget->setSong(song);

    connect(itemWidget, &PlaylistSongItem::loveClicked, this, &PlaylistDetailWidget::songLoveClicked);
    connect(itemWidget, &PlaylistSongItem::addToPlaylistClicked, this, &PlaylistDetailWidget::songAddToPlaylistClicked);

    QListWidgetItem *item = new QListWidgetItem(m_listWidget);
    item->setSizeHint(QSize(865, 55));
    item->setData(Qt::UserRole, song.songId());
    item->setData(Qt::UserRole + 1, song.filePath());
    m_listWidget->addItem(item);
    m_listWidget->setItemWidget(item, itemWidget);
  }
}

void PlaylistDetailWidget::clearSongs()
{
  m_allSongs.clear();
  refreshSongList();
}

void PlaylistDetailWidget::setPlayAllCount(int count)
{
  m_playAllBtn->setText(QStringLiteral("▶ 播放全部 %1").arg(count));
}

void PlaylistDetailWidget::filterSongs(const QString &keyword)
{
  if (keyword.trimmed().isEmpty())
  {
    // 显示全部
    for (int i = 2; i < m_listWidget->count(); ++i)
    {
      m_listWidget->item(i)->setHidden(false);
    }
    return;
  }

  const QString lowerKey = keyword.toLower();
  for (int i = 2; i < m_listWidget->count(); ++i)
  {
    PlaylistSongItem *widget = qobject_cast<PlaylistSongItem *>(m_listWidget->itemWidget(m_listWidget->item(i)));
    if (widget)
    {
      bool match = widget->songTitle().toLower().contains(lowerKey) ||
                   widget->songArtist().toLower().contains(lowerKey) ||
                   widget->songAlbum().toLower().contains(lowerKey);
      m_listWidget->item(i)->setHidden(!match);
    }
  }
}

void PlaylistDetailWidget::setCurrentPlayingSongId(int songId)
{
  m_currentPlayingSongId = songId;
  updatePlayingState();
}

void PlaylistDetailWidget::updatePlayingState()
{
  for (int i = 2; i < m_listWidget->count(); ++i)
  {
    QListWidgetItem *item = m_listWidget->item(i);
    PlaylistSongItem *widget = qobject_cast<PlaylistSongItem *>(m_listWidget->itemWidget(item));
    if (widget)
    {
      int itemSongId = item->data(Qt::UserRole).toInt();
      widget->setSelected(itemSongId == m_currentPlayingSongId);
    }
  }
}

void PlaylistDetailWidget::showEditPlaylistDialog()
{
  QDialog dialog(this);
  dialog.setWindowTitle(QStringLiteral("编辑歌单信息"));
  dialog.setFixedSize(320, 220);

  QVBoxLayout *layout = new QVBoxLayout(&dialog);

  // 歌单名称
  QLabel *nameLabel = new QLabel(QStringLiteral("歌单名称："), &dialog);
  layout->addWidget(nameLabel);
  QLineEdit *nameEdit = new QLineEdit(&dialog);
  nameEdit->setPlaceholderText(QStringLiteral("请输入歌单名称"));
  nameEdit->setText(m_nameLabel->text());
  layout->addWidget(nameEdit);

  // 歌单简介
  QLabel *descLabel = new QLabel(QStringLiteral("歌单简介："), &dialog);
  layout->addWidget(descLabel);
  QTextEdit *descEdit = new QTextEdit(&dialog);
  descEdit->setPlaceholderText(QStringLiteral("请输入歌单简介（可选）"));
  descEdit->setMaximumHeight(60);
  descEdit->setPlainText(m_descLabel->text());
  layout->addWidget(descEdit);

  // 按钮
  QDialogButtonBox *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  connect(btnBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(btnBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(btnBox);

  if (dialog.exec() == QDialog::Accepted)
  {
    QString name = nameEdit->text().trimmed();
    QString description = descEdit->toPlainText().trimmed();
    if (!name.isEmpty())
    {
      m_nameLabel->setText(name);
      m_descLabel->setText(description);
      emit playlistInfoEdited(m_currentPlaylistId, name, description);
    }
  }
}

bool PlaylistDetailWidget::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == m_listWidget->viewport() && event->type() == QEvent::Wheel)
  {
    QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);
    // 每次滚轮事件只滚动 1/3 行，显著降低速度
    int delta = wheelEvent->angleDelta().y();
    int step = (delta > 0) ? -1 : 1; // 向上 -1，向下 +1
    m_listWidget->verticalScrollBar()->setValue(
        m_listWidget->verticalScrollBar()->value() + step);
    return true; // 阻止默认的 3 行滚动
  }
  // 编辑图标点击事件
  if (watched == m_editIcon && event->type() == QEvent::MouseButtonRelease)
  {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::LeftButton)
    {
      showEditPlaylistDialog();
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

void PlaylistDetailWidget::onItemClicked(QListWidgetItem *item)
{
  if (!item)
    return;
  int row = m_listWidget->row(item);
  if (row < 2)
    return; // 跳过 header 和 tableHeader
  int songId = item->data(Qt::UserRole).toInt();
  setCurrentPlayingSongId(songId);
}

void PlaylistDetailWidget::onItemDoubleClicked(QListWidgetItem *item)
{
  if (!item)
    return;
  int row = m_listWidget->row(item);
  if (row < 2)
    return; // 跳过 header 和 tableHeader
  int songId = item->data(Qt::UserRole).toInt();
  QString filePath = item->data(Qt::UserRole + 1).toString();
  emit songDoubleClicked(songId, filePath);
}
