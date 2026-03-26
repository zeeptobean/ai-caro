#include "alpha_beta.h"

AlphaBetaAgent::AlphaBetaAgent(unsigned time_limit_ms, int max_depth, int radius)
    : max_depth_(max_depth < 1 ? 1 : max_depth), move_radius_(radius) {
  time_limit_ = std::chrono::milliseconds(time_limit_ms);
}

[[nodiscard]] std::pair<unsigned, unsigned> AlphaBetaAgent::GetMove(Caro state) {
  ClearCancel();
  start_time_ = std::chrono::steady_clock::now();
  std::pair<unsigned, unsigned> best_move = {0, 0};
  node_counter_ = 0;

  auto move_list = state.GetCandidateMoves(move_radius_);
  if (!move_list.empty()) best_move = move_list[0];

  // Iterative deepening
  for (int d = 1; d <= max_depth_; d++) {
    try {
      bool move_found = false;
      Integer best_value = Integer::NegInf();
      std::pair<unsigned, unsigned> current_best_move = {0, 0};

      for (const auto& [i, j] : move_list) {
        if (!state.PlaceMove(i, j, Caro::kMarkComputer)) continue;

        auto value = AlphaBeta(state, 1, Integer::NegInf(), Integer::Inf(), false, d);

        state.UndoMove(i, j);  // backtrack

        if (!move_found || value > best_value) {
          best_value = value;
          current_best_move = {i, j};
          move_found = true;
        }
      }
      best_move = current_best_move;
    } catch (const TimeOutException&) {
      break;
    }
  }

  return best_move;
}

Integer AlphaBetaAgent::TerminalScore(Caro::GameState state, int depth) {
  if (state == Caro::GameState::kComputerWin) return Integer::Max() - Integer(depth);
  if (state == Caro::GameState::kPlayerWin) return Integer::Min() + Integer(depth);
  if (state == Caro::GameState::kDraw) return Integer::Zero();
  return Integer::Zero();
}

Integer AlphaBetaAgent::EvaluateBoard(const Caro& state) const {
  int64_t total_score = 0;
  auto [m, n] = state.GetBoardSize();

  auto evaluate_window = [&](unsigned x, unsigned y, int dx, int dy) {
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

      if (state.GetCell(nx, ny) == Caro::kMarkComputer) {
        comp_count++;
      } else if (state.GetCell(nx, ny) == Caro::kMarkPlayer) {
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

  for (unsigned i = 0; i < m; ++i) {
    for (unsigned j = 0; j < n; ++j) {
      evaluate_window(i, j, 0, 1);   // Horizontal
      evaluate_window(i, j, 1, 0);   // Vertical
      evaluate_window(i, j, 1, 1);   // Diagonal down
      evaluate_window(i, j, 1, -1);  // Diagonal up
    }
  }

  return Integer(total_score);
}

Integer AlphaBetaAgent::AlphaBeta(Caro& state, int depth, Integer alpha, Integer beta,
                                  bool is_maximizing, int max_depth) {
  CheckTime();

  Caro::GameState gs = state.CheckGameState();
  Integer ret = Integer::Zero();
  if (gs != Caro::GameState::kNormal) {
    ret = TerminalScore(gs, depth);
    // std::cerr << std::format("Depth: {}, Alpha: {}, Beta: {}, IsMaximizing: {} -> {}\n", depth,
    //                          alpha.ToString(), beta.ToString(), is_maximizing, ret.ToString());
    return ret;
  }
  if (depth >= max_depth) {
    ret = EvaluateBoard(state);
    // std::cerr << std::format("Depth: {}, Alpha: {}, Beta: {}, IsMaximizing: {} -> {}\n", depth,
    //                          alpha.ToString(), beta.ToString(), is_maximizing, ret.ToString());
    return ret;
  }

  bool has_move = false;
  auto move_list = state.GetCandidateMoves(move_radius_);

  if (is_maximizing) {
    Integer best = Integer::NegInf();
    for (const auto& [i, j] : move_list) {
      if (!state.PlaceMove(i, j, Caro::kMarkComputer)) continue;
      has_move = true;

      auto score = AlphaBeta(state, depth + 1, alpha, beta, false, max_depth);
      state.UndoMove(i, j);  // Backtracking

      if (score > best) best = score;
      if (best > alpha) alpha = best;
      if (beta <= alpha) return best;  // Prune
    }
    ret = has_move ? best : Integer::Zero();
  } else {
    Integer best = Integer::Inf();
    for (const auto& [i, j] : move_list) {
      if (!state.PlaceMove(i, j, Caro::kMarkPlayer)) continue;
      has_move = true;

      auto score = AlphaBeta(state, depth + 1, alpha, beta, true, max_depth);
      state.UndoMove(i, j);  // Backtracking

      if (score < best) best = score;
      if (best < beta) beta = best;
      if (beta <= alpha) return best;  // Prune
    }

    ret = has_move ? best : Integer::Zero();
  }

  // std::cerr << std::format("Depth: {}, Alpha: {}, Beta: {}, IsMaximizing: {} -> {}\n", depth,
  //                          alpha.ToString(), beta.ToString(), is_maximizing, ret.ToString());
  return ret;
}
