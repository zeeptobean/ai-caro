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

  Integer(bool neg, bool inf) : is_neg_(neg), is_inf_(inf) {}
  Integer(uint64_t value, bool is_neg, bool is_inf = false)
      : value_(value), is_neg_(is_neg), is_inf_(is_inf) {}

  std::string ToString() const;

  static Integer Inf() { return Integer(false, true); }
  static Integer NegInf() { return Integer(true, true); }
  static Integer Max() { return Integer(~0ull, false, false); }
  static Integer Min() { return Integer(~0ull, true, false); }
  static Integer Zero() { return Integer(0); }

  [[nodiscard]] bool operator==(const Integer& rhs) const;

  [[nodiscard]] std::strong_ordering operator<=>(const Integer& rhs) const;

  [[nodiscard]] Integer operator+(const Integer& rhs) const;
  [[nodiscard]] Integer operator-(const Integer& rhs) const;

 private:
  uint64_t value_ = 0;
  bool is_neg_ = false;
  bool is_inf_ = false;
};

#endif  // AICARO_INTEGER_H