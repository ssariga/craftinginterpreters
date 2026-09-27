package com.craftinginterpreters.lox;

import java.util.List;

//Modified this file to provide anonymous function support.
//Chapter 10, challenge question 2.
class LoxFunction implements LoxCallable 
{
  private final List<Token> params;
  private final List<Stmt> body;
  private final Environment closure;
  private final String name;

  // Named declaration:
  // fun name(parameters) { body }
  LoxFunction(Stmt.Function declaration, Environment closure) 
  {
    this.params = declaration.params;
    this.body = declaration.body;
    this.closure = closure;
    this.name = declaration.name.lexeme;
  }

  // Anonymous expression:
  // fun (parameters) { body }
  LoxFunction(Expr.Function expression, Environment closure) 
  {
    this.params = expression.params;
    this.body = expression.body;
    this.closure = closure;
    this.name = null;
  }

  @Override
  public int arity() {
    return params.size();
  }

  // Overrides the call() method required by LoxCallable.
 // interpreter is the currently active interpreter.
// arguments contains the already-evaluated Lox argument values.
  @Override
  public Object call(Interpreter interpreter, List<Object> arguments) 
  {
    Environment environment = new Environment(closure);

    // Assigns each supplied argument value to its matching parameter name.
    for (int i = 0; i < params.size(); i++) 
    {
      Token parameter = params.get(i);
      int slot = interpreter.getParameterSlot(parameter);
      environment.defineAt(slot, arguments.get(i));
    }

    // Runs the statements in the function body using the newly created
    // function-call environment.
    try 
    {
      interpreter.executeBlock(body, environment);
    } 
    catch (Return returnValue) 
    {
      return returnValue.value;
    }

    return null;
  }

  @Override
  public String toString() 
  {
    if (name == null) return "<fn>";
    return "<fn " + name + ">";
  }

}