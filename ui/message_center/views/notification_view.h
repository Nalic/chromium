// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_H_

#include <vector>

#include "ui/message_center/message_center_export.h"
#include "ui/message_center/views/message_view.h"

namespace views {
class ProgressBar;
}

namespace message_center {

class BoundedLabel;
class MessageCenter;
class NotificationView;
class PaddedButton;

// View that displays all current types of notification (web, basic, image, and
// list). Future notification types may be handled by other classes, in which
// case instances of those classes would be returned by the Create() factory
// method below.
class MESSAGE_CENTER_EXPORT NotificationView : public MessageView {
 public:
  // Creates appropriate MessageViews for notifications. Those currently are
  // always NotificationView instances but in the future
  // may be instances of other classes, with the class depending on the
  // notification type. A notification is top level if it needs to be rendered
  // outside the browser window. No custom shadows are created for top level
  // notifications on Linux with Aura.
  static NotificationView* Create(const Notification& notification,
                                  MessageCenter* message_center,
                                  MessageCenterTray* tray,
                                  bool expanded,
                                  bool top_level);

  virtual ~NotificationView();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual int GetHeightForWidth(int width) OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void ScrollRectToVisible(const gfx::Rect& rect) OVERRIDE;
  virtual views::View* GetEventHandlerForRect(const gfx::Rect& rect) OVERRIDE;
  virtual gfx::NativeCursor GetCursor(const ui::MouseEvent& event) OVERRIDE;

  // Overridden from MessageView:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  std::string notification_id() { return notification_id_; }

 protected:
  NotificationView(const Notification& notification,
                   MessageCenter* message_center,
                   MessageCenterTray* tray,
                   bool expanded);

  // Overrides from base class MessageView:
  virtual void ClickOnNotification() OVERRIDE;
  virtual void RemoveNotification(bool by_user) OVERRIDE;
  virtual void DisableNotificationsFromThisSource() OVERRIDE;
  virtual void ShowNotifierSettingsBubble() OVERRIDE;

 private:
  bool IsExpansionNeeded(int width);
  bool IsMessageExpansionNeeded(int width);
  int GetMessageLineLimit(int width);
  int GetMessageLines(int width, int limit);
  int GetMessageHeight(int width, int limit);

  MessageCenter* message_center_;  // Weak.
  MessageCenterTray* tray_;  // Weak.
  std::string notification_id_;
  message_center::NotifierId notifier_id_;

  // Describes whether the view should display a hand pointer or not.
  bool clickable_;
  bool is_expanded_;

  // Weak references to NotificationView descendants owned by their parents.
  views::View* background_view_;
  views::View* top_view_;
  BoundedLabel* title_view_;
  BoundedLabel* message_view_;
  BoundedLabel* context_message_view_;
  std::vector<views::View*> item_views_;
  views::View* icon_view_;
  views::View* bottom_view_;
  views::View* image_view_;
  views::ProgressBar* progress_bar_view_;
  std::vector<views::View*> action_buttons_;
  PaddedButton* expand_button_;

  DISALLOW_COPY_AND_ASSIGN(NotificationView);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_H_
