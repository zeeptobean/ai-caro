#include "caro.h"

#include <iostream>
#include <random>

Caro::Caro(unsigned height, unsigned width, unsigned k) {
  if (height < k && width < k) {
    throw std::invalid_argument("Invalid board specifications");
  }
  if (k > 18) {
    throw std::invalid_argument("k is too large");
  }
  board_ = std::vector<std::vector<char>>(height, std::vector<char>(width, '.'));

  std::mt19937_64 rng(1337);
  std::uniform_int_distribution<uint64_t> dist;
  zobrist_table_ = std::vector<std::vector<std::pair<uint64_t, uint64_t>>>(
      height, std::vector<std::pair<uint64_t, uint64_t>>(width, {0, 0}));

  for (unsigned i = 0; i < height; ++i) {
    for (unsigned j = 0; j < width; ++j) {
      zobrist_table_.at(i).at(j) = {dist(rng), dist(rng)};
    }
  }
  this->n_ = height;
  this->m_ = width;
  this->k_ = k;
}

void Caro::Display() const {
  for (const auto& row : board_) {
    for (const auto& cell : row) {
      std::cout << cell << " ";
    }
    std::cout << "\n";
  }
}

[[nodiscard]] bool Caro::PlaceMove(unsigned row, unsigned col, char mark) {
  if (row >= n_ || col >= m_ || board_[row][col] != '.') {
    return false;  // Invalid move
  }
  board_[row][col] = mark;

  const auto& pp = zobrist_table_.at(row).at(col);
  current_hash_ ^= (mark == kMarkPlayer) ? pp.first : pp.second;
  return true;
}

void Caro::UndoMove(unsigned row, unsigned col) {
  if (row < n_ && col < m_) {
    const auto& pp = zobrist_table_.at(row).at(col);
    char& mark = board_.at(row).at(col);
    current_hash_ ^= (mark == kMarkPlayer) ? pp.first : pp.second;

    mark = '.';
  }
}

Caro::GameState Caro::CheckGameState() const {
  bool has_blank_space = false;

  auto check_direction = [&](unsigned row, unsigned col, int dr, int dc, char mark) {
    for (unsigned step = 0; step < k_; ++step) {
      int tr = static_cast<int>(row) + static_cast<int>(step) * dr;
      int tc = static_cast<int>(col) + static_cast<int>(step) * dc;
      if (tr < 0 || tc < 0 || tr >= static_cast<int>(n_) || tc >= static_cast<int>(m_) ||
          board_.at(static_cast<unsigned>(tr)).at(static_cast<unsigned>(tc)) != mark) {
        return false;
      }
    }
    return true;
  };

  for (unsigned i = 0; i < n_; i++) {
    for (unsigned j = 0; j < m_; j++) {
      char cell = board_[i][j];
      if (cell == '.') {
        has_blank_space = true;
        continue;
      }

      // 4 directions: horizontal, vertical, diagonal-up, diagonal-down
      if (check_direction(i, j, 0, 1, cell) || check_direction(i, j, 1, 0, cell) ||
          check_direction(i, j, 1, 1, cell) || check_direction(i, j, 1, -1, cell)) {
        return (cell == kMarkPlayer) ? kPlayerWin : kComputerWin;
      }
    }
  }

  return has_blank_space ? kNormal : kDraw;
}

[[nodiscard]] std::vector<std::pair<unsigned, unsigned>> Caro::GetCandidateMoves(int radius) const {
  std::vector<std::pair<unsigned, unsigned>> moves;
  bool has_pieces = false;

  if (radius < 1) radius = 1;

  for (unsigned i = 0; i < n_; i++) {
    for (unsigned j = 0; j < m_; j++) {
      if (board_.at(i).at(j) != '.') {
        has_pieces = true;
        continue;
      }

      int start_x = std::max(0, static_cast<int>(j) - radius);
      int end_x = std::min(static_cast<int>(m_) - 1, static_cast<int>(j) + radius);
      int start_y = std::max(0, static_cast<int>(i) - radius);
      int end_y = std::min(static_cast<int>(n_) - 1, static_cast<int>(i) + radius);

      for (int r = start_y; r <= end_y; r++) {
        for (int c = start_x; c <= end_x; c++) {
          if (board_.at(static_cast<unsigned>(r)).at(static_cast<unsigned>(c)) != '.') {
            moves.push_back({i, j});
            goto Caro_GetCandidateMoves_LoopCont;
          }
        }
      }

    Caro_GetCandidateMoves_LoopCont:
      continue;
    }
  }

  // edge case: Put at center if board is empty
  if (!has_pieces) {
    moves.push_back({n_ / 2, m_ / 2});
  }

  return moves;
}