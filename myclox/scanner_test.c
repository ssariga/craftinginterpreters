#include <stdio.h>

#include "common.h"
#include "scanner.h"

// Convert a token type to a readable name.
static const char* tokenTypeName(TokenType type) {
  switch (type) {
    case TOKEN_LEFT_PAREN: return "LEFT_PAREN";
    case TOKEN_RIGHT_PAREN: return "RIGHT_PAREN";
    case TOKEN_LEFT_BRACE: return "LEFT_BRACE";
    case TOKEN_RIGHT_BRACE: return "RIGHT_BRACE";
    case TOKEN_COMMA: return "COMMA";
    case TOKEN_DOT: return "DOT";
    case TOKEN_MINUS: return "MINUS";
    case TOKEN_PLUS: return "PLUS";
    case TOKEN_SEMICOLON: return "SEMICOLON";
    case TOKEN_SLASH: return "SLASH";
    case TOKEN_STAR: return "STAR";
    case TOKEN_BANG: return "BANG";
    case TOKEN_BANG_EQUAL: return "BANG_EQUAL";
    case TOKEN_EQUAL: return "EQUAL";
    case TOKEN_EQUAL_EQUAL: return "EQUAL_EQUAL";
    case TOKEN_GREATER: return "GREATER";
    case TOKEN_GREATER_EQUAL: return "GREATER_EQUAL";
    case TOKEN_LESS: return "LESS";
    case TOKEN_LESS_EQUAL: return "LESS_EQUAL";
    case TOKEN_IDENTIFIER: return "IDENTIFIER";
    case TOKEN_STRING: return "STRING";
    case TOKEN_NUMBER: return "NUMBER";
    case TOKEN_AND: return "AND";
    case TOKEN_CLASS: return "CLASS";
    case TOKEN_ELSE: return "ELSE";
    case TOKEN_FALSE: return "FALSE";
    case TOKEN_FOR: return "FOR";
    case TOKEN_FUN: return "FUN";
    case TOKEN_IF: return "IF";
    case TOKEN_NIL: return "NIL";
    case TOKEN_OR: return "OR";
    case TOKEN_PRINT: return "PRINT";
    case TOKEN_RETURN: return "RETURN";
    case TOKEN_SUPER: return "SUPER";
    case TOKEN_THIS: return "THIS";
    case TOKEN_TRUE: return "TRUE";
    case TOKEN_VAR: return "VAR";
    case TOKEN_WHILE: return "WHILE";
    case TOKEN_ERROR: return "ERROR";
    case TOKEN_EOF: return "EOF";
    default: return "UNKNOWN";
  }
}

// Print one token's type, lexeme, and line number.
static void printToken(Token token) {
  printf("%-14s line %d: ", tokenTypeName(token.type), token.line);

  // Print the token's source text safely.
  for (int i = 0; i < token.length; i++) {
    putchar(token.start[i]);
  }

  putchar('\n');
}

int main(void) {
  // A small program that exercises keywords, identifiers, strings,
  // numbers, operators, punctuation, comments, and line tracking.
  const char* source =
      "print \"value\";\n"
      "var message = \"hello\";\n"
      "print 1 + 2 * 3; // arithmetic\n"
      "if (true) print message;\n";

  initScanner(source);

  // Scan tokens until the scanner reports EOF or an error.
  for (;;) {
    Token token = scanToken();

    printToken(token);

    if (token.type == TOKEN_EOF || token.type == TOKEN_ERROR) {
      break;
    }
  }

  return 0;
}