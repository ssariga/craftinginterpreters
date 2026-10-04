package com.craftinginterpreters.lox;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Stack;
import java.util.Iterator;

class Resolver implements Expr.Visitor<Void>, Stmt.Visitor<Void> 
{
  private final Interpreter interpreter;
  private final Stack<Map<String, LocalVariable>> scopes = new Stack<>();
  private FunctionType currentFunction = FunctionType.NONE;
  private final Stack<Integer> nextLocalIndex = new Stack<>();
 

  Resolver(Interpreter interpreter) 
  {
    this.interpreter = interpreter;
  }

   private enum FunctionType 
   {
    NONE,
    FUNCTION,
    INITIALIZER,
    METHOD
  }

   private enum ClassType 
  {
    NONE,
    CLASS,
    SUBCLASS,
    MIXIN //added mixin support, challenge question 1, chapter 13.
  }

  private ClassType currentClass = ClassType.NONE;

  //added support for the resolver to report an error if a local 
  //variable is never used, Challenge question 3, chapter 11.
  private enum VariableState
  {
    DECLARED, // local name has been introduced, but its initializer is still being resolved
    DEFINED, // local variable is initialized and valid to read, but it has not been read yet.
    READ // at least one Expr.Variable has referred to the local variable.
  }

  private static class LocalVariable
{
  final Token name;
  VariableState state;
   final int index;
   //added support for the resolver to report an error if a local 
  //variable is never used, Challenge question 3, chapter 11.
  LocalVariable(Token name, int index, VariableState state) 
  {
    this.name = name;
    this.state = state;
    this.index = index;
  }
    
}


 
 


   @Override //creation of the block statement scope.
  public Void visitBlockStmt(Stmt.Block stmt) 
  {
    beginScope();
    resolve(stmt.statements);
    endScope();
    return null;
  }

   @Override
  public Void visitClassStmt(Stmt.Class stmt) 
  {

    ClassType enclosingClass = currentClass;
    currentClass = ClassType.CLASS;
    declare(stmt.name);
    define(stmt.name);


     // Reject: class Foo < Foo { ... }
    if (stmt.superclass != null && stmt.name.lexeme.equals(stmt.superclass.name.lexeme)) 
    {
      Lox.error(stmt.superclass.name,
          "A class can't inherit from itself.");
    }

    // Resolve the superclass expression, if present.
    if (stmt.superclass != null) 
    {
      currentClass = ClassType.SUBCLASS;
       // This scope is captured by methods so "super" can be resolved.
      resolve(stmt.superclass);
    }

    /* Removed super for BETA support, challenge question 2, chapter 12
    if (stmt.superclass != null) 
    {
      beginScope();

      Token superToken = new Token(TokenType.SUPER, "super", null, stmt.name.line);

      declare(superToken);
      define(superToken);
    
    } */


    //added mixin support, challenge question 1, chapter 13
    for (Expr.Variable mixin : stmt.mixins) 
    {
      resolve(mixin);
    }

    // Every instance method gets an implicit "this" and BETA-style "inner".
    beginScope();
    // Add the implicit receiver variable used by all instance methods.
    Token thisToken = new Token(TokenType.THIS,"this",null,stmt.name.line);

    declare(thisToken);
    define(thisToken);

    
     for (Stmt.Function method : stmt.methods) 
    {
      FunctionType declaration = FunctionType.METHOD;
       if (method.name.lexeme.equals("init")) 
      {
        declaration = FunctionType.INITIALIZER;
      }
      resolveFunction(method, declaration); 
    }

    //added support for static methods, challenge question 1, chapter 12.
  for (Stmt.Function method : stmt.classMethods) 
  {
    resolveFunction(method, FunctionType.FUNCTION);
  }

   // Close the this/inner scope opened immediately above.
  endScope();

  /*  Removed superclass support,
      challenge question 2, chapter 13.
    if (stmt.superclass != null)
      {
          endScope();
      } 
      */
      
    currentClass = enclosingClass;
    return null;
  }

  //An expression statement contains a single expression to traverse.
   @Override
  public Void visitExpressionStmt(Stmt.Expression stmt) 
  {
    resolve(stmt.expression);
    return null;
  }


  @Override //allows for the creation of the function scope.
  public Void visitFunctionStmt(Stmt.Function stmt) 
  {
    declare(stmt.name);
    if (!scopes.isEmpty()) 
    {
      LocalVariable local = scopes.peek().get(stmt.name.lexeme);
      interpreter.resolve(stmt, local.index);
    }

  define(stmt.name);

    resolveFunction(stmt, FunctionType.FUNCTION);
    
    return null;
  }

  //An if statement has an expression for its condition 
  // and one or two statements for the branches.
   @Override
  public Void visitIfStmt(Stmt.If stmt) 
  {
    resolve(stmt.condition);
    resolve(stmt.thenBranch);
    if (stmt.elseBranch != null) resolve(stmt.elseBranch);
    return null;
  }


  //Like expression statements, 
  // a print statement contains a single subexpression.
   @Override
  public Void visitPrintStmt(Stmt.Print stmt) 
  {
    resolve(stmt.expression);
    return null;
  }


  //the same as visitPrintStatement
   @Override
  public Void visitReturnStmt(Stmt.Return stmt) 
  {
    if (currentFunction == FunctionType.NONE) 
    {
      Lox.error(stmt.keyword, "Can't return from top-level code.");
    }
    if (stmt.value != null) 
    {


       if (currentFunction == FunctionType.INITIALIZER) 
      {
        Lox.error(stmt.keyword,
            "Can't return a value from an initializer.");
      }
    resolve(stmt.value);
    }

    return null;
  }


    @Override //allows for the creation of the statement scope.
  public Void visitVarStmt(Stmt.Var stmt) 
  {
    
    declare(stmt.name);


    /*At global scope, scopes is empty.
    Therefore globals continue using the normal name-based Map<String, Object> environment.
    Inside a block or function, the resolver already assigned an index in declare().
    This stores the relationship:
    
    this exact Stmt.Var node → slot index
    in Interpreter.localDeclarationSlots.
    
    The resolver’s scope stack represents nested lexical scopes; 
    entering a block/function pushes one, and leaving it pops it.

    Challenge Question 4, Chapter 11.

 */
    if (!scopes.isEmpty()) 
    {
      LocalVariable local = scopes.peek().get(stmt.name.lexeme);
      interpreter.resolve(stmt, local.index);
    }

    if (stmt.initializer != null) 
    {
    resolve(stmt.initializer);
    }

    define(stmt.name);
    return null;
  }

  //As in if statements, with a while statement, 
  //we resolve its condition and resolve the body exactly once.
    @Override
  public Void visitWhileStmt(Stmt.While stmt) 
  {
    resolve(stmt.condition);
    resolve(stmt.body);
    return null;
  }

  //allows variables to write to scope maps.
   @Override
  public Void visitVariableExpr(Expr.Variable expr) 
  {
    //if the scope is not empty, 
    //run the following code block.
    //Challenge Question 3, Chapter 11.
    if (!scopes.isEmpty()) 
    {
        LocalVariable variable = scopes.peek().get(expr.name.lexeme);

        if (variable != null &&variable.state == VariableState.DECLARED) 
        {
            Lox.error( expr.name,"Can't read local variable in its own initializer.");
        }
   }
    resolveLocal(expr, expr.name);
    return null;
  }



  @Override
public Void visitMixinStmt(Stmt.Mixin stmt) 
{
  // Save the surrounding class state so it can be restored once the
  // resolver is finished resolving this mixin declaration.
  ClassType enclosingClass = currentClass;
  currentClass = ClassType.MIXIN;

  declare(stmt.name);
  define(stmt.name);

  beginScope();
  Token thisToken = new Token(TokenType.THIS, "this", null, stmt.name.line);
  declare(thisToken);
  define(thisToken);


   // Resolve each method declared in the mixin.
  for (Stmt.Function method : stmt.methods) 
  {
    if (method.name.lexeme.equals("init")) 
    {
      // Mixins are method-reuse bundles, not constructible classes.
    // Therefore, they must not declare an initializer named "init". 
      Lox.error(method.name,"A mixin cannot declare an initializer.");
    }

    resolveFunction(method, FunctionType.METHOD);
  }

  endScope();

  currentClass = enclosingClass;
  return null;
}

   @Override
  public Void visitAssignExpr(Expr.Assign expr) 
  {
    resolve(expr.value);
    resolveLocal(expr, expr.name);
    return null;
  }

 @Override
  public Void visitBinaryExpr(Expr.Binary expr) 
  {
    resolve(expr.left);
    resolve(expr.right);
    return null;
  }

  @Override
public Void visitConditionalExpr(Expr.Conditional expr) 
{
  resolve(expr.condition);
  resolve(expr.thenBranch);
  resolve(expr.elseBranch);
  return null;
}

@Override
  public Void visitCallExpr(Expr.Call expr) 
  {
    resolve(expr.callee);

    for (Expr argument : expr.arguments) 
    {
      resolve(argument);
    }

    return null;
  }  

   @Override
  public Void visitGetExpr(Expr.Get expr) 
  {
    resolve(expr.object);
    return null;
  }

    @Override
  public Void visitGroupingExpr(Expr.Grouping expr) 
  {
    resolve(expr.expression);
    return null;
  }

   @Override
  public Void visitLiteralExpr(Expr.Literal expr) 
  {
    return null;
  }

   @Override
  public Void visitLogicalExpr(Expr.Logical expr) 
  {
    resolve(expr.left);
    resolve(expr.right);
    return null;
  }

   @Override
  public Void visitSetExpr(Expr.Set expr) 
  {
    resolve(expr.value);
    resolve(expr.object);
    return null;
  }

    @Override
  public Void visitSuperExpr(Expr.Super expr) 
  {

    //prevents mixin from being used in a superclass.
    //challenge question 1, chapter 13
    if (currentClass == ClassType.MIXIN) 
      {
        Lox.error(expr.keyword,"Can't use 'super' inside a mixin.");
      }
    if (currentClass == ClassType.NONE) 
    {
      Lox.error(expr.keyword,
          "Can't use 'super' outside of a class.");
    } 
    else if (currentClass != ClassType.SUBCLASS) 
    {
      Lox.error(expr.keyword,
          "Can't use 'super' in a class with no superclass.");
    }
    resolveLocal(expr, expr.keyword);
    return null;
  }

  @Override
  public Void visitThisExpr(Expr.This expr) 
  {

     if (currentClass == ClassType.NONE) 
      {
      Lox.error(expr.keyword,"Can't use 'this' outside of a class.");
      return null;
    }
    resolveLocal(expr, expr.keyword);
    return null;
  }

  @Override
  public Void visitUnaryExpr(Expr.Unary expr) 
  {
    resolve(expr.right);
    return null;
  }

  @Override
public Void visitBreakStmt(Stmt.Break stmt) 
{

  return null;
}


/*
Handles anonymous functions.
fun (value) {
  return value + 1;
} */
@Override
public Void visitFunctionExpr(Expr.Function expr) 
{
  resolveFunction(expr, FunctionType.FUNCTION);
  return null;
}


/*
Modifications for challenge question 4, Chapter 11. 
Begin function scope.

Declare left:
  left receives local slot 0.
  Save the relationship: left token --> 0.

Declare right:
  right receives local slot 1.
  Save the relationship: right token -->1.

Resolve function body.
End function scope.
*/

private void resolveFunction(Expr.Function function, FunctionType type) 
{
  FunctionType enclosingFunction = currentFunction;
  currentFunction = type;

  beginScope();

  for (Token param : function.params) 
  {
    declare(param);
    LocalVariable local = scopes.peek().get(param.lexeme);
    interpreter.resolve(param, local.index);
    define(param);
  }

  resolve(function.body);

  endScope();

  currentFunction = enclosingFunction;
}

  //resolves the varaible itself.
  private void resolveLocal(Expr expr, Token name) 
{
  for (int i = scopes.size() - 1; i >= 0; i--) 
  {
    Map<String, LocalVariable> scope = scopes.get(i);

    LocalVariable variable = scope.get(name.lexeme);

    if (variable != null) 
    {
      variable.state = VariableState.READ;

      int distance = scopes.size() - 1 - i;

      interpreter.resolve(expr, distance, variable.index);
      return;
    }
  }
}

    //walks a list of statments, resolves each one.
   void resolve(List<Stmt> statements) 
   {
    for (Stmt statement : statements) {
      resolve(statement);
    }
  }


  /*
Modifications for challenge question 4, Chapter 11. 
Begin function scope.

Declare left:
  left receives local slot 0.
  Save the relationship: left token --> 0.

Declare right:
  right receives local slot 1.
  Save the relationship: right token -->1.

Resolve function body.
End function scope.
*/
   private void resolveFunction(Stmt.Function function, FunctionType type) 
   {

    FunctionType enclosingFunction = currentFunction;
    currentFunction = type;
    beginScope();
    for (Token param : function.params) 
    {
      declare(param);
      LocalVariable local = scopes.peek().get(param.lexeme);
      interpreter.resolve(param, local.index);
      define(param);
    }

    resolve(function.body);
    endScope();
    currentFunction = enclosingFunction;
  }

  //creates a new block scope
   private void beginScope() 
   {
    scopes.push(new HashMap<String, LocalVariable>());
     nextLocalIndex.push(0);
   }

   //exits the scope, as scopes are stored in explicit stacks, it is easy to exit.
   private void endScope() 
 {
    Map<String, LocalVariable> scope =scopes.pop();
    nextLocalIndex.pop();

    Iterator<LocalVariable> iterator = scope.values().iterator();

    while (iterator.hasNext()) 
    {
         LocalVariable variable = iterator.next();

        if(variable.state == VariableState.DEFINED && !variable.name.lexeme.equals("this"))
          {
            Lox.error(variable.name, "Local variable '" +  variable.name.lexeme + "' is never used.");
          } 
        
            
    }
}

  //Declaration adds the variable to the innermost scope 
  // so that it shadows any outer one and so that we know the variable exists
  //added support for the resolver to report an error if a local 
  //variable is never used, Challenge question 3, chapter 11.
  private void declare(Token name) 
  {
    if (scopes.isEmpty()) 
        return;

    Map<String, LocalVariable> scope = scopes.peek();

    if (scope.containsKey(name.lexeme)) 
    {
      Lox.error(name,
          "Already a variable with this name in this scope.");
    }
     int index = nextLocalIndex.pop();
     nextLocalIndex.push(index + 1);

  scope.put(name.lexeme, new LocalVariable(name, index, VariableState.DECLARED));
  }

  //primes the variable
  //added support for the resolver to report an error if a local 
  //variable is never used, Challenge question 3, chapter 11.
   private void define(Token name) 
   {
        if (scopes.isEmpty()) 
        {
            return;
        }
            
        LocalVariable variable = scopes.peek().get(name.lexeme);

         if (variable != null) 
        {
            variable.state = VariableState.DEFINED;
        }
    }

  private void resolve(Stmt stmt) 
  {
    stmt.accept(this);
  }

   private void resolve(Expr expr) 
   {
    expr.accept(this);
  }


}