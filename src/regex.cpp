#include "regexforge/regex.h"

#include <utility>

namespace regexforge {

Regex::Regex(std::string pattern, RegexConfig config)
    : pattern_(std::move(pattern)),
      ast_(parse_pattern(pattern_)),
      config_(std::move(config)),
      selector_(PredictiveAnalyzer{}, config_.strategy_thresholds) {}

std::unique_ptr<Matcher> Regex::make_matcher(Strategy strategy) const {
    switch (strategy) {
        case Strategy::Backtracking:
            return std::make_unique<BacktrackingMatcher>(ast_, config_.backtracking_limits);
        case Strategy::ThompsonNFA:
            return std::make_unique<ThompsonNFAMatcher>(ast_, config_.nfa_limits);
        case Strategy::LazyDFA:
            return std::make_unique<LazyDFAMatcher>(ast_, config_.lazy_dfa_limits);
    }
    return nullptr;
}

RegexMatchResult Regex::full_match(std::string_view input) {
    const WorkloadCharacteristics workload{
        input.size(), config_.expected_repetitions, config_.max_dfa_states, config_.allow_lazy_dfa};

    const auto selection = selector_.select(ast_, workload);
    if (!matcher_ || matcher_strategy_ != selection.strategy) {
        matcher_ = make_matcher(selection.strategy);
        matcher_strategy_ = selection.strategy;
    }
    return RegexMatchResult{matcher_->full_match(input), selection.strategy, selection};
}

}  // namespace regexforge
