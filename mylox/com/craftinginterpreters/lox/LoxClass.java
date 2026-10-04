package com.craftinginterpreters.lox;

import java.util.List;
import java.util.Map;

class LoxClass implements LoxCallable 
{
  final String name;
  final LoxClass superclass;
  private final Map<String, LoxFunction> methods;
  private final Map<String, LoxFunction> classMethods;// added support for static methods, challenge question 1, chapter 12.
   

  LoxClass(String name, LoxClass superclass, Map<String, LoxFunction> methods, Map<String, LoxFunction> classMethods) 
  {
    this.name = name;
    this.superclass = superclass;
    this.methods = methods;
    this.classMethods = classMethods;
  }

   LoxFunction findMethod(LoxInstance instance, String name) 
  {
     // "method" becomes the highest/super-most implementation found.
    LoxFunction method = null;

    // "inner" tracks the implementation immediately below "method" in
    // the hierarchy. It will be made available to the chosen method.
    LoxFunction inner = null;

    // Start with the concrete runtime class, then walk upward through
    // superclass references until there are no more parents.
    LoxClass klass = this;

    while (klass != null) 
    {
      // Do not return early. We must inspect every ancestor in order to
      // prefer the highest ancestor's implementation.
      if (klass.methods.containsKey(name)) 
      {
        // The last method found lower in the hierarchy becomes the
        // current method's "inner" implementation.
        inner = method;

        // Because we are walking upward, every newly found method is
        // more general than the previous method.
        method = klass.methods.get(name);
      }

      klass = klass.superclass;
    }

  // If some class in the hierarchy provided this method, bind both the
  // receiver instance and the next lower implementation.
    if (method != null) 
    {
      return method.bind(instance, inner);
    }


    return null;
  }


  //added support for static methods, challenge question 1, chapter 12.
LoxFunction findClassMethod(String name) 
{
  return classMethods.get(name);
}

  @Override
  public String toString() 
  {
    return name;
  }

   @Override
  public Object call(Interpreter interpreter, List<Object> arguments) 
  {
    LoxInstance instance = new LoxInstance(this);

    // Use BETA-style lookup for init
    LoxFunction initializer = findMethod(instance,"init");
    if (initializer != null) 
    {
      // findMethod() already binds this and inner.
      initializer.bind(instance).call(interpreter, arguments);
    }
    return instance;
  }

  @Override
  public int arity() 
  {
     LoxFunction initializer = findTopMethod("init");
    if (initializer == null) 
        return 0;
    
    return initializer.arity();
  }


  //helper method to assist artiy() method.
  //returns the highest matching implementation without binding it
  //Challenge Question 2, Chapter 13.
  private LoxFunction findTopMethod(String name) 
{
  LoxFunction method = null;

  for (LoxClass klass = this;
      klass != null;
      klass = klass.superclass) 
  {
    if (klass.methods.containsKey(name)) 
    {
      method = klass.methods.get(name);
    }
  }

  return method;
}
}