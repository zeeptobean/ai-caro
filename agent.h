#ifndef AICARO_AGENT_H
#define AICARO_AGENT_H

#include <utility>

#include "caro.h"

class Agent {
 public:
  virtual ~Agent() = default;
  virtual std::pair<unsigned, unsigned> GetMove(Caro game_state) = 0;

  // void SetMark(char mark) { my_mark_ = mark; }
  char GetMark() const { return my_mark_; }

 protected:
  char my_mark_ = Caro::kMarkComputer;  // default
};

#endif  // AICARO_AGENT_H