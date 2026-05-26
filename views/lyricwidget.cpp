#include "lyricwidget.h"
#include "ui_lyricwidget.h"
#include "./models/lyricmodel.h"

#include <QFileInfo>
#include <QPainter>
#include <QFontMetrics>
#include <QDir>
#include <QDebug>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QUrl>

LyricWidget::LyricWidget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::LyricWidget),
      m_isLocked(false),
      m_fontSize(22),
      m_textAlpha(255),
      m_currentIndex(-1),
      m_scrollOffset(0.0),
      m_targetScrollOffset(0.0),
      m_userScrolling(false)
{
  ui->setupUi(this);

  setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
  setAttribute(Qt::WA_TranslucentBackground);
  resize(800, 170);
  setMouseTracking(true);

  // 按钮栏样式：恢复半透明背景
  ui->widget->setStyleSheet(
      "QWidget { background: rgba(50, 50, 50, 110); border-radius: 0px 0px 10px 10px; }"
      "QPushButton { border: none; color: #ccc; font-size: 13px; font-weight: bold; background: transparent; }"
      "QPushButton:hover { color: #fff; background: rgba(255,255,255,30); border-radius: 4px; }");
  ui->widget->hide();

  centerControlWidget();

  // 初始化平滑滚动定时器（12ms ≈ 83fps）
  m_scrollTimer = new QTimer(this);
  m_scrollTimer->setInterval(12);
  connect(m_scrollTimer, &QTimer::timeout, this, &LyricWidget::onSmoothScrollTick);

  // 滚轮回弹定时器
  m_wheelTimer = new QTimer(this);
  m_wheelTimer->setSingleShot(true);
  m_wheelTimer->setInterval(2000);
  connect(m_wheelTimer, &QTimer::timeout, this, [this]()
          { m_userScrolling = false;
            scrollToCurrent(true); });
}

LyricWidget::~LyricWidget() { delete ui; }

void LyricWidget::setLyricModel(LyricModel *model)
{
  if (m_lyricModel == model)
    return;

  if (m_lyricModel)
  {
    disconnect(m_lyricModel, nullptr, this, nullptr);
  }

  m_lyricModel = model;
  m_currentIndex = -1;
  m_scrollOffset = 0.0;
  m_targetScrollOffset = 0.0;
  update();

  if (m_lyricModel)
  {
    connect(m_lyricModel, &LyricModel::linesChanged, this, &LyricWidget::onLinesChanged);
  }
}

void LyricWidget::onLinesChanged()
{
  m_currentIndex = -1;
  m_scrollOffset = centerOffsetForNoCurrent();
  m_targetScrollOffset = m_scrollOffset;
  update();
}

// ==================== 尺寸辅助函数 ====================
int LyricWidget::lineHeight() const
{
  return m_fontSize + LINE_SPACING;
}

int LyricWidget::contentHeight() const
{
  if (!m_lyricModel || m_lyricModel->count() == 0)
    return 0;
  return m_lyricModel->count() * lineHeight();
}

int LyricWidget::centerLineOffset() const
{
  // 高亮行放在控件垂直正中心
  return height() / 2 - lineHeight() / 2;
}

int LyricWidget::centerOffsetForNoCurrent() const
{
  // 无当前行时，歌词整体在控件内垂直居中
  return qMax(0, (height() - contentHeight()) / 2);
}

// ==================== 核心自绘逻辑 ====================
void LyricWidget::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event)
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // 1. 绘制背景（锁定时纯透明，不绘制背景）
  if (!m_isLocked)
  {
    QLinearGradient bgGradient(0, 0, 0, height());
    bgGradient.setColorAt(0.0, QColor(90, 90, 90, 70));
    bgGradient.setColorAt(1.0, QColor(110, 110, 110, 70));
    painter.setBrush(bgGradient);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect(), 5, 5);
  }

  if (!m_lyricModel || m_lyricModel->count() == 0)
  {
    painter.setPen(QColor(255, 255, 255, m_textAlpha));
    painter.setFont(QFont("Microsoft YaHei", m_fontSize / 2));
    painter.drawText(rect(), Qt::AlignCenter, "暂无歌词");
    return;
  }

  // 2. 设置裁剪区域，防止歌词画到圆角外
  painter.setClipRect(rect());

  int lh = lineHeight();

  // 3. 绘制所有歌词行
  for (int i = 0; i < m_lyricModel->count(); ++i)
  {
    qreal drawY = m_scrollOffset + i * lh;

    // 跳过完全在可视区域外的行
    if (drawY + lh < 0 || drawY > height())
      continue;

    bool isCenter = (i == m_currentIndex);

    QFont font("Microsoft YaHei",
               m_fontSize,
               isCenter ? QFont::Bold : QFont::Normal);
    painter.setFont(font);

    QColor textColor;
    if (isCenter)
    {
      // 高亮行：纯红色，透明度由 m_textAlpha 控制
      textColor = QColor(255, 0, 0, m_textAlpha);
    }
    else
    {
      // 其他行：白色，按距离降低透明度
      int distance = qAbs(i - m_currentIndex);
      if (distance == 1)
      {
        textColor = QColor(255, 255, 255, static_cast<int>(m_textAlpha * 0.75));
      }
      else if (distance == 2)
      {
        textColor = QColor(255, 255, 255, static_cast<int>(m_textAlpha * 0.55));
      }
      else
      {
        textColor = QColor(255, 255, 255, static_cast<int>(m_textAlpha * 0.35));
      }
    }

    int marginX = 24;
    QRectF textRect(marginX, drawY, width() - marginX * 2, lh);

    painter.setPen(textColor);

    painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignHCenter, m_lyricModel->getLine(i).text);
  }
}

// ==================== 平滑滚动插值 ====================
void LyricWidget::onSmoothScrollTick()
{
  qreal diff = m_targetScrollOffset - m_scrollOffset;
  if (qAbs(diff) < 0.5)
  {
    m_scrollOffset = m_targetScrollOffset;
    m_scrollTimer->stop();
    update();
    return;
  }
  // ease-out 插值：每次移动剩余距离的 8%，滚动更慢更柔和
  m_scrollOffset += diff * 0.08;
  update();
}

// ==================== 滚动到当前行 ====================
void LyricWidget::scrollToCurrent(bool animated)
{
  if (!m_lyricModel || m_lyricModel->count() == 0)
    return;

  if (m_currentIndex < 0)
  {
    m_targetScrollOffset = centerOffsetForNoCurrent();
  }
  else
  {
    m_targetScrollOffset = centerLineOffset() - m_currentIndex * lineHeight();
  }

  if (animated)
  {
    m_scrollTimer->start();
  }
  else
  {
    m_scrollOffset = m_targetScrollOffset;
    m_scrollTimer->stop();
    update();
  }
}

// ==================== 进度同步 ====================
void LyricWidget::onProgressChanged(qint64 position)
{
  if (!m_lyricModel || m_lyricModel->count() == 0)
    return;

  int index = m_lyricModel->findIndex(position);
  if (index < 0)
    return;

  if (index != m_currentIndex)
  {
    m_currentIndex = index;
    if (!m_userScrolling)
    {
      scrollToCurrent(true);
    }
    else
    {
      update();
    }
  }
}

// ==================== 滚轮查看歌词 ====================
void LyricWidget::wheelEvent(QWheelEvent *event)
{
  m_scrollTimer->stop();
  m_userScrolling = true;

  // 滚动方向：滚轮向上(y>0) => 歌词向下滚动(m_scrollOffset 增大)
  //               滚轮向下(y<0) => 歌词向上滚动(m_scrollOffset 减小)
  int delta = event->angleDelta().y() > 0 ? 40 : -40;
  m_scrollOffset += delta;

  int lh = lineHeight();
  int ch = contentHeight();

  // 放宽边界：允许歌词上方和下方各空出最多 2 行高度的空白
  int maxLimit = lh * 2;
  int minLimit = height() - ch - lh * 2;

  m_scrollOffset = qBound(static_cast<qreal>(minLimit), m_scrollOffset, static_cast<qreal>(maxLimit));

  update();

  m_wheelTimer->start();
  event->accept();
}

// ==================== 窗口大小变化时重绘 ====================
void LyricWidget::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  centerControlWidget();
  if (!m_userScrolling)
  {
    scrollToCurrent(false);
  }
}

// ==================== 其他交互逻辑 ====================
void LyricWidget::loadLyricForSong(const QUrl &mediaUrl)
{
  QString lrcPath;
  if (mediaUrl.isLocalFile())
  {
    lrcPath = findLrcFile(mediaUrl.toLocalFile());
  }
  loadLyricFile(lrcPath);
}

void LyricWidget::loadLyricFile(const QString &lrcPath)
{
  if (!m_lyricModel)
    return;

  m_currentIndex = -1;
  m_scrollOffset = 0.0;
  m_targetScrollOffset = 0.0;

  if (!lrcPath.trimmed().isEmpty() && QFileInfo::exists(lrcPath))
  {
    QFile file(lrcPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
      QString content = QString::fromUtf8(file.readAll());
      file.close();
      m_lyricModel->parse(content);
      return;
    }
  }

  m_lyricModel->clear();
  update();
}

void LyricWidget::toggleVisible() { isVisible() ? hide() : show(); }

void LyricWidget::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton && !m_isLocked)
  {
    m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
    event->accept();
  }
}
void LyricWidget::mouseMoveEvent(QMouseEvent *event)
{
  if (event->buttons() & Qt::LeftButton && !m_isLocked)
  {
    move(event->globalPosition().toPoint() - m_dragPos);
    event->accept();
  }
}

void LyricWidget::enterEvent(QEnterEvent *event)
{
  if (!m_isLocked)
  {
    ui->widget->show();
    centerControlWidget();
  }
  QWidget::enterEvent(event);
}
void LyricWidget::leaveEvent(QEvent *event)
{
  QTimer::singleShot(100, this, [this]()
                     {
        if (!rect().contains(mapFromGlobal(QCursor::pos())) && !m_isLocked) {
            ui->widget->hide();
        } });
  QWidget::leaveEvent(event);
}

void LyricWidget::centerControlWidget()
{
  // 按钮栏靠左，宽度与歌词栏一致
  ui->widget->resize(width(), ui->widget->height());
  ui->widget->move(0, 0);
  ui->widget->raise();
}

QString LyricWidget::findLrcFile(const QString &songFilePath)
{
  QFileInfo fileInfo(songFilePath);
  QString baseName = fileInfo.completeBaseName();
  QDir dir = fileInfo.absoluteDir();
  QStringList filters;
  filters << baseName + ".lrc" << baseName + ".LRC";
  QStringList foundFiles = dir.entryList(filters, QDir::Files);
  if (!foundFiles.isEmpty())
  {
    return dir.absoluteFilePath(foundFiles.first());
  }
  QDir lyricDir(QDir::currentPath() + "/Lyrics");
  foundFiles = lyricDir.entryList(filters, QDir::Files);
  if (!foundFiles.isEmpty())
  {
    return lyricDir.absoluteFilePath(foundFiles.first());
  }
  return QString();
}

void LyricWidget::unlockLyric()
{
  m_isLocked = false;
  setAttribute(Qt::WA_TransparentForMouseEvents, false);
  update();
}

void LyricWidget::on_btnClose_clicked()
{
  hide();
}

void LyricWidget::on_btnDecreaseFontSize_clicked()
{
  if (m_fontSize > 12)
  {
    m_fontSize -= 2;
    update();
  }
}

void LyricWidget::on_btnIncreaseFontSize_clicked()
{
  if (m_fontSize < 48)
  {
    m_fontSize += 2;
    update();
  }
}

void LyricWidget::on_btnDecreaseOpacity_clicked()
{
  m_textAlpha = qMax(20, m_textAlpha - 20);
  update();
}

void LyricWidget::on_btnIncreaseOpacity_clicked()
{
  m_textAlpha = qMin(255, m_textAlpha + 20);
  update();
}

void LyricWidget::on_btnToggleLyricLock_clicked()
{
  m_isLocked = !m_isLocked;

  if (m_isLocked)
  {
    // 锁定：隐藏按钮栏，背景变透明，鼠标穿透到桌面
    ui->widget->hide();
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
  }
  else
  {
    // 解锁：恢复鼠标接收，重绘显示背景
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
  }
  emit sigLockStateChanged(m_isLocked);
  update();
}
