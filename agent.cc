#include "agent.h"

void Agent::CheckTime() {
  if ((++node_counter_ & 1023) == 0) {
    if (std::chrono::steady_clock::now() - start_time_ > time_limit_) {
      throw TimeOutException();
    }
  }
}