#include "caro.h"

#include <iostream>

Caro::Caro(unsigned m, unsigned n, unsigned k) {
  if (m < k && n < k) {
    throw std::invalid_argument("Invalid board specifications");
  }
  board_ = std::vector<std::vector<char>>(m, std::vector<char>(n, '.'));
  this->m_ = m;
  this->n_ = n;
  this->k_ = k;
}

void Caro::Display() const {
  for (const auto& row : board_) {
    for (const auto& cell : row) {
      std::cout << cell;
    }
    std::cout << "\n";
  }
}

[[nodiscard]] bool Caro::PlaceMove(unsigned x, unsigned y, char mark) {
  if (x >= m_ || y >= n_ || board_[x][y] != '.') {
    return false;  // Invalid move
  }
  board_[x][y] = mark;
  return true;
}

void Caro::UndoMove(unsigned x, unsigned y) {
  if (x < m_ && y < n_) {
    board_[x][y] = '.';
  }
}

Caro::GameState Caro::CheckGameState() const {
  bool has_blank_space = false;

  auto check_direction = [&](unsigned x, unsigned y, int dx, int dy, char mark) {
    for (unsigned step = 0; step < k_; ++step) {
      int tnx = static_cast<int>(x) + static_cast<int>(step) * dx;
      int tny = static_cast<int>(y) + static_cast<int>(step) * dy;
      unsigned nx = static_cast<unsigned>(tnx);
      unsigned ny = static_cast<unsigned>(tny);
      if (tnx < 0 || tny < 0 || tnx >= static_cast<int>(m_) || tny >= static_cast<int>(n_) ||
          board_[nx][ny] != mark) {
        return false;
      }
    }
    return true;
  };

  for (unsigned i = 0; i < m_; i++) {
    for (unsigned j = 0; j < n_; j++) {
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

  for (unsigned i = 0; i < m_; i++) {
    for (unsigned j = 0; j < n_; j++) {
      if (board_[i][j] != '.') {
        has_pieces = true;
        continue;
      }

      int start_x = std::max(0, static_cast<int>(i) - radius);
      int end_x = std::min(static_cast<int>(m_) - 1, static_cast<int>(i) + radius);
      int start_y = std::max(0, static_cast<int>(j) - radius);
      int end_y = std::min(static_cast<int>(n_) - 1, static_cast<int>(j) + radius);

      for (int r = start_x; r <= end_x; r++) {
        for (int c = start_y; c <= end_y; c++) {
          if (board_[static_cast<unsigned>(r)][static_cast<unsigned>(c)] != '.') {
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
    moves.push_back({m_ / 2, n_ / 2});
  }

  return moves;
}