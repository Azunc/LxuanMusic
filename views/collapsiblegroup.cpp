#include "collapsiblegroup.h"
#include "playlistitem.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QMouseEvent>
#include <QInputDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPixmap>

CollapsibleGroup::CollapsibleGroup(const QString &title, bool itemsDeletable, QWidget *parent)
    : QWidget(parent), m_expanded(true), m_itemsDeletable(itemsDeletable)
{
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // 标题栏
  m_header = new QWidget(this);
  m_header->setFixedHeight(HEADER_HEIGHT);
  m_header->setCursor(Qt::PointingHandCursor);
  m_header->installEventFilter(this);

  QHBoxLayout *headerLayout = new QHBoxLayout(m_header);
  headerLayout->setContentsMargins(12, 0, 8, 0);
  headerLayout->setSpacing(4);

  m_toggleBtn = new QPushButton("▼", m_header);
  m_toggleBtn->setFixedSize(20, 20);
  m_toggleBtn->setStyleSheet("QPushButton { border: none; background: transparent; color: #999; font-size: 10px; }");
  m_toggleBtn->setCursor(Qt::PointingHandCursor);
  connect(m_toggleBtn, &QPushButton::clicked, this, &CollapsibleGroup::toggle);

  QLabel *titleLabel = new QLabel(title, m_header);
  titleLabel->setStyleSheet("color: #888888; font-size: 12px; font-weight: bold;");

  m_addBtn = new QPushButton("+", m_header);
  m_addBtn->setFixedSize(24, 24);
  m_addBtn->setStyleSheet(
      "QPushButton { border: none; background: transparent; color: #888; font-size: 16px; font-weight: bold; }"
      "QPushButton:hover { color: #333; }");
  m_addBtn->setCursor(Qt::PointingHandCursor);
  connect(m_addBtn, &QPushButton::clicked, this, &CollapsibleGroup::onAddClicked);

  headerLayout->addWidget(m_toggleBtn);
  headerLayout->addWidget(titleLabel);
  headerLayout->addStretch();
  headerLayout->addWidget(m_addBtn);

  // 内容区
  m_content = new QWidget(this);
  m_contentLayout = new QVBoxLayout(m_content);
  m_contentLayout->setContentsMargins(0, 0, 0, 0);
  m_contentLayout->setSpacing(0);
  m_contentLayout->addStretch();

  mainLayout->addWidget(m_header);
  mainLayout->addWidget(m_content);

  updateHeight();
}

void CollapsibleGroup::addPlaylist(const QString &name, int playlistId, const QPixmap &cover)
{
  PlaylistItem *item = new PlaylistItem(name, m_itemsDeletable, m_content);
  item->setCover(cover);
  item->setPlaylistId(playlistId);
  connect(item, &PlaylistItem::deleted, this, [this, item]()
          {
        const int pid = item->playlistId();
        const QString pname = item->playlistName();
        removePlaylist(item);
        if (pid > 0) {
            emit playlistRemoved(pid, pname);
        } });
  connect(item, &PlaylistItem::clicked, this, [this, item]()
          {
        qDebug() << "选中歌单:" << item->playlistName() << "id:" << item->playlistId();
        emit playlistClicked(item->playlistId()); });

  // 插入到 stretch 之前
  m_contentLayout->insertWidget(m_contentLayout->count() - 1, item);
  m_items.append(item);
  updateHeight();
  emit sizeChanged();
}

void CollapsibleGroup::removePlaylist(PlaylistItem *item)
{
  if (!item || !m_items.contains(item))
    return;

  m_items.removeOne(item);
  m_contentLayout->removeWidget(item);
  item->deleteLater();
  updateHeight();
  emit sizeChanged();
}

void CollapsibleGroup::updatePlaylistName(int playlistId, const QString &name)
{
  for (PlaylistItem *item : m_items)
  {
    if (item && item->playlistId() == playlistId)
    {
      item->setPlaylistName(name);
      break;
    }
  }
}

bool CollapsibleGroup::isExpanded() const
{
  return m_expanded;
}

int CollapsibleGroup::contentHeight() const
{
  return m_expanded ? (m_items.count() * ITEM_HEIGHT) : 0;
}

int CollapsibleGroup::headerHeight() const
{
  return HEADER_HEIGHT;
}

int CollapsibleGroup::totalHeight() const
{
  return HEADER_HEIGHT + contentHeight();
}

void CollapsibleGroup::toggle()
{
  m_expanded = !m_expanded;
  m_toggleBtn->setText(m_expanded ? "▼" : "▶");
  m_content->setVisible(m_expanded);
  updateHeight();
  emit sizeChanged();
}

void CollapsibleGroup::onAddClicked()
{
  QDialog dialog(this);
  dialog.setWindowTitle("新建歌单");
  dialog.setFixedSize(320, 200);

  QVBoxLayout *layout = new QVBoxLayout(&dialog);

  // 歌单名称
  QLabel *nameLabel = new QLabel("歌单名称：", &dialog);
  layout->addWidget(nameLabel);
  QLineEdit *nameEdit = new QLineEdit(&dialog);
  nameEdit->setPlaceholderText("请输入歌单名称");
  nameEdit->setText("新建歌单");
  layout->addWidget(nameEdit);

  // 歌单简介
  QLabel *descLabel = new QLabel("歌单简介：", &dialog);
  layout->addWidget(descLabel);
  QTextEdit *descEdit = new QTextEdit(&dialog);
  descEdit->setPlaceholderText("请输入歌单简介（可选）");
  descEdit->setMaximumHeight(60);
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
      emit playlistAdded(name, description);
    }
  }
}

void CollapsibleGroup::updateHeight()
{
  setFixedHeight(totalHeight());
}

bool CollapsibleGroup::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == m_header && event->type() == QEvent::MouseButtonRelease)
  {
    toggle();
    return true;
  }
  return QWidget::eventFilter(watched, event);
}
