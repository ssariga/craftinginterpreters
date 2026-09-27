package com.craftinginterpreters.lox;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;

public class Lox 
{
    private static final Interpreter interpreter = new Interpreter();
    static boolean hadError = false; //variable to check to see if the code has an error
    static boolean hadRuntimeError = false;

  public static void main(String[] args) throws IOException 
  {
    if (args.length > 1) 
    {
      System.out.println("Usage: jlox [script]");
      System.exit(64); 
    } 
    else if (args.length == 1) 
    {
      runFile(args[0]);
    } 
    else 
    {
      runPrompt();
    }


  }



private static void runFile(String path) throws IOException 
{
    byte[] bytes = Files.readAllBytes(Paths.get(path));
    //run(new String(bytes, Charset.defaultCharset()));

    //Challenge question 1 chapter 8, this one accepts a flag distinguish
    //a file from a REPL input.
    run(new String(bytes, Charset.defaultCharset()), false);

    //indicate an error in the exit code
    if(hadError)
    {
      System.exit(65);
    }
    if (hadRuntimeError)
      {
        System.exit(70);
      } 
  }



  private static void runPrompt() throws IOException 
 {
    InputStreamReader input = new InputStreamReader(System.in);
    BufferedReader reader = new BufferedReader(input);

    for (;;) 
    { 
      System.out.print("> ");
      String line = reader.readLine();
      if (line == null) break;
      //run(line);
      
      //challenge question 1 Chapter 8
      run(line, true);
      hadRuntimeError = false; //reset so that one bad REPL does not make later inputs look like they failed.
      hadError = false;
    }
  }


  /*Modified for challenge Question 1 Chapter 8, can now run REPL inputs. */
  private static void run(String source, boolean isRepl) 
  {
     
    Scanner scanner = new Scanner(source);
    List<Token> tokens = scanner.scanTokens();
    Parser parser = new Parser(tokens);

    //if REPL input is detected
 if (isRepl) 
  {
    //asign a parser object to input
    Object input = parser.parseRepl();

    //if there is an error or input is null,
    //return to main.
    if (hadError || input == null)
      {
        return;
      } 

      //if input is an instace of Expression (a REPL)
      //interpet the REPL
      //otherwise interpret as usual.
    if (input instanceof Expr) 
    {
      interpreter.interpretRepl((Expr) input);
    } 
    else 
    {
      interpreter.interpret((Stmt) input);
    }

    return;
  }

    List<Stmt> statements = parser.parse();

    // Stop if there was a syntax error.
    if (hadError) return;

    //run the resolver.
    Resolver resolver = new Resolver(interpreter);
    resolver.resolve(statements);

     if (hadError) 
      return;
    
    interpreter.interpret(statements);
  }

  
  static void error(int line, String message) 
  {
    report(line, "", message);
  }

   static void runtimeError(RuntimeError error) 
  {
    System.err.println(error.getMessage() + "\n[line " + error.token.line + "]");
    hadRuntimeError = true;
  }

  private static void report(int line, String where, String message) 
  {
    System.err.println ("[line " + line + "] Error" + where + ": " + message);
    hadError = true;
  }

  static void error(Token token, String message)
  {
    if (token.type == TokenType.EOF) {
      report(token.line, " at end", message);
    } else {
      report(token.line, " at '" + token.lexeme + "'", message);
    }
  }


}

 



