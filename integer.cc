#include "integer.h"

Integer::Integer(int64_t v) : is_neg_(v < 0) {
  if (v < 0) {
    value_ = static_cast<uint64_t>(-(v + 1)) + 1;
  } else {
    value_ = static_cast<uint64_t>(v);
  }
}

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

[[nodiscard]] Integer Integer::operator+(const Integer& rhs) const {
  // Handle inf: use lhs sign
  if (is_inf_ && rhs.is_inf_) {
    return is_neg_ ? Integer::NegInf() : Integer::Inf();
  }
  if (is_inf_) {
    return is_neg_ ? Integer::NegInf() : Integer::Inf();
  }
  if (rhs.is_inf_) {
    return rhs.is_neg_ ? Integer::NegInf() : Integer::Inf();
  }

  // Finite, same sign
  if (is_neg_ == rhs.is_neg_) {
    uint64_t out = value_ + rhs.value_;
    // Unsigned overflow detection.
    if (out < value_) {
      return is_neg_ ? Integer::NegInf() : Integer::Inf();
    }
    return Integer(out, is_neg_);
  }

  // Finite, different signs: a + (-b) == a - b
  if (value_ == rhs.value_) {
    return Integer::Zero();
  }
  if (value_ > rhs.value_) {
    return Integer(value_ - rhs.value_, is_neg_);
  } else {
    return Integer(rhs.value_ - value_, rhs.is_neg_);
  }
}

Integer Integer::operator-(const Integer& rhs) const {
  // a - b = a + (-b)
  Integer neg_rhs = rhs;
  if (!neg_rhs.is_inf_ && neg_rhs.value_ == 0) {  // reset sign of zero
    neg_rhs.is_neg_ = false;
  } else {
    neg_rhs.is_neg_ = !neg_rhs.is_neg_;
  }
  return *this + neg_rhs;
}

Integer& Integer::operator+=(const Integer& rhs) {
  *this = *this + rhs;
  return *this;
}

Integer& Integer::operator-=(const Integer& rhs) {
  *this = *this - rhs;
  return *this;
}

Integer Integer::operator-() const {
  Integer neg_rhs = *this;
  if (!is_inf_) {
    neg_rhs.is_neg_ = !neg_rhs.is_neg_;
  }
  return neg_rhs;
}