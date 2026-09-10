package com.craftinginterpreters.lox;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import static com.craftinginterpreters.lox.TokenType.*; 

class Scanner 
{
  private final String source;
  private final List<Token> tokens = new ArrayList<>();
  private int start = 0;
  private int current = 0;
  private int line = 1;

  private static final Map<String, TokenType> keywords;

  static 
  {
    keywords = new HashMap<>();
    keywords.put("and",    AND);
    keywords.put("class",  CLASS);
    keywords.put("else",   ELSE);
    keywords.put("false",  FALSE);
    keywords.put("for",    FOR);
    keywords.put("fun",    FUN);
    keywords.put("if",     IF);
    keywords.put("nil",    NIL);
    keywords.put("or",     OR);
    keywords.put("print",  PRINT);
    keywords.put("return", RETURN);
    keywords.put("super",  SUPER);
    keywords.put("this",   THIS);
    keywords.put("true",   TRUE);
    keywords.put("var",    VAR);
    keywords.put("while",  WHILE);
  }


  Scanner(String source) 
  {
    this.source = source;
  }



    //stores the raw source doe as a simple string
    //fill it with tokens we will generate.
  List<Token> scanTokens() 
  {
    while (!isAtEnd()) 
    {
      // We are at the beginning of the next lexeme.
      start = current;
      scanToken();
    }

    tokens.add(new Token(EOF, "", null, line));
    return tokens;
  }


    //checks to see if we consumed all the characters.
   private boolean isAtEnd() 
  {
    return current >= source.length();
  }




    //consumes the next character in the source file and returns it
  private char advance() 
  {
    return source.charAt(current++);
  }


    //handles  the output.
  private void addToken(TokenType type) 
  {
    addToken(type, null);
  }

    //handles output with literal values.
  private void addToken(TokenType type, Object literal) 
  {
    String text = source.substring(start, current);
    tokens.add(new Token(type, text, literal, line));
  }



    //scans  a single token at each turn in the loop.
   private void scanToken() 
   {
    char c = advance();
    switch (c) {
      case '(': addToken(LEFT_PAREN); break;
      case ')': addToken(RIGHT_PAREN); break;
      case '{': addToken(LEFT_BRACE); break;
      case '}': addToken(RIGHT_BRACE); break;
      case ',': addToken(COMMA); break;
      case '.': addToken(DOT); break;
      case '-': addToken(MINUS); break;
      case '+': addToken(PLUS); break;
      case ';': addToken(SEMICOLON); break;
      case '*': addToken(STAR); break;
       case '!':
        addToken(match('=') ? BANG_EQUAL : BANG);
        break;
      case '=':
        addToken(match('=') ? EQUAL_EQUAL : EQUAL);
        break;
      case '<':
        addToken(match('=') ? LESS_EQUAL : LESS);
        break;
      case '>':
        addToken(match('=') ? GREATER_EQUAL : GREATER);
        break;
      case '/':
        if (match('/')) 
        {
          // A comment goes until the end of the line.
          while (peek() != '\n' && !isAtEnd()) 
          {
              advance();
          }
        } 
        else if (match ('*'))
        {
          blockComment();
        }
        else 
        {
          addToken(SLASH);
        }
        break;

      case ' ':
      case '\r':
      case '\t':
        // Ignore whitespace.
        break;

      case '\n':
        line++;
        break;

      case '"': string(); break;

      //added case ? and case : for challenge question 2 chapter 6
      case '?':
        addToken(TokenType.QUESTION);
        break;
      case ':':
        addToken(TokenType.COLON);
        break;

      default:
        if (isDigit(c)){
            number();
        } 
        else if (isAlpha(c)) 
        {
          identifier();
        }
        else
        {
            Lox.error(line, "Unexpected character.");
        }
        break; 
    }
  }

  private void identifier() 
  {
    while (isAlphaNumeric(peek())) advance();

    String text = source.substring(start, current);
    TokenType type = keywords.get(text);
    if (type == null) type = IDENTIFIER;

    addToken(type);
  }

  private void number() 
  {
    while (isDigit(peek())) advance();

    // Look for a fractional part.
    if (peek() == '.' && isDigit(peekNext())) 
    {
      // Consume the "."
      advance();

      while (isDigit(peek())) advance();
    }

    addToken(NUMBER,
        Double.parseDouble(source.substring(start, current)));
  }



   private void string() 
   {
    while (peek() != '"' && !isAtEnd()) 
    {
      if (peek() == '\n') line++;
      advance();
    }

    if (isAtEnd()) 
    {
      Lox.error(line, "Unterminated string.");
      return;
    }

    // The closing ".
    advance();

    // Trim the surrounding quotes.
    String value = source.substring(start + 1, current - 1);
    addToken(STRING, value);
  }


      //conditional advance, only consume the current character if it's what we're looking for.
   private boolean match(char expected) 
   {
    if (isAtEnd()) return false;
    if (source.charAt(current) != expected) return false;

    current++;
    return true;
  }

    //a lookahead method, it looks at the current unconsumed character,
  private char peek() 
  {
    if (isAtEnd()) return '\0';
    return source.charAt(current);
  }


    //looks ahead at the sceond character of the lookahead we don't consume the . unlesss
    //we are sure there is a digit after it.
  private char peekNext() 
  {
    if (current + 1 >= source.length()) return '\0';
    return source.charAt(current + 1);
  } 


   private boolean isAlpha(char c) 
   {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
            c == '_';
  }

  private boolean isAlphaNumeric(char c) 
  {
    return isAlpha(c) || isDigit(c);
  }

    private boolean isDigit(char c) 
  {
    return c >= '0' && c <= '9';
  } 


  //blockComment(), allows for /* */ (block comments) and keeps line #'s accurate

  private void blockComment()
  {
    int depth = 0;
    depth = 1; //start at 1 since scanToken has already consumed the opening /* 

    while(depth > 0 && !isAtEnd())
    {
      if (peek() == '\n') //if newline, add line.
      {
        line++;
        advance();// read next character
      }
      else if (peek() == '/' && peekNext() == '*' ) 
      {
        advance(); //consumes '/'
        advance(); //consumes '*'
        depth++; // each nested /* increases the depth by 1
      }
       else if (peek() == '*' && peekNext() == '/' )
      {
        advance(); //consumes '*'
        advance(); //consumes '/'
        depth--; //each nested */ decreases the depth by 1
      }
      else
      {
        advance(); //read character normally
      }
    }

    if (depth > 0)
    {
          Lox.error(line, "Undetermined block comment.");
    }

  }

}
