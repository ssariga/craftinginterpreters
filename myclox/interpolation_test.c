#include <stdio.h>

#include "common.h"
#include "scanner.h"

// Convert a token type into a readable name for test output.
static const char* tokenTypeName(TokenType type)
{
    switch (type)
    {
        case TOKEN_STRING: return "TOKEN_STRING";
        case TOKEN_INTERPOLATION: return "TOKEN_INTERPOLATION";
        case TOKEN_INTERPOLATION_END: return "TOKEN_INTERPOLATION_END";
        case TOKEN_IDENTIFIER: return "TOKEN_IDENTIFIER";
        case TOKEN_PLUS: return "TOKEN_PLUS";
        case TOKEN_EOF: return "TOKEN_EOF";
        case TOKEN_ERROR: return "TOKEN_ERROR";
        default: return "OTHER";
    }
}

// Print a token's type and the exact source text it covers.
static void printToken(Token token)
{
    printf("%-24s '%.*s'\n",tokenTypeName(token.type),token.length, token.start);
}

int main(void) {
    // The nested interpolation example from the challenge.
    const char* source =
        "\"Nested ${\"interpolation?! Are you ${\"mad?!\"}\"}\"";

    initScanner(source);

    
    // Scan until EOF or an error token.
    for (;;)
    {
        Token token = scanToken();

        printToken(token);

        if (token.type == TOKEN_EOF || token.type == TOKEN_ERROR) {
            break;
        }
    }

    return 0;
}