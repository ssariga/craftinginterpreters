package com.craftinginterpreters.lox;

class ResolvedLocal 
{
  final int distance;
  final int index;

  //distance says how many enclosing environments away the variable lives.
  // index says the variable’s position in that environment’s local-value array.
  ResolvedLocal(int distance, int index) 
  {
    this.distance = distance;
    this.index = index;
  }
}