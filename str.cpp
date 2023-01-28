
#include "str.h"
#include <cstring>

StringSplitter::StringSplitter() {
  input = NULL;
  inputCap = 0;
  p = NULL;
}

void StringSplitter::set(const char* str, char x) {

  int len = strlen(str);

  if(input == NULL) {
    inputCap = len + 1;
    input = new char[inputCap];
  }else{
    if(len + 1 > inputCap) {
      delete[] input;
      inputCap = len + 1;
      input = new char[inputCap];
    }
  }
  
  memcpy(input, str, len + 1);

  p = input;
  this->x = x;

}

bool StringSplitter::hasNext() {
  return p != NULL;
}

char* StringSplitter::next() {

  char* t = strchr(p, x);
  char* result = p;

  if(t == NULL) {
    p = NULL;
  }else{
    *t = 0;
    p = t + 1;
  }

  return result;

}

char* StringSplitter::all() {
  return p;
}

StringSplitter::~StringSplitter() {
  if(input != NULL) delete[] input;
}
