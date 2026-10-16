#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

#include <stdlib.h>

typedef struct
{
    const char* start;
    const char* current;
    int line;
} Scanner;

Scanner scanner;


typedef enum
{
    SCAN_CODE,
    SCAN_STRING,
} ScannerMode;


//Structure for interpolation framework.
typedef struct ScannerState ScannerState;

struct ScannerState
{
    ScannerMode mode;
    ScannerState* previous;
};

static ScannerState* scannerState = NULL;
//initializes the scanner.
void initScanner(const char* source)
{
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
    scannerState = NULL;
}


//Helper method, challenge question 1, chapter 16.
//allocates the scanner state
static void pushScannerState(ScannerMode mode)
{
    ScannerState* state = malloc(sizeof(ScannerState));

    if (state == NULL)
    {
        fprintf(stderr, "Could not allocate scanner state.\n");
        exit(EXIT_FAILURE);
    }

    state->mode = mode;
    state->previous = scannerState;
    scannerState = state;
}

//Helper method, challenge question 1, chapter 16.
//frees the scanner state
static void popScannerState(void)
{
    ScannerState* finished = scannerState;
    scannerState = finished->previous;
    free(finished);
}


//checks to see if a character is an alphabet character.
static bool isAlpha(char c)
{
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
            c == '_';
}

//checks to see if a character is a digit.
static bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

//We require the source string to be
//a good null-terminated C string.
//If the current character is the null byte,
//then we’ve reached the end.
static bool isAtEnd()
{
    return *scanner.current == '\0';
}

//consumes the current character and returns it.
static char advance()
{
    scanner.current++;
    return scanner.current[-1];
}

//helps make sure we don't consume any non-whitespace characters.
//It simply returns the current character, but it does not consume it.
static char peek()
{
    return *scanner.current;
}

/* like peek() but for one character past the current one.
 * If the current character and the next one are both /,
 * we consume them and then any other characters
 * until the next newline or the end of the source code. */
static char peekNext()
{
    if (isAtEnd()) return '\0';
    return scanner.current[1];
}


//creates/handles the logic for consuming the second character
//in a less than or equal, not equal, equals, and greater than or equal
//tokens.
static bool match(char expected)
{
    if (isAtEnd()) return false;
    if (*scanner.current != expected) return false;
    scanner.current++;
    return true;
}

//creates the token.
static Token makeToken(TokenType type)
{
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;
    return token;
}

//handles the return of error tokens.
static Token errorToken(const char* message)
{
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner.line;
    return token;
}

//advances the scanner past any leading whitespace.
static void skipWhitespace()
{
    for (;;)
    {
        char c = peek();
        switch (c)
        {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                scanner.line++;
                advance();
                break;
            case '/':
                if (peekNext() == '/') {
                    // A comment goes until the end of the line.
                    while (peek() != '\n' && !isAtEnd()) advance();
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static TokenType checkKeyword(int start, int length,
    const char* rest, TokenType type)
{
    if (scanner.current - scanner.start == start + length && memcmp(scanner.start + start, rest, length) == 0)
    {
        return type;
    }

    return TOKEN_IDENTIFIER;
}
static TokenType identifierType()
{

    switch (scanner.start[0])
    {
        case 'a': return checkKeyword(1, 2, "nd", TOKEN_AND);
        case 'c': return checkKeyword(1, 4, "lass", TOKEN_CLASS);
        case 'e': return checkKeyword(1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
                    case 'u': return checkKeyword(2, 1, "n", TOKEN_FUN);
                }
            }
            break;
        case 'i': return checkKeyword(1, 1, "f", TOKEN_IF);
        case 'n': return checkKeyword(1, 2, "il", TOKEN_NIL);
        case 'o': return checkKeyword(1, 1, "r", TOKEN_OR);
        case 'p': return checkKeyword(1, 4, "rint", TOKEN_PRINT);
        case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
        case 's': return checkKeyword(1, 4, "uper", TOKEN_SUPER);
        case 't':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'h': return checkKeyword(2, 2, "is", TOKEN_THIS);
                    case 'r': return checkKeyword(2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        case 'v': return checkKeyword(1, 2, "ar", TOKEN_VAR);
        case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);
    }
    return TOKEN_IDENTIFIER;
}

/*After the first letter, we allow digits too,
 *and we keep consuming alphanumerics until we run out of them.
 *Then we produce a token with the proper type. */
static Token identifier()
{
    while (isAlpha(peek()) || isDigit(peek())) advance();
    return makeToken(identifierType());
}


//finishes scanning the number
static Token number()
{
    while (isDigit(peek())) advance();

    // Look for a fractional part.
    if (peek() == '.' && isDigit(peekNext()))
    {
        // Consume the ".".
        advance();

        while (isDigit(peek())) advance();
    }

    return makeToken(TOKEN_NUMBER);
}


//consume characters until we reach the closing quote.
//We also track newlines inside the string literal.
static Token string()
{
    while (peek() != '"' && !isAtEnd())
    {
        if (peek() == '\n') scanner.line++;
        advance();
    }

    if (isAtEnd()) return errorToken("Unterminated string.");

    // The closing quote.
    advance();
    return makeToken(TOKEN_STRING);
}
//scans the interpolated string token. Challenge question 1, chapter 16.
static Token scanStringToken()
{
    // scanner.start was set at the beginning of scanToken().
    // It currently points at the first character after the opening quote,
    // or at the first character after ${.

    // An interpolation can begin immediately.
    if (peek() == '$' && peekNext() == '{') {
        advance();  // Consume $.
        advance();  // Consume {.

        // Remember to return to string mode after this expression.
        pushScannerState(SCAN_STRING);

        return makeToken(TOKEN_INTERPOLATION);
    }

    // Scan literal text until the string ends or interpolation begins.
    while (peek() != '"' && peek() != '$' && !isAtEnd()) {
        if (peek() == '\n') {
            scanner.line++;
        }

        advance();
    }

    if (isAtEnd()) {
        return errorToken("Unterminated string.");
    }

    if (peek() == '"') {
        // Consume the closing quote and return to the enclosing code mode.
        advance();
        popScannerState();

        return makeToken(TOKEN_STRING);
    }

    // We stopped at ${, so return this literal piece.
    return makeToken(TOKEN_STRING);
}

//allows you to scan the tokens.
Token scanToken()
{
    skipWhitespace();
    scanner.start = scanner.current;

    if (isAtEnd())
        return makeToken(TOKEN_EOF);

    // Check string mode BEFORE consuming a character
    if (scannerState != NULL && scannerState->mode == SCAN_STRING)
    {
        return scanStringToken();
    }

    char c = advance();
    if (isAlpha(c))
        return identifier();

    if (isDigit(c))
        return number();


    //scans tokens that are a single character.
    //Modified '}' method for interpolation support, challenge question 1 chapter 16.
    switch (c)
    {
        case '(': return makeToken(TOKEN_LEFT_PAREN);
        case ')': return makeToken(TOKEN_RIGHT_PAREN);
        case '{': return makeToken(TOKEN_LEFT_BRACE);
        case '}':
            if (scannerState != NULL) {
                popScannerState();
                return makeToken(TOKEN_INTERPOLATION_END);
            };
            return makeToken(TOKEN_RIGHT_BRACE);
        case ';': return makeToken(TOKEN_SEMICOLON);
        case ',': return makeToken(TOKEN_COMMA);
        case '.': return makeToken(TOKEN_DOT);
        case '-': return makeToken(TOKEN_MINUS);
        case '+': return makeToken(TOKEN_PLUS);
        case '/': return makeToken(TOKEN_SLASH);
        case '*': return makeToken(TOKEN_STAR);
        case '!':
            return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
            return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<':
            return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>':
            return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '"':
            // Enter string mode before scanning its contents.
            pushScannerState(SCAN_STRING);
            return scanStringToken();

    }

    return errorToken("Unexpected character.");
}

