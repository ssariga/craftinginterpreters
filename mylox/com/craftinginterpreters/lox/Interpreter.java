package com.craftinginterpreters.lox;

import java.util.List;

import com.craftinginterpreters.lox.Lox;
import com.craftinginterpreters.lox.Stmt.Break;

class Interpreter implements Expr.Visitor<Object>, Stmt.Visitor<Void>  
{

   private Environment environment = new Environment();

   //challenge Queston 2, Chapter 8
   //creation of new unitialized object
   private static Object uninitialized = new Object();

  //converts a tree node to a runtime value
  @Override 
  public Object visitLiteralExpr(Expr.Literal expr) 
  {
    return expr.value;
  }

   @Override
  public Object visitLogicalExpr(Expr.Logical expr) 
  {
    Object left = evaluate(expr.left);

    if (expr.operator.type == TokenType.OR) 
    {
      if (isTruthy(left)) return left;
    } 
    else
    {
      if (!isTruthy(left)) return left;
    }

    return evaluate(expr.right);
  }


  //evalutes grouping aka ( )
   @Override
  public Object visitGroupingExpr(Expr.Grouping expr) 
  {
    return evaluate(expr.expression);
  }

  //allows the interpreter to visit expression statements
   @Override 
  public Void visitExpressionStmt(Stmt.Expression stmt) 
  {
    evaluate(stmt.expression);
    return null;
  }


  //If truthy, it executes the then branch. 
  //Otherwise, if there is an else branch, it executes that.
   @Override
  public Void visitIfStmt(Stmt.If stmt) 
  {
    if (isTruthy(evaluate(stmt.condition))) 
    {
      execute(stmt.thenBranch);
    } 
    else if (stmt.elseBranch != null) 
    {
      execute(stmt.elseBranch);
    }
    return null;
  }


  //allows the interpreter to visit print statements
  @Override
  public Void visitPrintStmt(Stmt.Print stmt) 
  {
    Object value = evaluate(stmt.expression);
    System.out.println(stringify(value));
    return null;
  }

  @Override
  public Void visitVarStmt(Stmt.Var stmt) 
  {
    //Object value = null;
    //modificaiton for Challenge Question 2, Chapter 8
    //We no longer initialize to null.

    Object value = uninitialized;

    if (stmt.initializer != null) 
    {
      value = evaluate(stmt.initializer);
    }

    environment.define(stmt.name.lexeme, value);
    return null;
  }

  //execute the while sytanx
   @Override
  public Void visitWhileStmt(Stmt.While stmt) 
  {
    while (isTruthy(evaluate(stmt.condition))) 
    {
      //execute(stmt.body);execute(stmt.body);

      try
      {
        execute(stmt.body);
      }
        catch (BreakSignal signal)
      {
        break;
      } 
    }
   
    return null;
  }

   @Override
  public Object visitAssignExpr(Expr.Assign expr) 
  {
    Object value = evaluate(expr.value);
    environment.assign(expr.name, value);
    return value;
  }

  //helps handle left hand sides of assignment expressions.
   private Object evaluate(Expr expr) 
  {
    return expr.accept(this);
  }


  private void execute(Stmt stmt) 
  {
    stmt.accept(this);
  }

    void executeBlock(List<Stmt> statements,
                    Environment environment) {
    Environment previous = this.environment;
    try {
      this.environment = environment;

      for (Stmt statement : statements) {
        execute(statement);
      }
    } finally {
      this.environment = previous;
    }
  }

  @Override
  public Void visitBlockStmt(Stmt.Block stmt) 
  {
    executeBlock(stmt.statements, new Environment(environment));
    return null;
  }

  //calls the Break method, Challenge Question 3, Chapter 9.
@Override
public Void visitBreakStmt(Stmt.Break stmt) 
{
  throw new BreakSignal();
}



  //evaluates the binary expressions
  // +, -, /, !=, ==, <=. >=. <, >, and *
   @Override
  public Object visitBinaryExpr(Expr.Binary expr) 
  {
    Object left = evaluate(expr.left);
    Object right = evaluate(expr.right); 

    switch (expr.operator.type) 
    {
      case GREATER:
        checkNumberOperands(expr.operator, left, right);
        return (double)left > (double)right;
      case GREATER_EQUAL:
        checkNumberOperands(expr.operator, left, right);
        return (double)left >= (double)right;
      case LESS:
        checkNumberOperands(expr.operator, left, right);
        return (double)left < (double)right;
      case LESS_EQUAL:
        checkNumberOperands(expr.operator, left, right);
        return (double)left <= (double)right;
      case MINUS:
        checkNumberOperand(expr.operator, right);
        return (double)left - (double)right;
      case PLUS: //modified for challenge question 2, chapter 7
        if (left instanceof Double && right instanceof Double) 
        {
          return (double)left + (double)right;
        } 
        if (left instanceof String || right instanceof String) 
        {
          return stringify(left) + stringify(right);
        }
        throw new RuntimeError(expr.operator,"Operands must be two numbers or two strings.");
      
        //modification to case SLASH for challenge question 3, chapter 7.
        case SLASH:
        checkNumberOperands(expr.operator, left, right);
         if ((double) right == 0.0) // if divisor is 0, throw an error.
          {
            throw new RuntimeError(expr.operator, "Cannot divide by zero.");
          }
        return (double)left / (double)right;
      case STAR:
        checkNumberOperands(expr.operator, left, right);
        return (double)left * (double)right;
      case BANG_EQUAL: return !isEqual(left, right);
      case EQUAL_EQUAL: return isEqual(left, right);
    }
    // Unreachable.
    return null;
  }




   //evalutes unary expressions.
    @Override
  public Object visitUnaryExpr(Expr.Unary expr) 
  {
    Object right = evaluate(expr.right);
    switch (expr.operator.type) 
    {
      case BANG:
        return !isTruthy(right);
      case MINUS:
        return -(double)right;
    }
    // Unreachable.
    return null;
  }

  //forwards to the environment which does the heavy lifting to make sure the variable is defined
   @Override
   /*Modification for Challenge Question 2, chapter 8 
    the method now does a check to see if the variable has been initialized. If it has, return a value
    otherwise throw an error message.*/
  public Object visitVariableExpr(Expr.Variable expr) 
  {
    // return environment.get(expr.name);
    Object value = environment.get(expr.name);

    if (value == uninitialized)
    {
      throw new RuntimeError(expr.name, "Variable must be intialized before it can be used. ");
    }
    return value;
  }

  //handles conditional expressions like ?:
  @Override
public Object visitConditionalExpr(Expr.Conditional expr) 
{
  Object condition = evaluate(expr.condition);

  if (isTruthy(condition)) {
    return evaluate(expr.thenBranch);
  }

  return evaluate(expr.elseBranch);
}


  //handles every case other than false or nil.
  //Truty == true || truthy == truthful
  private boolean isTruthy(Object object) 
  {
    if (object == null) return false;
    if (object instanceof Boolean) return (boolean)object;
    return true;
  }

  //handles the equality logic.
   private boolean isEqual(Object a, Object b) 
   {
        if (a == null && b == null) return true;
        if (a == null) return false;
        return a.equals(b);
   }


   //converts a lox value to a string
    private String stringify(Object object) 
    {
        if (object == null) return "nil";
        if (object instanceof Double) 
        {
            String text = object.toString();
            if (text.endsWith(".0")) 
            {
                text = text.substring(0, text.length() - 2);
            }
            return text;
        }
    return object.toString();
  }

   //checks to see if a number is being used in the operand
   //throws an error if otherwise.
    private void checkNumberOperand(Token operator, Object operand) 
    {
        if (operand instanceof Double) return;
        throw new RuntimeError(operator, "Operand must be a number.");
    }

    //evaluattes both operands before checking the type of either.
    private void checkNumberOperands(Token operator, Object left, Object right) 
    {
        if (left instanceof Double && right instanceof Double) return;
        throw new RuntimeError(operator, "Operands must be numbers.");
    }


    //accepts a list of statements (a program)    
    void interpret(List<Stmt> statements) 
    {
    try 
    {
      for (Stmt statement : statements) 
        {
        execute(statement);
      }
    } catch (RuntimeError error) 
    {
      Lox.runtimeError(error);
    }
  }


  /*Challenge Question 1, chapter 8 */

  //interpret method serves as a helper for REPL
    void interpret(Stmt statement)
  {
    try
    {
      execute(statement);
    }
    catch (RuntimeError error)
    {
      Lox.runtimeError(error);
    }
  }

  //helper method
  //interprets the REPL expression
  void interpretRepl(Expr expression)
  {
    try{
      Object value = evaluate(expression);
      System.out.println(stringify(value));

    }
    catch (RuntimeError error)
    {
      Lox.runtimeError(error);
    }
  }
  

}