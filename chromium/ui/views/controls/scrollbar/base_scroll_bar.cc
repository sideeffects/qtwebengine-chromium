// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scrollbar/base_scroll_bar.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"
#include "ui/views/widget/widget.h"

#undef min
#undef max

namespace views {

///////////////////////////////////////////////////////////////////////////////
// BaseScrollBar, public:

BaseScrollBar::BaseScrollBar(bool horizontal, BaseScrollBarThumb* thumb)
    : ScrollBar(horizontal),
      thumb_(thumb),
      contents_size_(0),
      contents_scroll_offset_(0),
      viewport_size_(0),
      thumb_track_state_(CustomButton::STATE_NORMAL),
      last_scroll_amount_(SCROLL_NONE),
      repeater_(base::Bind(&BaseScrollBar::TrackClicked,
                           base::Unretained(this))),
      context_menu_mouse_position_(0) {
  AddChildView(thumb_);

  set_context_menu_controller(this);
  thumb_->set_context_menu_controller(this);
}

void BaseScrollBar::ScrollByAmount(ScrollAmount amount) {
  int offset = contents_scroll_offset_;
  switch (amount) {
    case SCROLL_START:
      offset = GetMinPosition();
      break;
    case SCROLL_END:
      offset = GetMaxPosition();
      break;
    case SCROLL_PREV_LINE:
      offset -= GetScrollIncrement(false, false);
      offset = std::max(GetMinPosition(), offset);
      break;
    case SCROLL_NEXT_LINE:
      offset += GetScrollIncrement(false, true);
      offset = std::min(GetMaxPosition(), offset);
      break;
    case SCROLL_PREV_PAGE:
      offset -= GetScrollIncrement(true, false);
      offset = std::max(GetMinPosition(), offset);
      break;
    case SCROLL_NEXT_PAGE:
      offset += GetScrollIncrement(true, true);
      offset = std::min(GetMaxPosition(), offset);
      break;
    default:
      break;
  }
  contents_scroll_offset_ = offset;
  ScrollContentsToOffset();
}

BaseScrollBar::~BaseScrollBar() {
}

void BaseScrollBar::ScrollToThumbPosition(int thumb_position,
                                          bool scroll_to_middle) {
  contents_scroll_offset_ =
      CalculateContentsOffset(thumb_position, scroll_to_middle);
  if (contents_scroll_offset_ < GetMinPosition()) {
    contents_scroll_offset_ = GetMinPosition();
  } else if (contents_scroll_offset_ > GetMaxPosition()) {
    contents_scroll_offset_ = GetMaxPosition();
  }
  ScrollContentsToOffset();
  SchedulePaint();
}

bool BaseScrollBar::ScrollByContentsOffset(int contents_offset) {
  int old_offset = contents_scroll_offset_;
  contents_scroll_offset_ -= contents_offset;
  if (contents_scroll_offset_ < GetMinPosition()) {
    contents_scroll_offset_ = GetMinPosition();
  } else if (contents_scroll_offset_ > GetMaxPosition()) {
    contents_scroll_offset_ = GetMaxPosition();
  }
  if (old_offset == contents_scroll_offset_)
    return false;

  ScrollContentsToOffset();
  return true;
}

void BaseScrollBar::OnThumbStateChanged(CustomButton::ButtonState old_state,
                                        CustomButton::ButtonState new_state) {
  if (old_state == CustomButton::STATE_PRESSED &&
      new_state == CustomButton::STATE_NORMAL &&
      GetThumbTrackState() == CustomButton::STATE_HOVERED) {
    SetThumbTrackState(CustomButton::STATE_NORMAL);
  }
}

///////////////////////////////////////////////////////////////////////////////
// BaseScrollBar, View implementation:

bool BaseScrollBar::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton())
    ProcessPressEvent(event);
  return true;
}

void BaseScrollBar::OnMouseReleased(const ui::MouseEvent& event) {
  SetState(HitTestPoint(event.location()) ?
           CustomButton::STATE_HOVERED : CustomButton::STATE_NORMAL);
}

void BaseScrollBar::OnMouseCaptureLost() {
  SetState(CustomButton::STATE_NORMAL);
}

void BaseScrollBar::OnMouseEntered(const ui::MouseEvent& event) {
  SetThumbTrackState(CustomButton::STATE_HOVERED);
}

void BaseScrollBar::OnMouseExited(const ui::MouseEvent& event) {
  if (GetThumbTrackState() == CustomButton::STATE_HOVERED)
    SetState(CustomButton::STATE_NORMAL);
}

bool BaseScrollBar::OnKeyPressed(const ui::KeyEvent& event) {
  ScrollAmount amount = SCROLL_NONE;
  switch (event.key_code()) {
    case ui::VKEY_UP:
      if (!IsHorizontal())
        amount = SCROLL_PREV_LINE;
      break;
    case ui::VKEY_DOWN:
      if (!IsHorizontal())
        amount = SCROLL_NEXT_LINE;
      break;
    case ui::VKEY_LEFT:
      if (IsHorizontal())
        amount = SCROLL_PREV_LINE;
      break;
    case ui::VKEY_RIGHT:
      if (IsHorizontal())
        amount = SCROLL_NEXT_LINE;
      break;
    case ui::VKEY_PRIOR:
      amount = SCROLL_PREV_PAGE;
      break;
    case ui::VKEY_NEXT:
      amount = SCROLL_NEXT_PAGE;
      break;
    case ui::VKEY_HOME:
      amount = SCROLL_START;
      break;
    case ui::VKEY_END:
      amount = SCROLL_END;
      break;
    default:
      break;
  }
  if (amount != SCROLL_NONE) {
    ScrollByAmount(amount);
    return true;
  }
  return false;
}

bool BaseScrollBar::OnMouseWheel(const ui::MouseWheelEvent& event) {
  OnScroll(event.x_offset(), event.y_offset());
  return true;
}

void BaseScrollBar::OnGestureEvent(ui::GestureEvent* event) {
  // If a fling is in progress, then stop the fling for any incoming gesture
  // event (except for the GESTURE_END event that is generated at the end of the
  // fling).
  if (scroll_animator_.get() && scroll_animator_->is_scrolling() &&
      (event->type() != ui::ET_GESTURE_END ||
       event->details().touch_points() > 1)) {
    scroll_animator_->Stop();
  }

  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    ProcessPressEvent(*event);
    event->SetHandled();
    return;
  }

  if (event->type() == ui::ET_GESTURE_LONG_PRESS) {
    // For a long-press, the repeater started in tap-down should continue. So
    // return early.
    return;
  }

  SetState(CustomButton::STATE_NORMAL);

  if (event->type() == ui::ET_GESTURE_TAP) {
    // TAP_DOWN would have already scrolled some amount. So scrolling again on
    // TAP is not necessary.
    event->SetHandled();
    return;
  }

  if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN ||
      event->type() == ui::ET_GESTURE_SCROLL_END) {
    event->SetHandled();
    return;
  }

  if (event->type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    float scroll_amount_f;
    int scroll_amount;
    if (IsHorizontal()) {
      scroll_amount_f = event->details().scroll_x() - roundoff_error_.x();
      scroll_amount = gfx::ToRoundedInt(scroll_amount_f);
      roundoff_error_.set_x(scroll_amount - scroll_amount_f);
    } else {
      scroll_amount_f = event->details().scroll_y() - roundoff_error_.y();
      scroll_amount = gfx::ToRoundedInt(scroll_amount_f);
      roundoff_error_.set_y(scroll_amount - scroll_amount_f);
    }
    if (ScrollByContentsOffset(scroll_amount))
      event->SetHandled();
    return;
  }

  if (event->type() == ui::ET_SCROLL_FLING_START) {
    if (!scroll_animator_.get())
      scroll_animator_.reset(new ScrollAnimator(this));
    scroll_animator_->Start(
        IsHorizontal() ?  event->details().velocity_x() : 0.f,
        IsHorizontal() ? 0.f : event->details().velocity_y());
    event->SetHandled();
  }
}

///////////////////////////////////////////////////////////////////////////////
// BaseScrollBar, ScrollDelegate implementation:

bool BaseScrollBar::OnScroll(float dx, float dy) {
  return IsHorizontal() ? ScrollByContentsOffset(dx) :
                          ScrollByContentsOffset(dy);
}

///////////////////////////////////////////////////////////////////////////////
// BaseScrollBar, ContextMenuController implementation:

enum ScrollBarContextMenuCommands {
  ScrollBarContextMenuCommand_ScrollHere = 1,
  ScrollBarContextMenuCommand_ScrollStart,
  ScrollBarContextMenuCommand_ScrollEnd,
  ScrollBarContextMenuCommand_ScrollPageUp,
  ScrollBarContextMenuCommand_ScrollPageDown,
  ScrollBarContextMenuCommand_ScrollPrev,
  ScrollBarContextMenuCommand_ScrollNext
};

void BaseScrollBar::ShowContextMenuForView(View* source,
                                           const gfx::Point& p,
                                           ui::MenuSourceType source_type) {
  Widget* widget = GetWidget();
  gfx::Rect widget_bounds = widget->GetWindowBoundsInScreen();
  gfx::Point temp_pt(p.x() - widget_bounds.x(), p.y() - widget_bounds.y());
  View::ConvertPointFromWidget(this, &temp_pt);
  context_menu_mouse_position_ = IsHorizontal() ? temp_pt.x() : temp_pt.y();

  views::MenuItemView* menu = new views::MenuItemView(this);
  // MenuRunner takes ownership of |menu|.
  menu_runner_.reset(new MenuRunner(
      menu, MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU));
  menu->AppendDelegateMenuItem(ScrollBarContextMenuCommand_ScrollHere);
  menu->AppendSeparator();
  menu->AppendDelegateMenuItem(ScrollBarContextMenuCommand_ScrollStart);
  menu->AppendDelegateMenuItem(ScrollBarContextMenuCommand_ScrollEnd);
  menu->AppendSeparator();
  menu->AppendDelegateMenuItem(ScrollBarContextMenuCommand_ScrollPageUp);
  menu->AppendDelegateMenuItem(ScrollBarContextMenuCommand_ScrollPageDown);
  menu->AppendSeparator();
  menu->AppendDelegateMenuItem(ScrollBarContextMenuCommand_ScrollPrev);
  menu->AppendDelegateMenuItem(ScrollBarContextMenuCommand_ScrollNext);
  if (menu_runner_->RunMenuAt(GetWidget(),
                              NULL,
                              gfx::Rect(p, gfx::Size()),
                              MENU_ANCHOR_TOPLEFT,
                              source_type) == MenuRunner::MENU_DELETED) {
    return;
  }
}

///////////////////////////////////////////////////////////////////////////////
// BaseScrollBar, Menu::Delegate implementation:

base::string16 BaseScrollBar::GetLabel(int id) const {
  int ids_value = 0;
  switch (id) {
    case ScrollBarContextMenuCommand_ScrollHere:
      ids_value = IDS_APP_SCROLLBAR_CXMENU_SCROLLHERE;
      break;
    case ScrollBarContextMenuCommand_ScrollStart:
      ids_value = IsHorizontal() ? IDS_APP_SCROLLBAR_CXMENU_SCROLLLEFTEDGE
                                 : IDS_APP_SCROLLBAR_CXMENU_SCROLLHOME;
      break;
    case ScrollBarContextMenuCommand_ScrollEnd:
      ids_value = IsHorizontal() ? IDS_APP_SCROLLBAR_CXMENU_SCROLLRIGHTEDGE
                                 : IDS_APP_SCROLLBAR_CXMENU_SCROLLEND;
      break;
    case ScrollBarContextMenuCommand_ScrollPageUp:
      ids_value = IDS_APP_SCROLLBAR_CXMENU_SCROLLPAGEUP;
      break;
    case ScrollBarContextMenuCommand_ScrollPageDown:
      ids_value = IDS_APP_SCROLLBAR_CXMENU_SCROLLPAGEDOWN;
      break;
    case ScrollBarContextMenuCommand_ScrollPrev:
      ids_value = IsHorizontal() ? IDS_APP_SCROLLBAR_CXMENU_SCROLLLEFT
                                 : IDS_APP_SCROLLBAR_CXMENU_SCROLLUP;
      break;
    case ScrollBarContextMenuCommand_ScrollNext:
      ids_value = IsHorizontal() ? IDS_APP_SCROLLBAR_CXMENU_SCROLLRIGHT
                                 : IDS_APP_SCROLLBAR_CXMENU_SCROLLDOWN;
      break;
    default:
      NOTREACHED() << "Invalid BaseScrollBar Context Menu command!";
  }

  return ids_value ? l10n_util::GetStringUTF16(ids_value) : base::string16();
}

bool BaseScrollBar::IsCommandEnabled(int id) const {
  switch (id) {
    case ScrollBarContextMenuCommand_ScrollPageUp:
    case ScrollBarContextMenuCommand_ScrollPageDown:
      return !IsHorizontal();
  }
  return true;
}

void BaseScrollBar::ExecuteCommand(int id) {
  switch (id) {
    case ScrollBarContextMenuCommand_ScrollHere:
      ScrollToThumbPosition(context_menu_mouse_position_, true);
      break;
    case ScrollBarContextMenuCommand_ScrollStart:
      ScrollByAmount(SCROLL_START);
      break;
    case ScrollBarContextMenuCommand_ScrollEnd:
      ScrollByAmount(SCROLL_END);
      break;
    case ScrollBarContextMenuCommand_ScrollPageUp:
      ScrollByAmount(SCROLL_PREV_PAGE);
      break;
    case ScrollBarContextMenuCommand_ScrollPageDown:
      ScrollByAmount(SCROLL_NEXT_PAGE);
      break;
    case ScrollBarContextMenuCommand_ScrollPrev:
      ScrollByAmount(SCROLL_PREV_LINE);
      break;
    case ScrollBarContextMenuCommand_ScrollNext:
      ScrollByAmount(SCROLL_NEXT_LINE);
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////
// BaseScrollBar, ScrollBar implementation:

void BaseScrollBar::Update(int viewport_size,
                           int content_size,
                           int contents_scroll_offset) {
  ScrollBar::Update(viewport_size, content_size, contents_scroll_offset);

  // Make sure contents_size is always > 0 to avoid divide by zero errors in
  // calculations throughout this code.
  contents_size_ = std::max(1, content_size);

  viewport_size_ = std::max(1, viewport_size);

  if (content_size < 0)
    content_size = 0;
  if (contents_scroll_offset < 0)
    contents_scroll_offset = 0;
  if (contents_scroll_offset > content_size)
    contents_scroll_offset = content_size;
  contents_scroll_offset_ = contents_scroll_offset;

  // Thumb Height and Thumb Pos.
  // The height of the thumb is the ratio of the Viewport height to the
  // content size multiplied by the height of the thumb track.
  double ratio =
      std::min(1.0, static_cast<double>(viewport_size) / contents_size_);
  int thumb_size = static_cast<int>(ratio * GetTrackSize());
  thumb_->SetSize(thumb_size);

  int thumb_position = CalculateThumbPosition(contents_scroll_offset);
  thumb_->SetPosition(thumb_position);
}

int BaseScrollBar::GetPosition() const {
  return thumb_->GetPosition();
}

///////////////////////////////////////////////////////////////////////////////
// BaseScrollBar, protected:

BaseScrollBarThumb* BaseScrollBar::GetThumb() const {
  return thumb_;
}

CustomButton::ButtonState BaseScrollBar::GetThumbTrackState() const {
  return thumb_track_state_;
}

void BaseScrollBar::ScrollToPosition(int position) {
  controller()->ScrollToPosition(this, position);
}

int BaseScrollBar::GetScrollIncrement(bool is_page, bool is_positive) {
  return controller()->GetScrollIncrement(this, is_page, is_positive);
}

///////////////////////////////////////////////////////////////////////////////
// BaseScrollBar, private:

int BaseScrollBar::GetThumbSizeForTest() {
  return thumb_->GetSize();
}

void BaseScrollBar::ProcessPressEvent(const ui::LocatedEvent& event) {
  SetThumbTrackState(CustomButton::STATE_PRESSED);
  gfx::Rect thumb_bounds = thumb_->bounds();
  if (IsHorizontal()) {
    if (GetMirroredXInView(event.x()) < thumb_bounds.x()) {
      last_scroll_amount_ = SCROLL_PREV_PAGE;
    } else if (GetMirroredXInView(event.x()) > thumb_bounds.right()) {
      last_scroll_amount_ = SCROLL_NEXT_PAGE;
    }
  } else {
    if (event.y() < thumb_bounds.y()) {
      last_scroll_amount_ = SCROLL_PREV_PAGE;
    } else if (event.y() > thumb_bounds.bottom()) {
      last_scroll_amount_ = SCROLL_NEXT_PAGE;
    }
  }
  TrackClicked();
  repeater_.Start();
}

void BaseScrollBar::SetState(CustomButton::ButtonState state) {
  SetThumbTrackState(state);
  repeater_.Stop();
}

void BaseScrollBar::TrackClicked() {
  if (last_scroll_amount_ != SCROLL_NONE)
    ScrollByAmount(last_scroll_amount_);
}

void BaseScrollBar::ScrollContentsToOffset() {
  ScrollToPosition(contents_scroll_offset_);
  thumb_->SetPosition(CalculateThumbPosition(contents_scroll_offset_));
}

int BaseScrollBar::GetTrackSize() const {
  gfx::Rect track_bounds = GetTrackBounds();
  return IsHorizontal() ? track_bounds.width() : track_bounds.height();
}

int BaseScrollBar::CalculateThumbPosition(int contents_scroll_offset) const {
  // In some combination of viewport_size and contents_size_, the result of
  // simple division can be rounded and there could be 1 pixel gap even when the
  // contents scroll down to the bottom. See crbug.com/244671
  int thumb_max = GetTrackSize() - thumb_->GetSize();
  if (contents_scroll_offset + viewport_size_ == contents_size_)
    return thumb_max;
  return (contents_scroll_offset * thumb_max) /
         (contents_size_ - viewport_size_);
}

int BaseScrollBar::CalculateContentsOffset(int thumb_position,
                                           bool scroll_to_middle) const {
  int thumb_size = thumb_->GetSize();
  int track_size = GetTrackSize();
  if (track_size == thumb_size)
    return 0;
  if (scroll_to_middle)
    thumb_position = thumb_position - (thumb_size / 2);
  return (thumb_position * (contents_size_ - viewport_size_)) /
         (track_size - thumb_size);
}

void BaseScrollBar::SetThumbTrackState(CustomButton::ButtonState state) {
  thumb_track_state_ = state;
  SchedulePaint();
}

}  // namespace views
