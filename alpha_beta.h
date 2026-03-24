#ifndef AICARO_ALPHA_BETA_H
#define AICARO_ALPHA_BETA_H

#include "agent.h"
#include "caro.h"
#include "integer.h"

class AlphaBetaAgent : public Agent {
  int max_depth_;
  int move_radius_;

 public:
  explicit AlphaBetaAgent(int depth = 4, int radius = 3)
      : max_depth_(depth < 1 ? 1 : depth), move_radius_(radius) {}

  [[nodiscard]] std::pair<unsigned, unsigned> GetMove(Caro state) override;

 private:
  Integer TerminalScore(Caro::GameState state, int depth);

  // Heuristic evaluation function for non-terminal states
  Integer EvaluateBoard(const Caro& state) const;

  Integer AlphaBeta(Caro& state, int depth, Integer alpha, Integer beta, bool is_maximizing,
                    int max_depth);
};

#endif  // AICARO_ALPHA_BETA_H