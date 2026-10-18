#pragma once

#include <memory>
#include <string>
#include <vector>

namespace regexforge {

struct AstNode {
    enum class Kind { Empty, Literal, Concatenation, Alternation, Star };

    Kind kind;
    char literal{};
    std::vector<std::shared_ptr<const AstNode>> children;

    static std::shared_ptr<const AstNode> empty();
    static std::shared_ptr<const AstNode> literal_node(char value);
    static std::shared_ptr<const AstNode> concatenation(std::vector<std::shared_ptr<const AstNode>> parts);
    static std::shared_ptr<const AstNode> alternation(std::vector<std::shared_ptr<const AstNode>> branches);
    static std::shared_ptr<const AstNode> star(std::shared_ptr<const AstNode> child);
};

using Ast = std::shared_ptr<const AstNode>;

}  // namespace regexforge
