package com.craftinginterpreters.lox;

import java.util.HashMap;
import java.util.Map;

class LoxInstance 
{
  private LoxClass klass;
private final Map<String, Object> fields = new HashMap<>();


  LoxInstance(LoxClass klass) 
  {
    this.klass = klass;
  }
   Object get(Interpreter interpreter, Token name) 
   {
          if (fields.containsKey(name.lexeme)) 
    {
      return fields.get(name.lexeme);
    }

     LoxFunction method = klass.findMethod(name.lexeme);
    if (method != null) 
    {
        LoxFunction boundMethod = method.bind(this);

        //added getter support challenge question 2 chapter 12.
         if (boundMethod.isGetter()) 
          {
            return boundMethod.call(interpreter, java.util.Collections.emptyList());
          }

    return boundMethod;
    }
     
    throw new RuntimeError(name, "Undefined property '" + name.lexeme + "'.");
  }

  


   void set(Token name, Object value) 
  {
    fields.put(name.lexeme, value);
  }

  @Override
  public String toString() 
  {
    return klass.name + " instance";
  }
}
