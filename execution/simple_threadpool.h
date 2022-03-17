// Copyright 2022 DeepMind Technologies Limited
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LEARNING_DEEPMIND_RESEARCH_CODEGEN_EXECUTION_SIMPLE_THREADPOOL_H_
#define LEARNING_DEEPMIND_RESEARCH_CODEGEN_EXECUTION_SIMPLE_THREADPOOL_H_

#include <cassert>
#include <cstddef>
#include <functional>
#include <queue>
#include <thread>  // NOLINT(build/c++11)
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"

namespace deepmind::code_contests {

class ThreadPool {
 public:
  explicit ThreadPool(int num_threads) : num_threads_(num_threads) {}

  ThreadPool(const ThreadPool &) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;

  ~ThreadPool() {
    if (!started_) {
      return;
    }
    {
      absl::MutexLock l(&mu_);
      for (size_t i = 0; i < threads_.size(); i++) {
        queue_.push(nullptr);  // Shutdown signal.
      }
    }
    for (auto &t : threads_) {
      t.join();
    }
  }

  void StartWorkers() {
    assert(!started_);
    for (int i = 0; i < num_threads_; ++i) {
      threads_.push_back(std::thread(&ThreadPool::WorkLoop, this));
    }
    started_ = true;
  }

  // Schedule a function to be run on a ThreadPool thread immediately.
  void Schedule(std::function<void()> func) {
    assert(started_);
    assert(func != nullptr);
    absl::MutexLock l(&mu_);
    queue_.push(std::move(func));
  }

 private:
  bool WorkAvailable() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
    return !queue_.empty();
  }

  void WorkLoop() {
    while (true) {
      std::function<void()> func;
      {
        absl::MutexLock l(&mu_);
        mu_.Await(absl::Condition(this, &ThreadPool::WorkAvailable));
        func = std::move(queue_.front());
        queue_.pop();
      }
      if (func == nullptr) {  // Shutdown signal.
        break;
      }
      func();
    }
  }

  absl::Mutex mu_;
  std::queue<std::function<void()>> queue_ ABSL_GUARDED_BY(mu_);
  std::vector<std::thread> threads_;
  int num_threads_;
  bool started_ = false;
};

}  // namespace deepmind::code_contests

#endif  // LEARNING_DEEPMIND_RESEARCH_CODEGEN_EXECUTION_SIMPLE_THREADPOOL_H_
