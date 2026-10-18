#include "regexforge/ast_printer.h"
#include "regexforge/parser.h"

#include <gtest/gtest.h>

using namespace regexforge;

TEST(ParserTest, RespectsPrecedenceAndGrouping) {
    EXPECT_EQ(print_ast(parse_pattern("(a|b)*abb")), "Concat(Star(Alt(Literal('a'), Literal('b'))), Literal('a'), Literal('b'), Literal('b'))");
    EXPECT_EQ(print_ast(parse_pattern("ab*")), "Concat(Literal('a'), Star(Literal('b')))");
    EXPECT_EQ(print_ast(parse_pattern("a|bc*")), "Alt(Literal('a'), Concat(Literal('b'), Star(Literal('c'))))");
}

TEST(ParserTest, RepresentsEmptyPatternAndAlternatives) {
    EXPECT_EQ(print_ast(parse_pattern("")), "Empty");
    EXPECT_EQ(print_ast(parse_pattern("|a")), "Alt(Empty, Literal('a'))");
    EXPECT_EQ(print_ast(parse_pattern("a|")), "Alt(Literal('a'), Empty)");
    EXPECT_EQ(print_ast(parse_pattern("()")), "Empty");
}

TEST(ParserTest, ReportsMalformedPatterns) {
    for (const auto* pattern : {"*", "a**", "(ab", "ab)", "a|*"}) {
        EXPECT_THROW(parse_pattern(pattern), ParseError) << pattern;
    }
    try { parse_pattern("(ab"); } catch (const ParseError& error) {
        EXPECT_NE(std::string(error.what()).find("expected ')'"), std::string::npos);
    }
}
