#include "regexforge/backtracking.h"
#include "regexforge/lazy_dfa.h"
#include "regexforge/nfa.h"
#include "regexforge/parser.h"

#include <gtest/gtest.h>

#include <string_view>

using namespace regexforge;

TEST(DifferentialTest, AllEnginesAgreeAcrossRepresentativeInputs) {
    for (const auto& pattern : {"", "a", "ab*", "a|bc*", "(a|b)*abb", "(a|aa)*b", "()*"}) {
        const auto ast = parse_pattern(pattern);
        for (const auto& input : {"", "a", "b", "ab", "abb", "aaab", "abababb", "c"}) {
            BacktrackingMatcher backtracking(ast);
            ThompsonNFAMatcher nfa(ast);
            LazyDFAMatcher dfa(ast);
            const auto expected = backtracking.full_match(std::string_view(input)).matched;
            EXPECT_EQ(nfa.full_match(std::string_view(input)).matched, expected)
                << "pattern=" << pattern << " input=" << input;
            EXPECT_EQ(dfa.full_match(std::string_view(input)).matched, expected)
                << "pattern=" << pattern << " input=" << input;
        }
    }
}
