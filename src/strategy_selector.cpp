#include "regexforge/strategy_selector.h"

#include <sstream>
#include <utility>

namespace regexforge {
StrategySelector::StrategySelector(PredictiveAnalyzer analyzer, StrategyThresholds thresholds)
    : analyzer_(std::move(analyzer)), thresholds_(thresholds) {}

SelectionDecision StrategySelector::select(const Ast& ast, const WorkloadCharacteristics& workload) const {
    SelectionDecision decision;
    decision.analysis = analyzer_.analyze(ast, workload.input_length);
    const double risk = decision.analysis.risk_score;
    std::ostringstream reason;
    reason << "risk=" << risk;
    const bool workload_amortizes_dfa =
        workload.expected_repetitions >= thresholds_.min_dfa_repetitions ||
        workload.input_length >= thresholds_.min_dfa_input_length;
    const bool dfa_risk_justified = risk >= thresholds_.dfa_risk ||
                                    (risk >= thresholds_.nfa_risk && workload_amortizes_dfa);
    if (dfa_risk_justified && workload.allow_lazy_dfa && workload.max_dfa_states > 0 &&
        workload_amortizes_dfa) {
        decision.strategy = Strategy::LazyDFA;
        reason << "; risk and workload justify DFA construction, state budget available";
    } else if (risk >= thresholds_.nfa_risk || workload.input_length > thresholds_.max_backtracking_input_length) {
        decision.strategy = Strategy::ThompsonNFA;
        reason << "; bounded backtracking is undesirable for this risk or input size";
    } else {
        decision.strategy = Strategy::Backtracking;
        reason << "; low predicted risk and compilation overhead favors baseline";
    }
    decision.reason = reason.str();
    return decision;
}

const char* strategy_name(Strategy strategy) {
    switch (strategy) {
        case Strategy::Backtracking: return "backtracking";
        case Strategy::ThompsonNFA: return "thompson-nfa";
        case Strategy::LazyDFA: return "lazy-dfa";
    }
    return "unknown";
}
}  // namespace regexforge
