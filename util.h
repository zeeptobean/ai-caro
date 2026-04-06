#ifndef AI_CARO_UTIL_H
#define AI_CARO_UTIL_H

struct MoveHistory {
  unsigned i, j;
  bool is_player;
  MoveHistory(unsigned i, unsigned j, bool is_player) : i(i), j(j), is_player(is_player) {}
  MoveHistory() : i(0), j(0), is_player(false) {}
};

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) (void)(P)
#endif

#endif  // AI_CARO_UTIL_H