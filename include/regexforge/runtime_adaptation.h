#pragma once

#include "regexforge/runtime_monitor.h"
#include "regexforge/strategy_selector.h"

#include <string>

namespace regexforge {

enum class AdaptationAction { Continue, RestartWithThompsonNFA, RestartWithLazyDFA, Abort };

struct AdaptationDecision {
    AdaptationAction action = AdaptationAction::Continue;
    Strategy target_strategy = Strategy::Backtracking;
    bool restart_from_input = false;
    std::string reason;
};

struct AdaptationPolicy {
    bool allow_nfa_fallback = true;
    bool allow_dfa_fallback = true;
    bool abort_if_no_safe_fallback = true;
};

class RuntimeAdapter {
public:
    explicit RuntimeAdapter(AdaptationPolicy policy = {});

    AdaptationDecision decide(Strategy current, const RuntimeMonitor& monitor) const;

private:
    AdaptationPolicy policy_;
};

const char* adaptation_action_name(AdaptationAction action);

}  // namespace regexforge
