#include "regexforge/lazy_dfa.h"
#include "regexforge/parser.h"

#include <gtest/gtest.h>

using namespace regexforge;

namespace {
bool matches(const std::string& pattern, const std::string& input) {
    LazyDFAMatcher matcher(parse_pattern(pattern));
    return matcher.full_match(input).matched;
}
}  // namespace

TEST(LazyDfaTest, PerformsFullStringMatching) {
    EXPECT_TRUE(matches("(a|b)*abb", "abababb"));
    EXPECT_FALSE(matches("ab*", "ac"));
    EXPECT_TRUE(matches("a|bc*", "bcc"));
    EXPECT_TRUE(matches("", ""));
    EXPECT_FALSE(matches("", "a"));
}

TEST(LazyDfaTest, ReusesGeneratedStatesAcrossCalls) {
    LazyDFAMatcher matcher(parse_pattern("(a|b)*abb"));
    EXPECT_TRUE(matcher.full_match("abababb").matched);
    const auto generated = matcher.generated_state_count();
    EXPECT_TRUE(matcher.full_match("abb").matched);
    EXPECT_EQ(matcher.generated_state_count(), generated);
}

TEST(LazyDfaTest, FallsBackWhenStateLimitIsReached) {
    LazyDFAMatcher matcher(parse_pattern("(a|aa)*b"), LazyDfaLimits{1, 32ULL * 1024ULL * 1024ULL, 3});
    const auto result = matcher.full_match("aaab");
    EXPECT_TRUE(result.matched);
    EXPECT_TRUE(result.metrics.fell_back);
}

TEST(LazyDfaTest, ReportsLimitFailureAfterConfiguredResets) {
    LazyDFAMatcher matcher(parse_pattern("(a|aa)*b"), LazyDfaLimits{1, 32ULL * 1024ULL * 1024ULL, 0});
    EXPECT_THROW(matcher.full_match("aaab"), std::runtime_error);
}
