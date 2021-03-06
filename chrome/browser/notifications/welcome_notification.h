// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_WELCOME_NOTIFICATION_H_
#define CHROME_BROWSER_NOTIFICATIONS_WELCOME_NOTIFICATION_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_member.h"
#include "chrome/browser/prefs/pref_service_syncable_observer.h"
#include "ui/message_center/notifier_settings.h"

namespace message_center {
class MessageCenter;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class Notification;
class Profile;

// WelcomeNotification is a part of DesktopNotificationService and manages
// showing and hiding a welcome notification for built-in components that
// show notifications. The Welcome Notification presumes network connectivity
// since it relies on synced preferences to work. This is generally fine since
// the current consumers on the welcome notification also presume network
// connectivity.
class WelcomeNotification
    : public PrefServiceSyncableObserver {
 public:
  WelcomeNotification(
      Profile* profile,
      message_center::MessageCenter* message_center);
  virtual ~WelcomeNotification();

  // PrefServiceSyncableObserver
  virtual void OnIsSyncingChanged() OVERRIDE;

  // Adds in a the welcome notification if required for components built
  // into Chrome that show notifications like Chrome Now.
  void ShowWelcomeNotificationIfNecessary(
      const Notification& notification);

  // Handles Preference Registeration for the Welcome Notification.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* prefs);

 private:

  enum PopUpRequest {
    POP_UP_HIDDEN = 0,
    POP_UP_SHOWN = 1,
  };

  // Unconditionally shows the welcome notification.
  void ShowWelcomeNotification(
      const message_center::NotifierId notifier_id,
      const string16& display_source,
      PopUpRequest pop_up_request);

  // Hides the welcome notification.
  void HideWelcomeNotification();

  // Called when the Welcome Notification Dismissed pref has been changed.
  void OnWelcomeNotificationDismissedChanged();

  // Prefs listener for welcome_notification_dismissed.
  BooleanPrefMember welcome_notification_dismissed_pref_;

  // The profile which owns this object.
  Profile* profile_;

  // Notification ID of the Welcome Notification.
  std::string welcome_notification_id_;

  // If the preferences are still syncing, store the last notification here
  // so we can replay ShowWelcomeNotificationIfNecessary once the sync finishes.
  // Simplifying Assumption: The delayed notification has passed the
  // extension ID check. This means we do not need to store all of the
  // notifications that may also show a welcome notification.
  scoped_ptr<Notification> delayed_notification_;

  message_center::MessageCenter* message_center_;  // Weak reference.
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_WELCOME_NOTIFICATION_H_
