#include "sidebarwidget.h"
#include "collapsiblegroup.h"
#include "../controllers/librarycontroller.h"
#include "../utils/metadataextractor.h"
#include <QVBoxLayout>
#include <QPushButton>

SideBarWidget::SideBarWidget(QWidget *scrollAreaWidget, QWidget *parent)
    : QWidget(parent), m_scrollAreaWidget(scrollAreaWidget)
{
  setupUI();
  updateSideBarHeight();
}

void SideBarWidget::setScrollAreaWidget(QWidget *widget)
{
  m_scrollAreaWidget = widget;
  updateSideBarHeight();
}
void SideBarWidget::setButtonChecked(int index)
{
  QPushButton *target = nullptr;
  if (index == 0)
    target = m_localBtn;
  else if (index == 1)
    target = m_favBtn;
  else if (index == 2)
    target = m_historyBtn;

  if (target)
  {
    updateButtonStates(target);
    target->setChecked(true);
  }
}
void SideBarWidget::updateButtonStates(QPushButton *activeBtn)
{
  QPushButton *btns[] = {m_localBtn, m_favBtn, m_historyBtn};
  for (auto *btn : btns)
  {
    btn->setChecked(btn == activeBtn);
  }
}

void SideBarWidget::setupUI()
{
  setStyleSheet(
      "SideBarWidget { background: #fafafa; }");

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 8, 0, 8);
  layout->setSpacing(2);

  // 三个固定功能按钮
  m_localBtn = new QPushButton("💻 本地音乐", this);
  m_favBtn = new QPushButton("❤  我的喜欢", this);
  m_historyBtn = new QPushButton("🕐 历史播放", this);

  QPushButton *btns[] = {m_localBtn, m_favBtn, m_historyBtn};
  for (auto *btn : btns)
  {
    btn->setFixedHeight(BTN_HEIGHT);
    btn->setStyleSheet(
        "QPushButton { text-align: left; padding-left: 16px; border: none; "
        "background: transparent; color: #333; font-size: 13px; border-radius: 4px; }"
        "QPushButton:hover { background: #f0f0f0;}"
        "QPushButton:checked { background: #e6e6e6; color: #c62f2f; font-weight: bold; }");
    btn->setCursor(Qt::PointingHandCursor);
    btn->setCheckable(true);
    layout->addWidget(btn);
  }

  // 绑定页面切换信号
  connect(m_localBtn, &QPushButton::clicked, this, [this]()
          { emit pageRequested(0); });
  connect(m_favBtn, &QPushButton::clicked, this, [this]()
          { emit pageRequested(1); });
  connect(m_historyBtn, &QPushButton::clicked, this, [this]()
          { emit pageRequested(2); });

  layout->addSpacing(8);

  // 创建的歌单（可右键删除）
  m_createList = new CollapsibleGroup("创建的歌单", true, this);
  connect(m_createList, &CollapsibleGroup::sizeChanged, this, &SideBarWidget::updateSideBarHeight);
  // 转发数据库相关信号
  connect(m_createList, &CollapsibleGroup::playlistAdded, this, &SideBarWidget::customPlaylistAdded);
  connect(m_createList, &CollapsibleGroup::playlistRemoved, this, &SideBarWidget::customPlaylistRemoved);
  connect(m_createList, &CollapsibleGroup::playlistClicked, this, &SideBarWidget::customPlaylistClicked);

  layout->addWidget(m_createList);
}

void SideBarWidget::setLibraryController(LibraryController *ctrl)
{
  m_libraryController = ctrl;
}

void SideBarWidget::addCustomPlaylist(const QString &name, int playlistId)
{
  m_createList->addPlaylist(name, playlistId, QPixmap());
}

void SideBarWidget::loadPlaylistsFromDb()
{
  if (!m_libraryController)
    return;

  const QList<Playlist> playlists = m_libraryController->allPlaylists();
  for (const Playlist &pl : playlists)
  {
    // 跳过系统歌单（本地音乐0、我喜欢1、播放历史2）
    if (pl.isSystem())
    {
      continue;
    }

    // 提取歌单封面：取歌单中第一首歌的封面
    QPixmap cover;
    const QList<Song> songs = m_libraryController->playlistSongs(pl.playlistId());
    if (!songs.isEmpty())
    {
      cover = MetaDataExtractor::extractCover(songs.first().filePath());
    }

    m_createList->addPlaylist(pl.name(), pl.playlistId(), cover);
  }
}

void SideBarWidget::updateCustomPlaylistName(int playlistId, const QString &name)
{
  if (m_createList)
  {
    m_createList->updatePlaylistName(playlistId, name);
  }
}

int SideBarWidget::calculateHeight() const
{
  QLayout *lay = layout();
  int h = lay->contentsMargins().top() + lay->contentsMargins().bottom();

  // 3 个固定按钮 + 2 个 spacing
  h += 3 * BTN_HEIGHT + 2 * 2;
  h += 8; // addSpacing(8)

  // 折叠组
  h += m_createList->totalHeight();

  return h;
}

void SideBarWidget::updateSideBarHeight()
{
  int h = calculateHeight();
  setFixedHeight(h);

  // 同步修改 scrollAreaWidgetContents 的高度，确保滚动区域正确
  if (m_scrollAreaWidget)
  {
    m_scrollAreaWidget->setFixedHeight(h);
  }
}
