#ifndef AI_CARO_UTIL_H
#define AI_CARO_UTIL_H

struct MoveHistory {
  unsigned i, j;
  bool is_player;
};

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) (void)(P)
#endif

#endif  // AI_CARO_UTIL_H