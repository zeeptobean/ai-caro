#include "agent.h"

void Agent::CheckTime() {
  if ((++node_counter_ & ((1ull << timer_check_factor) - 1)) == 0) {
    if (cancel_signal_.load(std::memory_order_relaxed)) {
      throw TimeOutException();
    }
    if (std::chrono::steady_clock::now() - start_time_ > time_limit_) {
      throw TimeOutException();
    }
  }
}

void Agent::RequestCancel() { cancel_signal_.store(true); }

void Agent::ClearCancel() { cancel_signal_.store(false); }

bool Agent::CancelStatus() const { return cancel_signal_.load(std::memory_order_relaxed); }