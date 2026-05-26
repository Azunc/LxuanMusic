/***************************************************
 *  @file      settingcontroller.h
 *  @brief     设置业务调度：接收用户修改的设置，存到存储层，同步给全局生效（比如注册全局快捷键）
 *             当前主要承担轻量设置调度与后续扩展预留职责。
 *
 *  @author    un
 *  @date      2026/04/13
 *  @history
 ****************************************************/
#ifndef SETTINGCONTROLLER_H
#define SETTINGCONTROLLER_H

#include <QObject>
#include <QString>
#include <QVariant>

class ConfigDao;

class SettingController : public QObject
{
  Q_OBJECT
public:
  explicit SettingController(QObject *parent = nullptr);

  // 配置读写（统一调度，避免View直接操作ConfigDao）
  QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
  bool boolValue(const QString &key, bool defaultValue = false) const;
  QString stringValue(const QString &key, const QString &defaultValue = QString()) const;
  void setValue(const QString &key, const QVariant &value);
  void sync();

  // 播放相关设置快捷接口
  int volume() const;
  void setVolume(int volume);

  QString lastImportDir() const;
  void setLastImportDir(const QString &dir);

  bool hotkeyEnabled() const;
  void setHotkeyEnabled(bool enabled);

signals:
  void settingChanged(const QString &key, const QVariant &value);

private:
  ConfigDao *m_configDao = nullptr;
};

#endif // SETTINGCONTROLLER_H
