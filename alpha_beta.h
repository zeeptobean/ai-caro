#ifndef AICARO_ALPHA_BETA_H
#define AICARO_ALPHA_BETA_H

#include "agent.h"
#include "caro.h"
#include "integer.h"

class AlphaBetaAgent : public Agent {
 protected:
  int max_depth_;
  uint64_t node_counter_ = 0;

 public:
  explicit AlphaBetaAgent(unsigned time_limit_ms = 2000, int max_depth = 20, int radius = 3);

 protected:
  void GetMoveImpl(Caro& game_state,
                   const std::vector<std::pair<unsigned, unsigned>>& moves) override;
  bool CheckTimeCondition() override;

  Integer TerminalScore(Caro::GameState state, int depth);

  // Heuristic evaluation function for non-terminal states
  Integer EvaluateBoard(const Caro& state) const;

 private:
  Integer AlphaBeta(Caro& state, int depth, Integer alpha, Integer beta, bool is_maximizing,
                    int max_depth);
};

#endif  // AICARO_ALPHA_BETA_H