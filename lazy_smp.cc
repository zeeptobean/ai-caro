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
  auto [rows, cols] = state.GetBoardSize();
  unsigned k = state.GetK();
  bool is_player_next_turn = (state.GetPlayerMovesCount() == state.GetComputerMovesCount());
  bool is_comp_turn = !is_player_next_turn;

  auto evaluate_line = [&](int r, int c, int dr, int dc, char target_mark, char opp_mark,
                           bool is_my_turn) -> Integer {
    Integer score = Integer::Zero();
    std::vector<char> line;

    while (r >= 0 && r < static_cast<int>(rows) && c >= 0 && c < static_cast<int>(cols)) {
      line.push_back(state.GetCell(static_cast<unsigned>(r), static_cast<unsigned>(c)));
      r += dr;
      c += dc;
    }

    if (line.size() < k) return Integer::Zero();

    // Sliding window
    for (size_t i = 0; i <= line.size() - k; ++i) {
      unsigned target_count = 0;
      unsigned opp_count = 0;

      for (size_t j = 0; j < k; ++j) {
        if (line[i + j] == target_mark)
          target_count++;
        else if (line[i + j] == opp_mark)
          opp_count++;
      }

      if (target_count > 0 && opp_count == 0) {
        unsigned missing = k - target_count;

        if (is_my_turn) {
          if (missing == 0)
            score += Integer(10000000);  // k stones
          else if (missing == 1)
            score += Integer(500000);  // k-1 stones (Unstoppable win)
          else if (missing == 2)
            score += Integer(10000);  // k-2 stones (Will become k-1)
          else if (missing == 3)
            score += Integer(500);  // k-3 stones
          else
            score += Integer(50);  // k-4 or smaller
        } else {
          if (missing == 0)
            score += Integer(10000000);  // k stones
          else if (missing == 1)
            score += Integer(100000);  // k-1 stones (Forces a block)
          else if (missing == 2)
            score += Integer(1000);  // k-2 stones (Standard threat)
          else if (missing == 3)
            score += Integer(100);  // k-3 stones
          else
            score += Integer(10);  // k-4 or smaller
        }
      }
    }
    return score;
  };

  auto evaluate_all_lines_for_mark = [&](char mark, char opp_mark, bool is_my_turn) -> Integer {
    Integer total_score = Integer::Zero();
    // bool buff = (mark == state.GetComputerMark()) ? !is_player_next_turn : is_player_next_turn;

    // 1. Evaluate Rows (Horizontal: dr=0, dc=1)
    for (int r = 0; r < static_cast<int>(rows); ++r) {
      total_score += evaluate_line(r, 0, 0, 1, mark, opp_mark, is_my_turn);
    }

    // 2. Evaluate Columns (Vertical: dr=1, dc=0)
    for (int c = 0; c < static_cast<int>(cols); ++c) {
      total_score += evaluate_line(0, c, 1, 0, mark, opp_mark, is_my_turn);
    }

    // 3. Evaluate Diagonals (Down-Right: dr=1, dc=1)
    for (int c = 0; c < static_cast<int>(cols); ++c) {
      total_score += evaluate_line(0, c, 1, 1, mark, opp_mark, is_my_turn);
    }
    for (int r = 1; r < static_cast<int>(rows); ++r) {
      total_score += evaluate_line(r, 0, 1, 1, mark, opp_mark, is_my_turn);
    }

    // 4. Evaluate Diagonals (Down-Left: dr=1, dc=-1)
    for (int c = 0; c < static_cast<int>(cols); ++c) {
      total_score += evaluate_line(0, c, 1, -1, mark, opp_mark, is_my_turn);
    }
    for (int r = 1; r < static_cast<int>(rows); ++r) {
      total_score +=
          evaluate_line(r, static_cast<int>(cols) - 1, 1, -1, mark, opp_mark, is_my_turn);
    }

    return total_score;
  };

  Integer bot_score =
      evaluate_all_lines_for_mark(state.GetComputerMark(), state.GetPlayerMark(), is_comp_turn);
  Integer player_score = evaluate_all_lines_for_mark(state.GetPlayerMark(), state.GetComputerMark(),
                                                     is_player_next_turn);

  return bot_score - player_score;
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