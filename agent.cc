#include "agent.h"

#include <thread>
#include <utility>

Agent::Agent(unsigned time_limit, int move_radius) : move_radius_(move_radius) {
  time_limit_ = std::chrono::milliseconds(time_limit);
}

std::pair<unsigned, unsigned> Agent::GetMove(Caro game_state) {
  ClearCancel();
  start_time_ = std::chrono::steady_clock::now();

  auto moves = game_state.GetCandidateMoves(move_radius_);
  if (!moves.empty()) return best_move_ = moves.front();

  try {
    GetMoveImpl(game_state, moves);
  } catch (const TimeOutException&) {
    // catch!
  }
  return best_move_;
}

bool Agent::CheckTimeCondition() {
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  return true;
}

void Agent::CheckTime() {
  while (CheckTimeCondition()) {
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