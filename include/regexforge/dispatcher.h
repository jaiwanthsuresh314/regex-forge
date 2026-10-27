#pragma once

#include "regexforge/engine.h"
#include "regexforge/strategy_selector.h"

#include <functional>
#include <memory>
#include <string_view>

namespace regexforge {

using MatcherFactory = std::function<std::unique_ptr<Matcher>(Strategy)>;

struct DispatchResult {
    Strategy strategy = Strategy::Backtracking;
    SelectionDecision selection;
    MatchResult match;
};

class EngineDispatcher {
public:
    explicit EngineDispatcher(StrategySelector selector = {});

    DispatchResult full_match(const Ast& ast, std::string_view input,
                              const WorkloadCharacteristics& workload,
                              const MatcherFactory& factory) const;

private:
    StrategySelector selector_;
};

}  // namespace regexforge
