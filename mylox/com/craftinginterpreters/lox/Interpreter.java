package com.craftinginterpreters.lox;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Scanner; //allows for user input, challenge question 3, chapter 13.
import com.craftinginterpreters.lox.Lox;



class Interpreter implements Expr.Visitor<Object>, Stmt.Visitor<Void>  
{

  final Environment globals = new Environment();
  private Environment environment = globals;
  private final Scanner input = new Scanner(System.in); //allows for user input, challenge question 3, chapter 13.

  //changed for Challenge question 4, chapter 11.
  private final Map<Expr, ResolvedLocal> locals = new HashMap<>();
  private final Map<Stmt.Var, Integer> localDeclarationSlots = new HashMap<>();
  private final Map<Stmt.Function, Integer> localFunctionSlots = new HashMap<>(); //allows for multiple local functions in the same file

  private final Map<Token, Integer> parameterSlots = new HashMap<>();

   Interpreter() 
   {
    globals.define("clock", new LoxCallable() 
    {
      @Override
      public int arity() { return 0; }

      @Override
      public Object call(Interpreter interpreter,
                         List<Object> arguments) {
        return (double)System.currentTimeMillis() / 1000.0;
      }

      @Override
      public String toString() { return "<native fn>"; }
    });

//allows for user input, challenge question 3, chapter 13.
    globals.define("input", new LoxCallable() 
    {
      @Override
      public int arity() 
      {
        // input() takes no Lox arguments.
        return 0;
      }

      @Override
      public Object call(Interpreter interpreter,List<Object> arguments) 
      {
        // Return nil at end-of-file instead of throwing a Java exception.
        // This occurs if standard input is closed or redirected input ends.
          if (!input.hasNextLine()) 
            {
              return null;
            }

          // Read one complete line and return it as a Lox string.
          return input.nextLine();
      }

        @Override
        public String toString() 
      {
        return "<native fn input>";
      }
    } );
  }
  

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

   @Override
  public Object visitSetExpr(Expr.Set expr) 
  {
    Object object = evaluate(expr.object);

    if (!(object instanceof LoxInstance)) 
    { 
      throw new RuntimeError(expr.name,"Only instances have fields.");
    }

    Object value = evaluate(expr.value);
    ((LoxInstance)object).set(expr.name, value);
    return value;
  }

   @Override
  public Object visitSuperExpr(Expr.Super expr) 
  {
    ResolvedLocal local = locals.get(expr);
    int distance = local.distance;
    LoxClass superclass = (LoxClass)environment.getAt(distance, "super");
    LoxInstance object = (LoxInstance)environment.getAt(distance - 1, "this");
    LoxFunction method = superclass.findMethod(expr.method.lexeme);


    if (method == null) 
    {
      throw new RuntimeError(expr.method, "Undefined property '" + expr.method.lexeme + "'.");
    }

    return method.bind(object);

  }


   @Override
  public Object visitThisExpr(Expr.This expr) 
  {
    return lookUpVariable(expr.keyword, expr);
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

   @Override
  public Void visitFunctionStmt(Stmt.Function stmt) 
  {
    LoxFunction function = new LoxFunction(stmt, environment);
     Integer slot = localFunctionSlots.get(stmt);
    if(slot != null)
    {
      environment.defineAt(slot, function);
      
    }
    else
    {
      environment.define(stmt.name.lexeme, function); 
    }
    
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
  public Void visitReturnStmt(Stmt.Return stmt) 
  {
    Object value = null;
    if (stmt.value != null) value = evaluate(stmt.value);

    throw new Return(value);
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

     Integer index = localDeclarationSlots.get(stmt);

     /*
      Resolver records a numeric slot. The interpreter uses:
      environment.defineAt(index, value);
      which writes to the locals ArrayList in Environment.

      Challenge Question 4 Chapter 11.
     */
    if (index != null) 
    {
      environment.defineAt(index, value);
    } 
    else 
    {
      environment.define(stmt.name.lexeme, value);
    }
    
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

  //global environment can remain map-based. 
  //Only resolver-known local variables need indexed lookup.
  //Challenge Question 4, Chapter 11.
   @Override
  public Object visitAssignExpr(Expr.Assign expr) 
  {
    Object value = evaluate(expr.value);

    ResolvedLocal local = locals.get(expr);
    if (local != null) 
    {
      environment.assignAt(local.distance, local.index, value);
    } 
    else 
    {
      globals.assign(expr.name, value);
    }
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

  //modifications made to allow for local variables
  //in an array and look them up by index.
  //challenge question 4 Chapter 11.
  void resolve(Expr expr, int distance, int index) 
  {
    locals.put(expr, new ResolvedLocal(distance, index));
  }

  void resolve(Stmt.Var stmt, int index) 
  {
    localDeclarationSlots.put(stmt, index);
  }

  void resolve(Stmt.Function stmt, int index) 
{
  localFunctionSlots.put(stmt, index);
}

  //saves parameter slot information
  //Challenge Question 4, Chapter 11.
  void resolve(Token parameter, int index) 
  {
    parameterSlots.put(parameter, index);
  }

  int getParameterSlot(Token parameter) 
  {
    return parameterSlots.get(parameter);
  }

  @Override
  public Void visitBlockStmt(Stmt.Block stmt) 
  {
    executeBlock(stmt.statements, new Environment(environment));
    return null;
  }

   @Override
  public Void visitClassStmt(Stmt.Class stmt) 
  {

    //creation of superclass support.
    Object superclass = null;
    if (stmt.superclass != null) 
    {
      superclass = evaluate(stmt.superclass);
      if (!(superclass instanceof LoxClass)) 
      {
        throw new RuntimeError(stmt.superclass.name,
            "Superclass must be a class.");
      }
    }

    
    environment.define(stmt.name.lexeme, null);

     if (stmt.superclass != null) 
    {
      environment = new Environment(environment);
      environment.define("super", superclass);
    }


    // It starts empty. First, mixin methods are copied into it.
  // Then methods written directly inside the class are added afterward.
  // This order lets class methods override mixin methods.
    Map<String, LoxFunction> methods = new HashMap<>();
    // Copy instance methods from each mixin listed after "with".
    for (Expr.Variable mixinReference : stmt.mixins) 
    {
      // Evaluate the mixin name in the current environment.
      Object value = evaluate(mixinReference);


       // Reject ordinary classes, numbers, strings, functions, or undefined
      // values. Only a LoxMixin may appear after "with".
      if (!(value instanceof LoxMixin)) 
      {
        throw new RuntimeError(mixinReference.name,"Can only include mixins after 'with'.");
      }

        // The type check above makes this cast safe.
      LoxMixin mixin = (LoxMixin) value;

       // Copy every method from this mixin into the class's instance-method map.
        for (Map.Entry<String, LoxFunction> entry : mixin.methods.entrySet()) 
        {
           // Extract the method name, such as "label" or "timestamp".
          String methodName = entry.getKey();

          // Two different mixins providing the same method is ambiguous.
          if (methods.containsKey(methodName)) 
          {
              throw new RuntimeError(mixinReference.name,"Duplicate mixin method '" + methodName + "'.");
          }

          // Copy the reusable mixin method into the receiving class's method map.
          // Normal LoxClass.findMethod() and LoxInstance.get() can now find it
    // exactly as if it had been written directly in the class.
        methods.put(methodName, entry.getValue());
    }
}

// Methods written directly in the class override methods supplied by mixins.
  for (Stmt.Function method : stmt.methods) 
  {
    LoxFunction function = new LoxFunction(method,environment,method.name.lexeme.equals("init"));
    methods.put(method.name.lexeme, function);
  }

    Map<String, LoxFunction> classMethods = new HashMap<>();

    //support for visitng static methods. Challenge question 1, chapter 12.
    for (Stmt.Function method : stmt.classMethods) 
    {
        LoxFunction function = new LoxFunction(method,environment,false);
        classMethods.put(method.name.lexeme, function);
    }

    LoxClass klass = new LoxClass(stmt.name.lexeme, (LoxClass)superclass, methods, classMethods);

    if (superclass != null) 
    {
      environment = environment.enclosing;
    }

    environment.assign(stmt.name, klass);
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

    @Override
  public Object visitCallExpr(Expr.Call expr) 
  {
    Object callee = evaluate(expr.callee);

    List<Object> arguments = new ArrayList<>();
    for (Expr argument : expr.arguments) { 
      arguments.add(evaluate(argument));
    }

     if (!(callee instanceof LoxCallable)) 
      {
        throw new RuntimeError(expr.paren,"Can only call functions and classes.");
      }
      

    LoxCallable function = (LoxCallable)callee;

      //if the arguments length does not match callable's arity.
    if (arguments.size() != function.arity()) 
      {
          throw new RuntimeError(expr.paren, "Expected " +
          function.arity() + " arguments but got " +
          arguments.size() + ".");
      }
    return function.call(this, arguments);
  }

   @Override
  public Object visitGetExpr(Expr.Get expr) 
  {
    Object object = evaluate(expr.object);
    if (object instanceof LoxInstance) 
      {
      return ((LoxInstance) object).get(this, expr.name);
      }


       if (object instanceof LoxClass) 
       {
          LoxFunction method = ((LoxClass) object).findClassMethod(expr.name.lexeme);
      
           if (method != null) 
          {
            return method;
          }
      }

   

    throw new RuntimeError(expr.name,
        "Only instances have properties.");
  }



  @Override
public Object visitFunctionExpr(Expr.Function expr) 
{
  return new LoxFunction(expr, environment);
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
    
     Object value = lookUpVariable(expr.name, expr);

        if (value == uninitialized) 
        { 
          throw new RuntimeError(expr.name,"Variable must be initialized before it can be used.");
        }
    return value;
  }

  private Object lookUpVariable(Token name, Expr expr) 
  {
     ResolvedLocal local = locals.get(expr);

    if (local != null) 
    {
      return environment.getAt(local.distance, local.index);
    } 
    else 
    {
      return globals.get(name);
    }
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
  
  @Override

public Void visitMixinStmt(Stmt.Mixin stmt) 
{

  // Build a method table for this mixin.
  // The key is a method name, such as "label".
  // The value is the compiled LoxFunction for that method.
  Map<String, LoxFunction> methods = new HashMap<>();

    // Convert every parsed mixin method into a callable LoxFunction.
  for (Stmt.Function method : stmt.methods) 
  {


    // A mixin cannot define an initializer because it cannot be called
    // like a class and cannot create its own instance.
    if (method.name.lexeme.equals("init")) 
    {
      throw new RuntimeError(method.name,"A mixin cannot declare an initializer.");
    }

    // Wrap the method's AST node in a LoxFunction.
    // "environment" is captured so the method retains lexical scoping,
    // just like an ordinary Lox method or function.
    // false means this is not an initializer.
    LoxFunction function = new LoxFunction(method, environment,false);

    // Add the method to the mixin's method table.
    methods.put(method.name.lexeme, function);
  }

    // Store the finished mixin in the current environment under its name.
  environment.define(stmt.name.lexeme,new LoxMixin(stmt.name.lexeme, methods));

  return null;
}

}