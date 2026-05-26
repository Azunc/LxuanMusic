#include "playlistitem.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

static QPixmap roundedPixmap(const QPixmap &source, int size, int radius)
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

PlaylistItem::PlaylistItem(const QString &name, bool deletable, QWidget *parent)
    : QWidget(parent), m_deletable(deletable)
{
  setFixedHeight(40);
  setCursor(Qt::PointingHandCursor);

  QHBoxLayout *layout = new QHBoxLayout(this);
  layout->setContentsMargins(12, 4, 12, 4);
  layout->setSpacing(8);

  // 封面
  m_coverLabel = new QLabel(this);
  m_coverLabel->setFixedSize(32, 32);
  m_coverLabel->setScaledContents(false);
  m_coverLabel->setStyleSheet("background: #e0e0e0; border-radius: 4px;");
  layout->addWidget(m_coverLabel);

  // 歌单名称
  m_label = new QLabel(name, this);
  m_label->setStyleSheet("color: #333333; font-size: 13px;");
  layout->addWidget(m_label, 1);

  setStyleSheet("PlaylistItem { background: transparent; border-radius: 4px; }");
}

void PlaylistItem::setCover(const QPixmap &cover)
{
  QPixmap defaultCover(":/resource/images/default_music.png");
  if (!cover.isNull())
  {
    m_coverLabel->setPixmap(roundedPixmap(cover, 32, 4));
    m_coverLabel->setStyleSheet("");
  }
  else if (!defaultCover.isNull())
  {
    m_coverLabel->setPixmap(roundedPixmap(defaultCover, 32, 4));
    m_coverLabel->setStyleSheet("");
  }
  else
  {
    m_coverLabel->setPixmap(QPixmap());
    m_coverLabel->setStyleSheet("background: #e0e0e0; border-radius: 4px;");
  }
}

QString PlaylistItem::playlistName() const
{
  return m_label->text();
}

void PlaylistItem::setPlaylistName(const QString &name)
{
  m_label->setText(name);
}

int PlaylistItem::playlistId() const
{
  return m_playlistId;
}

void PlaylistItem::setPlaylistId(int id)
{
  m_playlistId = id;
}

void PlaylistItem::contextMenuEvent(QContextMenuEvent *event)
{
  if (!m_deletable)
  {
    QWidget::contextMenuEvent(event);
    return;
  }
  QMenu menu(this);
  QAction *delAction = menu.addAction("删除歌单");
  connect(delAction, &QAction::triggered, this, &PlaylistItem::deleted);
  menu.exec(event->globalPos());
}

void PlaylistItem::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton)
  {
    emit clicked();
  }
  QWidget::mousePressEvent(event);
}

void PlaylistItem::enterEvent(QEnterEvent *event)
{
  setStyleSheet("PlaylistItem { background: #f0f0f0; border-radius: 4px; }");
  QWidget::enterEvent(event);
}

void PlaylistItem::leaveEvent(QEvent *event)
{
  setStyleSheet("PlaylistItem { background: transparent; border-radius: 4px; }");
  QWidget::leaveEvent(event);
}
