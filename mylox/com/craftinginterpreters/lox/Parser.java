package com.craftinginterpreters.lox;

import java.util.List;

import static com.craftinginterpreters.lox.TokenType.*;

class Parser 
{

  private static class ParseError extends RuntimeException {} //unwinds the parser, returns error instead of throwing.
  private final List<Token> tokens;
  private int current = 0;



  Parser(List<Token> tokens) 
  {
    this.tokens = tokens;
  }

    //starts the parse method.
    Expr parse() 
    {
    try {
      return expression();
    } catch (ParseError error) {
      return null;
    }
  }

//exapnds to the equality rule.
 private Expr expression() 
 {
    return comma();
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

  
}