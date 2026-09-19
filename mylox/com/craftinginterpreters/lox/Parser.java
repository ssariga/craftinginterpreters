package com.craftinginterpreters.lox;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import static com.craftinginterpreters.lox.TokenType.*;

class Parser 
{

  private static class ParseError extends RuntimeException {} //unwinds the parser, returns error instead of throwing.
  private final List<Token> tokens;
  private int current = 0;
  private int loopDepth = 0; //tracks how many loops the parser is currently inside of, Challenge Question 3, Chapter 9.



  Parser(List<Token> tokens) 
  {
    this.tokens = tokens;
  }

    //starts the parse method.
 List<Stmt> parse() 
 {
    List<Stmt> statements = new ArrayList<>();
    while (!isAtEnd()) 
    {
      statements.add(declaration());
    }

    return statements; 
  }

//exapnds to the equality rule.
 private Expr expression() 
 {
    return assignment();
 }


  private Stmt declaration() 
  {
    try 
    {
      if (match(VAR)) return varDeclaration();

      return statement();
    } 
    catch (ParseError error) 
    {
      synchronize();
      return null;
    }
  }

 //parses a statement
  private Stmt statement() 
  {
    if (match(BREAK)) return breakStatement(); //calls breakStatement method, Challenge Question 3, Chapter 9.
    if (match(FOR)) return forStatement();
    if (match(IF)) return ifStatement();
    if (match(PRINT)) return printStatement();
    if (match(WHILE)) return whileStatement();
    if (match(LEFT_BRACE)) return new Stmt.Block(block());

    return expressionStatement();
  }


  private Stmt breakStatement() 
  {
  Token keyword = previous();

  if (loopDepth == 0) {
    error(keyword, "Cannot use 'break' outside of a loop.");
  }

  consume(SEMICOLON, "Expect ';' after 'break'.");
  return new Stmt.Break(keyword);
}




   private Stmt forStatement() 
   {
    consume(LEFT_PAREN, "Expect '(' after 'for'.");

    Stmt initializer;

      /*If the token following the ( is a semicolon then the initializer has been omitted. 
      Otherwise, we check for a var keyword to see if it’s a variable declaration. 
      If neither of those matched, it must be an expression. 
      We parse that and wrap it in an expression statement so that the initializer is always of type Stmt. */
    if (match(SEMICOLON)) 
    {
      initializer = null;
    } 
    else if (match(VAR)) 
    {
      initializer = varDeclaration();
    } 
    else 
    {
      initializer = expressionStatement();
    }

    Expr condition = null;
    //look for a semicolon to see if the clause has been omitted. The last clause is the increment.
    if (!check(SEMICOLON)) 
    {
      condition = expression();
    }
    consume(SEMICOLON, "Expect ';' after loop condition.");


    Expr increment = null;
    if (!check(RIGHT_PAREN)) 
    {
      increment = expression();
    }
    consume(RIGHT_PAREN, "Expect ')' after for clauses.");

    Stmt body = statement();

    //desugaring
    //We take those and use them to synthesize syntax tree nodes that express the semantics of the for loop
      if (increment != null) 
      {
      body = new Stmt.Block( Arrays.asList( body, new Stmt.Expression(increment)));
      }


      //evaluate the increment.
    if (condition == null) condition = new Expr.Literal(true);
    body = new Stmt.While(condition, body);

    //take the condition and the body and build the loop using a primitive while loop. 
    // If the condition is omitted, we set true to make an infinite loop.
    if (initializer != null) 
    {
      body = new Stmt.Block(Arrays.asList(initializer, body));
    }
    return body;
  }

   private Stmt ifStatement() 
   {
    consume(LEFT_PAREN, "Expect '(' after 'if'.");
    Expr condition = expression();
    consume(RIGHT_PAREN, "Expect ')' after if condition."); 

    Stmt thenBranch = statement();
    Stmt elseBranch = null;
    if (match(ELSE)) {
      elseBranch = statement();
    }

    return new Stmt.If(condition, thenBranch, elseBranch);
  }

  //parse the subsequent expression, consume the terminating semicolon, and emit the syntax tree.
   private Stmt printStatement() 
   {
    Expr value = expression();
    consume(SEMICOLON, "Expect ';' after value.");
    return new Stmt.Print(value);
  }

  private Stmt varDeclaration() 
  {
    Token name = consume(IDENTIFIER, "Expect variable name.");

    Expr initializer = null;
    if (match(EQUAL)) 
      {
      initializer = expression();
    }

    consume(SEMICOLON, "Expect ';' after variable declaration.");
    return new Stmt.Var(name, initializer);
  }

  private Stmt whileStatement() 
  {
    consume(LEFT_PAREN, "Expect '(' after 'while'.");
    Expr condition = expression();
    consume(RIGHT_PAREN, "Expect ')' after condition.");
    
    /* Break willl remain valid inside nested blocks, and if statements.
      Parsing occurs while loopDepth > 0. Increment while parsing loop body
      decrement afterwards, reject break when loopdepth == 0 */
    loopDepth++;
    Stmt body = statement();
    loopDepth--;

    return new Stmt.While(condition, body);
  }

  //wrap the Expr into the right type.
  private Stmt expressionStatement() 
  {
    Expr expr = expression();
    consume(SEMICOLON, "Expect ';' after expression.");
    return new Stmt.Expression(expr);
  }

  private List<Stmt> block() 
  {
    List<Stmt> statements = new ArrayList<>();

    while (!check(RIGHT_BRACE) && !isAtEnd()) 
    {
      statements.add(declaration());
    }

    consume(RIGHT_BRACE, "Expect '}' after block.");
    return statements;
  }

   private Expr assignment() 
   {
    Expr expr = or();

    if (match(EQUAL)) {
      Token equals = previous();
      Expr value = assignment();

      if (expr instanceof Expr.Variable) {
        Token name = ((Expr.Variable)expr).name;
        return new Expr.Assign(name, value);
      }

      error(equals, "Invalid assignment target."); 
    }

    return expr;
  }

   private Expr or() 
   {
    Expr expr = and();

    while (match(OR)) 
    {
      Token operator = previous();
      Expr right = and();
      expr = new Expr.Logical(expr, operator, right);
    }

    return expr;
  }

  private Expr and() 
  {
    Expr expr = equality();

    while (match(AND)) {
      Token operator = previous();
      Expr right = equality();
      expr = new Expr.Logical(expr, operator, right);
    }

    return expr;
  }


/* Handles comma implemnetation Challenge Question 1, Chapter 6 */
private Expr comma ()
{

  //Expr expr = equality(); //replaced with conditional() for challenge question 2, Chapter 6
  Expr expr = conditional();
  while (match(TokenType.COMMA)) 
  {
    Token operator = previous();
    Expr right = equality();
    expr = new Expr.Binary(expr, operator, right);
  }

  return expr;
}

private Expr conditional()
{
  Expr expr = equality();

  if(match(TokenType.QUESTION))
  {
    Expr thenBranch = expression();

    consume(TokenType.COLON, "Expect ':' after the branch of conditionnal expression");
    
    Expr elseBranch = conditional();
    expr = new Expr.Conditional(expr, thenBranch, elseBranch);
  
  }

  return expr;
}
private Expr equality() 
 {
    Expr expr = comparison(); //translate the first call and store in a local variable

    //ends if we do not see a != or a == token 
    while (match(BANG_EQUAL, EQUAL_EQUAL)) 
    {
      Token operator = previous();
      Expr right = comparison();
      expr = new Expr.Binary(expr, operator, right);
    }

    return expr;
  }

    //we must first find either a != or == token. So, 
    // if we don’t see one of those, 
    // we must be done with the sequence of equality operators.
   private boolean match(TokenType... types) 
   {
    for (TokenType type : types) {
      if (check(type)) {
        advance();
        return true;
      }
    }

    return false;
  }

    //looks for the ')' 
   private Token consume(TokenType type, String message) 
   {
    if (check(type)) return advance();

    throw error(peek(), message);
  }

    //identical to equality(), only difference is the token
    //types for the operators we match in this case >, >=. <. <= 
  private Expr comparison() 
  {
    Expr expr = term();

    while (match(GREATER, GREATER_EQUAL, LESS, LESS_EQUAL)) 
    {
      Token operator = previous();
      Expr right = term();
      expr = new Expr.Binary(expr, operator, right);
    }

    return expr;
  }

   //identical to equality(), only difference is the token
  //types for the operators we match in this case + or - 
    private Expr term() 
    {
    Expr expr = factor();

    while (match(MINUS, PLUS)) {
      Token operator = previous();
      Expr right = factor();
      expr = new Expr.Binary(expr, operator, right);
    }

    return expr;
  }

//identical to equality(), only difference is the token
  //types for the operators we match in this case * or / 
  private Expr factor() 
  {
    Expr expr = unary();

    while (match(SLASH, STAR)) {
      Token operator = previous();
      Expr right = unary();
      expr = new Expr.Binary(expr, operator, right);
    }

    return expr;
  }

    // look at the current token to see how to parse. 
    // If it’s a ! or -, we must have a unary expression. 
    // In that case, we grab the token and then 
    // recursively call unary() again to parse the operand. 
  private Expr unary() 
  {
    if (match(BANG, MINUS)) {
      Token operator = previous();
      Expr right = unary();
      return new Expr.Unary(operator, right);
    }

    return primary();
  }

    //creation of unary expression tree
    //primary  → NUMBER | STRING | "true" | "false" | "nil" | "(" expression ")" ;

   private Expr primary() 
   {

     //error check for Challenge Question 3, Chapter 6
    //handles == and !=
    if(match(TokenType.EQUAL_EQUAL, TokenType.BANG_EQUAL))
    {
      Token operator = previous();
      equality();
      throw error(operator, "Missing left-hand operand.");
    }

    //error check for Challenge Question 3, Chapter 6
    //handles >, >=, <, <=
    if (match(TokenType.GREATER, TokenType.GREATER_EQUAL, TokenType.LESS, TokenType.LESS_EQUAL))
    {
      Token operator = previous();
      comparison();
      throw error(operator, "Missing left hand operand.");
    }

     //error check for Challenge Question 3, Chapter 6
    //handles + and -
    if(match(TokenType.PLUS, TokenType.MINUS))
    {
      Token operator = previous();
      term();
      throw error(operator, "Missing left hand operand.");
    }

     //error check for Challenge Question 3, Chapter 6
    //handles * and /
    if(match(TokenType.STAR,TokenType.SLASH))
    {
      Token operator = previous();
      factor();
      throw error (operator, "Missing left hand operand");
    }

     //error check for Challenge Question 3, Chapter 6
    //handles ,
     if (match(TokenType.COMMA)) 
    {
      Token operator = previous();
      conditional();
      throw error(operator, "Missing left-hand operand.");
    }

    if (match(FALSE)) return new Expr.Literal(false);
    if (match(TRUE)) return new Expr.Literal(true);
    if (match(NIL)) return new Expr.Literal(null);

    if (match(NUMBER, STRING)) 
    {
      return new Expr.Literal(previous().literal);
    }


    if (match(IDENTIFIER)) 
    {
      return new Expr.Variable(previous());
    }

    if (match(LEFT_PAREN)) 
    {
      Expr expr = expression();
      consume(RIGHT_PAREN, "Expect ')' after expression.");
      return new Expr.Grouping(expr);
    }

    //as parser descends through the parsing methods, it will eventually hit primary()
    //if none of the cases match there, we have a token that cannot start an expression
    //this will handle that error case.
    throw error(peek(), "Expect expression."); 
  }

    //returns true is a token is of the given type
    //false otherwise
    //only looks at a token, no consumption.
    private boolean check(TokenType type) 
    {
        if (isAtEnd()) return false;
        return peek().type == type;
    }

 

    //consumes the current token and returns it
     private Token advance() 
     {
        if (!isAtEnd()) current++;
        return previous();
     }


    // checks if we’ve run out of tokens to parse
  private boolean isAtEnd() 
  {
    return peek().type == EOF;
  }

    //returns the current token we have yet to consume
  private Token peek() 
  {
    return tokens.get(current);
  }

  //Returns the most recently consumed token.  Makes it easier to use match()
  private Token previous() 
  {
    return tokens.get(current - 1);
  }

    //handles parser errors
   private ParseError error(Token token, String message) 
  {
    Lox.error(token, message);
    return new ParseError();
  }


    //discard tokens until we’re right at the beginning of the next statement. 
    // That boundary is ';'
   private void synchronize() 
   {
    advance();

    while (!isAtEnd()) 
    {
      if (previous().type == SEMICOLON) return;

      switch (peek().type) {
        case CLASS:
        case FUN:
        case VAR:
        case FOR:
        case IF:
        case WHILE:
        case PRINT:
        case RETURN:
          return;
      }

      advance();
    }
  }


  /*Challenge Question 1, Chapter 8 */

  Object parseRepl()
  {
    try
    {
      //statements that being with these tokens MUST use regular parsing.
      if(check(PRINT) || check(VAR) || check (LEFT_BRACE))
      {
        return declaration();
      }

      //parses a bare expression first
      Expr expression = expression();

      //if user entered a semicolon (;) it is an expression statement.
      if(match(SEMICOLON))
      {
        return new Stmt.Expression(expression);
      }

      // No semicolon means this is a REPL expression, return its vvalue producing AST.
      if(isAtEnd())
      {
        return expression;
      }

      
      throw error(peek(), "Expect ';' after expression. ");
    }
    catch(ParseError error)
    {
      synchronize();
      return null;
    }
    
  }

  
}