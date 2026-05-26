#include "configdao.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

namespace
{
  QString configFilePath()
  {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDir.isEmpty())
    {
      configDir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config"));
    }

    QDir dir(configDir);
    if (!dir.exists())
    {
      dir.mkpath(QStringLiteral("."));
    }

    return dir.filePath(QStringLiteral("LxuanMusic.ini"));
  }
}

ConfigDao::ConfigDao()
    : m_settings(configFilePath(), QSettings::IniFormat)
{
}

ConfigDao *ConfigDao::instance()
{
  static ConfigDao ins;
  return &ins;
}

QVariant ConfigDao::value(const QString &key, const QVariant &defaultValue) const
{
  return m_settings.value(key, defaultValue);
}

bool ConfigDao::boolValue(const QString &key, bool defaultValue) const
{
  return m_settings.value(key, defaultValue).toBool();
}

QString ConfigDao::stringValue(const QString &key, const QString &defaultValue) const
{
  return m_settings.value(key, defaultValue).toString();
}

void ConfigDao::setValue(const QString &key, const QVariant &value)
{
  m_settings.setValue(key, value);
  m_settings.sync();
}

void ConfigDao::sync()
{
  m_settings.sync();
}

QString ConfigDao::filePath() const
{
  return m_settings.fileName();
}
