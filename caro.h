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

  Caro(unsigned height, unsigned width, unsigned k);

  void Display() const;

  std::pair<unsigned, unsigned> GetBoardSize() const { return {n_, m_}; }
  unsigned GetK() const { return k_; }
  char GetCell(unsigned row, unsigned col) const { return board_[row][col]; }
  uint64_t GetHash() const { return current_hash_; }

  [[nodiscard]] bool PlaceMove(unsigned row, unsigned col, char mark);

  void UndoMove(unsigned row, unsigned col);

  GameState CheckGameState() const;

  [[nodiscard]] std::vector<std::pair<unsigned, unsigned>> GetCandidateMoves(int radius = 1) const;
};

#endif  // AICARO_CARO_H