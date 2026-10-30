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
} Local;


//Loop context provides support for continue operation, Challenge Question 2, Chapter 23.
typedef struct LoopContext
{
    struct LoopContext* enclosing;
    int continueTarget;
    int localCount;
} LoopContext;

typedef struct {
    Local locals[UINT8_COUNT];
    int localCount;
    int scopeDepth;
    LoopContext* loop;
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


static void emitLocalOperand(uint8_t instruction, int slot)
{
    emitBytes(instruction, (uint8_t)slot);
}

static void emitLoop(int loopStart)
{
    emitByte(OP_LOOP);

    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX) error("Loop body too large.");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

/*A continue skips the ordinary cleanup instructions
 *at the ends of the blocks it exits.
 *It therefore needs its own runtime cleanup for those locals.
 *Local values occupy stack slots, and normal scope exit removes them using OP_POP
 */
static void continueStatement(void)
{
    consume(TOKEN_SEMICOLON, "Expect ';' after 'continue'.");

    LoopContext* loop = current->loop;

    if (loop == NULL)
    {
        error("Can't use 'continue' outside of a loop.");
        return;
    }

    // Discard locals belonging to the body scopes we are leaving.
    // Do not change the compiler's localCount.
    for (int i = current->localCount; i > loop->localCount; i--)
    {
        emitByte(OP_POP);
    }

    emitLoop(loop->continueTarget);
}

static int emitJump(uint8_t instruction)
{
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->count - 2;
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

// Added compiler->loop = NULL;, to allow for continue operation
//Challenge Question 2, Chapter 23.
static void initCompiler(Compiler* compiler)
{
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->loop = NULL;
    current = compiler;
}

// goes back into the bytecode and replaces the operand at the given location with the calculated jump offset.
// We call patchJump() right before we emit the next instruction that we want the jump to land on,
// so it uses the current bytecode count to determine how far to jump. In the case of an if statement,
// that means right after we compile the then branch and before we compile the next statement.
static void patchJump(int offset)
{
    // -2 to adjust for the bytecode for the jump offset itself.
    int jump = currentChunk()->count - offset - 2;

    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }

    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
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

    if (current->localCount == UINT8_COUNT)
    {
        error("Too many local variables in function.");
        return;
    }

    Local* local = &current->locals[current->localCount++];
    local->name = name;
    // Keep it uninitialized until its initializer is compiled.
    local->depth = -1;

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

static void and_(bool canAssign)
{
    int endJump = emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);
    parsePrecedence(PREC_AND);

    patchJump(endJump);
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

static void varDeclaration()
{
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
}

static void expressionStatement()
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}

static void forStatement()
{
    beginScope();
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(TOKEN_SEMICOLON))
    {
        // No initializer.
    }
    else if (match(TOKEN_VAR))
    {
        varDeclaration();
    }
    else
    {
        expressionStatement();
    }

    int loopStart = currentChunk()->count;
    int exitJump = -1;

    if (!match(TOKEN_SEMICOLON))
    {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        // Jump out of the loop if the condition is false.
        exitJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP); // Condition.
    }

    if (!match(TOKEN_RIGHT_PAREN))
    {
        int bodyJump = emitJump(OP_JUMP);
        int incrementStart = currentChunk()->count;

        expression();
        emitByte(OP_POP);

        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emitLoop(loopStart);

        loopStart = incrementStart;
        patchJump(bodyJump);
    }

    //Added to provide support for continue operation
    //challenge question 2, chapter 23.
    LoopContext loop;

    // Remember the surrounding loop, if one exists.s
    loop.enclosing = current->loop;

    // A continue jumps to the increment if present;
    // otherwise, it jumps directly to the condition.
    loop.continueTarget = loopStart;

    // Record how many locals exist before entering the body.
    // This includes any variable declared in the for initializer.
    // A continue must preserve those locals and discard body locals.
    loop.localCount = current->localCount;

    // Make this the nearest enclosing loop while compiling its body.
    current->loop = &loop;

    //compile the body.
    statement();

    // Restore the surrounding loop after finishing this body's compilation.
    current->loop = loop.enclosing;

    // When the body finishes normally, jump to the increment if present,
    // or directly to the condition otherwise.
    emitLoop(loopStart);

    if (exitJump != -1)
    {
        patchJump(exitJump);
        emitByte(OP_POP); // Condition.
    }

    endScope();
}


//First we compile the condition expression, bracketed by parentheses.
//At runtime, that will leave the condition value on top of the stack.
//We’ll use that to determine whether to execute the then branch or skip it.
static void ifStatement()
{
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();

    int elseJump = emitJump(OP_JUMP);

    patchJump(thenJump);
    emitByte(OP_POP);

    if (match(TOKEN_ELSE))
        statement();

    patchJump(elseJump);
}
//If we did match the print token, then we compile the rest of the statement here:
static void printStatement()
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}


/*Most of this mirrors if statements—we compile the condition expression,
 *surrounded by mandatory parentheses.
 *That’s followed by a jump instruction that skips over the subsequent body statement
 *if the condition is falsey. We patch the jump after
 *compiling the body and take care to pop the condition
value from the stack on either path.  */

/* Replaced for bounded-while loop suppport, challenge question 3, chapter 23.
static void whileStatement()
{
    int loopStart = currentChunk()->count;
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);

    //Added to provide support for continue operation,
    //The context is active only while compiling the body.
    //Its saved local count identifies
    //the variables that existed before entering that body.
    //Challenge question 2, Chapter 23.
    LoopContext loop;

    // Remember the surrounding loop, if one exists.
    loop.enclosing = current->loop;

    // A continue jumps to the increment if present;
    // otherwise, it jumps directly to the condition.
    loop.continueTarget = loopStart;

    // Record how many locals exist before entering the body.
    // This includes any variable declared in the for initializer.
    // A continue must preserve those locals and discard body locals.
    loop.localCount = current->localCount;

    // Make this the nearest enclosing loop while compiling its body.
    current->loop = &loop;

    statement();

    // Restore the surrounding loop after finishing this body's compilation.
    current->loop = loop.enclosing;

    // When the body finishes normally, jump to the increment if present,
    // or directly to the condition otherwise.
    emitLoop(loopStart);

    patchJump(exitJump);
    emitByte(OP_POP);
} */


//MODIFIED whileStatement
//Supports both
//while (condition) statement
//while (condition) limit (maximum) {
// ...
//} exhausted {
    // ...
//}
//In short this provides bounded while support, challenge question 3, chapter 23.
static void whileStatement(void)
{
    // The budget appears after the condition in source, but must
    // execute before the first condition check.
    int initializationJump = emitJump(OP_JUMP);

    int conditionStart = currentChunk()->count;

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int conditionExit = emitJump(OP_JUMP_IF_FALSE);

    // True-condition path: discard the condition.
    emitByte(OP_POP);

    // Skip the initialization code on each successful condition check.
    int bodyJump = emitJump(OP_JUMP);

    // First entry into the loop lands here.
    patchJump(initializationJump);

    if (!match(TOKEN_LIMIT))
    {
        // Ordinary while: no initialization is necessary.
        emitLoop(conditionStart);

        patchJump(bodyJump);

        LoopContext loop;
        loop.enclosing = current->loop;
        loop.continueTarget = conditionStart;
        loop.localCount = current->localCount;

        current->loop = &loop;
        statement();
        current->loop = loop.enclosing;

        emitLoop(conditionStart);

        patchJump(conditionExit);
        emitByte(OP_POP);

        return;
    }

    // This private scope owns the hidden iteration budget.
    beginScope();

    int budgetSlot = current->localCount;

    Token hiddenName;
    hiddenName.type = TOKEN_IDENTIFIER;
    hiddenName.start = "";
    hiddenName.length = 0;
    hiddenName.line = parser.previous.line;

    // An empty identifier cannot be written by a Lox programmer.
    addLocal(hiddenName);

    // addLocal() reports an error if local storage is exhausted.
    if (current->localCount != budgetSlot + 1)
    {
        endScope();
        return;
    }

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'limit'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after loop limit.");

    emitByte(OP_CHECK_LIMIT);

    // The validated value becomes the hidden local's initial value.
    markInitialized();

    // Initialization runs once, then execution checks the condition.
    emitLoop(conditionStart);

    // A true condition lands here to inspect the remaining budget.
    patchJump(bodyJump);

    emitLocalOperand(OP_GET_LOCAL, budgetSlot);
    emitConstant(NUMBER_VAL(0));
    emitByte(OP_GREATER);

    int exhaustedJump = emitJump(OP_JUMP_IF_FALSE);

    // Discard the successful budget comparison.
    emitByte(OP_POP);

    // Consume an iteration before entering the body.
    emitLocalOperand(OP_GET_LOCAL, budgetSlot);
    emitConstant(NUMBER_VAL(1));
    emitByte(OP_SUBTRACT);
    emitLocalOperand(OP_SET_LOCAL, budgetSlot);

    // Assignment leaves its result on the stack; discard that copy.
    emitByte(OP_POP);

    LoopContext loop;
    loop.enclosing = current->loop;
    loop.continueTarget = conditionStart;

    // Preserve the budget across continue, but discard body locals.
    loop.localCount = current->localCount;

    current->loop = &loop;

    consume(TOKEN_LEFT_BRACE, "Expect '{' before bounded loop body.");
    beginScope();
    block();
    endScope();

    current->loop = loop.enclosing;

    // Both normal completion and continue return to the condition.
    emitLoop(conditionStart);

    // Budget exhausted: remove the failed comparison result.
    patchJump(exhaustedJump);
    emitByte(OP_POP);

    if (match(TOKEN_EXHAUSTED))
    {
        consume(
            TOKEN_LEFT_BRACE,
            "Expect '{' after 'exhausted'."
        );

        beginScope();
        block();
        endScope();
    }

    // Skip the condition-false cleanup when coming from exhaustion.
    int finishedJump = emitJump(OP_JUMP);

    // Normal termination: discard the false condition.
    patchJump(conditionExit);
    emitByte(OP_POP);

    patchJump(finishedJump);

    // Both termination paths discard the hidden budget.
    endScope();
}
/*We skip tokens indiscriminately until we reach something that looks like a statement boundary.
 *We recognize the boundary by looking for a preceding token that can end a statement,
 *like a semicolon. Or we’ll look for a subsequent token that begins a statement,
 *usually one of the control flow or declaration keywords.
 *Addec TOKEN_CONTINUE for continue support, challenge Question 2, chapter 23.
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
            case TOKEN_CONTINUE:
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
        varDeclaration();
    } else
    {
        statement();
    }

    //If we hit a compile error while parsing the previous statement,
    //we enter panic mode. When that happens, after the statement we start synchronizing.
    if (parser.panicMode)
        synchronize();
}

//Added switch statement support, challenge question 1, chapter 23.
static void switchStatement(void)
{
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'switch'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after switch value.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before switch cases.");

    // Each matching case needs a jump to the end of the switch.
    int* endJumps = NULL;
    int endCount = 0;
    int endCapacity = 0;

    bool hasDefault = false;

    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
    {
        if (match(TOKEN_CASE))
        {
            // Before:
            //   [..., switchValue]
            //
            // After comparison:
            //   [..., switchValue, comparisonResult]
            emitByte(OP_DUP);
            expression();
            consume(TOKEN_COLON, "Expect ':' after case value.");
            emitByte(OP_EQUAL);

            int nextCaseJump = emitJump(OP_JUMP_IF_FALSE);

            // Matching path: discard both the comparison result
            // and the switch value before executing statements.
            emitByte(OP_POP);
            emitByte(OP_POP);

            while (!check(TOKEN_CASE) &&!check(TOKEN_DEFAULT) &&!check(TOKEN_RIGHT_BRACE) &&!check(TOKEN_EOF))
            {
                statement();

                if (parser.panicMode)
                {
                    break;
                }
            }

            if (endCount == endCapacity)
            {
                int capacity = endCapacity < 8
                    ? 8
                    : endCapacity * 2;

                int* jumps = realloc(
                    endJumps,
                    sizeof(int) * (size_t)capacity
                );

                if (jumps == NULL)
                {
                    free(endJumps);
                    fprintf(stderr, "Could not allocate switch jumps.\n");
                    exit(EXIT_FAILURE);
                }

                endJumps = jumps;
                endCapacity = capacity;
            }

            // No fallthrough: a matched case skips all later clauses.
            endJumps[endCount++] = emitJump(OP_JUMP);

            // Nonmatching path: discard only the comparison result.
            // Leave the switch value for the next case.
            patchJump(nextCaseJump);
            emitByte(OP_POP);

            if (parser.panicMode)
            {
                break;
            }
        }
        else if (match(TOKEN_DEFAULT))
        {
            hasDefault = true;

            consume(TOKEN_COLON, "Expect ':' after 'default'.");

            // No more comparisons are needed.
            emitByte(OP_POP);

            while (!check(TOKEN_CASE) &&
                   !check(TOKEN_DEFAULT) &&
                   !check(TOKEN_RIGHT_BRACE) &&
                   !check(TOKEN_EOF))
            {
                statement();

                if (parser.panicMode)
                {
                    break;
                }
            }

            if (check(TOKEN_CASE) || check(TOKEN_DEFAULT))
            {
                errorAtCurrent(
                    "Default must be the last switch clause."
                );
            }

            break;
        }
        else
        {
            errorAtCurrent("Expect 'case' or 'default' in switch.");
            break;
        }
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after switch.");

    if (!hasDefault)
    {
        // Reached only when no case matched, including an empty switch.
        emitByte(OP_POP);
    }

    // Matched cases land after the unmatched-path cleanup.
    for (int i = 0; i < endCount; i++)
    {
        patchJump(endJumps[i]);
    }

    free(endJumps);
}

static void statement()
{
    if (match(TOKEN_PRINT))
    {
        printStatement();
    }
    else if (match(TOKEN_FOR))
    {
        forStatement();
    }
    else if (match(TOKEN_IF))
    {
        ifStatement();
    }
    else if (match(TOKEN_WHILE))
    {
        whileStatement();
    }
    else if (match(TOKEN_LEFT_BRACE))
    {
        beginScope();
        block();
        endScope();
    }
    else if (match(TOKEN_SWITCH))
    {
        switchStatement();
    }
    else if (match(TOKEN_CONTINUE))
    {
    continueStatement();
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


static void or_(bool canAssign)
{
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);

    patchJump(elseJump);
    emitByte(OP_POP);

    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

static void string(bool canAssign)
{
    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1,parser.previous.length - 2)));
}

static void namedVariable(Token name, bool canAssign)
{
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);
    if (arg != -1)
    {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    }
    else
    {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(TOKEN_EQUAL))
    {
        expression();
        emitBytes(setOp, (uint8_t)arg);
    }
    else
    {
        emitBytes(getOp, (uint8_t)arg);
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

//[TOKEN_SWITCH],[TOKEN_CASE],[TOKEN_DEFAULT]
//Added for switch support, challenge question 1, chapter 23.
//[TOKEN_CONTINUE], added for continue support, challenge question 2, chapter 23.
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
    [TOKEN_AND]           = {NULL,     and_,   PREC_AND},
  [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
  [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
    [TOKEN_OR]            = {NULL,     or_,    PREC_OR},
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
    [TOKEN_SWITCH]  = {NULL, NULL, PREC_NONE},
    [TOKEN_CASE]    = {NULL, NULL, PREC_NONE},
    [TOKEN_DEFAULT] = {NULL, NULL, PREC_NONE},
    [TOKEN_CONTINUE] = {NULL, NULL, PREC_NONE},
    [TOKEN_LIMIT]     = {NULL, NULL, PREC_NONE},
    [TOKEN_EXHAUSTED] = {NULL, NULL, PREC_NONE},
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

    endCompiler();
    return !parser.hadError;
}