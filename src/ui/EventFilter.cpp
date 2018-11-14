//
// Created by Tobias Hieta on 07/03/16.
//

#include "EventFilter.h"
#include "system/SystemComponent.h"
#include "settings/SettingsComponent.h"
#include "input/InputKeyboard.h"
#include "KonvergoWindow.h"

#include <QKeyEvent>
#include <QObject>

static QStringList win32AppcommandBlackListedKeys = { "Media Play",
                                                      "Media Pause",
                                                      "Media Stop",
                                                      "Media Next",
                                                      "Media Previous",
                                                      "Media Rewind",
                                                      "Media FastForward",
                                                      "Back"};

///////////////////////////////////////////////////////////////////////////////////////////////////
static QString keyEventToKeyString(QKeyEvent *kevent)
{
  // We ignore the KeypadModifier here since it's practically useless
  QKeySequence modifiers(kevent->modifiers() &= ~Qt::KeypadModifier);

  QKeySequence keySeq(kevent->key());
  QString key = keySeq.toString();

  // Qt tends to make up something weird for keys which don't cleanly map to text.
  // See e.g. QKeySequencePrivate::keyName() in the Qt sources.
  // We can't really know for sure which names are "good" or "bad", so we simply
  // allow printable latin1 characters, and mangle everything else.
  if ((key.size() > 0 && (key[0].unicode() < 32 || key[0].unicode() > 255)))
  {
    if (kevent->nativeVirtualKey() != 0)
    {
      key = "0x" + QString::number(kevent->nativeVirtualKey(), 16) + "V";
    }
    else
    {
      QString properKey;
      for (int n = 0; n < key.size(); n++)
        properKey += QString(n > 0 ? "+" : "") + "0x" + QString::number(key[n].unicode(), 16) + "Q";
      key = properKey;
    }
  }

  return modifiers.toString() + key;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool EventFilter::eventFilter(QObject* watched, QEvent* event)
{
  KonvergoWindow* window = qobject_cast<KonvergoWindow*>(parent());

  SystemComponent& system = SystemComponent::Get();

  // ignore mouse events if mouse is disabled
  if  (SettingsComponent::Get().value(SETTINGS_SECTION_MAIN, "disablemouse").toBool() &&
       ((event->type() == QEvent::MouseMove) ||
        (event->type() == QEvent::MouseButtonPress) ||
        (event->type() == QEvent::MouseButtonRelease) ||
        (event->type() == QEvent::MouseButtonDblClick)))
  {
    return true;
  }

  if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
  {
    // In konvergo we intercept all keyboard events and translate them
    // into web client actions. We need to do this so that we can remap
    // keyboard buttons to different events.
    //

    InputBase::InputkeyState keystatus;

    if (event->type() == QEvent::KeyPress)
      keystatus = InputBase::KeyDown;
    else
      keystatus = InputBase::KeyUp;

    QKeyEvent* kevent = dynamic_cast<QKeyEvent*>(event);
    if (!kevent)
      return QObject::eventFilter(watched, event);

    QString keyName = keyEventToKeyString(kevent);

#ifdef Q_OS_WIN32
    // On Windows, we get some media keys twice. This is because Windows supports both
    // normal key events and APPCOMMAND_ messages for media keys. To avoid getting two
    // key events per key press, filter out non-APPCOMMAND media keys by using the fact
    // that APPCOMMAND media keys won't have a key code.
    if (win32AppcommandBlackListedKeys.contains(keyName) && kevent->nativeVirtualKey())
      return QObject::eventFilter(watched, event);
#endif

    if (keystatus == InputBase::KeyDown)
    {
      // Swallow auto-repeated keys (isAutoRepeat doesn't always work - QTBUG-57335)
      // Do this only for non-modifier keys (QKeyEvent::text serves as a heuristic
      // to distinguish them from normal key presses)
      if (kevent->text().size())
      {
        if (m_currentKeyDown)
          return true;
        m_currentKeyDown = true;
      }
    }
    else
      m_currentKeyDown = false;

    system.setCursorVisibility(false);
    if (kevent->spontaneous() && !kevent->isAutoRepeat())
    {
      InputKeyboard::Get().keyPress(keyName, keystatus);
      return true;
    }
  }
  else if (event->type() == QEvent::MouseMove)
  {
    system.setCursorVisibility(true);
  }
  else if (event->type() == QEvent::Wheel)
  {
    return true;
  }
  else if (event->type() == QEvent::MouseButtonPress)
  {
    // ignore right clicks that would show context menu
    QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent*>(event);
    if ((mouseEvent) && (mouseEvent->button() == Qt::RightButton))
      return true;
  }
  else if (event->type() == QEvent::Drop)
  {
    // QtWebEngine would accept the drop and unload web-client.
    return true;
  }

  return QObject::eventFilter(watched, event);
}
