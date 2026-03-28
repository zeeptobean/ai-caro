#ifndef AICARO_AGENT_H
#define AICARO_AGENT_H

#include <atomic>
#include <chrono>
#include <utility>

#include "caro.h"

class Agent {
  std::chrono::steady_clock::time_point start_time_;
  std::atomic<bool> cancel_signal_{false};
  struct TimeOutException : public std::exception {};

 public:
  virtual ~Agent() = default;
  std::pair<unsigned, unsigned> GetMove(Caro game_state);

  // void SetMark(char mark) { my_mark_ = mark; }
  char GetMark() const { return my_mark_; }
  void RequestCancel();
  void ClearCancel();
  bool CancelStatus() const;

 protected:
  char my_mark_ = Caro::kMarkComputer;  // default
  int move_radius_ = 3;
  std::chrono::milliseconds time_limit_{2000};  // default 2 seconds
  std::pair<unsigned, unsigned> best_move_{0, 0};

  Agent(unsigned time_limit, int move_radius);

  void CheckTime();

  // Default implementation, should be overridden
  virtual void GetMoveImpl(Caro&, const std::vector<std::pair<unsigned, unsigned>>&) {}

  // Default check time condition, default to wait 50ms
  // Can be overridden for finer control
  virtual bool CheckTimeCondition();
};

#endif  // AICARO_AGENT_H