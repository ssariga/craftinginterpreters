package com.craftinginterpreters.lox;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

class Environment 
{
  final Environment enclosing;
  private final Map<String, Object> values = new HashMap<>();
  private final List<Object> locals = new ArrayList<>(); // provides local variable lookup via index support, challenge question 4, chapter 11.

  //find a token, if value is found return the value bound to it
  //otherwise return a syntax error.
  Object get(Token name) 
  {
    if (values.containsKey(name.lexeme)) 
    {
      return values.get(name.lexeme);
    }

    if (enclosing != null) 
      return enclosing.get(name);


    throw new RuntimeError(name, "Undefined variable '" + name.lexeme + "'.");
  }

  void assign(Token name, Object value) 
  {
    if (values.containsKey(name.lexeme)) 
    {
      values.put(name.lexeme, value);
      return;
    }

     if (enclosing != null) 
      {
      enclosing.assign(name, value);
      return;
     }

    throw new RuntimeError(name, "Undefined variable '" + name.lexeme + "'.");
  }

  //binds a name to a value
  void define(String name, Object value) 
  {
    values.put(name, value);
  }


  void defineAt(int index, Object value) 
{
  while (locals.size() <= index) 
  {
    locals.add(null);
  }

  locals.set(index, value);
}

  Object getAt(int distance, String name) 
  {
    return ancestor(distance).values.get(name);
  }


  void assignAt(int distance, Token name, Object value) 
  {
    ancestor(distance).values.put(name.lexeme, value);
  }

  //This walks a fixed number of hops up the parent chain 
  // and returns the environment there
  Environment ancestor(int distance) 
  {
    Environment environment = this;
    for (int i = 0; i < distance; i++) 
    {
      environment = environment.enclosing; 
    }

    return environment;
  }

  Environment() 
  {
    enclosing = null;
  }

  Environment(Environment enclosing) 
  {
    this.enclosing = enclosing;
  }


  //set and get by index, challnge question 4, chapter 11.
  Object getAt(int distance, int index) 
{
  Environment environment = ancestor(distance);
  return environment.locals.get(index);
}


void assignAt(int distance, int index, Object value) 
{
  Environment environment = ancestor(distance);
  environment.locals.set(index, value);
}


}

