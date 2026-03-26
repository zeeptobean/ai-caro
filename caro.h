#ifndef AICARO_CARO_H
#define AICARO_CARO_H

#include <cstdint>
#include <exception>
#include <vector>

class Caro {
  std::vector<std::vector<char>> board_;
  unsigned m_, n_, k_;
  std::vector<std::vector<std::pair<uint64_t, uint64_t>>> zobrist_table_;
  uint64_t current_hash_ = 0;

 public:
  enum GameState { kNormal, kPlayerWin, kComputerWin, kDraw };

  static const char kMarkPlayer = 'X';
  static const char kMarkComputer = 'O';
  static const char kEmptyCell = '.';

  Caro(unsigned m, unsigned n, unsigned k);

  void Display() const;

  std::pair<unsigned, unsigned> GetBoardSize() const { return {m_, n_}; }
  unsigned GetK() const { return k_; }
  char GetCell(unsigned x, unsigned y) const { return board_[x][y]; }
  uint64_t GetHash() const { return current_hash_; }

  [[nodiscard]] bool PlaceMove(unsigned x, unsigned y, char mark);

  void UndoMove(unsigned x, unsigned y);

  GameState CheckGameState() const;

  [[nodiscard]] std::vector<std::pair<unsigned, unsigned>> GetCandidateMoves(int radius = 1) const;
};

#endif  // AICARO_CARO_H