#include "ybwc.h"

#include <algorithm>
#include <future>
#include <mutex>
#include <vector>

YBWCAgent::YBWCAgent(unsigned threads, unsigned time_soft_limit_ms, int max_depth, int radius)
    : AlphaBetaAgent(time_soft_limit_ms, max_depth, radius), max_threads_(threads) {
  timer_check_factor = 6;
}

std::pair<unsigned, unsigned> YBWCAgent::GetMove(Caro state) {
  ClearCancel();
  start_time_ = std::chrono::steady_clock::now();
  std::pair<unsigned, unsigned> best_move = {0, 0};

  auto moves = state.GetCandidateMoves(move_radius_);
  if (moves.empty()) return best_move;

  // Iterative deepening
  for (int d = 1; d <= max_depth_; d++) {
    try {
      Integer best_value = Integer::NegInf();
      std::pair<unsigned, unsigned> current_best_move = best_move;

      for (const auto& [i, j] : moves) {
        if (!state.PlaceMove(i, j, my_mark_)) continue;

        Integer value =
            SearchYBWC(state, 1, Integer::NegInf(), Integer::Inf(), false, d, max_threads_);

        state.UndoMove(i, j);

        if (value > best_value) {
          best_value = value;
          current_best_move = {i, j};
        }
      }
      best_move = current_best_move;
    } catch (const TimeOutException&) {
      break;
    }
  }

  return best_move;
}

void YBWCAgent::OrderMoves(Caro& state, std::vector<std::pair<unsigned, unsigned>>& moves,
                           bool is_maximizing) {
  // Pair each move with its static evaluation score

  CheckTime();

  struct OrderScoring {
    std::pair<unsigned, unsigned> move;
    Integer score;
  };

  std::vector<OrderScoring> scored_moves;
  scored_moves.reserve(moves.size());

  for (const auto& move : moves) {
    if (state.PlaceMove(move.first, move.second,
                        is_maximizing ? Caro::kMarkComputer : Caro::kMarkPlayer)) {
      Integer score = EvaluateBoard(state);
      state.UndoMove(move.first, move.second);
      scored_moves.push_back({move, score});
    }
  }

  // Sort the moves based on the score.
  // If maximizing (Computer), we want the highest scores first.
  // If minimizing (Player), the player wants the lowest scores first.
  if (is_maximizing) {
    std::sort(scored_moves.begin(), scored_moves.end(),
              [](const auto& a, const auto& b) { return a.score > b.score; });
  } else {
    std::sort(scored_moves.begin(), scored_moves.end(),
              [](const auto& a, const auto& b) { return a.score < b.score; });
  }

  for (size_t i = 0; i < moves.size(); ++i) {
    moves[i] = scored_moves[i].move;
  }
}

Integer YBWCAgent::SearchYBWC(Caro& state, int depth, Integer alpha, Integer beta,
                              bool is_maximizing, int max_depth, unsigned available_threads) {
  CheckTime();

  Caro::GameState gs = state.CheckGameState();
  if (gs != Caro::GameState::kNormal) return TerminalScore(gs, depth);
  if (depth >= max_depth) return EvaluateBoard(state);

  auto moves = state.GetCandidateMoves(move_radius_);
  char mark = is_maximizing ? Caro::kMarkComputer : Caro::kMarkPlayer;
  if (moves.empty()) return Integer::Zero();
  OrderMoves(state, moves, is_maximizing);

  // Oldest brother: Search the first child sequentially to establish better bounds for pruning
  if (!state.PlaceMove(moves[0].first, moves[0].second, mark)) {
    return Integer::Zero();
  }

  Integer best_score =
      SearchYBWC(state, depth + 1, alpha, beta, !is_maximizing, max_depth, available_threads);
  state.UndoMove(moves[0].first, moves[0].second);

  if (is_maximizing) {
    if (best_score > alpha) alpha = best_score;
    if (alpha >= beta) return best_score;  // Prune
  } else {
    if (best_score < beta) beta = best_score;
    if (alpha >= beta) return best_score;  // Prune
  }

  // Young brothers: Parallel search

  // If we are out of threads, or only have 1 move left, fall back to sequential
  if (available_threads <= 1 || moves.size() == 1) {
    return SequentialAlphaBeta(state, depth, alpha, beta, is_maximizing, max_depth, moves,
                               best_score, 1);  // Starts at index 1
  }

  std::vector<std::future<void>> futures;
  std::mutex bounds_mutex;

  // Distribute remaining thread budget to the children to prevent thread explosion
  unsigned threads_per_child =
      std::max(1u, available_threads / static_cast<unsigned>(moves.size() - 1));

  for (size_t i = 1; i < moves.size(); ++i) {
    // Spawn async tasks for the young brothers
    futures.push_back(std::async(std::launch::async, [&, i]() {
      try {
        Caro child_state = state;
        if (!child_state.PlaceMove(moves[i].first, moves[i].second, mark)) {
          return;
        }

        Integer local_alpha, local_beta;
        // Safely read the current shared bounds
        {
          std::lock_guard<std::mutex> lock(bounds_mutex);
          local_alpha = alpha;
          local_beta = beta;
        }

        Integer score = SearchYBWC(child_state, depth + 1, local_alpha, local_beta, !is_maximizing,
                                   max_depth, threads_per_child);

        // Safely update the shared global bounds and best score
        std::lock_guard<std::mutex> lock(bounds_mutex);
        if (is_maximizing) {
          if (score > best_score) best_score = score;
          if (best_score > alpha) alpha = best_score;
        } else {
          if (score < best_score) best_score = score;
          if (best_score < beta) beta = best_score;
        }

        return;
      } catch (const TimeOutException&) {
        return;
      }
    }));
  }

  // Wait for all young brothers
  for (auto& f : futures) {
    f.get();
  }

  return best_score;
}

Integer YBWCAgent::SequentialAlphaBeta(Caro& state, int depth, Integer alpha, Integer beta,
                                       bool is_maximizing, int max_depth,
                                       const std::vector<std::pair<unsigned, unsigned>>& moves,
                                       Integer best_score, size_t start_idx) {
  for (size_t i = start_idx; i < moves.size(); ++i) {
    if (!state.PlaceMove(moves[i].first, moves[i].second,
                         is_maximizing ? Caro::kMarkComputer : Caro::kMarkPlayer)) {
      continue;
    }
    Integer score = SearchYBWC(state, depth + 1, alpha, beta, !is_maximizing, max_depth, 1);
    state.UndoMove(moves[i].first, moves[i].second);

    if (is_maximizing) {
      if (score > best_score) best_score = score;
      if (best_score > alpha) alpha = best_score;
      if (alpha >= beta) return best_score;
    } else {
      if (score < best_score) best_score = score;
      if (best_score < beta) beta = best_score;
      if (alpha >= beta) return best_score;
    }
  }
  return best_score;
}