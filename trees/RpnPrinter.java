package com.craftinginterpreters.lox;

class RpnPrinter implements Expr.Visitor<String>
{
    //takes an expression tree, returns it in RPN form
    String print(Expr expr)
    {
        return expr.accept(this);
    }



    //Handles binary expressions such as: 1+2 or (1+2)*3
    @Override
    public String visitBinaryExpr(Expr.Binary expr)
    {
        //put left expression first
        return expr.left.accept(this) + " " +
        //then right expression
        expr.right.accept(this) + " " + expr.operator.lexeme;
    }



    //handles a grouping expression like (1+2)
    public String visitGroupingExpr(Expr.Grouping expr)
    {
        //as RPN does not need parentheses
        //the evaluation order is unambigous, therefore the grouping ndoe is ignored
        //and the expression inside the parenthesis is returned.
        return expr.expression.accept(this);
    }


    //handles literal values like numbers, strings, true, false, and nil.
    public String visitLiteralExpr(Expr.Literal expr)
    {
        //Lox represents nil using java's nil value.
        if (expr.value == null)
        {
            return "nil";
        }

        //converts a non-null literal value such as 1.0 or "hello", into text 
        //So RPN can output it.
         return expr.value.toString();
    }
    
    //handles expressions like -123 or !true.
    public String visitUnaryExpr(Expr.Unary expr)
    {
        return expr.right.accept(this) + " " + expr.operator.lexeme;
    }



    //the below section handles testing.
    //comment it out when running Lox based RPN programs.
    public static void main(String[] args) 
    {
        Expr expression = new Expr.Binary(
          new Expr.Grouping(
            //creates the binary node for 1 + 2
          new Expr.Binary(
            //create left literal 1.0
          new Expr.Literal(1.0),
            //creates the + operator token. the 1 at the end indicates the token is on line 1.
          new Token(TokenType.PLUS, "+", null, 1),
          //create the right literal 2.0
          new Expr.Literal(2.0)
          )
      ),
      //creates outer * (multipication) operator token.
      new Token(TokenType.STAR, "*", null, 1),
      new Expr.Grouping(
        //creates the binary node for 4-3.
          new Expr.Binary(
              new Expr.Literal(4.0), //creates the left literal 4.0.

              //Creates the - operator token
              new Token(TokenType.MINUS, "-", null, 1),
              //creates the right literal 3.
              new Expr.Literal(3.0)
          )
      )
  );


    /* Create a RpnPrinter object.
    Call print(expression) to convert the AST to RPN.
    Print the completed RPN string to the terminal.*/

  System.out.println(new RpnPrinter().print(expression));
}


   
}