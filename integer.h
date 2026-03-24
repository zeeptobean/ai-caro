#ifndef AICARO_INTEGER_H
#define AICARO_INTEGER_H

#include <compare>
#include <cstdint>
#include <format>
#include <string>

class Integer {
 public:
  Integer() = default;

  explicit Integer(int64_t v);

  Integer(bool inf, bool neg);

  std::string ToString() const;

  static Integer Inf() { return Integer(true, false); }
  static Integer NegInf() { return Integer(true, true); }
  static Integer Zero() { return Integer(0); }

  [[nodiscard]] bool operator==(const Integer& rhs) const;

  [[nodiscard]] std::strong_ordering operator<=>(const Integer& rhs) const;

  void Set(uint64_t value, bool is_neg, bool is_inf);

 private:
  uint64_t value_ = 0;
  bool is_inf_ = false;
  bool is_neg_ = false;
};

#endif  // AICARO_INTEGER_H