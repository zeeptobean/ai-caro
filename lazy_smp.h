#ifndef AICARO_LAZY_SMP_H
#define AICARO_LAZY_SMP_H

#include <atomic>
#include <cstdint>
#include <vector>

#include "agent.h"
#include "caro.h"

struct TTEntryAtomic {
  std::atomic_flag lock = ATOMIC_FLAG_INIT;
  TTEntry entry;
};

class LazySmpAgent : public Agent {
 public:
  LazySmpAgent(unsigned time_limit_ms, int num_threads, int radius);

  void ResetBot();

 protected:
  void GetMoveImpl(Caro& state, const std::vector<std::pair<unsigned, unsigned>>& moves) override;
  bool CheckTimeCondition() override;

 private:
  std::atomic<uint64_t> node_counter_{0};
  std::atomic_flag best_move_lock_ = ATOMIC_FLAG_INIT;
  int num_threads_;
  std::vector<TTEntryAtomic> tt_;
  int best_depth_ = 0;

  void SearchWorker(int, Caro, std::vector<std::pair<unsigned, unsigned>>);

  Integer AlphaBeta(Caro& state, int depth, Integer alpha, Integer beta, bool is_maximizing,
                    int max_depth);

  Integer TerminalScore(Caro::GameState state, int depth);

  // Heuristic evaluation function for non-terminal states
  Integer EvaluateBoard(const Caro& state) const;

  std::pair<unsigned, unsigned> UnpackMove(uint64_t packed_move) const {
    return {static_cast<unsigned>(packed_move >> 32),
            static_cast<unsigned>(packed_move & 0xFFFFFFFF)};
  }

  uint64_t PackMove(unsigned row, unsigned col) const {
    return (static_cast<uint64_t>(row) << 32) | col;
  }

  TTEntry GetTTEntry(uint64_t hash);
  void SetTTEntry(uint64_t hash, const TTEntry& new_entry);
  std::pair<unsigned int, unsigned int> GetBestMove();
  void SetBestMove(const std::pair<unsigned int, unsigned int>& new_move, int depth);
};

#endif  // AICARO_LAZY_SMP_H