
#include <iostream>
#include <cstring>
#include <fstream>
#include "fs.h"
#include "printc.h"

#define min(a, b) ( a < b ? a : b )
#define max(a, b) ( a > b ? a : b )

#define streq(a, b) (strcmp(a, b) == 0)

class StringSplitter {

  private:

  char* input;
  int inputCap;
  char* p;
  char x;

  public:

  StringSplitter() {
    input = NULL;
    inputCap = 0;
    p = NULL;
  }

  void set(const char* str, char x = ' ') {
      
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
  
  bool hasNext() {
    return p != NULL;
  }

  char* next() {

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

  char* all() {
    return p;
  }

  ~StringSplitter() {
    if(input != NULL) delete[] input;
  }

};

class Input {

  private:

  static const int inputBufferCapacity = 100;
  char inputBuffer[inputBufferCapacity];

  StringSplitter splitter;

  public:

  void read() {
    std::cin.getline(inputBuffer, inputBufferCapacity);
    splitter.set(inputBuffer);
  }

  bool hasNext() {
    return splitter.hasNext();
  }

  char* next() {
    if(!splitter.hasNext()) throw 0;
    return splitter.next();
  }

  char* p() {
    return splitter.all();
  }

};

char* cap(int size) {
  
  static char buffer[256];

  if(size < 1024) sprintf(buffer, "%d B", size);
  else if(size < 1024 * 1024) sprintf(buffer, "%.1f KB", KB(size));
  else sprintf(buffer, "%.1f MB", MB(size));

  return buffer;

}

char* date(uint64 date) {

  static char buffer[256];

  time_t time_t_data;

  time_t time_time = (time_t)date;
  tm* time_tm = localtime(&time_time);

  strftime(buffer, 256, "%d-%m-%Y %H:%M:%S", time_tm);
  return buffer;

}

void listDirectory(FileSystem &fs, const char* path) {

  DirectoryIterator it = fs.directoryIterator(path);
  printfc("%-32s | %-4s | %-20s | %-20s | %s\n", COLOR_GREEN, "name", "type", "date created", "date modified", "size");

  while(it.hasItems()) {

    char type = it.type();
    if(type == 'D') {
      printfc("%-32s | %-4s | %-20s | %-20s | %s\n", COLOR_YELLOW, it.name(), "dir", date(it.dateCreated()), "", "");
    }else{
      printfc("%-32s | %-4s | %-20s | %-20s | %s\n", COLOR_YELLOW, it.name(), "file", date(it.dateCreated()), date(it.dateModified()), cap(it.fileSize()));
    }

    it.nextItem();

  }

}

void listDirectoryTree(FileSystem &fs, DirectoryIterator it, int level = 1) {

  while(it.hasItems()) {

    for(int i = 0; i < level; i++) printf("    ");
    
    if(it.type() == 'D') {

      printfc("%s\n", COLOR_MAGENTA, it.name());

      Path path(it.directoryPath());
      path.push(it.name());
      
      listDirectoryTree(fs, fs.directoryIterator(path.string()), level + 1);

    }else{

      printfc("%s\n", COLOR_BLUE, it.name());

    }

    it.nextItem();

  }

}

void openFile(FileSystem &fs, const char* path, const char* name) {

  char fileName[256];
  sprintf(fileName, "__%s", name);

  File file = fs.openFile(path, READ);
  if(!file.isValid()) return;

  int len = file.size();
  char* buffer = new char[len];
  
  file.read(buffer, len);
  file.close();

  std::fstream out(fileName, std::ios::out | std::ios::binary);
  if(!out) return;

  out.write(buffer, len);
  out.close();

  delete[] buffer;

  char cmdBuffer[256];
  sprintf(cmdBuffer, "explorer %s", fileName);
  system(cmdBuffer);

  system("pause");

  std::fstream in(fileName, std::ios::in | std::ios::binary | std::ios::ate);
  if(!in) return;

  len = in.tellg();
  in.seekg(0);
  
  buffer = new char[len];
  in.read(buffer, len);
  in.close();

  file = fs.openFile(path, WRITE);
  if(!file.isValid()) return;

  file.write(buffer, len);
  file.close();

  delete[] buffer;
  remove(fileName);

}

int main() {

  // g++ main.cpp fs.cpp -o app -D USE_PRINTFC ; if($?) { ./app }

  FileSystem fs;

  bool ok = fs.load("storage.fs");
  if(!ok) fs.create(32 * 1024 * 1024);

  Input input;
  Path currentPath;

  while(true) {

    printfc("%s$ ", COLOR_GREEN, currentPath.string());

    try {

      input.read();
      char* cmd = input.next();

      if(streq(cmd, "exit")) {
        break;
      } else if(streq(cmd, "status")) {

        int total = fs.totalBlocks();
        int used = fs.usedBlocks();
        int free = fs.freeBlocks();

        printfc("total blocks: %-5d ( %s )\n", COLOR_BLUE, total, cap(total * BLOCK_SIZE));
        printfc("used  blocks: %-5d  %.1f %c ( %s )\n", COLOR_BLUE, used, (float)used / (float)total * 100.0, '%', cap(used * BLOCK_SIZE));
        printfc("free  blocks: %-5d  %.1f %c ( %s )\n", COLOR_BLUE, free, (float)free / (float)total * 100.0, '%', cap(free * BLOCK_SIZE));
        
      } else if(streq(cmd, "mkdir")) {

        char* name = input.next();

        Path path = currentPath;
        path.push(name);

        printfc("Creating directory %s\n", COLOR_BLUE, path.string());
        fs.createDirectory(path.string());

      } else if(streq(cmd, "ls")) {

        listDirectory(fs, currentPath.string());

      } else if(streq(cmd, "tree")) {

        printfc("%s\n", COLOR_MAGENTA, currentPath.string());
        listDirectoryTree(fs, fs.directoryIterator(currentPath.string()));

      } else if(streq(cmd, "write")) {

        char* name = input.next();

        Path path = currentPath;
        path.push(name);

        File file = fs.openFile(path.string(), WRITE);
        if(!file.isValid()) continue;

        if(input.hasNext()) {

          char* p = input.p();

          printfc("Writing %s\n", COLOR_GREEN, p);
          file.write(p, strlen(p));

          char c = '\n';
          file.write(&c, 1);

        }

        file.close();

      } else if(streq(cmd, "app")) {

        char* name = input.next();

        Path path = currentPath;
        path.push(name);

        File file = fs.openFile(path.string(), APPEND);
        if(!file.isValid()) continue;

        if(input.hasNext()) {

          char* p = input.p();

          printfc("Appending %s\n", COLOR_GREEN, p);
          file.write(p, strlen(p));

          char c = '\n';
          file.write(&c, 1);

        }

        file.close();

      } else if(streq(cmd, "read")) {

        char* name = input.next();

        Path path = currentPath;
        path.push(name);

        File file = fs.openFile(path.string(), READ);
        if(!file.isValid()) continue;

        int size = file.size();
        char* buffer = new char[size + 1];

        file.read(buffer, size);
        file.close();

        buffer[size] = 0;
        printfc("%s", COLOR_BLUE, buffer);

        delete[] buffer;

      } else if(streq(cmd, "cd")) {

        char* name = input.next();

        if(streq(name, "..")) {
          currentPath.pop();
          continue;
        }

        Path path = currentPath;
        path.push(name);

        bool ok = fs.directoryExist(path.string());
        if(!ok) {
          printfc("Directory %s does not exist\n", COLOR_RED, path.string());
          continue;
        }

        currentPath = path;

      } else if(streq(cmd, "appb")) {

        char* name = input.next();
        int bytes = atoi(input.next());
        char* x = input.next();

        if(bytes < 1) continue;

        Path path = currentPath;
        path.push(name);

        char* buffer = new char[bytes];
        memset(buffer, x[0], bytes);

        File file = fs.openFile(path.string(), APPEND);
        file.write(buffer, bytes);
        file.close();

        printfc("Appended %s bytes\n", COLOR_BLUE, bytes);
        delete[] buffer;

      } else if(streq(cmd, "upload")) {

        char* name = input.next();

        std::fstream inputFile(name, std::ios::in | std::ios::binary | std::ios::ate);
        if(!inputFile) {
          printfc("Upload failed: file %s does not exist\n", COLOR_RED, name);
          continue;
        }

        int inputFileSize = inputFile.tellg();
        inputFile.seekg(0);

        char* inputBytes = new char[inputFileSize];
        inputFile.read(inputBytes, inputFileSize);
        inputFile.close();

        Path path = currentPath;
        path.push(name);

        File file = fs.openFile(path.string(), WRITE);
        if(!file.isValid()) {
          printfc("Upload failed: cannot write file %s\n", COLOR_RED, path.string());
          delete[] inputBytes;
          continue;
        }

        file.write(inputBytes, inputFileSize);
        file.close();

        ///////////////////////////////

        file = fs.openFile(path.string(), READ);
        if(!file.isValid()) {
          printfc("Upload failed: cannot read file %s\n", COLOR_RED, path.string());
          delete[] inputBytes;
          continue;
        }

        int fileSize = file.size();
        if(fileSize != inputFileSize) {
          printc("Upload failed: data integrity check\n", COLOR_RED);
          file.close();
          delete[] inputBytes;
          continue;
        }

        char* buffer = new char[fileSize];
        file.read(buffer, fileSize);
        file.close();

        int compare = memcmp(inputBytes, buffer, fileSize);
        if(compare == 0) printfc("File %s uploaded successfully to %s\n", COLOR_BLUE, name, path.string());
        else printc("Upload failed: data integrity check\n", COLOR_RED);

        delete[] inputBytes;
        delete[] buffer;

      } else if(streq(cmd, "open")) {
        
        char* name = input.next();

        Path path = currentPath;
        path.push(name);

        openFile(fs, path.string(), name);
        
      } else if(streq(cmd, "rm")) {
        
        char* name = input.next();

        Path path = currentPath;
        path.push(name);

        fs.deleteFile(path.string());

      } else if(streq(cmd, "rmdir")) {

        char* name = input.next();

        Path path = currentPath;
        path.push(name);

        fs.deleteDirectory(path.string());
        
      } else if(streq(cmd, "rename")) {

        char* name = input.next();
        char* newName = input.next();

        Path path = currentPath;
        path.push(name);

        fs.renameFile(path.string(), newName);

      } else if(streq(cmd, "renamedir")) {

        char* name = input.next();
        char* newName = input.next();

        Path path = currentPath;
        path.push(name);

        fs.renameDirectory(path.string(), newName);

      } else if(streq(cmd, "pdir")) {

        Path parent;
        bool ok = fs.parentDirectory(&parent, currentPath.string());
        if(ok) printfc("Parent dir: %s\n", COLOR_BLUE, parent.string());

      }

    } catch (int x) {
      printc("Wrong usage of command\n", COLOR_RED);
    }

  }

  fs.save("storage.fs");
  return 0;

}
