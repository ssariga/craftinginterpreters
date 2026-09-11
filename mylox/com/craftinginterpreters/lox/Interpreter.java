package com.craftinginterpreters.lox;

import com.craftinginterpreters.lox.Lox;

class Interpreter implements Expr.Visitor<Object> 
{

  //converts a tree node to a runtime value
  @Override 
  public Object visitLiteralExpr(Expr.Literal expr) 
  {
    return expr.value;
  }


  //evalutes grouping aka ( )
   @Override
  public Object visitGroupingExpr(Expr.Grouping expr) 
  {
    return evaluate(expr.expression);
  }

  //helps handle left hand sides of assingment expressions.
   private Object evaluate(Expr expr) 
  {
    return expr.accept(this);
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


   //conveerts a lox value to a string
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


     void interpret(Expr expression) 
    { 
    try 
    {
      Object value = evaluate(expression);
      System.out.println(stringify(value));
    } 
    catch (RuntimeError error) 
    {
      Lox.runtimeError(error);
    } 
  }




}