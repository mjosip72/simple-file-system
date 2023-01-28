
#pragma once

#define streq(a, b) (strcmp(a, b) == 0)

class StringSplitter {

  private:
  char* input;
  int inputCap;
  char* p;
  char x;

  public:

  StringSplitter();

  void set(const char* str, char x = ' ');
  
  bool hasNext();
  char* next();
  char* all();

  ~StringSplitter();

};
