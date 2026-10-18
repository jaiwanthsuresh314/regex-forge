#include "regexforge/lexer.h"

#include <gtest/gtest.h>

using namespace regexforge;

TEST(LexerTest, EmitsOperatorsAndPositions) {
    const auto tokens = Lexer("(a|b)*").tokenize();
    ASSERT_EQ(tokens.size(), 7U);
    EXPECT_EQ(tokens[0].kind, TokenKind::LeftParen);
    EXPECT_EQ(tokens[1].value, 'a');
    EXPECT_EQ(tokens[2].kind, TokenKind::Pipe);
    EXPECT_EQ(tokens[5].kind, TokenKind::Star);
    EXPECT_EQ(tokens.back().kind, TokenKind::End);
    EXPECT_EQ(tokens.back().position, 6U);
}

TEST(LexerTest, TreatsOrdinaryCharactersAsLiterals) {
    const auto tokens = Lexer("a1_").tokenize();
    EXPECT_EQ(tokens[0].kind, TokenKind::Literal);
    EXPECT_EQ(tokens[1].value, '1');
    EXPECT_EQ(tokens[2].value, '_');
}
