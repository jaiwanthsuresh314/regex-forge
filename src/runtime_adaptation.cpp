#include "regexforge/runtime_adaptation.h"

namespace regexforge {

RuntimeAdapter::RuntimeAdapter(AdaptationPolicy policy) : policy_(policy) {}

AdaptationDecision RuntimeAdapter::decide(Strategy current, const RuntimeMonitor& monitor) const {
    AdaptationDecision decision;
    decision.target_strategy = current;
    if (!monitor.exceeded()) {
        decision.reason = "observed runtime metrics remain within configured limits";
        return decision;
    }

    const std::string violation = monitor.violation();
    if (current == Strategy::Backtracking && policy_.allow_nfa_fallback) {
        decision.action = AdaptationAction::RestartWithThompsonNFA;
        decision.target_strategy = Strategy::ThompsonNFA;
        decision.restart_from_input = true;
        decision.reason = violation + "; restart from input with Thompson NFA because backtracking state is not transferable";
        return decision;
    }
    if (current == Strategy::ThompsonNFA && policy_.allow_dfa_fallback) {
        decision.action = AdaptationAction::RestartWithLazyDFA;
        decision.target_strategy = Strategy::LazyDFA;
        decision.restart_from_input = true;
        decision.reason = violation + "; restart from input with lazy DFA because active NFA state is not transferable";
        return decision;
    }
    if (current == Strategy::LazyDFA && policy_.allow_nfa_fallback) {
        decision.action = AdaptationAction::RestartWithThompsonNFA;
        decision.target_strategy = Strategy::ThompsonNFA;
        decision.restart_from_input = true;
        decision.reason = violation + "; restart from input with Thompson NFA because DFA resource growth exceeded its budget";
        return decision;
    }
    if (policy_.abort_if_no_safe_fallback) {
        decision.action = AdaptationAction::Abort;
        decision.reason = violation + "; no safe fallback is enabled";
    } else {
        decision.reason = violation + "; no fallback is enabled, so the caller must apply its own policy";
    }
    return decision;
}

const char* adaptation_action_name(AdaptationAction action) {
    switch (action) {
        case AdaptationAction::Continue: return "continue";
        case AdaptationAction::RestartWithThompsonNFA: return "restart-with-thompson-nfa";
        case AdaptationAction::RestartWithLazyDFA: return "restart-with-lazy-dfa";
        case AdaptationAction::Abort: return "abort";
    }
    return "unknown";
}

}  // namespace regexforge
