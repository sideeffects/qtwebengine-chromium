// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_ripple.h"

#include <memory>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_ripple_observer.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/animation/square_ink_drop_ripple.h"
#include "ui/views/animation/test/flood_fill_ink_drop_ripple_test_api.h"
#include "ui/views/animation/test/ink_drop_ripple_test_api.h"
#include "ui/views/animation/test/square_ink_drop_ripple_test_api.h"
#include "ui/views/animation/test/test_ink_drop_ripple_observer.h"

namespace views {
namespace test {

// Represents all the derivatives of the InkDropRipple class. To be used with
// the InkDropRippleTest fixture to test all derviatives.
enum InkDropRippleTestTypes {
  SQUARE_INK_DROP_RIPPLE,
  FLOOD_FILL_INK_DROP_RIPPLE
};

// Test fixture for all InkDropRipple class derivatives.
//
// To add a new derivative:
//    1. Add a value to the InkDropRippleTestTypes enum.
//    2. Implement set up and tear down code for the new enum value in
//       InkDropRippleTest() and
//      ~InkDropRippleTest().
//    3. Add the new enum value to the INSTANTIATE_TEST_CASE_P) Values list.
class InkDropRippleTest
    : public testing::TestWithParam<InkDropRippleTestTypes> {
 public:
  InkDropRippleTest();
  ~InkDropRippleTest() override;

 protected:
  TestInkDropRippleObserver observer_;

  std::unique_ptr<InkDropRipple> ink_drop_ripple_;

  std::unique_ptr<InkDropRippleTestApi> test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InkDropRippleTest);
};

InkDropRippleTest::InkDropRippleTest() {
  switch (GetParam()) {
    case SQUARE_INK_DROP_RIPPLE: {
      SquareInkDropRipple* square_ink_drop_ripple =
          new SquareInkDropRipple(gfx::Size(10, 10), 2, gfx::Size(8, 8), 1,
                                  gfx::Point(), SK_ColorBLACK);
      ink_drop_ripple_.reset(square_ink_drop_ripple);
      test_api_.reset(new SquareInkDropRippleTestApi(square_ink_drop_ripple));
      break;
    }
    case FLOOD_FILL_INK_DROP_RIPPLE: {
      FloodFillInkDropRipple* flood_fill_ink_drop_ripple =
          new FloodFillInkDropRipple(gfx::Rect(0, 0, 10, 10), gfx::Point(),
                                     SK_ColorBLACK);
      ink_drop_ripple_.reset(flood_fill_ink_drop_ripple);
      test_api_.reset(
          new FloodFillInkDropRippleTestApi(flood_fill_ink_drop_ripple));
      break;
    }
  }
  ink_drop_ripple_->set_observer(&observer_);
  observer_.set_ink_drop_ripple(ink_drop_ripple_.get());
  test_api_->SetDisableAnimationTimers(true);
}

InkDropRippleTest::~InkDropRippleTest() {}

// Note: First argument is optional and intentionally left blank.
// (it's a prefix for the generated test cases)
INSTANTIATE_TEST_CASE_P(,
                        InkDropRippleTest,
                        testing::Values(SQUARE_INK_DROP_RIPPLE,
                                        FLOOD_FILL_INK_DROP_RIPPLE));

TEST_P(InkDropRippleTest, InitialStateAfterConstruction) {
  EXPECT_EQ(views::InkDropState::HIDDEN,
            ink_drop_ripple_->target_ink_drop_state());
}

// Verify no animations are used when animating from HIDDEN to HIDDEN.
TEST_P(InkDropRippleTest, AnimateToHiddenFromInvisibleState) {
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop_ripple_->target_ink_drop_state());

  ink_drop_ripple_->AnimateToState(InkDropState::HIDDEN);
  EXPECT_EQ(1, observer_.last_animation_started_ordinal());
  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());
  EXPECT_EQ(InkDropRipple::kHiddenOpacity, test_api_->GetCurrentOpacity());
  EXPECT_FALSE(ink_drop_ripple_->IsVisible());
}

TEST_P(InkDropRippleTest, AnimateToHiddenFromVisibleState) {
  ink_drop_ripple_->AnimateToState(InkDropState::ACTION_PENDING);
  test_api_->CompleteAnimations();
  EXPECT_EQ(1, observer_.last_animation_started_ordinal());
  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());

  EXPECT_NE(InkDropState::HIDDEN, ink_drop_ripple_->target_ink_drop_state());

  ink_drop_ripple_->AnimateToState(InkDropState::HIDDEN);
  test_api_->CompleteAnimations();

  EXPECT_EQ(3, observer_.last_animation_started_ordinal());
  EXPECT_EQ(4, observer_.last_animation_ended_ordinal());
  EXPECT_EQ(InkDropRipple::kHiddenOpacity, test_api_->GetCurrentOpacity());
}

TEST_P(InkDropRippleTest, ActionPendingOpacity) {
  ink_drop_ripple_->AnimateToState(views::InkDropState::ACTION_PENDING);
  test_api_->CompleteAnimations();

  EXPECT_EQ(InkDropRipple::kVisibleOpacity, test_api_->GetCurrentOpacity());
}

TEST_P(InkDropRippleTest, QuickActionOpacity) {
  ink_drop_ripple_->AnimateToState(views::InkDropState::ACTION_PENDING);
  ink_drop_ripple_->AnimateToState(views::InkDropState::ACTION_TRIGGERED);
  test_api_->CompleteAnimations();

  EXPECT_EQ(InkDropRipple::kHiddenOpacity, test_api_->GetCurrentOpacity());
}

TEST_P(InkDropRippleTest, SlowActionPendingOpacity) {
  ink_drop_ripple_->AnimateToState(views::InkDropState::ACTION_PENDING);
  ink_drop_ripple_->AnimateToState(
      views::InkDropState::ALTERNATE_ACTION_PENDING);
  test_api_->CompleteAnimations();

  EXPECT_EQ(InkDropRipple::kVisibleOpacity, test_api_->GetCurrentOpacity());
}

TEST_P(InkDropRippleTest, SlowActionOpacity) {
  ink_drop_ripple_->AnimateToState(views::InkDropState::ACTION_PENDING);
  ink_drop_ripple_->AnimateToState(
      views::InkDropState::ALTERNATE_ACTION_PENDING);
  ink_drop_ripple_->AnimateToState(
      views::InkDropState::ALTERNATE_ACTION_TRIGGERED);
  test_api_->CompleteAnimations();

  EXPECT_EQ(InkDropRipple::kHiddenOpacity, test_api_->GetCurrentOpacity());
}

TEST_P(InkDropRippleTest, ActivatedOpacity) {
  ink_drop_ripple_->AnimateToState(views::InkDropState::ACTIVATED);
  test_api_->CompleteAnimations();

  EXPECT_EQ(InkDropRipple::kVisibleOpacity, test_api_->GetCurrentOpacity());
}

TEST_P(InkDropRippleTest, DeactivatedOpacity) {
  ink_drop_ripple_->AnimateToState(views::InkDropState::ACTIVATED);
  ink_drop_ripple_->AnimateToState(views::InkDropState::DEACTIVATED);
  test_api_->CompleteAnimations();

  EXPECT_EQ(InkDropRipple::kHiddenOpacity, test_api_->GetCurrentOpacity());
}

// Verify animations are aborted during deletion and the
// InkDropRippleObservers are notified.
TEST_P(InkDropRippleTest, AnimationsAbortedDuringDeletion) {
  ink_drop_ripple_->AnimateToState(views::InkDropState::ACTION_PENDING);
  ink_drop_ripple_.reset();
  EXPECT_EQ(1, observer_.last_animation_started_ordinal());
  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            observer_.last_animation_ended_context());
  EXPECT_EQ(InkDropAnimationEndedReason::PRE_EMPTED,
            observer_.last_animation_ended_reason());
}

TEST_P(InkDropRippleTest, VerifyObserversAreNotified) {
  ink_drop_ripple_->AnimateToState(InkDropState::ACTION_PENDING);

  EXPECT_TRUE(test_api_->HasActiveAnimations());
  EXPECT_EQ(1, observer_.last_animation_started_ordinal());
  EXPECT_TRUE(observer_.AnimationHasNotEnded());
  EXPECT_EQ(InkDropState::ACTION_PENDING,
            observer_.last_animation_started_context());

  test_api_->CompleteAnimations();

  EXPECT_FALSE(test_api_->HasActiveAnimations());
  EXPECT_EQ(1, observer_.last_animation_started_ordinal());
  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());
  EXPECT_EQ(InkDropState::ACTION_PENDING,
            observer_.last_animation_ended_context());
}

TEST_P(InkDropRippleTest, VerifyObserversAreNotifiedOfSuccessfulAnimations) {
  ink_drop_ripple_->AnimateToState(InkDropState::ACTION_PENDING);
  test_api_->CompleteAnimations();

  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());
  EXPECT_EQ(InkDropAnimationEndedReason::SUCCESS,
            observer_.last_animation_ended_reason());
}

TEST_P(InkDropRippleTest, VerifyObserversAreNotifiedOfPreemptedAnimations) {
  ink_drop_ripple_->AnimateToState(InkDropState::ACTION_PENDING);
  ink_drop_ripple_->AnimateToState(InkDropState::ALTERNATE_ACTION_PENDING);

  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());
  EXPECT_EQ(InkDropAnimationEndedReason::PRE_EMPTED,
            observer_.last_animation_ended_reason());
}

TEST_P(InkDropRippleTest, InkDropStatesPersistWhenCallingAnimateToState) {
  ink_drop_ripple_->AnimateToState(views::InkDropState::ACTION_PENDING);
  ink_drop_ripple_->AnimateToState(views::InkDropState::ACTIVATED);
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            ink_drop_ripple_->target_ink_drop_state());
}

TEST_P(InkDropRippleTest, HideImmediatelyWithoutActiveAnimations) {
  ink_drop_ripple_->AnimateToState(views::InkDropState::ACTION_PENDING);
  test_api_->CompleteAnimations();
  EXPECT_EQ(1, observer_.last_animation_started_ordinal());
  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());

  EXPECT_FALSE(test_api_->HasActiveAnimations());
  EXPECT_NE(InkDropState::HIDDEN, ink_drop_ripple_->target_ink_drop_state());

  ink_drop_ripple_->HideImmediately();

  EXPECT_FALSE(test_api_->HasActiveAnimations());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            ink_drop_ripple_->target_ink_drop_state());
  EXPECT_EQ(1, observer_.last_animation_started_ordinal());
  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());

  EXPECT_EQ(InkDropRipple::kHiddenOpacity, test_api_->GetCurrentOpacity());
  EXPECT_FALSE(ink_drop_ripple_->IsVisible());
}

// Verifies all active animations are aborted and the InkDropState is set to
// HIDDEN after invoking HideImmediately().
TEST_P(InkDropRippleTest, HideImmediatelyWithActiveAnimations) {
  ink_drop_ripple_->AnimateToState(views::InkDropState::ACTION_PENDING);
  EXPECT_TRUE(test_api_->HasActiveAnimations());
  EXPECT_NE(InkDropState::HIDDEN, ink_drop_ripple_->target_ink_drop_state());
  EXPECT_EQ(1, observer_.last_animation_started_ordinal());

  ink_drop_ripple_->HideImmediately();

  EXPECT_FALSE(test_api_->HasActiveAnimations());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            ink_drop_ripple_->target_ink_drop_state());
  EXPECT_EQ(1, observer_.last_animation_started_ordinal());
  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());
  EXPECT_EQ(InkDropState::ACTION_PENDING,
            observer_.last_animation_ended_context());
  EXPECT_EQ(InkDropAnimationEndedReason::PRE_EMPTED,
            observer_.last_animation_ended_reason());

  EXPECT_EQ(InkDropRipple::kHiddenOpacity, test_api_->GetCurrentOpacity());
  EXPECT_FALSE(ink_drop_ripple_->IsVisible());
}

TEST_P(InkDropRippleTest, SnapToActivatedWithoutActiveAnimations) {
  ink_drop_ripple_->AnimateToState(views::InkDropState::ACTION_PENDING);
  test_api_->CompleteAnimations();
  EXPECT_EQ(1, observer_.last_animation_started_ordinal());
  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());

  EXPECT_FALSE(test_api_->HasActiveAnimations());
  EXPECT_NE(InkDropState::ACTIVATED, ink_drop_ripple_->target_ink_drop_state());

  ink_drop_ripple_->SnapToActivated();

  EXPECT_FALSE(test_api_->HasActiveAnimations());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            ink_drop_ripple_->target_ink_drop_state());
  EXPECT_EQ(3, observer_.last_animation_started_ordinal());
  EXPECT_EQ(4, observer_.last_animation_ended_ordinal());

  EXPECT_EQ(InkDropRipple::kVisibleOpacity, test_api_->GetCurrentOpacity());
  EXPECT_TRUE(ink_drop_ripple_->IsVisible());
}

// Verifies all active animations are aborted and the InkDropState is set to
// ACTIVATED after invoking SnapToActivated().
TEST_P(InkDropRippleTest, SnapToActivatedWithActiveAnimations) {
  ink_drop_ripple_->AnimateToState(views::InkDropState::ACTION_PENDING);
  EXPECT_TRUE(test_api_->HasActiveAnimations());
  EXPECT_NE(InkDropState::ACTIVATED, ink_drop_ripple_->target_ink_drop_state());
  EXPECT_EQ(1, observer_.last_animation_started_ordinal());

  ink_drop_ripple_->SnapToActivated();

  EXPECT_FALSE(test_api_->HasActiveAnimations());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            ink_drop_ripple_->target_ink_drop_state());
  EXPECT_EQ(3, observer_.last_animation_started_ordinal());
  EXPECT_EQ(4, observer_.last_animation_ended_ordinal());
  EXPECT_EQ(InkDropState::ACTIVATED, observer_.last_animation_ended_context());
  EXPECT_EQ(InkDropAnimationEndedReason::SUCCESS,
            observer_.last_animation_ended_reason());

  EXPECT_EQ(InkDropRipple::kVisibleOpacity, test_api_->GetCurrentOpacity());
  EXPECT_TRUE(ink_drop_ripple_->IsVisible());
}

TEST_P(InkDropRippleTest, AnimateToVisibleFromHidden) {
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop_ripple_->target_ink_drop_state());
  ink_drop_ripple_->AnimateToState(views::InkDropState::ACTION_PENDING);
  EXPECT_TRUE(ink_drop_ripple_->IsVisible());
}

// Verifies that the value of InkDropRipple::target_ink_drop_state() returns
// the most recent value passed to AnimateToState() when notifying observers
// that an animation has started within the AnimateToState() function call.
TEST_P(InkDropRippleTest, TargetInkDropStateOnAnimationStarted) {
  ink_drop_ripple_->AnimateToState(views::InkDropState::ACTION_PENDING);
  ink_drop_ripple_->AnimateToState(views::InkDropState::HIDDEN);

  EXPECT_EQ(3, observer_.last_animation_started_ordinal());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            observer_.target_state_at_last_animation_started());
}

// Verifies that the value of InkDropRipple::target_ink_drop_state() returns
// the most recent value passed to AnimateToState() when notifying observers
// that an animation has ended within the AnimateToState() function call.
TEST_P(InkDropRippleTest, TargetInkDropStateOnAnimationEnded) {
  ink_drop_ripple_->AnimateToState(views::InkDropState::ACTION_PENDING);
  ink_drop_ripple_->AnimateToState(views::InkDropState::HIDDEN);

  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            observer_.target_state_at_last_animation_ended());
}

}  // namespace test
}  // namespace views
