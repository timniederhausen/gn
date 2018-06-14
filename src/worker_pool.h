// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WORKER_POOL_H_
#define WORKER_POOL_H_

#include "base/logging.h"
#include "base/macros.h"

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>

class WorkerPool {
 public:
  WorkerPool();
  WorkerPool(size_t thread_count);
  ~WorkerPool();

  void PostTask(std::function<void()> work);

 private:
  void Worker();

  std::vector<std::thread> threads_;
  std::queue<std::function<void()>> task_queue_;
  std::mutex queue_mutex_;
  std::condition_variable_any pool_notifier_;
  bool should_stop_processing_;

  DISALLOW_COPY_AND_ASSIGN(WorkerPool);
};

#endif  // WORKER_POOL_H_
