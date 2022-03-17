// Copyright 2022 DeepMind Technologies Limited
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_DEEPMIND_CODE_CONTESTS_EXECUTION_STATUS_MATCHERS_H_
#define THIRD_PARTY_DEEPMIND_CODE_CONTESTS_EXECUTION_STATUS_MATCHERS_H_

#include "gmock/gmock.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"

namespace deepmind::code_contests {
namespace internal {

#ifndef ASSERT_OK_AND_ASSIGN
#define ASSERT_OK_AND_ASSIGN_IMPL(statusor, lhs, rexpr) \
  auto statusor = (rexpr);                              \
  ASSERT_THAT(statusor.status(), IsOk());               \
  lhs = std::move(statusor).value();

#define ASSERT_OK_AND_ASSIGN(lhs, rexpr) \
  ASSERT_OK_AND_ASSIGN_IMPL(MACROS_IMPL_CONCAT(_statusor, __LINE__), lhs, rexpr)
#endif

// Implements a gMock matcher that checks that an absl::StatusOr<T> has
// an OK status and that the contained T value matches another matcher.
template <typename T>
class IsOkAndHoldsMatcher
    : public ::testing::MatcherInterface<const absl::StatusOr<T>&> {
 public:
  template <typename MatcherT>
  IsOkAndHoldsMatcher(MatcherT&& value_matcher)
      : value_matcher_(::testing::SafeMatcherCast<const T&>(value_matcher)) {}

  // From testing::MatcherInterface.
  void DescribeTo(std::ostream* os) const override {
    *os << "is OK and contains a value that ";
    value_matcher_.DescribeTo(os);
  }

  // From testing::MatcherInterface.
  void DescribeNegationTo(std::ostream* os) const override {
    *os << "is not OK or contains a value that ";
    value_matcher_.DescribeNegationTo(os);
  }

  // From testing::MatcherInterface.
  bool MatchAndExplain(
      const absl::StatusOr<T>& status_or,
      ::testing::MatchResultListener* listener) const override {
    if (!status_or.ok()) {
      *listener << "which is not OK";
      return false;
    }

    ::testing::StringMatchResultListener value_listener;
    bool is_a_match =
        value_matcher_.MatchAndExplain(status_or.value(), &value_listener);
    std::string value_explanation = value_listener.str();
    if (!value_explanation.empty()) {
      *listener << absl::StrCat("which contains a value ", value_explanation);
    }

    return is_a_match;
  }

 private:
  const ::testing::Matcher<const T&> value_matcher_;
};

template <typename ValueMatcherT>
class IsOkAndHoldsGenerator {
 public:
  explicit IsOkAndHoldsGenerator(ValueMatcherT value_matcher)
      : value_matcher_(std::move(value_matcher)) {}

  template <typename T>
  operator ::testing::Matcher<const absl::StatusOr<T>&>() const {
    return ::testing::MakeMatcher(new IsOkAndHoldsMatcher<T>(value_matcher_));
  }

 private:
  const ValueMatcherT value_matcher_;
};

class IsOkMatcher {
 public:
  template <typename StatusT>
  bool MatchAndExplain(const StatusT& status_container,
                       ::testing::MatchResultListener* listener) const {
    if (!status_container.ok()) {
      *listener << "which is not OK";
      return false;
    }
    return true;
  }

  void DescribeTo(std::ostream* os) const { *os << "is OK"; }

  void DescribeNegationTo(std::ostream* os) const { *os << "is not OK"; }
};

template <typename Enum>
class StatusIsMatcher {
 public:
  StatusIsMatcher(const StatusIsMatcher&) = default;
  StatusIsMatcher& operator=(const StatusIsMatcher&) = default;

  StatusIsMatcher(Enum code, absl::optional<absl::string_view> message)
      : code_{code}, message_{message} {}

  template <typename T>
  bool MatchAndExplain(const T& value,
                       ::testing::MatchResultListener* listener) const {
    auto status = GetStatus(value);
    if (code_ != status.code()) {
      *listener << "whose error code is generic::"
                << absl::StatusCodeToString(status.code());
      return false;
    }
    if (message_.has_value() && status.message() != message_.value()) {
      *listener << "whose error message is '" << message_.value() << "'";
      return false;
    }
    return true;
  }

  void DescribeTo(std::ostream* os) const {
    *os << "has a status code that is generic::"
        << absl::StatusCodeToString(code_);
    if (message_.has_value()) {
      *os << ", and has an error message that is '" << message_.value() << "'";
    }
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "has a status code that is not generic::"
        << absl::StatusCodeToString(code_);
    if (message_.has_value()) {
      *os << ", and has an error message that is not '" << message_.value()
          << "'";
    }
  }

 private:
  template <typename StatusT,
            typename std::enable_if<
                !std::is_void<decltype(std::declval<StatusT>().code())>::value,
                int>::type = 0>
  static const StatusT& GetStatus(const StatusT& status) {
    return status;
  }

  template <typename StatusOrT,
            typename StatusT = decltype(std::declval<StatusOrT>().status())>
  static StatusT GetStatus(const StatusOrT& status_or) {
    return status_or.status();
  }

  const Enum code_;
  const absl::optional<std::string> message_;
};

}  // namespace internal

inline ::testing::PolymorphicMatcher<internal::IsOkMatcher> IsOk() {
  return ::testing::MakePolymorphicMatcher(internal::IsOkMatcher{});
}

template <typename ValueMatcherT>
internal::IsOkAndHoldsGenerator<ValueMatcherT> IsOkAndHolds(
    ValueMatcherT value_matcher) {
  return internal::IsOkAndHoldsGenerator<ValueMatcherT>(value_matcher);
}

template <typename Enum>
::testing::PolymorphicMatcher<internal::StatusIsMatcher<Enum>> StatusIs(
    Enum code, absl::optional<absl::string_view> message = absl::nullopt) {
  return ::testing::MakePolymorphicMatcher(
      internal::StatusIsMatcher<Enum>{code, message});
}

}  // namespace deepmind::code_contests

#endif  // THIRD_PARTY_DEEPMIND_CODE_CONTESTS_EXECUTION_STATUS_MATCHERS_H_
