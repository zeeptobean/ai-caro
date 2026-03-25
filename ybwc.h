#ifndef AICARO_YBWC_H
#define AICARO_YBWC_H

#include "agent.h"
#include "caro.h"
#include "integer.h"

class YBWCAgent : public Agent {
  int max_depth_;
  int move_radius_;
  unsigned max_threads_;

 public:
  explicit YBWCAgent(int depth = 4, int radius = 3, unsigned threads = 4)
      : max_depth_(depth < 1 ? 1 : depth), move_radius_(radius), max_threads_(threads) {}

  [[nodiscard]] std::pair<unsigned, unsigned> GetMove(Caro state) override;

 private:
  void OrderMoves(Caro& state, std::vector<std::pair<unsigned, unsigned>>& moves,
                  bool is_maximizing);
  Integer TerminalScore(Caro::GameState state, int depth);

  // Heuristic evaluation function for non-terminal states
  Integer EvaluateBoard(const Caro& state) const;

  Integer SearchYBWC(Caro& state, int depth, Integer alpha, Integer beta, bool is_maximizing,
                     unsigned available_threads);

  Integer SequentialAlphaBeta(Caro& state, int depth, Integer alpha, Integer beta,
                              bool is_maximizing,
                              const std::vector<std::pair<unsigned, unsigned>>& moves,
                              Integer best_score, size_t start_idx);
};

#endif  // AICARO_YBWC_H