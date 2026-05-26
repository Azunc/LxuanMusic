#include "hotkeymanager.h"

#include <QCoreApplication>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

HotKeyManager::HotKeyManager(QObject *parent)
    : QObject(parent)
{
}

HotKeyManager::~HotKeyManager()
{
  setEnabled(false);
}

bool HotKeyManager::setEnabled(bool enabled)
{
  if (m_enabled == enabled)
  {
    return true;
  }

  if (enabled)
  {
    if (!registerDefaults())
    {
      qWarning() << "[HotKeyManager] 注册默认热键失败";
      unregisterAll();
      return false;
    }

    if (QCoreApplication::instance())
    {
      QCoreApplication::instance()->installNativeEventFilter(this);
      qDebug() << "[HotKeyManager] 已安装原生事件过滤器";
    }
    m_enabled = true;
    emit sigHotKeyRegistrationChanged(true);
    qDebug() << "[HotKeyManager] 热键注册成功，共" << m_hotKeys.size() << "个";
    return true;
  }

  if (QCoreApplication::instance())
  {
    QCoreApplication::instance()->removeNativeEventFilter(this);
    qDebug() << "[HotKeyManager] 已移除原生事件过滤器";
  }
  unregisterAll();
  m_enabled = false;
  emit sigHotKeyRegistrationChanged(false);
  qDebug() << "[HotKeyManager] 热键已注销";
  return true;
}

bool HotKeyManager::isEnabled() const
{
  return m_enabled;
}

bool HotKeyManager::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
  Q_UNUSED(eventType)

#ifdef Q_OS_WIN
  MSG *msg = static_cast<MSG *>(message);
  if (!msg || msg->message != WM_HOTKEY)
  {
    return false;
  }

  const int hotKeyId = static_cast<int>(msg->wParam);
  qDebug() << "[HotKeyManager] 收到 WM_HOTKEY, id=" << hotKeyId;
  if (!m_hotKeys.contains(hotKeyId))
  {
    qWarning() << "[HotKeyManager] 未知热键 id=" << hotKeyId;
    return false;
  }

  const QString action = m_hotKeys.value(hotKeyId).actionName;
  qDebug() << "[HotKeyManager] 触发动作:" << action;
  emitAction(action);
  if (result)
  {
    *result = 0;
  }
  return true;
#else
  Q_UNUSED(message)
  Q_UNUSED(result)
  return false;
#endif
}

bool HotKeyManager::registerDefaults()
{
#ifdef Q_OS_WIN
  if (!registerHotKey(QStringLiteral("play_pause"), MOD_CONTROL | MOD_SHIFT, VK_SPACE))
  {
    return false;
  }
  if (!registerHotKey(QStringLiteral("previous"), MOD_CONTROL | MOD_SHIFT, VK_LEFT))
  {
    unregisterAll();
    return false;
  }
  if (!registerHotKey(QStringLiteral("next"), MOD_CONTROL | MOD_SHIFT, VK_RIGHT))
  {
    unregisterAll();
    return false;
  }
  return true;
#else
  return false;
#endif
}

bool HotKeyManager::registerHotKey(const QString &actionName, unsigned int modifiers, unsigned int key)
{
#ifdef Q_OS_WIN
  const int id = nextHotKeyId();
  if (!RegisterHotKey(nullptr, id, modifiers, key))
  {
    const DWORD err = GetLastError();
    qWarning() << "[HotKeyManager] RegisterHotKey 失败, id=" << id
               << "mod=" << modifiers << "key=" << key
               << "err=" << err;
    return false;
  }
  qDebug() << "[HotKeyManager] RegisterHotKey 成功, id=" << id
           << "action=" << actionName << "mod=" << modifiers << "key=" << key;

  HotKeyDef def;
  def.id = id;
  def.actionName = actionName;
  def.modifiers = modifiers;
  def.key = key;
  m_hotKeys.insert(id, def);
  return true;
#else
  Q_UNUSED(actionName)
  Q_UNUSED(modifiers)
  Q_UNUSED(key)
  return false;
#endif
}

void HotKeyManager::unregisterAll()
{
#ifdef Q_OS_WIN
  const QList<int> ids = m_hotKeys.keys();
  for (int id : ids)
  {
    UnregisterHotKey(nullptr, id);
  }
#endif
  m_hotKeys.clear();
}

void HotKeyManager::emitAction(const QString &actionName)
{
  if (actionName == QStringLiteral("play_pause"))
  {
    emit sigTogglePlayPause();
  }
  else if (actionName == QStringLiteral("next"))
  {
    emit sigNextTrack();
  }
  else if (actionName == QStringLiteral("previous"))
  {
    emit sigPreviousTrack();
  }
}

int HotKeyManager::nextHotKeyId() const
{
  return m_baseId + m_hotKeys.size() + 1;
}
