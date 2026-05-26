/***************************************************
 *  @file      lyricwidget.h
 *  @brief     	歌词展示控件，负责主界面滚动歌词、桌面悬浮歌词的绘制、当前播放行高亮
 *
 *  @author    un
 *  @date      2026/04/13
 *  @history
 ****************************************************/
#ifndef LYRICWIDGET_H
#define LYRICWIDGET_H

#include <QWidget>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui
{
  class LyricWidget;
}
QT_END_NAMESPACE

class LyricModel;

class LyricWidget : public QWidget
{
  Q_OBJECT
public:
  explicit LyricWidget(QWidget *parent = nullptr);
  ~LyricWidget();

  void setLyricModel(LyricModel *model);

  void toggleVisible();
  void loadLyricForSong(const QUrl &mediaUrl);
  void loadLyricFile(const QString &lrcPath);

  bool isLocked() const { return m_isLocked; }

public slots:
  void unlockLyric();
  void onProgressChanged(qint64 position);

signals:
  void sigLockStateChanged(bool locked);

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;

private slots:
  void onLinesChanged();
  void on_btnClose_clicked();
  void on_btnDecreaseFontSize_clicked();
  void on_btnIncreaseFontSize_clicked();
  void on_btnDecreaseOpacity_clicked();
  void on_btnIncreaseOpacity_clicked();
  void on_btnToggleLyricLock_clicked();
  void onSmoothScrollTick();

private:
  Ui::LyricWidget *ui;
  LyricModel *m_lyricModel = nullptr;

  bool m_isLocked;
  int m_fontSize; // 中间行字体大小
  int m_textAlpha;
  int m_currentIndex;
  QPoint m_dragPos;

  // 歌词绘制常量
  static constexpr int LINE_SPACING = 25;    // 行间距
  static constexpr int SIDE_FONT_OFFSET = 6; // 非高亮行字体缩小量

  // 滚动相关
  qreal m_scrollOffset;       // 当前滚动像素偏移
  qreal m_targetScrollOffset; // 目标滚动偏移
  QTimer *m_scrollTimer;      // 平滑滚动定时器
  QTimer *m_wheelTimer;       // 滚轮停止后自动回弹的定时器
  bool m_userScrolling;       // 用户是否正在手动滚动

  void centerControlWidget();
  QString findLrcFile(const QString &songFilePath);
  void scrollToCurrent(bool animated = true);
  int lineHeight() const;
  int contentHeight() const;
  int centerLineOffset() const;         // 中间行（高亮行）距离顶部的偏移
  int centerOffsetForNoCurrent() const; // 无当前行时歌词整体居中偏移
};
#endif // LYRICWIDGET_H
