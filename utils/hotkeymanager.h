/***************************************************
 *  @file      hotkeymanager.h
 *  @brief     全局快捷键工具：注册系统级快捷键，实现后台切歌/暂停
 *
 *  @author    un
 *  @date      2026/04/13
 *  @history
 ****************************************************/
#ifndef HOTKEYMANAGER_H
#define HOTKEYMANAGER_H

#include <QAbstractNativeEventFilter>
#include <QHash>
#include <QObject>
#include <QString>

class HotKeyManager : public QObject, public QAbstractNativeEventFilter
{
  Q_OBJECT
public:
  explicit HotKeyManager(QObject *parent = nullptr);
  ~HotKeyManager() override;

  bool setEnabled(bool enabled);
  bool isEnabled() const;

  bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

signals:
  void sigTogglePlayPause();
  void sigNextTrack();
  void sigPreviousTrack();
  void sigHotKeyRegistrationChanged(bool enabled);

private:
  struct HotKeyDef
  {
    int id = 0;
    QString actionName;
    unsigned int modifiers = 0;
    unsigned int key = 0;
  };

  bool registerDefaults();
  bool registerHotKey(const QString &actionName, unsigned int modifiers, unsigned int key);
  void unregisterAll();
  void emitAction(const QString &actionName);
  int nextHotKeyId() const;

private:
  bool m_enabled = false;
  int m_baseId = 1200;
  QHash<int, HotKeyDef> m_hotKeys;
};

#endif // HOTKEYMANAGER_H
