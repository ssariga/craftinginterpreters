package com.craftinginterpreters.lox;

class AstPrinter implements Expr.Visitor<String> 
{
  String print(Expr expr) 
  {
    return expr.accept(this);
  }

   @Override
  public String visitBinaryExpr(Expr.Binary expr) 
  {
    return parenthesize(expr.operator.lexeme,
                        expr.left, expr.right);
  }

  @Override
  public String visitGroupingExpr(Expr.Grouping expr) 
  {
    return parenthesize("group", expr.expression);
  }

  @Override
  public String visitLiteralExpr(Expr.Literal expr) 
  {
    if (expr.value == null) return "nil";
    return expr.value.toString();
  }

  //required to deal with Java encforcing  the Visitor interface.
@Override
public String visitVariableExpr(Expr.Variable expr) 
{
  return expr.name.lexeme;
}


@Override
public String visitAssignExpr(Expr.Assign expr) 
{
  return expr.name.lexeme + " " + expr.value.accept(this) + " =";
}


  @Override
  public String visitLogicalExpr(Expr.Logical expr) 
  { 
    return expr.left.accept(this) + " " + expr.right.accept(this) + " " + expr.operator.lexeme;
  }

  @Override
  public String visitUnaryExpr(Expr.Unary expr) 
  {
    return parenthesize(expr.operator.lexeme, expr.right);
  }



 //added for challenge question 2, chapter 6
  @Override
public String visitConditionalExpr(Expr.Conditional expr) 
{
  return "(? " + expr.condition.accept(this)
      + " " + expr.thenBranch.accept(this)
      + " " + expr.elseBranch.accept(this)
      + ")";
}


    //handles exrpessions with subexrpessions.
   private String parenthesize(String name, Expr... exprs) 
   {
    StringBuilder builder = new StringBuilder();

    builder.append("(").append(name);
    for (Expr expr : exprs) 
    {

      builder.append(" ");
      builder.append(expr.accept(this));
    }
    builder.append(")");

    return builder.toString();
  }

    //manually instantiate the tree and print it.
   public static void main(String[] args) 
   {
    Expr expression = new Expr.Binary(
        new Expr.Unary(
            new Token(TokenType.MINUS, "-", null, 1),
            new Expr.Literal(123)),
        new Token(TokenType.STAR, "*", null, 1),
        new Expr.Grouping(
            new Expr.Literal(45.67)));

    System.out.println(new AstPrinter().print(expression));
  }



@Override
public String visitCallExpr(Expr.Call expr) 
{
  StringBuilder builder = new StringBuilder();

  builder.append(print(expr.callee));
  builder.append("(");

  for (int i = 0; i < expr.arguments.size(); i++) {
    if (i > 0) builder.append(", ");
    builder.append(print(expr.arguments.get(i)));
  }

  builder.append(")");
  return builder.toString();
}


@Override
public String visitFunctionExpr(Expr.Function expr) 
{
  return "<fn>";
}

@Override
public String visitGetExpr(Expr.Get expr) 
{
  return print(expr.object) + "." + expr.name.lexeme;
}

@Override
public String visitSetExpr(Expr.Set expr) 
{
  return print(expr.object) + "." + expr.name.lexeme +
      " = " + print(expr.value);
}

@Override
public String visitThisExpr(Expr.This expr) 
{
  return "this";
}

@Override
public String visitSuperExpr(Expr.Super expr) 
{
  return "super." + expr.method.lexeme;
}



}
