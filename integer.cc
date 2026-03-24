#include "integer.h"

Integer::Integer(int64_t v)
    : value_(v < 0 ? -static_cast<uint64_t>(v) : static_cast<uint64_t>(v)), is_neg_(v < 0) {}

Integer::Integer(bool inf, bool neg) : is_inf_(inf), is_neg_(neg) {}

std::string Integer::ToString() const {
  std::string sign = is_neg_ ? "-" : "";
  if (is_inf_) {
    return std::format("{}inf", sign);
  }
  return std::format("{}{}", sign, value_);
}

[[nodiscard]] bool Integer::operator==(const Integer& rhs) const {
  return is_inf_ == rhs.is_inf_ && is_neg_ == rhs.is_neg_ && value_ == rhs.value_;
}

[[nodiscard]] std::strong_ordering Integer::operator<=>(const Integer& rhs) const {
  // Handle inf
  if (is_inf_ && rhs.is_inf_) {
    if (is_neg_ == rhs.is_neg_) return std::strong_ordering::equal;
    return is_neg_ ? std::strong_ordering::less : std::strong_ordering::greater;
  }
  if (is_inf_) {
    return is_neg_ ? std::strong_ordering::less : std::strong_ordering::greater;
  }
  if (rhs.is_inf_) {
    return rhs.is_neg_ ? std::strong_ordering::greater : std::strong_ordering::less;
  }

  // Not inf, handle different signs
  if (is_neg_ != rhs.is_neg_) {
    return is_neg_ ? std::strong_ordering::less : std::strong_ordering::greater;
  }

  // Both are finite and have the same sign
  if (value_ == rhs.value_) {
    return std::strong_ordering::equal;
  }

  if (!is_neg_) {  // Both positive
    return value_ <=> rhs.value_;
  } else {  // Both negative, reverse the order
    return rhs.value_ <=> value_;
  }
}

void Integer::Set(uint64_t value, bool is_neg, bool is_inf) {
  value_ = value;
  is_inf_ = is_inf;
  is_neg_ = is_neg;
}