
// #region base

#include "fs.h"

#include <iostream>
#include <chrono>
#include <cstring>
#include <fstream>
#include "printc.h"

#define min(a, b) ( a < b ? a : b )
#define max(a, b) ( a > b ? a : b )

uint64 getCurrentTime() {
  const auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}



uint64 getCurrentTimeMicros() {
  const auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
}



Path::Path() {
  strcpy(data, "/");
}

Path::Path(const char* path) {
  strcpy(data, path);
}

char* Path::string() {
  return data;
}

void Path::push(const char* name) {
  if(strcmp(data, "/") != 0) strcat(data, "/");
  strcat(data, name);
}

void Path::pop() {

  char* t = strrchr(data, '/');

  if(t == data || t == NULL) {
    strcpy(data, "/");
    return;
  }

  *t = 0;

}

void Path::set(const char* path) {
  strcpy(data, path);
}

PathSeparator::PathSeparator() {
  bufferCap = 256;
  buffer = new char[bufferCap];
  pName = NULL;
  p = NULL;
}

bool PathSeparator::set(const char* input) {
  
  pName = NULL;
  p = NULL;

  int len = strlen(input);

  if(len + 1 > bufferCap) {
    bufferCap = len + 1;
    delete[] buffer;
    buffer = new char[bufferCap];
  }

  memcpy(buffer, input, len + 1);

  if(buffer[0] != '/') return false;
  p = buffer + 1;

  char* t = strrchr(buffer, '/');
  if(t == buffer) p = NULL;
  *t = 0;
  pName = t + 1;

  return true;

}

bool PathSeparator::hasNext() {
  return p != NULL;
}

char* PathSeparator::next() {

  if(p == NULL) return NULL;

  char* t = strchr(p, '/');
  char* result  = p;

  if(t == NULL) {
    p = NULL;
  }else{
    *t = 0;
    p = t + 1;
  }

  return result;

}

char* PathSeparator::name() {
  return pName;
}

PathSeparator::~PathSeparator() {
  delete[] buffer;
}

// #endregion

// #region FileSystem


FileSystem::FileSystem() {
  capacity = 0;
  memory = NULL;
  /*
  printfc("directory block: %d\n", COLOR_YELLOW, sizeof(DirectoryBlock));
  printfc("  files capacity: %d\n", COLOR_YELLOW, DirectoryBlock::capacity);
  printfc("file info:  %d\n", COLOR_YELLOW, sizeof(FileInfo));
  printfc("file block: %d\n", COLOR_YELLOW, sizeof(FileBlock));
  printfc("  bytes: %d\n", COLOR_YELLOW, FileBlock::capacity);
  */
}

void FileSystem::create(uint32_t capacity) {

  printfc("Creating memory  ( %.1f MB )\n", COLOR_BLUE, MB(capacity));

  this->capacity = capacity;
  memory = new char[capacity];

  format();

}

bool FileSystem::load(const char* file) {

  std::fstream in(file, std::ios::in | std::ios::binary | std::ios::ate);
  if(!in) return false;

  int size = in.tellg();
  in.seekg(0);

  if(size < MB(1)) {
    in.close();
    return false;
  }

  capacity = size;
  memory = new char[size];

  in.read(memory, size);
  in.close();

  return true;

}

void FileSystem::save(const char* file) {
  if(memory == NULL) return;
  std::fstream out(file, std::ios::out| std::ios::binary);
  out.write(memory, capacity);
  out.close();
}





void FileSystem::format() {

  printc("Formating memory\n", COLOR_BLUE);

  HeaderBlock* header = getHeaderBlock();

  header->firstEmptyBlock = 1;
  header->totalBlocks = (capacity - headerSize) / BLOCK_SIZE;
  header->usedBlocks = 1;

  DirectoryBlock* rootDir = getDirectoryBlock(0);

  rootDir->parentDirectory = -1;
  rootDir->previousBlock = -1;
  rootDir->nextBlock = -1;

  rootDir->fileCount = 0;

  /*
  uint32_t unusedCapacity = capacity - headerSize - header->totalBlocks * BLOCK_SIZE;
  printfc("  capacity: %d B  (%.1f MB)\n", COLOR_GREEN, capacity, (float)capacity / (float)(1024*1024));
  printfc("  unused capacity: %d B (%.1f KB)\n", COLOR_GREEN, unusedCapacity, (float)unusedCapacity / (float)(1024));
  printfc("  total blocks: %d\n", COLOR_GREEN, header->totalBlocks);
  printfc("  used blocks: %d\n", COLOR_GREEN, header->usedBlocks);
  */
  
  for(int i = header->firstEmptyBlock; i < header->totalBlocks; i++) {
    
    EmptyBlock* block = getEmptyBlock(i);

    if(i == header->firstEmptyBlock) {
      block->previousBlock = -1;
      block->nextBlock = i + 1;
      continue;
    }

    if(i == header->totalBlocks - 1) {
      block->previousBlock = i - 1;
      block->nextBlock = -1;
      continue;
    }

    block->previousBlock = i - 1;
    block->nextBlock = i + 1;
    
  }

  printc("Formating done\n", COLOR_BLUE);
  
}











bool FileSystem::directoryExist(const char* path) {

  if(!initPathSeparator(path)) return false;
  Directory dir = locateParentDirectory();

  if(!dir.isValid()) {
    printfc("Cannot find parent directory %s\n", COLOR_RED, path);
    return false;
  }

  return dir.directoryExists(ps.name());

}

void FileSystem::createDirectory(const char* path) {

  if(!initPathSeparator(path)) return;
  Directory dir = locateParentDirectory();

  if(!dir.isValid()) {
    printfc("Cannot find parent directory %s\n", COLOR_RED, path);
    return;
  }

  dir.createDirectory(ps.name());

}

bool FileSystem::parentDirectory(char* parentDir, const char* path) {

  if(!initPathSeparator(path)) return false;

  Directory dir = openRootDirectory();
  parentDir[0] = 0;

  while(ps.hasNext()) {
    char* n = ps.next();
    strcat(parentDir, "/");
    strcat(parentDir, n);
    Directory child = dir.openDirectory(n);
    if(child.isValid()) {
       dir = child;
    } else {
      return false;
    }
  }

  if(parentDir[0] == 0) strcpy(parentDir, "/");
  return true;

}


DirectoryIterator FileSystem::directoryIterator(const char* path) {

  if(strcmp(path, "/") == 0) return openRootDirectory().iterator("/");

  if(!initPathSeparator(path)) return DirectoryIterator();
  Directory dir = locateParentDirectory();
  
  if(!dir.isValid()) {
    printfc("Cannot find parent directory %s\n", COLOR_RED, path);
    return DirectoryIterator();
  }

  dir = dir.openDirectory(ps.name());
  if(!dir.isValid()) {
    printfc("Cannot find directory %s\n", COLOR_RED, path);
    return DirectoryIterator();
  }

  return dir.iterator(path);

}


bool FileSystem::fileExist(const char* path) {

  if(!initPathSeparator(path)) return false;
  Directory dir = locateParentDirectory();
  
  if(!dir.isValid()) {
    printfc("Cannot find parent directory %s\n", COLOR_RED, path);
    return false;
  }

  return dir.fileExists(ps.name());

}

File FileSystem::openFile(const char* path, FileOpenMode mode) {

  if(!initPathSeparator(path)) return File();
  Directory dir = locateParentDirectory();
  
  if(!dir.isValid()) {
    printfc("Cannot find parent directory %s\n", COLOR_RED, path);
    return File();
  }

  return dir.openFile(ps.name(), mode);

}




bool FileSystem::renameDirectory(const char* path, const char* name) {

  if(!initPathSeparator(path)) return false;
  Directory dir = locateParentDirectory();
  
  if(!dir.isValid()) {
    printfc("Cannot find parent directory %s\n", COLOR_RED, path);
    return false;
  }

  return dir.renameDirectory(ps.name(), name);

}

bool FileSystem::deleteDirectory(const char* path) {

  if(!initPathSeparator(path)) return false;
  Directory dir = locateParentDirectory();
  
  if(!dir.isValid()) {
    printfc("Cannot find parent directory %s\n", COLOR_RED, path);
    return false;
  }

  return dir.deleteDirectory(ps.name());

}

bool FileSystem::renameFile(const char* path, const char* name) {

  if(!initPathSeparator(path)) return false;
  Directory dir = locateParentDirectory();
  
  if(!dir.isValid()) {
    printfc("Cannot find parent directory %s\n", COLOR_RED, path);
    return false;
  }

  return dir.renameFile(ps.name(), name);

}

bool FileSystem::deleteFile(const char* path) {

  if(!initPathSeparator(path)) return false;
  Directory dir = locateParentDirectory();
  
  if(!dir.isValid()) {
    printfc("Cannot find parent directory %s\n", COLOR_RED, path);
    return false;
  }

  return dir.deleteFile(ps.name());

}







int FileSystem::totalBlocks() {
  return getHeaderBlock()->totalBlocks;
}

int FileSystem::usedBlocks() {
  return getHeaderBlock()->usedBlocks;
}

int FileSystem::freeBlocks() {
  HeaderBlock* header = getHeaderBlock();
  return header->totalBlocks - header->usedBlocks;
}

FileSystem::~FileSystem() {
  if(memory == NULL) return;
  printc("Destroying memory\n", COLOR_RED);
  delete[] memory;
}

bool FileSystem::initPathSeparator(const char* path) {

  bool ok = ps.set(path);
  if(!ok) {
    printfc("Invalid path %s\n", COLOR_RED, path);
    return false;
  }

  return true;

}

Directory FileSystem::locateParentDirectory() {

  Directory dir = openRootDirectory();

  while(ps.hasNext()) {
    char* n = ps.next();
    Directory child = dir.openDirectory(n);
    if(child.isValid()) {
       dir = child;
    } else {
      return Directory();
    }
  }

  return dir;

}

Directory FileSystem::openRootDirectory() {
  return Directory(this, getDirectoryBlock(0));
}

int FileSystem::allocateBlock() {

  HeaderBlock* header = getHeaderBlock();

  int i = header->firstEmptyBlock;
  
  if(i == -1) {
    printc("FATAL ERROR: CANNOT ALLOCATE NEW BLOCK\n", COLOR_RED);
    exit(1);
  }

  header->usedBlocks++;

  EmptyBlock* block = getEmptyBlock(i);
  header->firstEmptyBlock = block->nextBlock;

  EmptyBlock* firstEmptyblock = getEmptyBlock(header->firstEmptyBlock);
  if(firstEmptyblock != NULL) firstEmptyblock->previousBlock = -1;

  return i;

}

void FileSystem::deallocateBlock(int i) {

  HeaderBlock* header = getHeaderBlock();
  header->usedBlocks--;

  EmptyBlock* emptyBlock = getEmptyBlock(i);
  emptyBlock->previousBlock = -1;
  emptyBlock->nextBlock = header->firstEmptyBlock;

  EmptyBlock* secondEmptyBlock = getEmptyBlock(header->firstEmptyBlock);
  if(secondEmptyBlock != NULL) secondEmptyBlock->previousBlock = i;

  header->firstEmptyBlock = i;

}

HeaderBlock* FileSystem::getHeaderBlock() {
  return (HeaderBlock*)memory;
}

DirectoryBlock* FileSystem::getDirectoryBlock(int i) {
  return (DirectoryBlock*)blockAt(i);
}

FileBlock* FileSystem::getFileBlock(int i) {
  return (FileBlock*)blockAt(i);
}

EmptyBlock* FileSystem::getEmptyBlock(int i) {
  return (EmptyBlock*)blockAt(i);
}

char* FileSystem::blockAt(int i) {
  if(i == -1) return NULL;
  return memory + headerSize + i * BLOCK_SIZE;
}

int FileSystem::blockIndex(void* p) {
  if(p == NULL) return -1;
  return ((char*)p - memory - headerSize) / BLOCK_SIZE;
}

// #endregion

// #region Directory

Directory::Directory() {
  fs = NULL;
  block = NULL;
}

Directory::Directory(FileSystem* fs, DirectoryBlock* block) {
  this->fs = fs;
  this->block = block;
}

bool Directory::isValid() {
  return fs != NULL;
}

bool Directory::fileExists(const char* name) {
  return getFileInfo(name, 'F', NULL, NULL) != NULL;
}

void Directory::createFile(const char* name) {

  FileInfo* fileInfo = getFileInfo(name, 'F', NULL, NULL);
  if(fileInfo != NULL) {
    printfc("Cannot create file %s because it already exists\n", COLOR_RED, name);
    return;
  }

  fileInfo = addFileInfo(name, 'F');

  fileInfo->fileSize = 0;
  fileInfo->dateCreated = getCurrentTime();
  fileInfo->dateModified = fileInfo->dateCreated;

  fileInfo->firstBlock = -1;
  fileInfo->lastBlock = -1;

}

File Directory::openFile(const char* name, FileOpenMode mode) {

  FileInfo* fileInfo = getFileInfo(name, 'F', NULL, NULL);

  if(fileInfo == NULL) {

    if(mode == READ) {
      printfc("Cannot open file %s because it does not exists\n", COLOR_RED, name);
      return File();
    }

    fileInfo = addFileInfo(name, 'F');

    fileInfo->fileSize = 0;
    fileInfo->dateCreated = getCurrentTime();
    fileInfo->dateModified = fileInfo->dateCreated;

    fileInfo->firstBlock = -1;
    fileInfo->lastBlock = -1;

  }

  return File(fs, fileInfo, mode);

  /*
  DirectoryBlock* block = this->block;

  while(block != NULL) {

    for(int i = 0; i < block->fileCount; i++) {

      FileInfo* fileInfo = &block->files[i];
      if(fileInfo->fileType != 'F') continue;

      if(strcmp(fileInfo->fileName, name) == 0) {
        return File(fs, fileInfo, mode);
      }

    }

    block = fs->getDirectoryBlock(block->nextBlock);
  }

  if(mode == WRITE || mode == APPEND) {
    
    DirectoryBlock* block = this->block;

    while(block->nextBlock != -1) {
      block = fs->getDirectoryBlock(block->nextBlock);
    }

    if(block->fileCount == block->capacity) {

      int n = fs->allocateBlock();
      DirectoryBlock* newBlock = fs->getDirectoryBlock(n);

      block->nextBlock = n;

      newBlock->parentDirectory = block->parentDirectory;
      newBlock->previousBlock = fs->blockIndex(block);
      newBlock->nextBlock = -1;
      newBlock->fileCount = 0;

      block = newBlock;

    }

    FileInfo* fileInfo = &block->files[block->fileCount];
    block->fileCount++;

    strcpy(fileInfo->fileName, name);
    fileInfo->fileType = 'F';
    fileInfo->fileSize = 0;
    fileInfo->dateCreated = getCurrentTime();
    fileInfo->dateModified = fileInfo->dateCreated;

    fileInfo->firstBlock = -1;
    fileInfo->lastBlock = -1;
    
    return File(fs, fileInfo, mode);

  }
  // create file because it does not exists

  printfc("Cannot open file %s because it does not exists\n", COLOR_RED, name);
  return File();

  */

}

bool Directory::directoryExists(const char* name) {
  return getFileInfo(name, 'D', NULL, NULL) != NULL;
}

void Directory::createDirectory(const char* name) {

  FileInfo* fileInfo = getFileInfo(name, 'D', NULL, NULL);
  if(fileInfo != NULL) {
    printfc("Cannot create directory %s because it already exists\n", COLOR_RED, name);
    return;
  }

  fileInfo = addFileInfo(name, 'D');

  int n = fs->allocateBlock();
  DirectoryBlock* dir = fs->getDirectoryBlock(n);

  dir->parentDirectory = fs->blockIndex(this->block);
  dir->previousBlock = -1;
  dir->nextBlock = -1;
  dir->fileCount = 0;

  fileInfo->dateCreated = getCurrentTime();
  fileInfo->dateModified = fileInfo->dateCreated;

  fileInfo->firstBlock = n;

}

Directory Directory::openDirectory(const char* name) {

  FileInfo* fileInfo = getFileInfo(name, 'D', NULL, NULL);
  if(fileInfo == NULL) {
    printfc("Cannot open directory %s because it does not exists\n", COLOR_RED, name);
    return Directory();
  }

  return Directory(fs, fs->getDirectoryBlock(fileInfo->firstBlock)); 

}

DirectoryIterator Directory::iterator(const char* path) {
  return DirectoryIterator(fs, block, path);
}


bool Directory::deleteFile(const char* name) {

  DirectoryBlock* block;
  int index;
  FileInfo* fileInfo = getFileInfo(name, 'F', &block, &index);

  if(fileInfo == NULL) {
    printfc("Cannot delete file %s because it does not even exist\n", COLOR_RED, name);
    return false;
  }

  FileBlock* fb = fs->getFileBlock(fileInfo->firstBlock);
  while(fb != NULL) {
    FileBlock* t = fb;
    fb = fs->getFileBlock(fb->nextBlock);
    fs->deallocateBlock(fs->blockIndex(t));
  }

  removeFileInfo(block, index);

  return true;

}

bool Directory::renameFile(const char* name, const char* newName) {

  DirectoryBlock* block;
  int index;
  FileInfo* fileInfo = getFileInfo(name, 'F', &block, &index);

  if(fileInfo == NULL) {
    printfc("Cannot rename file %s because it does not exist\n", COLOR_RED, name);
    return false;
  }

  bool exist = fileExists(newName);
  if(exist) {
    printfc("Cannot rename file %s to %s because file with new name already exist\n", COLOR_RED, name, newName);
    return false;
  }

  FileInfo temp = *fileInfo;
  strcpy(temp.fileName, newName);

  removeFileInfo(block, index);
  fileInfo = addFileInfo(newName, 'F');
  *fileInfo = temp;

  return true;

}

bool Directory::deleteDirectory(const char* name) {

  DirectoryBlock* block;
  int index;
  FileInfo* fileInfo = getFileInfo(name, 'D', &block, &index);

  if(fileInfo == NULL) {
    printfc("Cannot delete directory %s because it does not even exist\n", COLOR_RED, name);
    return false;
  }

  DirectoryBlock* dirToDelete = fs->getDirectoryBlock(fileInfo->firstBlock);
  if(dirToDelete->fileCount != 0) {
    printfc("Cannot delete directory %s because it is not empty\n", COLOR_RED, name);
    return false;
  }

  fs->deallocateBlock(fileInfo->firstBlock);
  removeFileInfo(block, index);

  return true;

}

bool Directory::renameDirectory(const char* name, const char* newName) {

  DirectoryBlock* block;
  int index;
  FileInfo* fileInfo = getFileInfo(name, 'D', &block, &index);

  if(fileInfo == NULL) {
    printfc("Cannot rename directory %s because it does not exist\n", COLOR_RED, name);
    return false;
  }

  bool exist = directoryExists(newName);
  if(exist) {
    printfc("Cannot rename directory %s to %s because directory with new name already exist\n", COLOR_RED, name, newName);
    return false;
  }

  FileInfo temp = *fileInfo;
  strcpy(temp.fileName, newName);

  removeFileInfo(block, index);
  fileInfo = addFileInfo(newName, 'D');
  *fileInfo = temp;

  return true;

}










void Directory::print() {

  printf("----------------------------------\n");

  DirectoryBlock* block = this->block;

  while(block != NULL) {

    printf("Directory block %d\n", fs->blockIndex(block));
    printf("  parent dir: %d\n", block->parentDirectory);
    printf("  previous block: %d\n", block->previousBlock);
    printf("  next block: %d\n", block->nextBlock);
    printf("  file count: %d/%d\n", block->fileCount, block->capacity);
    
    for(int i = 0; i < block->fileCount; i++) {

      FileInfo* fileInfo = &block->files[i];
      if(fileInfo->fileType == 'F') {
        printf("  FILE %s (%.1f KB)\n", fileInfo->fileName, KB(fileInfo->fileSize));
      }else if(fileInfo->fileType == 'D') {
        printf("  DIR %s\n", fileInfo->fileName);
      }else{
        printf("  FILE ERROR\n");
      }

    }

    block = fs->getDirectoryBlock(block->nextBlock);

  }

}

int compareFileInfo(FileInfo* a, FileInfo* b) {

  if(a->fileType != b->fileType) {
    if(a->fileType == 'D') return -1;
    return 1;
  }

  return strcmp(a->fileName, b->fileName);

}

int compareWithFileInfo(const char* name, char type, FileInfo* fileInfo) {

  if(type != fileInfo->fileType) {
    if(type == 'D') return -1;
    return 1;
  }

  return strcmp(name, fileInfo->fileName);

}

void swapFileInfo(FileInfo* a, FileInfo* b) {
  FileInfo t = *a;
  *a = *b;
  *b = t;
}

FileInfo* Directory::getFileInfo(const char* name, char type, DirectoryBlock** outBlock, int* outIndex) {

  DirectoryBlock* block = this->block;

  while(block != NULL) {

    int fileCount = block->fileCount;
    if(fileCount == 0) return NULL;

    FileInfo* lastFile = &block->files[fileCount - 1];
    int compare = compareWithFileInfo(name, type, lastFile);

    if(compare <= 0) {

      if(compare == 0) {
        if(outBlock != NULL && outIndex != NULL) {
          *outBlock = block;
          *outIndex = fileCount - 1;
        }
        return lastFile;
      }

      int left = 0;
      int right = fileCount - 1;

      while(left <= right) {

        int middle = (left + right) / 2;
        int compare = compareWithFileInfo(name, type, &block->files[middle]);

        if(compare == 0) {
          if(outBlock != NULL && outIndex != NULL) {
            *outBlock = block;
            *outIndex = middle;
          }
          return &block->files[middle];
        }

        if(compare == -1) right = middle - 1;
        else left = middle + 1;

      }

      return NULL;

    }

    block = fs->getDirectoryBlock(block->nextBlock);

  }

  return NULL;

}

FileInfo* Directory::addFileInfo(const char* name, char type) {

  DirectoryBlock* block = this->block;

  while(block->nextBlock != -1) {
    block = fs->getDirectoryBlock(block->nextBlock);
  }

  if(block->fileCount == block->capacity) {

    int n = fs->allocateBlock();
    DirectoryBlock* newBlock = fs->getDirectoryBlock(n);

    block->nextBlock = n;

    newBlock->parentDirectory = block->parentDirectory;
    newBlock->previousBlock = fs->blockIndex(block);
    newBlock->nextBlock = -1;
    newBlock->fileCount = 0;

    block = newBlock;

  }

  FileInfo* fileInfo = &block->files[block->fileCount];
  block->fileCount++;

  strcpy(fileInfo->fileName, name);
  fileInfo->fileType = type;
  //fileInfo->fileSize = 0;
  //fileInfo->dateCreated = getCurrentTime();
  //fileInfo->dateModified = fileInfo->dateCreated;
  //fileInfo->firstBlock = -1;
  //fileInfo->lastBlock = -1;

  // samo je još potrebno sortirati :)

  /*
  fileInfo = NULL;
  bool sorted = false;

  while(true) {

    for(int i = block->fileCount - 1; i > 0; i--) {

      FileInfo* a = &block->files[i-1];
      FileInfo* b = &block->files[i];
      
      int compare = compareFileInfo(a, b);
      if(compare == -1) {
        sorted = true;
        fileInfo = b;
        break;
      }

      swapFileInfo(a, b);
      
    }

    if(sorted) break;

    fileInfo = fileInfo = &block->files[0];

    DirectoryBlock* previousBlock = fs->getDirectoryBlock(block->previousBlock);
    if(previousBlock == NULL) {
      sorted = true;
      break;
    }
  
    FileInfo* previous = &previousBlock->files[previousBlock->fileCount - 1];
    
    int compare = compareFileInfo(previous, fileInfo);

    if(compare == -1) {
      sorted = true;
      break;
    }

    swapFileInfo(previous, fileInfo);
    fileInfo = previous;

    block = previousBlock;

  }*/
  
  FileInfo* previousFileInfo = NULL;
  // file
  int fileInfoIndex = block->fileCount - 1;
  int previousFileInfoIndex;

  while(true) {

    if(fileInfoIndex != 0) {

      previousFileInfoIndex = fileInfoIndex - 1;
      previousFileInfo = &block->files[previousFileInfoIndex];

    }else{

      block = fs->getDirectoryBlock(block->previousBlock);
      if(block == NULL) break;

      previousFileInfoIndex = block->fileCount - 1;
      previousFileInfo = &block->files[previousFileInfoIndex];

    }
    
    int compare = compareFileInfo(previousFileInfo, fileInfo);
    
    if(compare == -1) break;

    swapFileInfo(previousFileInfo, fileInfo);

    fileInfo = previousFileInfo;
    fileInfoIndex = previousFileInfoIndex;

  };

  return fileInfo;

}

void Directory::removeFileInfo(DirectoryBlock* block, int index) {

  while(block != NULL) {

    for(int i = index; i < block->fileCount - 1; i++) {
      block->files[i] = block->files[i + 1];
    }

    DirectoryBlock* nextBlock = fs->getDirectoryBlock(block->nextBlock);

    if(nextBlock != NULL) {
      block->files[block->fileCount - 1] = nextBlock->files[0];
      index = 0;
    }else {
      block->fileCount--;
      if(block->fileCount == 0) {

        DirectoryBlock* previousBlock = fs->getDirectoryBlock(block->previousBlock);

        if(previousBlock != NULL) {
          fs->deallocateBlock(fs->blockIndex(block));
          previousBlock->nextBlock = -1;
        }

      }
    }

    block = nextBlock;

  }

}


// #endregion

// #region File

#define FILE_BLOCK_CAPACITY FileBlock::capacity

File::File() {
  fs = NULL;
  info = NULL;
}

File::File(FileSystem* fs, FileInfo* info, FileOpenMode mode) {

  this->fs = fs;
  this->info = info;
  this->mode = mode;

  switch (mode) {
    case READ:
    case WRITE:
      pos = 0;
      break;
    case APPEND:
      pos = info->fileSize;
      break;
  }

  cachedFileBlock = fs->getFileBlock(info->firstBlock);
  cachedBlockStartPos = 0;
  cachedBlockEndPos = FILE_BLOCK_CAPACITY - 1;

}

bool File::isValid() {
  return fs != NULL;
}

char* File::name() {
  return info->fileName;
}

int File::size() {
  return info->fileSize;
}

void File::setPosition(int pos) {
  if(pos < 0) pos = 0;
  else if(pos > info->fileSize) pos = info->fileSize;
  this->pos = pos;
}

int File::getPosition() {
  return pos;
}

void File::write(char* bytes, int len) {

  int written = 0;
  int remain = len;

  while(remain != 0) {

    int blockPos;
    FileBlock* block = blockAt(pos, &blockPos);

    int toWrite = min(FILE_BLOCK_CAPACITY - blockPos, remain);
    char* p = &block->fileData[blockPos];
    memcpy(p, bytes + written, toWrite);

    pos += toWrite;
    written += toWrite;
    remain -= toWrite;

  }

}

int File::read(char* bytes, int len) {

  int maxAllowed = info->fileSize - pos;
  if(len > maxAllowed) len = maxAllowed;

  int read = 0;
  int remain = len;

  while(remain != 0) {

    int blockPos;
    FileBlock* block = blockAt(pos, &blockPos);

    int toRead = min(FILE_BLOCK_CAPACITY - blockPos, remain);
    char* p = &block->fileData[blockPos];
    memcpy(bytes + read, p, toRead);

    pos += toRead;
    read += toRead;
    remain -= toRead;

  }

  return read;

}

void File::close() {

  if(mode == WRITE || mode == APPEND) {

    int blockPos;
    FileBlock* block = blockAt(pos, &blockPos);

    FileBlock* nb = fs->getFileBlock(block->nextBlock);
    while(nb != NULL) {
      int t = fs->blockIndex(nb);
      nb = fs->getFileBlock(nb->nextBlock);
      fs->deallocateBlock(t);
    }

    if(blockPos == 0) {
      int t = fs->blockIndex(block);
      block = fs->getFileBlock(block->previousBlock);
      fs->deallocateBlock(t);
    }

    if(block == NULL) info->firstBlock = -1;
    else block->nextBlock = -1;

    info->lastBlock = fs->blockIndex(block);
    info->fileSize = pos;

  }

}

void File::testfileblockat() {
  
  int len = info->fileSize;
  uint64_t TSTART, TEND;

  TSTART = getCurrentTimeMicros();
  {
    int maxAllowed = info->fileSize - pos;
    if(len > maxAllowed) len = maxAllowed;

    int read = 0;
    int remain = len;

    while(remain != 0) {

      int blockPos;
      FileBlock* block = blockAt(pos, &blockPos);

      int toRead = min(FILE_BLOCK_CAPACITY - blockPos, remain);

      pos += toRead;
      read += toRead;
      remain -= toRead;

    }
  }
  TEND = getCurrentTimeMicros();
  printf("%u us\n", TEND - TSTART);


}

FileBlock* File::blockAt(int pos, int* blockPos) {

  FileBlock* block = cachedFileBlock;
  FileBlock* last = fs->getFileBlock(block->previousBlock);
  
  int blockStartPos = cachedBlockStartPos;
  int blockEndPos = cachedBlockEndPos;

  while(true) {

    if(block == NULL) {

      if(mode == READ) {
        // NE SMIJE SE DOGODITI
        printfc("FATAL ERROR: FILE READ BLOCK AT %d DOES NOT EXIST\n", COLOR_RED, pos);
        exit(1);
      }

      int n = fs->allocateBlock();
      FileBlock* newBlock = fs->getFileBlock(n);

      if(last == NULL) info->firstBlock = n;
      else last->nextBlock = n;

      newBlock->previousBlock = fs->blockIndex(last);
      newBlock->nextBlock = -1;

      info->lastBlock = n;
      info->fileSize = pos;

      block = newBlock;
      
    }

    if(pos >= blockStartPos && pos <= blockEndPos) break;
    
    if(pos < blockStartPos) {

      blockStartPos -= FILE_BLOCK_CAPACITY;
      blockEndPos -= FILE_BLOCK_CAPACITY;

      block = last;

      if(block == NULL) {
        printc("FATAL ERROR: POS < BLOCK_START_POS && BLOCK == NULL\n", COLOR_RED);
        exit(1);
      }

      last = fs->getFileBlock(block->previousBlock);

    }else if(pos > blockEndPos) {
      
      blockStartPos += FILE_BLOCK_CAPACITY;
      blockEndPos += FILE_BLOCK_CAPACITY;

      last = block;
      block = fs->getFileBlock(block->nextBlock);

    }else{
      printc("FATAL ERROR: HOW CAN THIS EVEN HAPPEN\n", COLOR_RED);
      exit(1);
    }

  }

  cachedFileBlock = block;
  cachedBlockStartPos = blockStartPos;
  cachedBlockEndPos = blockEndPos;

  *blockPos = pos - blockStartPos;
  return block;

}

FileBlock* File::blockAtDeprecated(int pos, int* blockPos) {

  FileBlock* block = fs->getFileBlock(info->firstBlock);
  FileBlock* last = NULL;
  
  int i = FILE_BLOCK_CAPACITY;

  while(true) {

    if(block == NULL) {

      if(mode == READ) {
        // NE SMIJE SE DOGODITI
        printfc("FATAL ERROR: FILE READ BLOCK AT %d DOES NOT EXIST\n", COLOR_RED, pos);
        exit(1);
      }

      int n = fs->allocateBlock();
      FileBlock* newBlock = fs->getFileBlock(n);

      if(last == NULL) info->firstBlock = n;
      else last->nextBlock = n;

      newBlock->previousBlock = fs->blockIndex(last);
      newBlock->nextBlock = -1;

      info->lastBlock = n;
      info->fileSize = pos;

      block = newBlock;
      
    }

    if(pos < i) break;
    i += FILE_BLOCK_CAPACITY;

    last = block;
    block = fs->getFileBlock(block->nextBlock);

  }
  
  *blockPos = FILE_BLOCK_CAPACITY - (i - pos);
  return block;
  

}


void File::dump() {

  FileBlock* fileBlock = fs->getFileBlock(info->firstBlock);
  int a = 0;
  while(fileBlock != NULL) {
    printf("FILEBLOCK %d\n", fs->blockIndex(fileBlock));
    printf("  next %d\n", fileBlock->nextBlock);
    printf("  prev %d\n", fileBlock->previousBlock);
    printf("  data #");
    for(int i = 0; i < fileBlock->capacity && a < info->fileSize; i++) {
      printf("%c", fileBlock->fileData[i]);
      a++;
    }
    printf("#\n");
    fileBlock = fs->getFileBlock(fileBlock->nextBlock);
  }

}

// #endregion

// #region Iterator

DirectoryIterator::DirectoryIterator() {
  _hasItems = false;
}

DirectoryIterator::DirectoryIterator(FileSystem* fs, DirectoryBlock* block, const char* path) {
  
  this->fs = fs;
  this->block = block;
  this->path.set(path);

  if(block->fileCount == 0) {
    _hasItems = false;
    return;
  }

  fileInfo = &block->files[0];
  index = 0;
  _hasItems = true;

}

char* DirectoryIterator::directoryPath() {
  return path.string();
}

char* DirectoryIterator::name() {
  return fileInfo->fileName;
}

char DirectoryIterator::type() {
  return fileInfo->fileType;
}

int DirectoryIterator::fileSize() {
  return fileInfo->fileSize;
}

uint64 DirectoryIterator::dateCreated() {
  return fileInfo->dateCreated;
}

uint64 DirectoryIterator::dateModified() {
  return fileInfo->dateModified;
}

bool DirectoryIterator::hasItems() {
  return _hasItems;
}

void DirectoryIterator::nextItem() {

  if(!_hasItems) return;

  index++;
  if(index == block->fileCount) {

    block = fs->getDirectoryBlock(block->nextBlock);
    if(block == NULL) {
      _hasItems = false;
      return;
    }

    if(block->fileCount == 0) {
      _hasItems = false;
      return;
    }

    index = 0;

  }

  fileInfo = &block->files[index];

}

// #endregion