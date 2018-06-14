// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/hi_res_timer_manager.h"

#include <algorithm>

#include "base/atomicops.h"
#include "base/task_scheduler/post_task.h"
#include "base/time/time.h"

namespace base {

namespace {

constexpr TimeDelta kUsageSampleInterval = TimeDelta::FromMinutes(10);

void ReportHighResolutionTimerUsage() {
  // Reset usage for the next interval.
  Time::ResetHighResolutionTimerUsage();
}

}  // namespace

HighResolutionTimerManager::HighResolutionTimerManager()
    : hi_res_clock_available_(false) {
  // Start polling the high resolution timer usage.
  Time::ResetHighResolutionTimerUsage();
  timer_.Start(FROM_HERE, kUsageSampleInterval,
               Bind(&ReportHighResolutionTimerUsage));
}

HighResolutionTimerManager::~HighResolutionTimerManager() {
  UseHiResClock(false);
}

void HighResolutionTimerManager::UseHiResClock(bool use) {
  if (use == hi_res_clock_available_)
    return;
  hi_res_clock_available_ = use;
  Time::EnableHighResolutionTimer(use);
}

}  // namespace base
