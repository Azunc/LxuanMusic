#include "thememanager.h"

#include <QApplication>
#include <QFile>

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
{
}

bool ThemeManager::loadStyleSheet(const QString &qssPath)
{
  QFile file(qssPath);
  if (!file.open(QFile::ReadOnly | QFile::Text))
  {
    return false;
  }

  QString style = QString::fromUtf8(file.readAll());
  file.close();

  qApp->setStyleSheet(style);
  m_currentPath = qssPath;
  emit themeChanged(qssPath);
  return true;
}

bool ThemeManager::loadDefaultTheme()
{
  return loadStyleSheet(QStringLiteral(":/styles/style.qss"));
}

QString ThemeManager::currentThemePath() const
{
  return m_currentPath;
}
