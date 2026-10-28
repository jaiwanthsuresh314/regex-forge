#pragma once

#include "regexforge/ast.h"
#include "regexforge/engine.h"

#include <cstddef>
#include <memory>
#include <string_view>

namespace regexforge {

struct LazyDfaLimits {
    std::size_t max_states = 10'000;
    std::size_t max_memory_bytes = 32ULL * 1024ULL * 1024ULL;
    std::size_t max_cache_resets = 3;
};

class LazyDFAMatcher : public Matcher {
public:
    explicit LazyDFAMatcher(Ast ast, LazyDfaLimits limits = {});
    ~LazyDFAMatcher();

    MatchResult full_match(std::string_view input) override;
    std::size_t generated_state_count() const noexcept;
    std::size_t cache_reset_count() const noexcept { return cache_resets_; }

private:
    struct Impl;
    Ast ast_;
    LazyDfaLimits limits_;
    std::unique_ptr<Impl> impl_;
    std::size_t cache_resets_ = 0;
};

}  // namespace regexforge
