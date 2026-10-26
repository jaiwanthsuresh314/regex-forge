#pragma once

#include "regexforge/analyzer.h"

#include <cstddef>
#include <string>

namespace regexforge {
enum class Strategy { Backtracking, ThompsonNFA, LazyDFA };

struct WorkloadCharacteristics {
    std::size_t input_length = 0;
    std::size_t expected_repetitions = 1;
    std::size_t max_dfa_states = 1024;
    bool allow_lazy_dfa = true;
};

struct StrategyThresholds {
    double nfa_risk = 30.0;
    double dfa_risk = 60.0;
    std::size_t min_dfa_repetitions = 4;
    std::size_t min_dfa_input_length = 32;
    std::size_t max_backtracking_input_length = 512;
};

struct SelectionDecision {
    Strategy strategy = Strategy::Backtracking;
    AnalysisResult analysis;
    std::string reason;
};

class StrategySelector {
public:
    StrategySelector(PredictiveAnalyzer analyzer = PredictiveAnalyzer{}, StrategyThresholds thresholds = {});
    SelectionDecision select(const Ast& ast, const WorkloadCharacteristics& workload) const;

private:
    PredictiveAnalyzer analyzer_;
    StrategyThresholds thresholds_;
};

const char* strategy_name(Strategy strategy);
}  // namespace regexforge
