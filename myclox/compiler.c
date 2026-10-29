#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif


//creation of the parser structure.
typedef struct
{
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;

} Parser;


typedef enum
{
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_CONDITIONAL,  // ?: , added for mixfix support,Challenge Question 3, Chapter 17
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * /
    PREC_UNARY,       // ! -
    PREC_CALL,        // . ()
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct
{
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;


/*store the name of the variable. When we’re resolving an identifier,
 *we compare the identifier’s lexeme with each local’s name to find a match.
 *The depth field records the scope depth of the block where the local variable was declared.*/
typedef struct {
    Token name;
    int depth;
    bool isConst; //const support, challenge question 3, chapter 22.
} Local;


/*
typedef struct {
    Local locals[UINT8_COUNT];
    int localCount;
    int scopeDepth;
} Compiler; */



//new Compiler struct
//, we need to widen the local-slot operand and
//enlarge the compiler’s local-variable storage.
//A straightforward implementation is to use 16-bit local operands,
//allowing slots 0 through 65535, and dynamically grow the compiler’s local array.
#define LOCAL_LIMIT 65536

typedef struct
{
    Local* locals;
    int localCount;
    int localCapacity;
    int scopeDepth;
} Compiler;

Parser parser;
Compiler* current = NULL;
Chunk* compilingChunk;

static Chunk* currentChunk()
{
    return compilingChunk;
}

static void errorAt(Token* token, const char* message)
{
    if (parser.panicMode)
        return;

    parser.panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing.
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}
static void error(const char* message)
{
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message)
{
    errorAt(&parser.current, message);
}

//advances the parser
static void advance()
{
    parser.previous = parser.current;

    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) break;

        errorAtCurrent(parser.current.start);
    }
}

/*similar to advance() in that it reads the next token.
 *But it also validates that the token has an expected type.
 *If not, it reports an error. This function is the foundation of most syntax errors in the compiler.*/
static void consume(TokenType type, const char* message)
{
    if (parser.current.type == type) {
        advance();
        return;
    }

    errorAtCurrent(message);
}


//The check() function returns true if the current token has the given type.
static bool check(TokenType type)
{
    return parser.current.type == type;
}

/* If the current token has the given type, we consume the token and return true.
 * Otherwise we leave the token alone and return false.
 * This helper function is implemented in terms of this other helper:
 * */
static bool match(TokenType type)
{
    if (!check(type)) return false;
    advance();
    return true;
}

static void emitByte(uint8_t byte)
{
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2)
{
    emitByte(byte1);
    emitByte(byte2);
}

//Challenge Question 4, Chapter 22.
//A local read will now have this layout:
//OP_GET_LOCAL  high-byte  low-byte
//We will widen the existing local instructions rather than add separate “long” opcodes.
//This makes every local read or write one byte larger,
//but keeps the implementation simple.
static void emitShort(uint16_t value)
{
    // Emit the high byte first, then the low byte.
    emitByte((uint8_t)(value >> 8));
    emitByte((uint8_t)(value & 0xff));
}

static void emitReturn()
{
    emitByte(OP_RETURN);
}



static uint8_t makeConstant(Value value)
{
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX)
    {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

static void emitConstant(Value value)
{
    emitBytes(OP_CONSTANT, makeConstant(value));
}

//Extending clox to allow more than 256 local variables to be in scope at a time.
//Challenge Question 4, Chapter 22.
static void initCompiler(Compiler* compiler)
{
    compiler->locals = NULL;
    compiler->localCount = 0;
    compiler->localCapacity = 0;
    compiler->scopeDepth = 0;
    current = compiler;
}

static void endCompiler()
{
    emitReturn();

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError)
    {
        disassembleChunk(currentChunk(), "code");
    }
#endif
}

//call this function to enter a new local scope:
static void beginScope()
{
    current->scopeDepth++;
}

//call this function to exit a local scope:
static void endScope()
{
    current->scopeDepth--;

    while (current->localCount > 0 &&current->locals[current->localCount - 1].depth > current->scopeDepth)
    {
        emitByte(OP_POP);
        current->localCount--;
    }
}

static void expression();
static void statement();
static void declaration();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);


//This function takes the given token and adds its lexeme
//to the chunk’s constant table as a string.
//It then returns the index of that constant in the constant table.
static uint8_t identifierConstant(Token* name)
{
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static bool identifiersEqual(Token* a, Token* b)
{
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

//resolves a local variable
//. We walk the list of locals that are currently in scope. If one has the same name as the identifier token,
//the identifier must refer to that variable.
//We’ve found it,
//We walk the array backward so that we find the last declared variable with the identifier.
//That ensures that inner local variables correctly shadow locals with the same name in surrounding scopes.
static int resolveLocal(Compiler* compiler, Token* name)
{
    for (int i = compiler->localCount - 1; i >= 0; i--)
    {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name))
        {
            if (local->depth == -1)
            {
                error("Can't read local variable in its own initializer.");
            }

            return i;
        }
    }

    return -1;
}
// compiler needs to remember that the variable exists.
//This initializes the next available Local in the compiler’s array of variables.
//It stores the variable’s name and the depth of the scope that owns the variable.
static void addLocal(Token name)
{

    if (current->localCount >= LOCAL_LIMIT)
    {
        error("Too many local variables in function.");
        return;
    }

    if (current->localCount == current->localCapacity)
    {
        int capacity = current->localCapacity < 8 ? 8 : current->localCapacity * 2;

        if (capacity > LOCAL_LIMIT)
        {
            capacity = LOCAL_LIMIT;
        }

        Local* locals = realloc(current->locals,sizeof(Local) * (size_t)capacity);

        if (locals == NULL)
        {
            fprintf(stderr, "Could not allocate compiler locals.\n");
            exit(EXIT_FAILURE);
        }

        current->locals = locals;
        current->localCapacity = capacity;
    }

    Local* local = &current->locals[current->localCount++];
    local->name = name;
    // Keep it uninitialized until its initializer is compiled.
    local->depth = -1;

    local->isConst = false; // initialize const variable, challenge question 3, chapter 22.
}

static void declareVariable()
{
    if (current->scopeDepth == 0)
        return;
    Token* name = &parser.previous;

    for (int i = current->localCount - 1; i >= 0; i--)
    {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth)
        {
            break;
        }

        if (identifiersEqual(name, &local->name))
        {
            error("Already a variable with this name in this scope.");
        }
    }
    addLocal(*name);
}
//consume next token, sends here
static uint8_t parseVariable(const char* errorMessage)
{
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable();
    if (current->scopeDepth > 0)
        return 0;

    return identifierConstant(&parser.previous);
}


//. The variable exists, but in a special “uninitialized” state.
//Then we compile the initializer. If at any point in that expression we resolve an identifier that points back
//to this variable, we’ll see that it is not initialized yet and report an error.
//After we finish compiling the initializer, we mark the variable as initialized and ready for use.
static void markInitialized()
{
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global)
{
    if (current->scopeDepth > 0)
    {
        markInitialized();
        return;
    }
    emitBytes(OP_DEFINE_GLOBAL, global);
}

static void binary(bool canAssign)
{
    TokenType operatorType = parser.previous.type;
    ParseRule* rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_BANG_EQUAL:    emitBytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL:   emitByte(OP_EQUAL); break;
        case TOKEN_GREATER:       emitByte(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
        case TOKEN_LESS:          emitByte(OP_LESS); break;
        case TOKEN_LESS_EQUAL:    emitBytes(OP_GREATER, OP_NOT); break;
        case TOKEN_PLUS:          emitByte(OP_ADD); break;
        //case TOKEN_MINUS:         emitByte(OP_SUBTRACT); break;
        case TOKEN_MINUS: emitBytes(OP_NEGATE, OP_ADD); break; //reduction of binary operations, Challenge Question 1, Chapter 18
        case TOKEN_STAR:          emitByte(OP_MULTIPLY); break;
        case TOKEN_SLASH:         emitByte(OP_DIVIDE); break;
        default: return; // Unreachable.
    }
}

static void literal(bool canAssign)
{
    switch (parser.previous.type)
    {
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        case TOKEN_NIL: emitByte(OP_NIL); break;
        case TOKEN_TRUE: emitByte(OP_TRUE); break;
        default: return; // Unreachable.
    }
}

static void expression()
{
    parsePrecedence(PREC_ASSIGNMENT);
}


// keeps parsing declarations and statements until it hits the closing brace.
// As we do with any loop in the parser, we also check for the end of the token stream.
// This way, if there’s a malformed program with a missing closing curly,
// the compiler doesn’t get stuck in a loop.
static void block()
{
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
    {
        declaration();
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

//modified varDeclaration
//For const support, challenge Question 3, Chapter 22.
static void varDeclaration(bool isConst)
{
    int previousLocalCount = current->localCount;

    uint8_t global = parseVariable("Expect variable name.");

    if (isConst && current->scopeDepth == 0)
    {
        error("Const declarations are only supported in local scopes.");
    }

    // Set the flag only if parseVariable() actually added a local.
    // This avoids changing the previous local if the array is full.
    if (current->scopeDepth > 0 &&
        current->localCount > previousLocalCount)
    {
        current->locals[current->localCount - 1].isConst = isConst;
    }

    if (match(TOKEN_EQUAL))
    {
        expression();
    }
    else
    {
        if (isConst)
        {
            error("Expect initializer for const variable.");
        }

        // Maintain the compiler's expected stack layout while
        // recovering from the error.
        emitByte(OP_NIL);
    }

    consume(TOKEN_SEMICOLON,"Expect ';' after variable declaration.");

    //If addLocal() rejected a declaration because the new limit was reached,
    //this prevents markInitialized() from modifying the previous local.
    //Challenge Question 4, Chapter 22.
    if (current->scopeDepth == 0 || current->localCount > previousLocalCount)
    {
        defineVariable(global);
    }

    /*
    uint8_t global = parseVariable("Expect variable name.");

    if (match(TOKEN_EQUAL))
    {
        expression();
    }
    else
    {
        emitByte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON,
            "Expect ';' after variable declaration.");

    defineVariable(global);
    */
}

static void expressionStatement()
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}

//If we did match the print token, then we compile the rest of the statement here:
static void printStatement()
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}

/*We skip tokens indiscriminately until we reach something that looks like a statement boundary.
 *We recognize the boundary by looking for a preceding token that can end a statement,
 *like a semicolon. Or we’ll look for a subsequent token that begins a statement,
 *usually one of the control flow or declaration keywords.
 */
static void synchronize()
{
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF)
    {
        if (parser.previous.type == TOKEN_SEMICOLON) return;
        switch (parser.current.type)
        {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;

            default:
                ; // Do nothing.
        }

        advance();
    }
}

//keep compiling declarations until we hit the end of the source file.
static void declaration()
{
    if (match(TOKEN_VAR))
    {
        //varDeclaration();
        //modified for const support, challenge question 3, chapter 22.
        varDeclaration(false);
    }
    else if (match(TOKEN_CONST))
    {
        varDeclaration(true);
    }
    else
    {
        statement();
    }

    //If we hit a compile error while parsing the previous statement,
    //we enter panic mode. When that happens, after the statement we start synchronizing.
    if (parser.panicMode)
        synchronize();
}

static void statement()
{
    if (match(TOKEN_PRINT))
    {
        printStatement();
    }
    else if (match(TOKEN_LEFT_BRACE))
    {
        beginScope();
        block();
        endScope();
    }
    else
    {
        expressionStatement();
    }
}

static void grouping(bool canAssign)
{
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

//To compile number literals
static void number(bool canAssign)
{
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}
static void string(bool canAssign)
{
    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1,parser.previous.length - 2)));
}

//modified for const  support,
//challenge question 3, Chapter 22.
static void namedVariable(Token name, bool canAssign)
{
    uint8_t getOp, setOp;

    int arg = resolveLocal(current, &name);
    bool isLocal = arg != -1;
    bool isConst = false;
    if (isLocal)
    {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;

        isConst = current->locals[arg].isConst;
    }
    else
    {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(TOKEN_EQUAL))
    {
        if (isConst)
        {
            error("Can't assign to a const variable.");
        }

        // Still parse the right-hand side for error recovery.
        expression();

        if (!isConst)
        {
            emitByte(setOp);

            if (isLocal)
            {
                emitShort((uint16_t)arg);
            }
            else
            {
                emitByte((uint8_t)arg);
            }
        }
    }
    else
    {
        emitByte(getOp);

        if (isLocal)
        {
            emitShort((uint16_t)arg);
        }
        else
        {
            emitByte((uint8_t)arg);
        }
    }

}

static void variable(bool canAssign)
{
    namedVariable(parser.previous, canAssign);
}

/* we recursively call expression() to compile the operand.
 *After that, we emit the bytecode to perform the negation.
 *It might seem a little weird to write the negate instruction after its operand’s bytecode since
 *the - appears on the left, but think about it in terms of order of execution:

We evaluate the operand first which leaves its value on the stack.

Then we pop that value, negate it, and push the result.

So the OP_NEGATE instruction should be emitted last.
This is part of the compiler’s job—parsing the program in the order
it appears in the source code and rearranging it into the order that execution happens. */
static void unary(bool canAssign)
{
    TokenType operatorType = parser.previous.type;

    // Compile the operand.
    parsePrecedence(PREC_UNARY);

    // Emit the operator instruction.
    switch (operatorType) {
        case TOKEN_BANG: emitByte(OP_NOT); break;
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        default: return; // Unreachable.
    }
}

//Support for compiling mixfix expressions, question 3, Chapter 17.
static void conditional(bool canAssign)
{
    // The condition has already been parsed, and '?' consumed.
    // Parse the then branch.
    parsePrecedence(PREC_CONDITIONAL);

    consume(TOKEN_COLON,"Expect ':' after then branch of conditional operator.");
    // Parse the else branch.
    // Parse the expression selected when the condition is falsey.
    // Use the SAME precedence to make ?: right-associative.
    parsePrecedence(PREC_ASSIGNMENT);
}

ParseRule rules[] =
{
  [TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
  [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
  [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
  [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
  [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
    [TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
    [TOKEN_BANG_EQUAL]    = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
    [TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
  [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
  [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
  [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
  [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
  [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_QUESTION] = {NULL, conditional, PREC_CONDITIONAL},
    [TOKEN_COLON]    = {NULL, NULL,        PREC_NONE},
    [TOKEN_CONST] = {NULL, NULL, PREC_NONE}, //const support, challenge question 3, chapter 22
};


/*At the beginning of parsePrecedence(), we look up a prefix parser for the current token.
 *The first token is always going to belong to some kind of prefix expression, by definition.
 *It may turn out to be nested as an operand inside one or more infix expressions,
 *but as you read the code from left to right, the first token you hit always belongs to a prefix expression.
After parsing that, which may consume more tokens, the prefix expression is done.
Now we look for an infix parser for the next token.
If we find one, it means the prefix expression we already compiled might be an operand for it.
But only if the call to parsePrecedence() has a precedence that is low enough to permit that infix operator. */
static void parsePrecedence(Precedence precedence)
{
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    while (precedence <= getRule(parser.current.type)->precedence)
    {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL))
    {
        error("Invalid assignment target.");
    }
}



static ParseRule* getRule(TokenType type) {
    return &rules[type];
}



bool compile(const char* source, Chunk* chunk)
{
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler);
    compilingChunk = chunk;
    parser.hadError = false;
    parser.panicMode = false;

    advance();
    while (!match(TOKEN_EOF))
    {
        declaration();
    }

    //This cleanup runs whether compilation succeeds or reports a syntax error.
    //Challenge Question 4, Chapter 22.
    endCompiler();

    free(compiler.locals);
    current = NULL;

    return !parser.hadError;
}