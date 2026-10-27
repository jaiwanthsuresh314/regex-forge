#include "regexforge/dispatcher.h"

#include <stdexcept>
#include <utility>

namespace regexforge {

EngineDispatcher::EngineDispatcher(StrategySelector selector) : selector_(std::move(selector)) {}

DispatchResult EngineDispatcher::full_match(const Ast& ast, std::string_view input,
                                            const WorkloadCharacteristics& workload,
                                            const MatcherFactory& factory) const {
    if (!factory) throw std::invalid_argument("engine matcher factory is empty");

    DispatchResult result;
    result.selection = selector_.select(ast, workload);
    result.strategy = result.selection.strategy;
    std::unique_ptr<Matcher> matcher = factory(result.strategy);
    if (!matcher) throw std::invalid_argument("engine matcher factory returned null");
    result.match = matcher->full_match(input);
    return result;
}

}  // namespace regexforge
