/***************************************************
 *  @file      configdao.h
 *  @brief     本地配置文件读写：存轻量配置（窗口大小、上次播放位置、主题选择等），存在本地ini配置文件里
 *
 *  @author    un
 *  @date      2026/04/13
 *  @history
 ****************************************************/
#ifndef CONFIGDAO_H
#define CONFIGDAO_H

#include <QSettings>
#include <QString>
#include <QVariant>
#include <QtGlobal>

class ConfigDao
{
public:
  static ConfigDao *instance();

  QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
  bool boolValue(const QString &key, bool defaultValue = false) const;
  QString stringValue(const QString &key, const QString &defaultValue = QString()) const;
  void setValue(const QString &key, const QVariant &value);
  void sync();
  QString filePath() const;

private:
  ConfigDao();
  Q_DISABLE_COPY(ConfigDao)

private:
  QSettings m_settings;
};

#endif // CONFIGDAO_H
