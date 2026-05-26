#include "localmusicwidget.h"
#include "playlistsongitem.h"
#include "../entity/song.h"
#include "../controllers/librarycontroller.h"
#include "../utils/metadataextractor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QListWidget>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QFileInfo>
#include <QDir>
#include <QScrollBar>
#include <QDebug>
#include <QPointer>
#include <QtConcurrent>

LocalMusicWidget::LocalMusicWidget(QWidget *parent)
    : QWidget(parent)
{
  setupUI();
  reloadFromDb();
}

void LocalMusicWidget::setupUI()
{
  setFixedSize(865, 568);
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // ========== 头部区域（固定，不随滚动移动） ==========
  setupHeader();
  mainLayout->addWidget(m_titleLabel->parentWidget());

  // ========== 内容区域（StackedWidget切换视图，填充剩余空间） ==========
  m_contentStack = new QStackedWidget(this);
  m_contentStack->setFixedSize(865, 488);

  setupDefaultView();
  setupCategoryView();
  setupFolderView();

  m_contentStack->addWidget(m_defaultView);  // index 0
  m_contentStack->addWidget(m_categoryView); // index 1
  m_contentStack->addWidget(m_folderView);   // index 2

  mainLayout->addWidget(m_contentStack);

  updateSortButtonStyle();
}

void LocalMusicWidget::setupHeader()
{
  QWidget *headerWidget = new QWidget(this);
  headerWidget->setFixedSize(865, 80);
  QVBoxLayout *headerMainLayout = new QVBoxLayout(headerWidget);
  headerMainLayout->setContentsMargins(16, 8, 16, 8);
  headerMainLayout->setSpacing(8);

  // --- 第一行：标题 + 选择目录 ---
  QHBoxLayout *topRow = new QHBoxLayout();
  topRow->setSpacing(8);

  m_titleLabel = new QLabel(QStringLiteral("本地音乐"), headerWidget);
  m_titleLabel->setStyleSheet("font-size: 22px; font-weight: bold; color: #333;");
  topRow->addWidget(m_titleLabel);

  m_countLabel = new QLabel(QStringLiteral("共 0 首"), headerWidget);
  m_countLabel->setStyleSheet("font-size: 13px; color: #999;");
  topRow->addWidget(m_countLabel);

  topRow->addStretch();

  m_selectDirBtn = new QPushButton(QStringLiteral("选择目录 >"), headerWidget);
  m_selectDirBtn->setStyleSheet(
      "QPushButton { border: none; background: transparent; color: #5b7cfa; font-size: 13px; }"
      "QPushButton:hover { color: #3a5bd9; }");
  connect(m_selectDirBtn, &QPushButton::clicked, this, &LocalMusicWidget::onSelectDirectoryClicked);
  topRow->addWidget(m_selectDirBtn);

  headerMainLayout->addLayout(topRow);

  // --- 第二行：播放全部、刷新、更多 | 搜索 | 分类按钮 ---
  QHBoxLayout *bottomRow = new QHBoxLayout();
  bottomRow->setSpacing(10);

  // 播放全部
  m_playAllBtn = new QPushButton(QStringLiteral("▶ 播放全部"), headerWidget);
  m_playAllBtn->setFixedSize(90, 32);
  m_playAllBtn->setStyleSheet(
      "QPushButton { background: #ff3a3a; color: white; border: none; border-radius: 16px; "
      "font-size: 13px; font-weight: 500; }"
      "QPushButton:hover { background: #e63030; }");
  connect(m_playAllBtn, &QPushButton::clicked, this, &LocalMusicWidget::onPlayAllClicked);
  bottomRow->addWidget(m_playAllBtn);

  // 刷新
  m_refreshBtn = new QPushButton(headerWidget);
  m_refreshBtn->setFixedSize(32, 32);
  m_refreshBtn->setText(QStringLiteral("⟳"));
  m_refreshBtn->setStyleSheet(
      "QPushButton { border: 1px solid #ddd; border-radius: 16px; background: #fff; color: #666; font-size: 14px; }"
      "QPushButton:hover { border-color: #bbb; background: #f5f5f5; }");
  connect(m_refreshBtn, &QPushButton::clicked, this, &LocalMusicWidget::onRefreshClicked);
  bottomRow->addWidget(m_refreshBtn);

  // 更多
  m_moreBtn = new QPushButton(headerWidget);
  m_moreBtn->setFixedSize(32, 32);
  m_moreBtn->setText(QStringLiteral("⋯"));
  m_moreBtn->setStyleSheet(
      "QPushButton { border: 1px solid #ddd; border-radius: 16px; background: #fff; color: #666; font-size: 14px; }"
      "QPushButton:hover { border-color: #bbb; background: #f5f5f5; }");
  connect(m_moreBtn, &QPushButton::clicked, this, &LocalMusicWidget::onMoreClicked);
  bottomRow->addWidget(m_moreBtn);

  bottomRow->addSpacing(20);

  // 搜索框
  m_searchEdit = new QLineEdit(headerWidget);
  m_searchEdit->setFixedSize(160, 32);
  m_searchEdit->setPlaceholderText(QStringLiteral("🔍 搜索"));
  m_searchEdit->setStyleSheet(
      "QLineEdit { border: 1px solid #e0e0e0; border-radius: 16px; padding: 0 12px; "
      "font-size: 13px; color: #333; background: #f7f7f7; }"
      "QLineEdit:focus { border-color: #c0c0c0; background: #fff; }");
  connect(m_searchEdit, &QLineEdit::textChanged, this, &LocalMusicWidget::onSearchTextChanged);
  bottomRow->addWidget(m_searchEdit);

  bottomRow->addSpacing(16);

  // 分类按钮
  auto createSortBtn = [this, headerWidget](const QString &text, SortMode mode) -> QPushButton *
  {
    QPushButton *btn = new QPushButton(text, headerWidget);
    btn->setFixedSize(50, 28);
    btn->setCheckable(true);
    btn->setProperty("sortMode", mode);
    btn->setStyleSheet(
        "QPushButton { border: none; background: transparent; color: #666; font-size: 13px; border-radius: 14px; }"
        "QPushButton:hover { color: #333; }"
        "QPushButton:checked { background: #e8ebf0; color: #333; font-weight: 500; }");
    connect(btn, &QPushButton::clicked, this, &LocalMusicWidget::onSortModeChanged);
    return btn;
  };

  m_defaultSortBtn = createSortBtn(QStringLiteral("默认"), DefaultSort);
  m_artistSortBtn = createSortBtn(QStringLiteral("歌手"), ArtistSort);
  m_albumSortBtn = createSortBtn(QStringLiteral("专辑"), AlbumSort);
  m_folderSortBtn = createSortBtn(QStringLiteral("文件夹"), FolderSort);

  m_defaultSortBtn->setChecked(true);

  bottomRow->addWidget(m_defaultSortBtn);
  bottomRow->addWidget(m_artistSortBtn);
  bottomRow->addWidget(m_albumSortBtn);
  bottomRow->addWidget(m_folderSortBtn);

  bottomRow->addStretch();
  headerMainLayout->addLayout(bottomRow);
}

void LocalMusicWidget::setupDefaultView()
{
  m_defaultView = new QWidget(this);
  QVBoxLayout *layout = new QVBoxLayout(m_defaultView);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // 表头（固定，不随列表滚动）
  m_defaultHeader = new QWidget(m_defaultView);
  m_defaultHeader->setFixedHeight(36);
  m_defaultHeader->setStyleSheet("border-bottom: 1px solid #e8e8e8;");
  QHBoxLayout *hLayout = new QHBoxLayout(m_defaultHeader);
  hLayout->setContentsMargins(16, 0, 16, 0);
  hLayout->setSpacing(12);

  auto addHeaderLabel = [hLayout](const QString &text, int width, bool stretch = false, Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter)
  {
    QLabel *lab = new QLabel(text);
    lab->setStyleSheet("color: #999; font-size: 13px;");
    if (width > 0)
      lab->setFixedWidth(width);
    lab->setAlignment(align);
    hLayout->addWidget(lab, stretch ? 1 : 0);
  };

  addHeaderLabel(QStringLiteral("#"), 36);
  addHeaderLabel(QStringLiteral("标题"), 0, true);
  addHeaderLabel(QStringLiteral("专辑"), 210);
  addHeaderLabel(QStringLiteral("时长"), 70);

  layout->addWidget(m_defaultHeader);

  // 歌曲列表（填充剩余空间，内部滚动）
  m_defaultList = new QListWidget(m_defaultView);
  m_defaultList->setFrameShape(QFrame::NoFrame);
  m_defaultList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_defaultList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_defaultList->setSpacing(2);
  m_defaultList->setStyleSheet(
      "QListWidget { border: none; background: transparent; outline: none; }"
      "QListWidget::item { border: none; padding: 0px; }");
  layout->addWidget(m_defaultList, 1);
}

void LocalMusicWidget::setupCategoryView()
{
  m_categoryView = new QWidget(this);
  QHBoxLayout *mainLayout = new QHBoxLayout(m_categoryView);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // 左侧分类列表
  m_categoryList = new QListWidget(m_categoryView);
  m_categoryList->setFixedWidth(200);
  m_categoryList->setFrameShape(QFrame::NoFrame);
  m_categoryList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_categoryList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_categoryList->setSpacing(2);
  m_categoryList->setStyleSheet(
      "QListWidget { border: none; border-right: 1px solid #eee; background: transparent; outline: none; }"
      "QListWidget::item { border: none; padding: 4px 8px; }"
      "QListWidget::item:selected { background: #f0f0f0; border-radius: 6px; }"
      "QListWidget::item:hover { background: #f7f7f7; border-radius: 6px; }");
  connect(m_categoryList, &QListWidget::itemClicked, this, &LocalMusicWidget::onCategoryItemClicked);
  mainLayout->addWidget(m_categoryList);

  // 右侧歌曲区
  m_categoryRight = new QWidget(m_categoryView);
  QVBoxLayout *rightLayout = new QVBoxLayout(m_categoryRight);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->setSpacing(0);

  // 表头（固定，不随列表滚动）
  m_categoryTableHeader = new QWidget(m_categoryRight);
  m_categoryTableHeader->setFixedHeight(36);
  m_categoryTableHeader->setStyleSheet("border-bottom: 1px solid #e8e8e8;");
  QHBoxLayout *thLayout = new QHBoxLayout(m_categoryTableHeader);
  thLayout->setContentsMargins(16, 0, 16, 0);
  thLayout->setSpacing(12);

  auto addTHLabel = [thLayout](const QString &text, int width, bool stretch = false, Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter)
  {
    QLabel *lab = new QLabel(text);
    lab->setStyleSheet("color: #999; font-size: 13px;");
    if (width > 0)
      lab->setFixedWidth(width);
    lab->setAlignment(align);
    thLayout->addWidget(lab, stretch ? 1 : 0);
  };

  addTHLabel(QStringLiteral("#"), 36);
  addTHLabel(QStringLiteral("标题"), 0, true);
  addTHLabel(QStringLiteral("专辑"), 210);
  addTHLabel(QStringLiteral("时长"), 70);

  rightLayout->addWidget(m_categoryTableHeader);

  // 歌曲列表（填充剩余空间，内部滚动）
  m_categorySongList = new QListWidget(m_categoryRight);
  m_categorySongList->setFrameShape(QFrame::NoFrame);
  m_categorySongList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_categorySongList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_categorySongList->setSpacing(2);
  m_categorySongList->setStyleSheet(
      "QListWidget { border: none; background: transparent; outline: none; }"
      "QListWidget::item { border: none; padding: 0px; }");
  rightLayout->addWidget(m_categorySongList, 1);

  mainLayout->addWidget(m_categoryRight, 1);
}

void LocalMusicWidget::setupFolderView()
{
  m_folderView = new QWidget(this);
  QHBoxLayout *mainLayout = new QHBoxLayout(m_folderView);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // 左侧文件夹树
  m_folderTree = new QTreeWidget(m_folderView);
  m_folderTree->setFixedWidth(200);
  m_folderTree->setFrameShape(QFrame::NoFrame);
  m_folderTree->setHeaderHidden(true);
  m_folderTree->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_folderTree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_folderTree->setStyleSheet(
      "QTreeWidget { border: none; border-right: 1px solid #eee; background: transparent; outline: none; }"
      "QTreeWidget::item { padding: 4px 8px; }"
      "QTreeWidget::item:selected { background: #f0f0f0; border-radius: 6px; }"
      "QTreeWidget::item:hover { background: #f7f7f7; border-radius: 6px; }");
  connect(m_folderTree, &QTreeWidget::itemClicked, this, &LocalMusicWidget::onCategoryTreeItemClicked);
  mainLayout->addWidget(m_folderTree);

  // 右侧歌曲区
  m_folderRight = new QWidget(m_folderView);
  QVBoxLayout *rightLayout = new QVBoxLayout(m_folderRight);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->setSpacing(0);

  // 表头（固定，不随列表滚动）
  m_folderTableHeader = new QWidget(m_folderRight);
  m_folderTableHeader->setFixedHeight(36);
  m_folderTableHeader->setStyleSheet("border-bottom: 1px solid #e8e8e8;");
  QHBoxLayout *thLayout = new QHBoxLayout(m_folderTableHeader);
  thLayout->setContentsMargins(16, 0, 16, 0);
  thLayout->setSpacing(12);

  auto addTHLabel = [thLayout](const QString &text, int width, bool stretch = false, Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter)
  {
    QLabel *lab = new QLabel(text);
    lab->setStyleSheet("color: #999; font-size: 13px;");
    if (width > 0)
      lab->setFixedWidth(width);
    lab->setAlignment(align);
    thLayout->addWidget(lab, stretch ? 1 : 0);
  };

  addTHLabel(QStringLiteral("#"), 36);
  addTHLabel(QStringLiteral("标题"), 0, true);
  addTHLabel(QStringLiteral("专辑"), 210);
  addTHLabel(QStringLiteral("时长"), 70);

  rightLayout->addWidget(m_folderTableHeader);

  // 歌曲列表（填充剩余空间，内部滚动）
  m_folderSongList = new QListWidget(m_folderRight);
  m_folderSongList->setFrameShape(QFrame::NoFrame);
  m_folderSongList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_folderSongList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_folderSongList->setSpacing(2);
  m_folderSongList->setStyleSheet(
      "QListWidget { border: none; background: transparent; outline: none; }"
      "QListWidget::item { border: none; padding: 0px; }");
  rightLayout->addWidget(m_folderSongList, 1);

  mainLayout->addWidget(m_folderRight, 1);
}

void LocalMusicWidget::updateHeaderCount(int count)
{
  m_countLabel->setText(QStringLiteral("共 %1 首").arg(count));
}

void LocalMusicWidget::updateSortButtonStyle()
{
  m_defaultSortBtn->setChecked(m_currentSort == DefaultSort);
  m_artistSortBtn->setChecked(m_currentSort == ArtistSort);
  m_albumSortBtn->setChecked(m_currentSort == AlbumSort);
  m_folderSortBtn->setChecked(m_currentSort == FolderSort);
}

void LocalMusicWidget::setLibraryController(LibraryController *ctrl)
{
  m_libraryController = ctrl;
}

std::vector<Song> LocalMusicWidget::fetchAllSongs() const
{
  if (m_libraryController)
  {
    QList<Song> list = m_libraryController->playlistSongs(Playlist::PL_LOCAL);
    return std::vector<Song>(list.begin(), list.end());
  }
  return std::vector<Song>();
}

void LocalMusicWidget::reloadFromDb()
{
  m_allSongs = fetchAllSongs();
  m_filteredSongs = m_allSongs;
  updateHeaderCount(static_cast<int>(m_allSongs.size()));

  // 重建分类数据
  m_artistMap.clear();
  m_albumMap.clear();

  for (const Song &song : m_allSongs)
  {
    QString artist = song.artist().isEmpty() ? QStringLiteral("未知歌手") : song.artist();
    QString album = song.album().isEmpty() ? QStringLiteral("未知专辑") : song.album();

    m_artistMap[artist].push_back(song);
    m_albumMap[album].push_back(song);
  }

  applyFilter();
}

void LocalMusicWidget::loadSongs(const QList<Song> &songs)
{
  m_allSongs = std::vector<Song>(songs.begin(), songs.end());
  m_filteredSongs = m_allSongs;
  updateHeaderCount(static_cast<int>(songs.size()));

  m_artistMap.clear();
  m_albumMap.clear();

  for (const Song &song : m_allSongs)
  {
    QString artist = song.artist().isEmpty() ? QStringLiteral("未知歌手") : song.artist();
    QString album = song.album().isEmpty() ? QStringLiteral("未知专辑") : song.album();

    m_artistMap[artist].push_back(song);
    m_albumMap[album].push_back(song);
  }

  applyFilter();
}

void LocalMusicWidget::applyFilter()
{
  QString kw = m_searchText.trimmed().toLower();

  if (m_currentSort == DefaultSort)
  {
    if (kw.isEmpty())
    {
      m_filteredSongs = m_allSongs;
    }
    else
    {
      m_filteredSongs.clear();
      for (const Song &s : m_allSongs)
      {
        if (s.title().toLower().contains(kw) ||
            s.artist().toLower().contains(kw) ||
            s.album().toLower().contains(kw))
        {
          m_filteredSongs.push_back(s);
        }
      }
    }
    refreshDefaultView();
  }
  else if (m_currentSort == ArtistSort)
  {
    refreshArtistView();
  }
  else if (m_currentSort == AlbumSort)
  {
    refreshAlbumView();
  }
  else if (m_currentSort == FolderSort)
  {
    refreshFolderView();
  }
}

// ========== 默认排序视图 ==========

void LocalMusicWidget::refreshDefaultView()
{
  m_defaultList->clear();

  for (size_t i = 0; i < m_filteredSongs.size(); ++i)
  {
    const Song &song = m_filteredSongs.at(i);
    PlaylistSongItem *itemWidget = new PlaylistSongItem(m_defaultList);
    itemWidget->setSongIndex(static_cast<int>(i) + 1);
    itemWidget->setSong(song);
    itemWidget->setCoverVisible(true);
    connect(itemWidget, &PlaylistSongItem::clicked, this, &LocalMusicWidget::onSongItemClicked);
    connect(itemWidget, &PlaylistSongItem::doubleClicked, this, &LocalMusicWidget::onSongItemDoubleClicked);
    connect(itemWidget, &PlaylistSongItem::loveClicked, this, &LocalMusicWidget::onSongItemLoveClicked);
    connect(itemWidget, &PlaylistSongItem::addToPlaylistClicked, this, &LocalMusicWidget::onSongItemAddToPlaylistClicked);

    QListWidgetItem *item = new QListWidgetItem(m_defaultList);
    item->setSizeHint(QSize(m_defaultList->width(), 60));
    item->setData(Qt::UserRole, song.songId());
    m_defaultList->addItem(item);
    m_defaultList->setItemWidget(item, itemWidget);
  }
}

// ========== 歌手分类视图 ==========

void LocalMusicWidget::refreshArtistView()
{
  m_categoryList->clear();
  m_categorySongList->clear();

  // 过滤歌手
  QString kw = m_searchText.trimmed().toLower();
  QMapIterator<QString, std::vector<Song>> it(m_artistMap);
  while (it.hasNext())
  {
    it.next();
    if (!kw.isEmpty() && !it.key().toLower().contains(kw))
      continue;

    const std::vector<Song> &songs = it.value();
    if (songs.empty())
      continue;

    // 创建分类项（显示歌手名和歌曲数）
    QWidget *catWidget = new QWidget(m_categoryList);
    QHBoxLayout *layout = new QHBoxLayout(catWidget);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(8);

    // 歌手封面（用第一首歌的封面）
    QLabel *coverLabel = new QLabel(catWidget);
    coverLabel->setFixedSize(36, 36);
    coverLabel->setScaledContents(true);
    coverLabel->setStyleSheet("border-radius: 18px;");
    coverLabel->setPixmap(QPixmap(QStringLiteral(":/resource/images/default_music.png")).scaled(36, 36, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));

    if (!songs.empty())
    {
      const QString firstPath = songs.front().filePath();
      if (m_coverCache.contains(firstPath))
      {
        coverLabel->setPixmap(m_coverCache.value(firstPath));
      }
      else
      {
        QPointer<QLabel> safeLabel = coverLabel;
        auto future1 = QtConcurrent::run([this, safeLabel, firstPath]()
                                         {
          QPixmap cover = MetaDataExtractor::extractCover(firstPath);
          if (cover.isNull())
            return;
          QPixmap scaled = cover.scaled(36, 36, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
          QMetaObject::invokeMethod(this, [this, safeLabel, firstPath, scaled]() {
            if (!safeLabel)
              return;
            m_coverCache.insert(firstPath, scaled);
            safeLabel->setPixmap(scaled);
          }, Qt::QueuedConnection); });
        Q_UNUSED(future1)
      }
    }

    QVBoxLayout *textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);
    textLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *nameLabel = new QLabel(it.key(), catWidget);
    nameLabel->setStyleSheet("font-size: 14px; color: #333;");

    QLabel *countLabel = new QLabel(QStringLiteral("%1首").arg(static_cast<int>(songs.size())), catWidget);
    countLabel->setStyleSheet("font-size: 12px; color: #999;");

    textLayout->addWidget(nameLabel);
    textLayout->addWidget(countLabel);

    layout->addWidget(coverLabel);
    layout->addLayout(textLayout, 1);

    QListWidgetItem *item = new QListWidgetItem(m_categoryList);
    item->setSizeHint(QSize(180, 52));
    item->setData(Qt::UserRole, it.key()); // 存储歌手名
    m_categoryList->addItem(item);
    m_categoryList->setItemWidget(item, catWidget);
  }

  // 默认选中第一个
  if (m_categoryList->count() > 0)
  {
    m_categoryList->setCurrentRow(0);
    onCategoryItemClicked(m_categoryList->item(0));
  }
}

// ========== 专辑分类视图 ==========

void LocalMusicWidget::refreshAlbumView()
{
  m_categoryList->clear();
  m_categorySongList->clear();

  QString kw = m_searchText.trimmed().toLower();
  QMapIterator<QString, std::vector<Song>> it(m_albumMap);
  while (it.hasNext())
  {
    it.next();
    if (!kw.isEmpty() && !it.key().toLower().contains(kw))
      continue;

    const std::vector<Song> &songs = it.value();
    if (songs.empty())
      continue;

    QWidget *catWidget = new QWidget(m_categoryList);
    QHBoxLayout *layout = new QHBoxLayout(catWidget);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(8);

    QLabel *coverLabel = new QLabel(catWidget);
    coverLabel->setFixedSize(36, 36);
    coverLabel->setScaledContents(true);
    coverLabel->setStyleSheet("border-radius: 4px;");
    coverLabel->setPixmap(QPixmap(QStringLiteral(":/resource/images/default_music.png")).scaled(36, 36, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));

    if (!songs.empty())
    {
      const QString firstPath = songs.front().filePath();
      if (m_coverCache.contains(firstPath))
      {
        coverLabel->setPixmap(m_coverCache.value(firstPath));
      }
      else
      {
        QPointer<QLabel> safeLabel = coverLabel;
        auto future2 = QtConcurrent::run([this, safeLabel, firstPath]()
                                         {
          QPixmap cover = MetaDataExtractor::extractCover(firstPath);
          if (cover.isNull())
            return;
          QPixmap scaled = cover.scaled(36, 36, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
          QMetaObject::invokeMethod(this, [this, safeLabel, firstPath, scaled]() {
            if (!safeLabel)
              return;
            m_coverCache.insert(firstPath, scaled);
            safeLabel->setPixmap(scaled);
          }, Qt::QueuedConnection); });
        Q_UNUSED(future2)
      }
    }

    QVBoxLayout *textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);
    textLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *nameLabel = new QLabel(it.key(), catWidget);
    nameLabel->setStyleSheet("font-size: 14px; color: #333;");

    QLabel *countLabel = new QLabel(QStringLiteral("%1首").arg(static_cast<int>(songs.size())), catWidget);
    countLabel->setStyleSheet("font-size: 12px; color: #999;");

    textLayout->addWidget(nameLabel);
    textLayout->addWidget(countLabel);

    layout->addWidget(coverLabel);
    layout->addLayout(textLayout, 1);

    QListWidgetItem *item = new QListWidgetItem(m_categoryList);
    item->setSizeHint(QSize(180, 52));
    item->setData(Qt::UserRole, it.key()); // 存储专辑名
    m_categoryList->addItem(item);
    m_categoryList->setItemWidget(item, catWidget);
  }

  if (m_categoryList->count() > 0)
  {
    m_categoryList->setCurrentRow(0);
    onCategoryItemClicked(m_categoryList->item(0));
  }
}

// ========== 文件夹分类视图 ==========

void LocalMusicWidget::refreshFolderView()
{
  m_folderTree->clear();
  m_folderSongList->clear();

  // 构建文件夹树
  QMap<QString, std::vector<Song>> folderMap;
  for (const Song &song : m_allSongs)
  {
    QFileInfo fi(song.filePath());
    QString dirPath = fi.absolutePath();
    folderMap[dirPath].push_back(song);
  }

  // 过滤
  QString kw = m_searchText.trimmed().toLower();

  // 构建树结构
  QMap<QString, QTreeWidgetItem *> pathToItem;

  QMapIterator<QString, std::vector<Song>> it(folderMap);
  while (it.hasNext())
  {
    it.next();
    if (!kw.isEmpty() && !it.key().toLower().contains(kw))
      continue;

    const QString &fullPath = it.key();
    QStringList parts = fullPath.split('/');
    if (parts.isEmpty())
      parts = fullPath.split('\\');

    QString currentPath;
    QTreeWidgetItem *parentItem = nullptr;

    for (int i = 0; i < parts.size(); ++i)
    {
      if (i == 0)
        currentPath = parts[i];
      else
        currentPath += QStringLiteral("/") + parts[i];

      if (!pathToItem.contains(currentPath))
      {
        QTreeWidgetItem *item;
        if (parentItem)
          item = new QTreeWidgetItem(parentItem);
        else
          item = new QTreeWidgetItem(m_folderTree);

        int songCount = static_cast<int>(folderMap.value(currentPath, std::vector<Song>()).size());
        item->setText(0, parts[i] + QStringLiteral("(%1)").arg(songCount));
        item->setData(0, Qt::UserRole, currentPath);
        item->setIcon(0, QIcon(QStringLiteral(":/resource/icons/文件.png")));

        pathToItem[currentPath] = item;
        parentItem = item;
      }
      else
      {
        parentItem = pathToItem[currentPath];
      }
    }
  }

  // 展开第一层
  for (int i = 0; i < m_folderTree->topLevelItemCount(); ++i)
  {
    m_folderTree->expandItem(m_folderTree->topLevelItem(i));
  }

  // 默认选中第一个有歌曲的项
  if (!pathToItem.isEmpty())
  {
    QTreeWidgetItem *firstItem = pathToItem.first();
    m_folderTree->setCurrentItem(firstItem);
    onCategoryTreeItemClicked(firstItem, 0);
  }
}

// ========== 事件处理 ==========

void LocalMusicWidget::onSortModeChanged()
{
  QPushButton *btn = qobject_cast<QPushButton *>(sender());
  if (!btn)
    return;

  SortMode newMode = static_cast<SortMode>(btn->property("sortMode").toInt());
  if (newMode == m_currentSort)
    return;

  m_currentSort = newMode;
  updateSortButtonStyle();

  // 切换stacked
  if (m_currentSort == DefaultSort)
    m_contentStack->setCurrentWidget(m_defaultView);
  else if (m_currentSort == ArtistSort || m_currentSort == AlbumSort)
    m_contentStack->setCurrentWidget(m_categoryView);
  else if (m_currentSort == FolderSort)
    m_contentStack->setCurrentWidget(m_folderView);

  applyFilter();
}

void LocalMusicWidget::onSearchTextChanged(const QString &text)
{
  m_searchText = text;
  applyFilter();
}

void LocalMusicWidget::onCategoryItemClicked(QListWidgetItem *item)
{
  if (!item)
    return;

  QString key = item->data(Qt::UserRole).toString();
  m_categorySongList->clear();

  const std::vector<Song> *songsPtr = nullptr;
  if (m_currentSort == ArtistSort)
  {
    songsPtr = &m_artistMap[key];
    m_currentArtist = key;
  }
  else if (m_currentSort == AlbumSort)
  {
    songsPtr = &m_albumMap[key];
    m_currentAlbum = key;
  }

  if (!songsPtr)
    return;

  m_currentSongs = *songsPtr;

  for (size_t i = 0; i < m_currentSongs.size(); ++i)
  {
    const Song &song = m_currentSongs.at(i);
    PlaylistSongItem *itemWidget = new PlaylistSongItem(m_categorySongList);
    itemWidget->setSongIndex(static_cast<int>(i) + 1);
    itemWidget->setSong(song);
    itemWidget->setCoverVisible(true);
    connect(itemWidget, &PlaylistSongItem::clicked, this, &LocalMusicWidget::onSongItemClicked);
    connect(itemWidget, &PlaylistSongItem::doubleClicked, this, &LocalMusicWidget::onSongItemDoubleClicked);
    connect(itemWidget, &PlaylistSongItem::loveClicked, this, &LocalMusicWidget::onSongItemLoveClicked);
    connect(itemWidget, &PlaylistSongItem::addToPlaylistClicked, this, &LocalMusicWidget::onSongItemAddToPlaylistClicked);

    QListWidgetItem *listItem = new QListWidgetItem(m_categorySongList);
    listItem->setSizeHint(QSize(m_categorySongList->width(), 60));
    listItem->setData(Qt::UserRole, song.songId());
    m_categorySongList->addItem(listItem);
    m_categorySongList->setItemWidget(listItem, itemWidget);
  }
}

void LocalMusicWidget::onCategoryTreeItemClicked(QTreeWidgetItem *item, int column)
{
  Q_UNUSED(column)
  if (!item)
    return;

  QString folderPath = item->data(0, Qt::UserRole).toString();
  m_currentFolder = folderPath;

  m_folderSongList->clear();

  // 收集该文件夹及子文件夹的所有歌曲
  m_currentSongs.clear();
  for (const Song &song : m_allSongs)
  {
    QFileInfo fi(song.filePath());
    QString dirPath = fi.absolutePath();
    if (dirPath == folderPath || dirPath.startsWith(folderPath + QStringLiteral("/")) ||
        dirPath.startsWith(folderPath + QStringLiteral("\\")))
    {
      m_currentSongs.push_back(song);
    }
  }

  for (size_t i = 0; i < m_currentSongs.size(); ++i)
  {
    const Song &song = m_currentSongs.at(i);
    PlaylistSongItem *itemWidget = new PlaylistSongItem(m_folderSongList);
    itemWidget->setSongIndex(static_cast<int>(i) + 1);
    itemWidget->setSong(song);
    itemWidget->setCoverVisible(true);
    connect(itemWidget, &PlaylistSongItem::clicked, this, &LocalMusicWidget::onSongItemClicked);
    connect(itemWidget, &PlaylistSongItem::doubleClicked, this, &LocalMusicWidget::onSongItemDoubleClicked);
    connect(itemWidget, &PlaylistSongItem::loveClicked, this, &LocalMusicWidget::onSongItemLoveClicked);
    connect(itemWidget, &PlaylistSongItem::addToPlaylistClicked, this, &LocalMusicWidget::onSongItemAddToPlaylistClicked);

    QListWidgetItem *listItem = new QListWidgetItem(m_folderSongList);
    listItem->setSizeHint(QSize(m_folderSongList->width(), 60));
    listItem->setData(Qt::UserRole, song.songId());
    m_folderSongList->addItem(listItem);
    m_folderSongList->setItemWidget(listItem, itemWidget);
  }
}

void LocalMusicWidget::onSongItemClicked(int songId)
{
  m_selectedSongId = songId;

  // 更新选中状态：清除上一个选中项的选中状态，设置当前项为选中
  auto updateSelection = [this, songId](QListWidget *list)
  {
    for (int i = 0; i < list->count(); ++i)
    {
      QListWidgetItem *item = list->item(i);
      PlaylistSongItem *widget = qobject_cast<PlaylistSongItem *>(list->itemWidget(item));
      if (widget)
      {
        widget->setSelected(item->data(Qt::UserRole).toInt() == songId);
      }
    }
  };

  updateSelection(m_defaultList);
  updateSelection(m_categorySongList);
  updateSelection(m_folderSongList);
}

void LocalMusicWidget::onSongItemDoubleClicked(int songId)
{
  // 查找对应的歌曲并发射信号
  const Song *targetSong = nullptr;
  for (const Song &s : m_allSongs)
  {
    if (s.songId() == songId)
    {
      targetSong = &s;
      break;
    }
  }

  if (targetSong)
  {
    emit songDoubleClicked(songId, targetSong->filePath());
  }
}

void LocalMusicWidget::onPlayAllClicked()
{
  std::vector<Song> songs = currentVisibleSongs();
  if (!songs.empty())
  {
    emit playAllRequested();
  }
}

void LocalMusicWidget::onRefreshClicked()
{
  // 刷新操作：由 MainWindow 重新扫描默认音乐目录并加载
  emit refreshRequested();
}

void LocalMusicWidget::onMoreClicked()
{
  // 更多功能菜单，可扩展
}

void LocalMusicWidget::onSelectDirectoryClicked()
{
  emit selectDirectoryRequested();
}

void LocalMusicWidget::onSongItemLoveClicked(int songId)
{
  if (m_libraryController)
  {
    m_libraryController->toggleFavorite(songId);
  }
}

void LocalMusicWidget::onSongItemAddToPlaylistClicked(int songId)
{
  emit songAddToPlaylistRequested(songId);
}

std::vector<Song> LocalMusicWidget::currentVisibleSongs() const
{
  if (m_currentSort == DefaultSort)
  {
    return m_filteredSongs;
  }
  else if (m_currentSort == ArtistSort || m_currentSort == AlbumSort || m_currentSort == FolderSort)
  {
    return m_currentSongs;
  }
  return std::vector<Song>();
}
