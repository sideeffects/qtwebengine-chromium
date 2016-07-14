// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_METRICS_ARC_METRICS_SERVICE_H
#define COMPONENTS_ARC_METRICS_ARC_METRICS_SERVICE_H

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/arc_bridge.mojom.h"
#include "components/arc/metrics/oom_kills_monitor.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

// Collects information from other ArcServices and send UMA metrics.
class ArcMetricsService : public ArcService,
                          public ArcBridgeService::Observer,
                          public mojom::MetricsHost {
 public:
  explicit ArcMetricsService(ArcBridgeService* bridge_service);
  ~ArcMetricsService() override;

  // ArcBridgeService::Observer overrides.
  void OnMetricsInstanceReady() override;
  void OnMetricsInstanceClosed() override;
  void OnProcessInstanceReady() override;
  void OnProcessInstanceClosed() override;

  // MetricsHost overrides.
  void ReportBootProgress(
      mojo::Array<arc::mojom::BootProgressEventPtr> events) override;

 private:
  bool CalledOnValidThread();
  void RequestProcessList();
  void ParseProcessList(
      mojo::Array<arc::mojom::RunningAppProcessInfoPtr> processes);

  // DBus callbacks.
  void OnArcStartTimeRetrieved(bool success, base::TimeTicks arc_start_time);

 private:
  mojo::Binding<mojom::MetricsHost> binding_;

  base::ThreadChecker thread_checker_;
  base::RepeatingTimer timer_;

  OomKillsMonitor oom_kills_monitor_;

  base::TimeTicks arc_start_time_;

  // Always keep this the last member of this class to make sure it's the
  // first thing to be destructed.
  base::WeakPtrFactory<ArcMetricsService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcMetricsService);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_METRICS_ARC_METRICS_SERVICE_H
