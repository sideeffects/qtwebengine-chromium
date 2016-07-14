// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/snapshot_controller.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class SnapshotControllerTest
    : public testing::Test,
      public SnapshotController::Client {
 public:
  SnapshotControllerTest();
  ~SnapshotControllerTest() override;

  SnapshotController* controller() { return controller_.get(); }
  int snapshot_count() { return snapshot_count_; }

  // testing::Test
  void SetUp() override;
  void TearDown() override;

  // SnapshotController::Client
  void StartSnapshot() override;

  // Utility methods.
  // Runs until all of the tasks that are not delayed are gone from the task
  // queue.
  void PumpLoop();
  // Fast-forwards virtual time by |delta|, causing tasks with a remaining
  // delay less than or equal to |delta| to be executed.
  void FastForwardBy(base::TimeDelta delta);

 private:
  std::unique_ptr<SnapshotController> controller_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  bool snapshot_started_;
  int snapshot_count_;
};

SnapshotControllerTest::SnapshotControllerTest()
    : task_runner_(new base::TestMockTimeTaskRunner),
      snapshot_started_(true),
      snapshot_count_(0) {
}

SnapshotControllerTest::~SnapshotControllerTest() {
}

void SnapshotControllerTest::SetUp() {
  controller_.reset(new SnapshotController(task_runner_, this));
  snapshot_started_ = true;
}

void SnapshotControllerTest::TearDown() {
  controller_.reset();
}

void SnapshotControllerTest::StartSnapshot() {
  snapshot_count_++;
}

void SnapshotControllerTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void SnapshotControllerTest::FastForwardBy(base::TimeDelta delta) {
  task_runner_->FastForwardBy(delta);
}

TEST_F(SnapshotControllerTest, OnLoad) {
  // Onload should make snapshot right away.
  EXPECT_EQ(0, snapshot_count());
  controller()->DocumentOnLoadCompletedInMainFrame();
  PumpLoop();
  EXPECT_EQ(1, snapshot_count());
}

TEST_F(SnapshotControllerTest, OnDocumentAvailable) {
  EXPECT_GT(controller()->GetDelayAfterDocumentAvailableForTest(), 0UL);
  // OnDOM should make snapshot after a delay.
  controller()->DocumentAvailableInMainFrame();
  PumpLoop();
  EXPECT_EQ(0, snapshot_count());
  FastForwardBy(base::TimeDelta::FromMilliseconds(
      controller()->GetDelayAfterDocumentAvailableForTest()));
  EXPECT_EQ(1, snapshot_count());
}

TEST_F(SnapshotControllerTest, OnLoadSnapshotIsTheLastOne) {
  // OnDOM should make snapshot after a delay.
  controller()->DocumentAvailableInMainFrame();
  PumpLoop();
  EXPECT_EQ(0, snapshot_count());
  // This should start snapshot immediately.
  controller()->DocumentOnLoadCompletedInMainFrame();
  EXPECT_EQ(1, snapshot_count());
  // Report that snapshot is completed.
  controller()->PendingSnapshotCompleted();
  // Even though previous snapshot is completed, new one should not start
  // when this delay expires.
  FastForwardBy(base::TimeDelta::FromMilliseconds(
      controller()->GetDelayAfterDocumentAvailableForTest()));
  EXPECT_EQ(1, snapshot_count());
}

TEST_F(SnapshotControllerTest, OnLoadSnapshotAfterLongDelay) {
  // OnDOM should make snapshot after a delay.
  controller()->DocumentAvailableInMainFrame();
  PumpLoop();
  EXPECT_EQ(0, snapshot_count());
  FastForwardBy(base::TimeDelta::FromMilliseconds(
      controller()->GetDelayAfterDocumentAvailableForTest()));
  EXPECT_EQ(1, snapshot_count());
  // Report that snapshot is completed.
  controller()->PendingSnapshotCompleted();
  // This should start snapshot immediately.
  controller()->DocumentOnLoadCompletedInMainFrame();
  EXPECT_EQ(2, snapshot_count());
}

TEST_F(SnapshotControllerTest, Stop) {
  // OnDOM should make snapshot after a delay.
  controller()->DocumentAvailableInMainFrame();
  PumpLoop();
  EXPECT_EQ(0, snapshot_count());
  controller()->Stop();
  FastForwardBy(base::TimeDelta::FromMilliseconds(
      controller()->GetDelayAfterDocumentAvailableForTest()));
  // Should not start snapshots
  EXPECT_EQ(0, snapshot_count());
  // Also should not start snapshot.
  controller()->DocumentOnLoadCompletedInMainFrame();
  EXPECT_EQ(0, snapshot_count());
}

TEST_F(SnapshotControllerTest, ClientReset) {
  controller()->DocumentAvailableInMainFrame();

  controller()->Reset();
  FastForwardBy(base::TimeDelta::FromMilliseconds(
      controller()->GetDelayAfterDocumentAvailableForTest()));
  // No snapshot since session was reset.
  EXPECT_EQ(0, snapshot_count());
  controller()->DocumentOnLoadCompletedInMainFrame();
  EXPECT_EQ(1, snapshot_count());

  controller()->Reset();
  controller()->DocumentAvailableInMainFrame();
  FastForwardBy(base::TimeDelta::FromMilliseconds(
      controller()->GetDelayAfterDocumentAvailableForTest()));
  // No snapshot since session was reset.
  EXPECT_EQ(2, snapshot_count());
}

}  // namespace offline_pages
