#ifndef AICARO_AGENT_H
#define AICARO_AGENT_H

#include <atomic>
#include <chrono>
#include <utility>

#include "caro.h"

class Agent {
 public:
  virtual ~Agent() = default;
  virtual std::pair<unsigned, unsigned> GetMove(Caro game_state) = 0;

  // void SetMark(char mark) { my_mark_ = mark; }
  char GetMark() const { return my_mark_; }

 protected:
  char my_mark_ = Caro::kMarkComputer;          // default
  std::chrono::milliseconds time_limit_{2000};  // default 2 seconds
  std::chrono::steady_clock::time_point start_time_;
  std::atomic<int> node_counter_{0};
  unsigned timer_check_factor = 10;  // Default: Check time every 2^10 = 1024 nodes

  struct TimeOutException : public std::exception {};

  void CheckTime();
};

#endif  // AICARO_AGENT_H