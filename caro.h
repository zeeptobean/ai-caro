#ifndef AICARO_CARO_H
#define AICARO_CARO_H

#include <cstdint>
#include <exception>
#include <vector>

#include "integer.h"

enum class TTFlag { kExact, kLowerBound, kUpperBound };

struct TTEntry {
  uint64_t hash = 0;
  Integer value;
  int remaining_depth = -1;
  TTFlag flag = TTFlag::kExact;
  std::pair<unsigned, unsigned> best_move = {0, 0};
};

class Caro {
  std::vector<std::vector<char>> board_;
  unsigned m_, n_, k_;
  std::vector<std::vector<std::pair<uint64_t, uint64_t>>> zobrist_table_;
  uint64_t current_hash_ = 0;

  char player_mark_ = kMarkX;
  char computer_mark_ = kMarkO;

 public:
  enum GameState { kNormal, kPlayerWin, kComputerWin, kDraw };

  static const char kMarkX = 'X';
  static const char kMarkO = 'O';
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

  [[nodiscard]] std::vector<std::pair<unsigned, unsigned>> GetCandidateMoves(int radius) const;

  void SetPlayerMark(char mark) {
    player_mark_ = mark;
    computer_mark_ = (mark == kMarkX) ? kMarkO : kMarkX;
  }

  char GetPlayerMark() const { return player_mark_; }
  char GetComputerMark() const { return computer_mark_; }
};

#endif  // AICARO_CARO_H