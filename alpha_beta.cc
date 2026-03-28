#include "alpha_beta.h"

AlphaBetaAgent::AlphaBetaAgent(unsigned time_limit_ms, int max_depth, int radius)
    : Agent(time_limit_ms, radius), max_depth_(max_depth < 1 ? 1 : max_depth) {
  tt_.resize(1ull << 20);
}

void AlphaBetaAgent::ResetBot() { std::fill(tt_.begin(), tt_.end(), TTEntry{}); }

bool AlphaBetaAgent::CheckTimeCondition() { return (++node_counter_ & ((1ull << 10) - 1)) == 0; }

void AlphaBetaAgent::GetMoveImpl(Caro& state,
                                 const std::vector<std::pair<unsigned, unsigned>>& moves) {
  node_counter_ = 0;
  std::vector<std::pair<unsigned, unsigned>> current_moves = moves;

  // Iterative deepening
  for (int d = 1; d <= max_depth_; d++) {
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
  Integer total_score = Integer::Zero();
  auto [n, m] = state.GetBoardSize();
  unsigned K = state.GetK();

  // Treating out-of-bounds as 'blocked' (#)
  auto get_cell = [&](int y, int x) -> char {
    if (x < 0 || y < 0 || x >= static_cast<int>(m) || y >= static_cast<int>(n)) {
      return '#';
    }
    return state.GetCell(static_cast<unsigned>(y), static_cast<unsigned>(x));
  };

  auto evaluate_direction = [&](int y, int x, int dy, int dx) {
    char color = get_cell(y, x);
    if (color == '.' || color == '#') return;

    if (get_cell(y - dy, x - dx) == color) return;

    // Count the contiguous sequence
    unsigned length = 1;
    int ty = y + dy, tx = x + dx;
    while (get_cell(ty, tx) == color) {
      length++;
      ty += dy;
      tx += dx;
    }

    // Check the ends
    char before = get_cell(y - dy, x - dx);
    char after = get_cell(ty, tx);

    int open_ends = 0;
    if (before == '.') open_ends++;
    if (after == '.') open_ends++;

    // If completely blocked and not a winning line, it's dead.
    if (open_ends == 0 && length < K) return;

    uint64_t raw_score = 1;

    if (length >= K) {
      raw_score = (~0ull) - 5;
    } else {
      // Base score is 10^length.
      for (unsigned i = 0; i < length; ++i) {
        raw_score *= 10;
      }

      if (open_ends == 2) {
        raw_score *= 5;
      }
    }

    if (color == state.GetComputerMark()) {
      total_score = total_score + Integer(raw_score, false);
    } else {
      // Defensive preference: multiply opponent score by 1.1x using integer math
      uint64_t def_score = raw_score + (raw_score / 10);
      total_score = total_score - Integer(def_score, false);
    }
  };

  for (unsigned i = 0; i < n; ++i) {
    for (unsigned j = 0; j < m; ++j) {
      int ii = static_cast<int>(i);
      int jj = static_cast<int>(j);
      evaluate_direction(ii, jj, 0, 1);   // Horizontal
      evaluate_direction(ii, jj, 1, 0);   // Vertical
      evaluate_direction(ii, jj, 1, 1);   // Diagonal down right
      evaluate_direction(ii, jj, 1, -1);  // Diagonal up right
    }
  }

  return total_score;
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
  }

  return ret;
}
