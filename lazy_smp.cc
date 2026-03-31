#include "lazy_smp.h"

#include <algorithm>
#include <limits>
#include <thread>

#include "integer.h"

LazySmpAgent::LazySmpAgent(unsigned time_limit_ms, int num_threads, int radius)
    : Agent(time_limit_ms, radius), num_threads_(num_threads) {
  tt_ = std::vector<TTEntryAtomic>(1ull << 20);
  ResetBot();
}

bool LazySmpAgent::CheckTimeCondition() { return (++node_counter_ & ((1ull << 10) - 1)) == 0; }

void LazySmpAgent::ResetBot() {
  for (auto& entry : tt_) {
    while (entry.lock.test_and_set(std::memory_order_acquire));
    entry.entry = TTEntry();
    entry.lock.clear(std::memory_order_release);
  }
  best_depth_ = 0;
}

void LazySmpAgent::GetMoveImpl(Caro& state,
                               const std::vector<std::pair<unsigned, unsigned>>& moves) {
  best_depth_ = 0;
  node_counter_ = 0;

  std::vector<std::thread> workers;

  for (int i = 0; i < num_threads_; ++i) {
    workers.push_back(std::thread(&LazySmpAgent::SearchWorker, this, i, state, moves));
  }

  for (auto& t : workers) {
    if (t.joinable()) t.join();
  }
}

void LazySmpAgent::SearchWorker(int thread_id, Caro state,
                                std::vector<std::pair<unsigned, unsigned>> current_moves) {
  int depth_offset = thread_id % 2;  // LazySMP diversity

  try {
    for (int d = 1 + depth_offset; d <= std::numeric_limits<int>::max(); d++) {
      bool move_found = false;
      Integer best_value = Integer::NegInf();
      std::pair<unsigned, unsigned> current_best_move = {0, 0};

      // PV-Move Ordering for the root level:
      // Place the best move from the previous iteration at the front
      if (d > 1) {
        auto best_move_copy = GetBestMove();
        for (size_t i = 0; i < current_moves.size(); ++i) {
          if (current_moves[i] == best_move_copy) {
            std::swap(current_moves[0], current_moves[i]);
            break;
          }
        }
      }

      Integer alpha = Integer::NegInf();
      for (const auto& [i, j] : current_moves) {
        if (!state.PlaceMove(i, j, state.GetComputerMark())) continue;

        auto value = AlphaBeta(state, 1, alpha, Integer::Inf(), false, d);

        state.UndoMove(i, j);  // backtrack

        if (!move_found || value > best_value) {
          best_value = value;
          current_best_move = {i, j};
          move_found = true;
          if (value > alpha) alpha = value;
        }
      }
      if (move_found) {
        SetBestMove(current_best_move, d);
      }
    }
  } catch (const TimeOutException&) {
    // Time expired; thread safely exits search
  }
}

Integer LazySmpAgent::TerminalScore(Caro::GameState state, int depth) {
  if (state == Caro::GameState::kComputerWin) return Integer::Max() - Integer(depth);
  if (state == Caro::GameState::kPlayerWin) return Integer::Min() + Integer(depth);
  if (state == Caro::GameState::kDraw) return Integer::Zero();
  return Integer::Zero();
}

Integer LazySmpAgent::EvaluateBoard(const Caro& state) const {
  int64_t total_score = 0;
  auto [n, m] = state.GetBoardSize();

  auto evaluate_window = [&](unsigned y, unsigned x, int dy, int dx) {
    int comp_count = 0;
    int player_count = 0;

    for (unsigned step = 0; step < state.GetK(); ++step) {
      int tnx = static_cast<int>(x) + static_cast<int>(step) * dx;
      int tny = static_cast<int>(y) + static_cast<int>(step) * dy;

      if (tnx < 0 || tny < 0 || tnx >= static_cast<int>(m) || tny >= static_cast<int>(n)) {
        return;
      }

      unsigned nx = static_cast<unsigned>(tnx);
      unsigned ny = static_cast<unsigned>(tny);

      char cell = state.GetCell(ny, nx);
      if (cell == state.GetComputerMark()) {
        comp_count++;
      } else if (cell == state.GetPlayerMark()) {
        player_count++;
      }
    }

    // Score the window
    if (comp_count > 0 && player_count == 0) {
      // Only computer marks (Unblocked)
      int64_t value = 1;
      for (int i = 0; i < comp_count; ++i) value *= 10;
      total_score += value;
    } else if (player_count > 0 && comp_count == 0) {
      // Only player marks (Unblocked)
      int64_t value = 1;
      for (int i = 0; i < player_count; ++i) value *= 10;
      total_score -= value;
    }
    // If both > 0, the line is blocked, score is 0 (we do nothing)
  };

  for (unsigned i = 0; i < n; ++i) {
    for (unsigned j = 0; j < m; ++j) {
      evaluate_window(i, j, 0, 1);   // Horizontal
      evaluate_window(i, j, 1, 0);   // Vertical
      evaluate_window(i, j, 1, 1);   // Diagonal down
      evaluate_window(i, j, 1, -1);  // Diagonal up
    }
  }

  return Integer(total_score);
}

Integer LazySmpAgent::AlphaBeta(Caro& state, int depth, Integer alpha, Integer beta,
                                bool is_maximizing, int max_depth) {
  CheckTime();

  Caro::GameState gs = state.CheckGameState();
  Integer ret = Integer::Zero();
  if (gs != Caro::GameState::kNormal) {
    ret = TerminalScore(gs, depth);
    return ret;
  }
  int remaining_depth = max_depth - depth;
  if (remaining_depth <= 0) {
    return EvaluateBoard(state);
  }

  // Transposition Table probe
  uint64_t hash = state.GetHash();
  TTEntry tte = GetTTEntry(hash);
  std::pair<unsigned, unsigned> pv_move = {unsigned(-1), unsigned(-1)};

  if (tte.hash == hash) {
    pv_move = tte.best_move;

    if (tte.remaining_depth >= remaining_depth) {
      if (tte.flag == TTFlag::kExact) return tte.value;
      if (tte.flag == TTFlag::kLowerBound) {
        if (tte.value > alpha) alpha = tte.value;
      } else if (tte.flag == TTFlag::kUpperBound) {
        if (tte.value < beta) beta = tte.value;
      }
      if (alpha >= beta) return tte.value;
    }
  }

  bool has_move = false;
  auto move_list = state.GetCandidateMoves(move_radius_);

  if (pv_move.first != unsigned(-1)) {
    for (size_t k = 0; k < move_list.size(); ++k) {
      if (move_list[k] == pv_move) {
        std::swap(move_list[0], move_list[k]);
        break;
      }
    }
  }

  Integer original_alpha = alpha;
  Integer original_beta = beta;
  std::pair<unsigned, unsigned> current_best_move = {unsigned(-1), unsigned(-1)};

  if (is_maximizing) {
    Integer best = Integer::NegInf();
    for (const auto& [i, j] : move_list) {
      if (!state.PlaceMove(i, j, state.GetComputerMark())) continue;
      has_move = true;

      auto score = AlphaBeta(state, depth + 1, alpha, beta, false, max_depth);
      state.UndoMove(i, j);  // Backtracking

      if (score > best) {
        best = score;
        current_best_move = {i, j};
      }
      if (best > alpha) alpha = best;
      if (beta <= alpha) break;  // Prune
    }
    ret = has_move ? best : Integer::Zero();
  } else {
    Integer best = Integer::Inf();
    for (const auto& [i, j] : move_list) {
      if (!state.PlaceMove(i, j, state.GetPlayerMark())) continue;
      has_move = true;

      auto score = AlphaBeta(state, depth + 1, alpha, beta, true, max_depth);
      state.UndoMove(i, j);  // Backtracking

      if (score < best) {
        best = score;
        current_best_move = {i, j};
      }
      if (best < beta) beta = best;
      if (beta <= alpha) break;  // Prune
    }

    ret = has_move ? best : Integer::Zero();
  }

  // Update TT table
  tte.hash = hash;
  tte.remaining_depth = remaining_depth;
  tte.value = ret;
  tte.best_move = current_best_move;
  if (ret <= original_alpha) {
    tte.flag = TTFlag::kUpperBound;
  } else if (ret >= original_beta) {
    tte.flag = TTFlag::kLowerBound;
  } else {
    tte.flag = TTFlag::kExact;
  }
  SetTTEntry(hash, tte);

  return ret;
}

TTEntry LazySmpAgent::GetTTEntry(uint64_t hash) {
  TTEntryAtomic& atomic_entry = tt_[hash % tt_.size()];
  while (atomic_entry.lock.test_and_set(std::memory_order_acquire));  // spin until lock is acquired
  TTEntry entry_copy = atomic_entry.entry;                            // copy the entry for use
  atomic_entry.lock.clear(std::memory_order_release);                 // release lock
  return entry_copy;
}

void LazySmpAgent::SetTTEntry(uint64_t hash, const TTEntry& new_entry) {
  TTEntryAtomic& atomic_entry = tt_[hash % tt_.size()];
  while (atomic_entry.lock.test_and_set(std::memory_order_acquire));  // spin until lock is acquired
  // Only overwrite if it's a new board state, or if the new search is deeper/equal
  if (atomic_entry.entry.hash != hash ||
      new_entry.remaining_depth >= atomic_entry.entry.remaining_depth) {
    atomic_entry.entry = new_entry;
  }
  atomic_entry.lock.clear(std::memory_order_release);  // release lock
};

std::pair<unsigned int, unsigned int> LazySmpAgent::GetBestMove() {
  while (best_move_lock_.test_and_set(std::memory_order_acquire));
  auto move_copy = best_move_;
  best_move_lock_.clear(std::memory_order_release);
  return move_copy;
}

void LazySmpAgent::SetBestMove(const std::pair<unsigned int, unsigned int>& new_move, int depth) {
  while (best_move_lock_.test_and_set(std::memory_order_acquire));
  if (depth > best_depth_) {
    best_depth_ = depth;
    best_move_ = new_move;
  }
  best_move_lock_.clear(std::memory_order_release);
}