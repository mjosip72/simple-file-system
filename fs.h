
#pragma once

#include <inttypes.h>

#define KB(x) ((float)(x)/1024.0)
#define MB(x) ((float)(x)/(1024.0*1024.0))

#define BLOCK_SIZE 4096

typedef uint32_t uint;
typedef uint64_t uint64;

class Path {

  private:
  static const int maxSize = 255;
  char data[maxSize + 1];

  public:

  Path();
  Path(const char* path);

  char* string();
  void push(const char* name);
  void pop();
  void set(const char* path);

};

class PathSeparator {

  private:
  
  int bufferCap;
  char* buffer;
  char* pName;
  char* p;

  public:

  PathSeparator();

  bool set(const char* input);
  bool hasNext();
  char* next();
  char* name();

  ~PathSeparator();

};

struct FileInfo {

  char fileName[32];
  char fileType;

  uint fileSize;
  uint64 dateCreated;
  uint64 dateModified;

  int firstBlock;
  int lastBlock;

};

struct FileBlock {

  int previousBlock;
  int nextBlock;

  static const int capacity = BLOCK_SIZE - 8;
  char fileData[capacity];

};

struct EmptyBlock {
  int previousBlock;
  int nextBlock;
};

struct HeaderBlock {

  uint totalBlocks;
  uint usedBlocks;

  int firstEmptyBlock;

};

struct DirectoryBlock {

  int parentDirectory;
  int previousBlock;
  int nextBlock;

  uint fileCount;

  static const int capacity = (BLOCK_SIZE - 8) / sizeof(FileInfo);
  FileInfo files[capacity];

};

class FileSystem;
class Directory;
class File;
class DirectoryIterator;
class PathSeparator;

enum FileOpenMode {
  WRITE, READ, APPEND
};

class FileSystem {

  friend Directory;
  friend File;
  friend DirectoryIterator;

  private:
  static const int headerSize = sizeof(HeaderBlock);

  uint32_t capacity;
  char* memory;

  PathSeparator ps;
  
  public:

  FileSystem();

  void create(uint32_t capacity);
  bool load(const char* file);
  void save(const char* file);

  void format();

  bool directoryExist(const char* path);
  bool createDirectory(const char* path);
  bool parentDirectory(Path* parentDir, const char* path);
  DirectoryIterator directoryIterator(const char* path);

  bool fileExist(const char* path);
  File openFile(const char* path, FileOpenMode mode);

  bool renameDirectory(const char* path, const char* name);
  bool deleteDirectory(const char* path);
  bool renameFile(const char* path, const char* name);
  bool deleteFile(const char* path);


  int totalBlocks();
  int usedBlocks();
  int freeBlocks();

  ~FileSystem();

  private:

  bool initPathSeparator(const char* path);
  Directory locateParentDirectory();
  Directory locateParentDirectoryFromPath(const char* path);

  Directory openRootDirectory();

  int allocateBlock();
  void deallocateBlock(int i);

  HeaderBlock* getHeaderBlock();
  DirectoryBlock* getDirectoryBlock(int i);
  FileBlock* getFileBlock(int i);
  EmptyBlock* getEmptyBlock(int i);

  char* blockAt(int i);
  int blockIndex(void* p);

};

class Directory {

  private:
  FileSystem* fs;
  DirectoryBlock* block;

  public:

  Directory(FileSystem* fs, DirectoryBlock* block);
  Directory();

  bool isValid();

  bool fileExists(const char* name);
  File openFile(const char* name, FileOpenMode mode);

  bool directoryExists(const char* name);
  bool createDirectory(const char* name);
  Directory openDirectory(const char* name);

  DirectoryIterator iterator(const char* path);

  bool deleteFile(const char* name);
  bool renameFile(const char* name, const char* newName);
  
  bool deleteDirectory(const char* name);
  bool renameDirectory(const char* name, const char* newName);

  private:

  FileInfo* getFileInfo(const char* name, char type, DirectoryBlock** outBlock, int* outIndex);
  FileInfo* addFileInfo(const char* name, char type);
  void removeFileInfo(DirectoryBlock* block, int index);

};

class File {

  private:
  FileSystem* fs;
  FileInfo* info;
  FileOpenMode mode;
  int pos;

  public:

  File(FileSystem* fs, FileInfo* info, FileOpenMode mode);
  File();

  bool isValid();

  char* name();
  int size();

  void setPosition(int pos);
  int getPosition();

  void write(char* bytes, int len);
  int read(char* bytes, int len);

  void close();

  private:

  FileBlock* cachedFileBlock;
  int cachedBlockStartPos;
  int cachedBlockEndPos;

  FileBlock* blockAt(int pos, int* blockPos);

};

class DirectoryIterator {

  private:

  FileSystem* fs;
  DirectoryBlock* block;
  FileInfo* fileInfo;
  int index;
  Path path;
  bool _hasItems;

  public:

  DirectoryIterator();
  DirectoryIterator(FileSystem* fs, DirectoryBlock* block, const char* path);

  char* directoryPath();
  char* name();
  char type();
  int fileSize();
  uint64 dateCreated();
  uint64 dateModified();

  bool hasItems();
  void nextItem();

};
