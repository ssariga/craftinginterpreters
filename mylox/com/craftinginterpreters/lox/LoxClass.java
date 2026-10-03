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

   LoxFunction findMethod(String name) 
  {
    if (methods.containsKey(name)) 
    {
      return methods.get(name);
    }

     if (superclass != null) 
    {
      return superclass.findMethod(name);
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

     LoxFunction initializer = findMethod("init");
    if (initializer != null) 
    {
      initializer.bind(instance).call(interpreter, arguments);
    }
    return instance;
  }

  @Override
  public int arity() 
  {
     LoxFunction initializer = findMethod("init");
    if (initializer == null) 
        return 0;
    
    return initializer.arity();
  }
}