package com.craftinginterpreters.lox;

import java.util.List;

// Supports named functions, anonymous functions, bound methods,
// and class initializers.
class LoxFunction implements LoxCallable 
{
  private final Stmt.Function declaration;
  private final List<Token> params;
  private final List<Stmt> body;
  private final Environment closure;
  private final String name;
  private final boolean isInitializer;
  private final boolean isGetter; //added getter function support, challenge question 2, chapter 12.

  // Named function or class method.
  LoxFunction(
      Stmt.Function declaration,
      Environment closure,
      boolean isInitializer) 
  {
    this.declaration = declaration;
    this.params = declaration.params;
    this.body = declaration.body;
    this.closure = closure;
    this.name = declaration.name.lexeme;
    this.isInitializer = isInitializer;
    this.isGetter = declaration.isGetter; //added getter function support, challenge question 2, chapter 12.
  }

  // Convenience constructor for ordinary named functions.
  LoxFunction(Stmt.Function declaration, Environment closure) 
  {
    this(declaration, closure, false);
  }

  // Anonymous function expression:
  // fun (parameters) { body }
  LoxFunction(Expr.Function expression, Environment closure) 
  {
    this.declaration = null;
    this.params = expression.params;
    this.body = expression.body;
    this.closure = closure;
    this.name = null;
    this.isInitializer = false;
    this.isGetter = false;   //added getter function support, challenge question 2, chapter 12.
    
  }

  // Binds a class method to the instance used to access it.
  LoxFunction bind(LoxInstance instance) 
  {
    Environment environment = new Environment(closure);
    environment.defineAt(0, instance);

    return new LoxFunction(declaration, environment, isInitializer);
  }

  @Override
  public int arity() 
  {
    return params.size();
  }

  @Override
  public Object call(Interpreter interpreter, List<Object> arguments) 
  {
    Environment environment = new Environment(closure);

    for (int i = 0; i < params.size(); i++) 
    {
      Token parameter = params.get(i);
      int slot = interpreter.getParameterSlot(parameter);

      environment.defineAt(slot, arguments.get(i));
    }

    try 
    {
      interpreter.executeBlock(body, environment);
    } 
    catch (Return returnValue) 
    {
      if (isInitializer) 
      {
        return closure.getAt(0, "this");
      }

      return returnValue.value;
    }

    if (isInitializer) 
    {
      return closure.getAt(0, "this");
    }

    return null;
  }


  //added getter support, challenge question 2, chapter 12.
boolean isGetter() 
{
  return isGetter;
}
  @Override
  public String toString() 
  {
    if (name == null) return "<fn>";
    return "<fn " + name + ">";
  }
}