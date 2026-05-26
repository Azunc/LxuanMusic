#include "settingcontroller.h"

#include "./dao/configdao.h"

SettingController::SettingController(QObject *parent)
    : QObject(parent)
{
  m_configDao = ConfigDao::instance();
}

QVariant SettingController::value(const QString &key, const QVariant &defaultValue) const
{
  return m_configDao ? m_configDao->value(key, defaultValue) : defaultValue;
}

bool SettingController::boolValue(const QString &key, bool defaultValue) const
{
  return m_configDao ? m_configDao->boolValue(key, defaultValue) : defaultValue;
}

QString SettingController::stringValue(const QString &key, const QString &defaultValue) const
{
  return m_configDao ? m_configDao->stringValue(key, defaultValue) : defaultValue;
}

void SettingController::setValue(const QString &key, const QVariant &value)
{
  if (m_configDao)
  {
    m_configDao->setValue(key, value);
    emit settingChanged(key, value);
  }
}

void SettingController::sync()
{
  if (m_configDao)
    m_configDao->sync();
}

int SettingController::volume() const
{
  return boolValue(QStringLiteral("volume/enabled"), true)
             ? value(QStringLiteral("volume/value"), 80).toInt()
             : 0;
}

void SettingController::setVolume(int volume)
{
  setValue(QStringLiteral("volume/value"), volume);
}

QString SettingController::lastImportDir() const
{
  return stringValue(QStringLiteral("library/lastImportDir"), QString());
}

void SettingController::setLastImportDir(const QString &dir)
{
  setValue(QStringLiteral("library/lastImportDir"), dir);
}

bool SettingController::hotkeyEnabled() const
{
  return boolValue(QStringLiteral("hotkey/enabled"), true);
}

void SettingController::setHotkeyEnabled(bool enabled)
{
  setValue(QStringLiteral("hotkey/enabled"), enabled);
}
