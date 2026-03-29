#include "alpha_beta.h"

#include "util.h"

AlphaBetaAgent::AlphaBetaAgent(unsigned time_limit_ms, int radius) : Agent(time_limit_ms, radius) {
  tt_.resize(1ull << 20);
}

void AlphaBetaAgent::ResetBot() { std::fill(tt_.begin(), tt_.end(), TTEntry{}); }

bool AlphaBetaAgent::CheckTimeCondition() { return (++node_counter_ & ((1ull << 10) - 1)) == 0; }

void AlphaBetaAgent::GetMoveImpl(Caro& state,
                                 const std::vector<std::pair<unsigned, unsigned>>& moves) {
  node_counter_ = 0;
  std::vector<std::pair<unsigned, unsigned>> current_moves = moves;

  // Iterative deepening
  for (int d = 1; d <= std::numeric_limits<int>::max(); d++) {
    bool move_found = false;
    Integer best_value = Integer::NegInf();
    std::pair<unsigned, unsigned> current_best_move = {0, 0};

    // PV-Move Ordering for the root level:
    // Place the best move from the previous iteration at the front
    if (d > 1) {
      for (size_t i = 0; i < current_moves.size(); ++i) {
        if (current_moves[i] == best_move_) {
          std::swap(current_moves[0], current_moves[i]);
          break;
        }
      }
    }

    for (const auto& [i, j] : current_moves) {
      if (!state.PlaceMove(i, j, state.GetComputerMark())) continue;

      auto value = AlphaBeta(state, 1, Integer::NegInf(), Integer::Inf(), false, d);

      state.UndoMove(i, j);  // backtrack

      if (!move_found || value > best_value) {
        best_value = value;
        current_best_move = {i, j};
        move_found = true;
      }
    }
    best_move_ = current_best_move;
  }
  // std::cerr << "Max depth reached!\n";
}

Integer AlphaBetaAgent::TerminalScore(Caro::GameState state, int depth) {
  if (state == Caro::GameState::kComputerWin) return Integer::Max() - Integer(depth);
  if (state == Caro::GameState::kPlayerWin) return Integer::Min() + Integer(depth);
  if (state == Caro::GameState::kDraw) return Integer::Zero();
  return Integer::Zero();
}

Integer AlphaBetaAgent::EvaluateBoard(const Caro& state) const {
  auto [rows, cols] = state.GetBoardSize();
  unsigned k = state.GetK();
  bool is_player_next_turn = (state.GetPlayerMovesCount() == state.GetComputerMovesCount());
  bool is_comp_turn = !is_player_next_turn;
  /*
    auto get_shape_score = [k](unsigned consecutive, unsigned blocks, bool buff) -> Integer {
      if (blocks == 2 && consecutive < k) {
        return Integer::Zero();
      }

      unsigned urgency = 0;
      if (consecutive >= k) {
        urgency = 6;
      } else if (consecutive == k - 1) {
        urgency = 4;
      } else if (consecutive == k - 2) {
        urgency = 2;
      } else if (consecutive == k - 3) {
        urgency = 1;
      } else {
        return Integer::Zero();
      }

      if (consecutive < k) {
        urgency += (blocks == 0);  // +1 tier if unblocked
        urgency += buff;           // +1 tier to whom side get to play the next stone
      }

      switch (urgency) {
        case 0:
          return Integer::Zero();
        case 1:
          return Integer(10);
        case 2:
          return Integer(100);
        case 3:
          return Integer(1000);  // e.g., Open (k-2)
        case 4:
          return Integer(10000);  // e.g., Closed (k-1)
        case 5:
          return Integer(100000);  // e.g., Open (k-1)
        default:
          return Integer(10000000);  // Actual Win (urgency 6+)
      }
    };
  */

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

Integer AlphaBetaAgent::AlphaBeta(Caro& state, int depth, Integer alpha, Integer beta,
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
  TTEntry& tte = tt_[hash % tt_.size()];
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

  return ret;
}
