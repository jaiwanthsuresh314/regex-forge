# Regex Forge

Regex Forge is a C++20 experimental framework for workload-aware regex execution. Milestones 1 and 2 provide the shared front-end foundation and a bounded baseline backtracking matcher.

## Supported grammar

```text
expression    := concatenation ('|' concatenation)*
concatenation := repetition*
repetition    := atom ('*')*
atom          := LITERAL | '(' expression ')'
```

The implementation permits at most one `*` after an atom; a second consecutive `*` is rejected. Ordinary characters are single-character literals. There are no escapes, character classes, Unicode-specific rules, anchors, or newline special cases yet.

An empty pattern matches the empty string. Because concatenation may contain zero repetitions, empty alternatives (`|a`, `a|`, and `|`) and empty groups (`()`) are valid and represented by an explicit `Empty` AST node. The backtracking matcher performs full-string matching.

## Progress

### Milestone 1 — complete

- Lexer and token positions.
- Shared immutable AST and debug printer.
- Recursive-descent parser with precedence, grouping, empty-string semantics, and useful zero-based diagnostics.
- GoogleTest unit tests for lexing, parsing, precedence, empty alternatives, and malformed patterns.

### Milestone 2 — complete

The baseline `BacktrackingMatcher` executes the shared AST using greedy depth-first backtracking. It supports literals, concatenation, alternation, empty nodes, and `*`, while preserving full-string semantics. Each attempted AST node increments a step counter. A configurable step limit throws `ExecutionLimitExceeded`, preventing unbounded pathological execution. Empty repetitions are detected so patterns such as `()*` terminate safely.

The matcher is intentionally a correctness baseline, not a production-safe general-purpose engine. Its metrics expose the ambiguous-work growth that later predictive selection and runtime monitoring will address.

## Build and test

GoogleTest must be available to CMake (for example, through a package configuration visible to `find_package(GTest CONFIG)`). The test target is omitted with a warning when GoogleTest is unavailable.

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build --output-on-failure -C Debug
```

## Current limitations

There is currently no Thompson NFA, lazy DFA, predictive analyzer, runtime monitor, strategy selector, or runtime switching. The parser accepts every non-metacharacter as a literal and reports syntax errors with a zero-based source position. The current matcher has no recursion-depth limit separate from its step limit.

## Next steps

Milestone 3 should add Thompson NFA construction and simulation over the same AST, with active-state metrics and differential tests against the backtracking baseline. After that, the predictive analyzer can compare structural risk and workload features while accounting for compilation and resource costs.

## Independent development workflows

The remaining work is divided into three independent vertical workflows. Integration and merging are intentionally kept separate from these assignments.

### Workflow 1 — Thompson NFA

Owner: Collaborator 1

- Implement NFA states and epsilon transitions.
- Build a Thompson NFA from the shared AST.
- Implement epsilon closure and NFA simulation.
- Track active states and simulation steps.
- Add NFA-specific correctness tests.

Suggested files:

```text
include/regexforge/nfa.h
src/nfa.cpp
tests/nfa_test.cpp
```

### Workflow 2 — Lazy DFA

Owner: Collaborator 2

- Implement on-demand DFA state construction.
- Cache DFA states keyed by NFA state sets.
- Add configurable state and memory limits.
- Define fallback behavior when limits are reached.
- Track generated states, cache hits, and construction cost.
- Add DFA-specific tests.

Suggested files:

```text
include/regexforge/lazy_dfa.h
src/lazy_dfa.cpp
tests/lazy_dfa_test.cpp
```

The lazy DFA workflow may use a private NFA representation rather than depending on internal classes from the Thompson NFA workflow. Both workflows should expose compatible matching results and metrics.

### Workflow 3 — Predictive execution layer

Owner: Project lead

- Extract AST features for nested quantifiers, overlapping alternations, ambiguity, quantifier depth, estimated NFA states, and input length.
- Implement configurable normalized risk scoring.
- Select among backtracking, Thompson NFA, and lazy DFA strategies.
- Record explainable selection decisions and thresholds.
- Define runtime monitoring interfaces.
- Design safe switching or restart-based fallback behavior.
- Add benchmark and evaluation scaffolding.

Suggested files:

```text
include/regexforge/analyzer.h
include/regexforge/strategy_selector.h
include/regexforge/runtime_monitor.h
src/analyzer.cpp
src/strategy_selector.cpp
src/runtime_monitor.cpp
tests/analyzer_test.cpp
tests/selector_test.cpp
benchmarks/
docs/
```

## Shared engine contract

Before parallel implementation, agree on a small public result and matcher contract. Internal NFA and DFA data structures should remain workflow-local.

```cpp
struct MatchResult {
    bool matched;
    ExecutionMetrics metrics;
};

class Matcher {
public:
    virtual MatchResult full_match(std::string_view input) = 0;
    virtual ~Matcher() = default;
};
```

Each workflow should avoid modifying the parser, AST, or existing backtracking engine. Shared changes should be additive interfaces, build configuration, or documentation updates.
