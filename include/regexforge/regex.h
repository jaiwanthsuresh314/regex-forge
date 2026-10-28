#pragma once

#include "regexforge/backtracking.h"
#include "regexforge/lazy_dfa.h"
#include "regexforge/nfa.h"
#include "regexforge/parser.h"
#include "regexforge/strategy_selector.h"

#include <cstddef>
#include <array>
#include <memory>
#include <string>
#include <string_view>

namespace regexforge {

struct RegexConfig {
    std::size_t expected_repetitions = 1;
    std::size_t max_dfa_states = 10'000;
    bool allow_lazy_dfa = true;

    BacktrackingLimits backtracking_limits{};
    NfaLimits nfa_limits{};
    LazyDfaLimits lazy_dfa_limits{};
    StrategyThresholds strategy_thresholds{};
};

struct RegexMatchResult {
    MatchResult match;
    Strategy strategy = Strategy::Backtracking;
    SelectionDecision selection;
};

struct EngineComparison {
    Strategy strategy = Strategy::Backtracking;
    MatchResult match;
};

/// Compiled, reusable regular expression using the shared predictive policy.
/// Instances are not thread-safe because lazy DFA caches are mutable.
class Regex {
public:
    explicit Regex(std::string pattern, RegexConfig config = {});

    RegexMatchResult full_match(std::string_view input);
    RegexMatchResult full_match(const std::string& input) { return full_match(std::string_view(input)); }
    RegexMatchResult full_match(const char* input) { return full_match(std::string_view(input)); }
    std::array<EngineComparison, 3> compare_all(std::string_view input) const;

    const std::string& pattern() const noexcept { return pattern_; }
    const Ast& ast() const noexcept { return ast_; }

private:
    std::string pattern_;
    Ast ast_;
    RegexConfig config_;
    StrategySelector selector_;
    std::unique_ptr<Matcher> matcher_;
    Strategy matcher_strategy_ = Strategy::Backtracking;

    std::unique_ptr<Matcher> make_matcher(Strategy strategy) const;
};

}  // namespace regexforge
