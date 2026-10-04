package com.craftinginterpreters.lox;

import java.util.Map;

class LoxMixin 
{
  final String name;
  final Map<String, LoxFunction> methods;

  LoxMixin(String name, Map<String, LoxFunction> methods) 
  {
    this.name = name;
    this.methods = methods;
  }

  @Override
  public String toString() 
  {
    return "<mixin " + name + ">";
  }
}