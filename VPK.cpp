//
// Created by joe on 22/08/18.
//

#include <sstream>
#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <iomanip>

#include "VPK.h"

// reads a std::string from the given memory area, adjusting the pointer position
std::string readString(char **character) {
  std::string token = "";
  while (**character != '\0') {
    token += **character;
    (*character)++;
  }
  (*character)++;
  return token;
}

// ask filesystem for size of file
size_t getFileSize(std::string filename) {
  struct stat filestat;
  stat(filename.c_str(), &filestat);
  return filestat.st_size;
}

// check if std::string ends with other string
bool endsWith(std::string const &value, std::string const &ending) {
  if (ending.size() > value.size()) return false;
  return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

// template for splitting a string
template<typename Out>
void split(const std::string &s, char delim, Out result) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        *(result++) = item;
    }
}

// split string into vector of strings
std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, std::back_inserter(elems));
    return elems;
}

// return path in VPK archive, creating if neccecary
Path *createPath(std::string path, VPKArchive *archive) {
  std::vector<std::string> pathparts = split(path, '/');
  Path *thispath;
  if (archive->paths.count(pathparts[0]) == 0) {
    thispath = &archive->paths[pathparts[0]];
    thispath->name = pathparts[0];
  } else {
    thispath = &archive->paths[pathparts[0]];
  }

  for (int i = 1; i < pathparts.size(); i++) {
    if (thispath->directories.count(pathparts[i]) == 0) {
      thispath = &thispath->directories[pathparts[i]];
      thispath->name = pathparts[i];
    } else {
      thispath = &thispath->directories[pathparts[i]];
    }
  }
  return thispath;
}

// return path in VPK archive, returns null if not found
Path *findPath(std::string path, VPKArchive *archive) {
  std::vector<std::string> pathparts = split(path, '/');
  Path *thispath;
  if (archive->paths.count(pathparts[0]) == 0) {
    return nullptr;
  } else {
    thispath = &archive->paths[pathparts[0]];
  }

  for (int i = 1; i < pathparts.size(); i++) {
    if (thispath->directories.count(pathparts[i]) == 0) {
      return nullptr;
    } else {
      thispath = &thispath->directories[pathparts[i]];
    }
  }
  return thispath;
}

// returns file in VPK archive, returns null if not found
Entry *findFile(std::string path, VPKArchive *archive) {
  std::vector<std::string> pathparts = split(path, '/');
  Path *thispath;
  if (archive->paths.count(pathparts[0]) == 0) {
    return nullptr;
  } else {
    thispath = &archive->paths[pathparts[0]];
  }

  for (int i = 1; i < pathparts.size()-1; i++) {
    if (thispath->directories.count(pathparts[i]) == 0) {
      return nullptr;
    } else {
      thispath = &thispath->directories[pathparts[i]];
    }
  }

  if (thispath->files.count(pathparts[pathparts.size()-1]) == 0) {
    return nullptr;
  } else {
    return &thispath->files[pathparts[pathparts.size()-1]];
  }
}

// check if file is split across tree and archive
bool isSplit(Entry *entry) {
  return entry->entry->PreloadBytes > 0 && entry->entry->EntryLength > 0;
}

// utility method to read the archive data of an Entry
void readArchive(VPKArchive *archive, Entry *entry, char *buffer, size_t size, off_t offset) {
  if (size+offset > entry->entry->EntryLength)
      throw std::runtime_error("Reading outside of file archive data");
  int archiveno = entry->entry->ArchiveIndex;
  std::cout << "  archive read number: " << archiveno << " fileoffset: " << entry->entry->EntryOffset << " size: " << size << std::endl;
  char *archiveStart = archiveno == 0x7fff ? reinterpret_cast<char*>(archive->vpkarchivesection) : reinterpret_cast<char*>(archive->archivedata[archiveno]);
  char *fileStart = archiveStart + entry->entry->EntryOffset;
  char *readStart = fileStart + offset;
  memcpy(buffer, readStart, size);
}

// utility method to read the preloaded tree data of an Entry
void readPreload(Entry* entry, char *buffer, size_t size, off_t offset) {
  std::cout << "  preload read index: " << offset << " size: " << size << std::endl;
  if (size+offset > entry->entry->PreloadBytes)
    throw std::runtime_error("Reading outside of file preload data");
  char *preloadStart = reinterpret_cast<char*>(entry->entry) + sizeof(VPKDirectoryEntry);
  char *readStart = preloadStart + offset;
  memcpy(buffer, readStart, size);
}

// read data from a file in a VPK archive, reading from preload and bulk archive data as needed
int readFile(VPKArchive *archive, Entry *entry, char *buffer, size_t size, off_t offset) {
  int preloadsize = entry->entry->PreloadBytes;
  int archivesize = entry->entry->EntryLength;
  if (offset > preloadsize + archivesize) return 0;
  if (size + offset > preloadsize + archivesize) size = preloadsize + archivesize - offset;
  if (offset < preloadsize) {             // read starts in preload
    if (offset + size <= preloadsize) {     // read ends in preload
      std::cout << "preload read: " << entry->name << std::endl;
      readPreload(entry, buffer, size, offset);
      return size;
    } else {                                // read ends in archive
      std::cout << "split read: " << entry->name << std::endl;
      int preloadreadlen = preloadsize - offset;
      readPreload(entry, buffer, preloadreadlen, offset);
      readArchive(archive, entry, buffer + preloadreadlen, size-preloadreadlen, 0);
      return size;
    }
  } else {                                // read starts and ends in archive
    std::cout << "archive read: " << entry->name << std::endl;
    readArchive(archive, entry, buffer, size, offset);
    return size;
  }
}

// return the size in bytes of an entry
int getSize(Entry* entry) {
  return entry->entry->PreloadBytes + entry->entry->EntryLength;
}

// load a VPK archive from disk
VPKArchive *loadFile(std::string filename) {

  // mmap file
  struct stat filestat;
  stat(filename.c_str(), &filestat);
  if (filestat.st_mode & S_IFDIR)
    throw std::runtime_error("Tried to open directory");
  size_t vpksize = filestat.st_size;
  int vpkfd = open(filename.c_str(), O_RDONLY,0);
  if (vpkfd < 0) throw std::runtime_error("could not open file descriptor");
  void* vpkdata = mmap(NULL, vpksize, PROT_READ, MAP_PRIVATE, vpkfd, 0);

  // load header from mmap
  VPKArchive *archive = new VPKArchive;
  archive->filepath = filename;
  archive->vpkdata = vpkdata;
  archive->vpksize = vpksize;
  archive->archivemd5 = nullptr;
//  archive->signature.Signature = nullptr;
//  archive->signature.PublicKey = nullptr;
  memcpy(&archive->header, vpkdata, sizeof(VPKHeader_v2));

  // check this is a version we can load
  if (archive->header.Signature != 0x55aa1234) throw std::runtime_error("file has wrong header");
  if (archive->header.Version != 2) throw std::runtime_error("file has unknown version");

  // go through vpk tree
  char *tree = (char*) archive->vpkdata + sizeof(VPKHeader_v2);
  char *treescanposition = tree;
  int maxarchiveindex = -1;
  while (true) {
    std::string extensionname = readString(&treescanposition);
    if (extensionname == "") break;
    while (true) {
      std::string pathname = readString(&treescanposition);
      if (pathname == "") break;
      if (pathname == " ") pathname = "root";
      Path *thispath = createPath(pathname, archive);
      thispath->name = pathname;
      while (true) {
        std::string entryname = readString(&treescanposition);
        if (entryname == "") break;
        if (extensionname != " ")
          entryname = entryname + "." + extensionname;
        Entry thisentry;
        thisentry.name = entryname;
        thisentry.entry = reinterpret_cast<VPKDirectoryEntry *>(treescanposition);
        thispath->files[entryname] = thisentry;
        if (thisentry.entry->ArchiveIndex != 0x7fff)
          maxarchiveindex = std::max(maxarchiveindex, (int)thisentry.entry->ArchiveIndex);
        treescanposition += sizeof(VPKDirectoryEntry);
        treescanposition += thisentry.entry->PreloadBytes;
      }
    }
  }

  archive->archives = maxarchiveindex;

  // load archives
  if (maxarchiveindex >= 0) {
    archive->split = true;
    archive->archivedata = new void*[maxarchiveindex+1];
    archive->archivesizes = new size_t[maxarchiveindex+1];
    std::string fileprefix = filename.substr(0, filename.size()-7);
    for (int i = 0; i <= maxarchiveindex; i++) {
      std::stringstream ss;
      ss << fileprefix << std::setfill('0') << std::setw(3) << i << ".vpk";
      std::string archivefile = ss.str();
      std::cout << "loading archive: " << archivefile << std::endl;
      size_t archivesize = getFileSize(archivefile);
      archive->archivesizes[i] = archivesize;
      int archivefd = open(archivefile.c_str(), O_RDONLY,0);
      if (archivefd == -1) {
        std::cerr << "Could not open file: " << archivefile << std::endl;
        throw std::runtime_error("could not open file descriptor");
      }
      void* archivedata = mmap(NULL, archivesize, PROT_READ, MAP_PRIVATE, archivefd, 0);
      archive->archivedata[i] = archivedata;
    }
  } else {
    archive->split = false;
  }


  // find location of vpk data
  char *filedata = tree + archive->header.TreeSize;
  archive->vpkarchivesection = filedata;

  // find location of md5 hashes in file
  char *archivehashdata = filedata + archive->header.FileDataSectionSize;
  int archivehashes = archive->header.ArchiveMD5SectionSize / sizeof(VPKArchiveMD5SectionEntry);
  archive->archivemd5count = archivehashes;
  archive->archivemd5 = reinterpret_cast<VPKArchiveMD5SectionEntry *>(archivehashdata);

  // copy vpk record hash data
  char *otherhashdata = archivehashdata + archive->header.ArchiveMD5SectionSize;
  if (archive->header.OtherMD5SectionSize == sizeof(VPKOtherMD5Section))
    memcpy(&archive->othermd5, otherhashdata, sizeof(VPKOtherMD5Section));

  // copy signature data
//  char *signaturedata = otherhashdata + archive->header.OtherMD5SectionSize;
//  char *signaturedataposition = signaturedata;
//  if (archive->header.SignatureSectionSize > 0) {
//
//    memcpy(&archive->signature.PublicKeySize, &signaturedataposition, sizeof(unsigned int));          // get key size
//    signaturedataposition += sizeof(unsigned int);                                                    // move read pointer
//    archive->signature.PublicKey = new char[archive->signature.PublicKeySize];                        // create memory for key
//    memcpy(archive->signature.PublicKey, &signaturedataposition, archive->signature.PublicKeySize);   // copy key into memory
//    signaturedataposition += archive->signature.PublicKeySize;                                        // move read pointer
//
//    memcpy(&archive->signature.SignatureSize, &signaturedataposition, sizeof(unsigned int));          // same as above, but for sig
//    signaturedataposition += sizeof(unsigned int);
//    archive->signature.Signature = new char[archive->signature.SignatureSize];
//    memcpy(archive->signature.Signature, &signaturedataposition, archive->signature.SignatureSize);
//  }

  return archive;
}

// unload an archive, freeing mmaps and allocated memory
void unloadFile(VPKArchive* archive) {
  if (archive->vpkdata != NULL) munmap(archive->vpkdata, archive->vpksize);
  for (int i = 0; i <= archive->archives; i++)
      munmap(archive->archivedata[i], archive->archivesizes[i]);
//  if (archive->signature.Signature != NULL) delete archive->signature.Signature;
//  if (archive->signature.PublicKey != NULL) delete archive->signature.PublicKey;
  if (archive->split) {
    delete[] archive->archivedata;
    delete[] archive->archivesizes;
  }
  delete archive;
}
