#include "caro5.h"

#include <algorithm>
#include <iostream>
#include <random>

Caro::Caro(int height, int width) {
  if (height != 64 || width != 64) {
    throw std::invalid_argument("Only 64x64 board is supported.");
  }

  std::mt19937_64 rng(1337);
  std::uniform_int_distribution<uint64_t> dist;
  zobrist_table_ = std::vector<std::vector<std::pair<uint64_t, uint64_t>>>(
      height, std::vector<std::pair<uint64_t, uint64_t>>(width, {0, 0}));

  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      zobrist_table_.at(i).at(j) = {dist(rng), dist(rng)};
    }
  }
  this->n_ = height;
  this->m_ = width;
}

[[nodiscard]] bool Caro::PlaceMove(int row, int col, bool is_player) {
  if (row >= n_ || col >= m_) {
    return false;
  }

  uint64_t bit = (1ULL << col);
  uint64_t occupied = player_bitboard_[row] | computer_bitboard_[row];

  if (is_player) {
    player_bitboard_[row] |= bit;
  } else {
    computer_bitboard_[row] |= bit;
  }

  const auto& pp = zobrist_table_.at(row).at(col);
  current_hash_ ^= (is_player) ? pp.first : pp.second;
  return true;
}

void Caro::UndoMove(int row, int col) {
  if (row < n_ && col < m_) {
    uint64_t bit = (1ULL << col);
    player_bitboard_[row] &= ~bit;
    computer_bitboard_[row] &= ~bit;

    const auto& pp = zobrist_table_.at(row).at(col);
    char& mark = board_.at(row).at(col);
    current_hash_ ^= (mark == GetPlayerMark()) ? pp.first : pp.second;
  }
}

bool Caro::CheckGameStateImpl(const uint64_t bb[64]) const {
  // Horizontal
  for (int r = 0; r < n_; ++r) {
    uint64_t x = bb[r];
    if ((x & (x >> 1) & (x >> 2) & (x >> 3) & (x >> 4)) != 0ULL) {
      return true;
    }
  }

  for (int r = 0; r + 4 < n_; ++r) {
    // Vertical
    if ((bb[r] & bb[r + 1] & bb[r + 2] & bb[r + 3] & bb[r + 4]) != 0ULL) {
      return true;
    }

    // Diagonal down-right (\)
    if ((bb[r] & (bb[r + 1] >> 1) & (bb[r + 2] >> 2) & (bb[r + 3] >> 3) & (bb[r + 4] >> 4)) !=
        0ULL) {
      return true;
    }

    // Diagonal down-left (/)
    if ((bb[r] & (bb[r + 1] << 1) & (bb[r + 2] << 2) & (bb[r + 3] << 3) & (bb[r + 4] << 4)) !=
        0ULL) {
      return true;
    }
  }

  return false;
}

Caro::GameState Caro::CheckGameState() const {
  if (CheckGameStateImpl(player_bitboard_)) {
    return kPlayerWin;
  }
  if (CheckGameStateImpl(computer_bitboard_)) {
    return kComputerWin;
  }
  for (unsigned r = 0; r < n_; ++r) {
    if ((player_bitboard_[r] | computer_bitboard_[r]) != ~0ULL) {
      return kNormal;
    }
  }
  return kDraw;
}

[[nodiscard]] std::vector<std::pair<int, int>> Caro::GetCandidateMoves(int radius) const {
  std::vector<std::pair<int, int>> moves;
  radius = std::clamp(radius, 1, 63);

  uint64_t occupied_rows[64];
  bool has_pieces = false;
  for (int r = 0; r < n_; ++r) {
    occupied_rows[r] = player_bitboard_[r] | computer_bitboard_[r];
    if (occupied_rows[r] != 0ULL) has_pieces = true;
  }

  // Empty board: play center.
  if (!has_pieces) {
    moves.push_back({n_ / 2, m_ / 2});
    return moves;
  }

  for (int i = 0; i < n_; ++i) {
    const int r0 = std::max(0, i - radius);
    const int r1 = std::min(n_ - 1, i + radius);

    // Union of occupied columns in neighboring rows.
    uint64_t near_cols = 0ULL;
    for (int rr = r0; rr <= r1; ++rr) {
      near_cols |= occupied_rows[rr];
    }

    // Dilate horizontally by radius.
    uint64_t dilated = near_cols;
    for (int s = 1; s <= radius; ++s) {
      dilated |= (near_cols << s);
      dilated |= (near_cols >> s);
    }

    uint64_t row_candidates = dilated & ~occupied_rows[i];
    while (row_candidates != 0ULL) {
      int j = std::countr_zero(row_candidates);
      moves.push_back({i, j});
      row_candidates &= (row_candidates - 1);  // clear lowest set bit
    }
  }
  return moves;
}