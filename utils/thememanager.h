/***************************************************
 *  @file      thememanager.h
 *  @brief     主题管理工具：加载明暗主题/自定义皮肤的qss文件，切换时自动刷新所有UI样式
 *
 *  @author    un
 *  @date      2026/04/13
 *  @history
 ****************************************************/
#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QString>

class ThemeManager : public QObject
{
  Q_OBJECT
public:
  explicit ThemeManager(QObject *parent = nullptr);

  // 加载 QSS 文件并应用到全局
  bool loadStyleSheet(const QString &qssPath);
  // 加载内置默认主题
  bool loadDefaultTheme();
  // 当前主题路径
  QString currentThemePath() const;

signals:
  void themeChanged(const QString &qssPath);

private:
  QString m_currentPath;
};

#endif // THEMEMANAGER_H
