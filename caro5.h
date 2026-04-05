#ifndef AICARO_CARO5_H
#define AICARO_CARO5_H

#include <cstdint>
#include <exception>
#include <vector>

enum class TTFlag { kExact, kLowerBound, kUpperBound };

struct TTEntry {
  uint64_t hash = 0;
  int value;
  int remaining_depth = -1;
  TTFlag flag = TTFlag::kExact;
  std::pair<int, int> best_move = {-1, -1};
};

class Caro {
  std::vector<std::vector<char>> board_;
  int m_, n_;
  const int k_ = 5;
  std::vector<std::vector<std::pair<uint64_t, uint64_t>>> zobrist_table_;
  uint64_t current_hash_ = 0;

  uint64_t player_bitboard_[64] = {0};
  uint64_t computer_bitboard_[64] = {0};

  char player_mark_ = kMarkX;
  char computer_mark_ = kMarkO;

  bool CheckGameStateImpl(const uint64_t* bitboard) const;

 public:
  enum GameState { kNormal, kPlayerWin, kComputerWin, kDraw };

  static const char kMarkX = 'X';
  static const char kMarkO = 'O';

  Caro(int, int);

  std::pair<int, int> GetBoardSize() const { return {n_, m_}; }
  int GetK() const { return k_; }
  char GetCell(int row, int col) const { return board_[row][col]; }
  uint64_t GetHash() const { return current_hash_; }

  [[nodiscard]] bool PlaceMove(int row, int col, bool is_player);

  void UndoMove(int row, int col);

  GameState CheckGameState() const;

  [[nodiscard]] std::vector<std::pair<int, int>> GetCandidateMoves(int radius) const;

  void SetPlayerMark(char mark) {
    player_mark_ = mark;
    computer_mark_ = (mark == kMarkX) ? kMarkO : kMarkX;
  }

  char GetPlayerMark() const { return player_mark_; }
  char GetComputerMark() const { return computer_mark_; }
};

#endif  // AICARO_CARO5_H