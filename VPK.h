//
// Created by joe on 22/08/18.
//

#ifndef FUSEVPK_VPK_H
#define FUSEVPK_VPK_H


#include <string>
#include <map>

#include "VPKStructs.h"

// reference a single file
struct Entry {
  std::string name;
  VPKDirectoryEntry* entry;
};

// reference a single directory
struct Path {
  std::string name;
  std::map<std::string, Path> directories;
  std::map<std::string, Entry> files;
};

// reference a vpk archive on disk
struct VPKArchive {
  std::string filepath;                   // filename of archive

  size_t vpksize;                         // size of archive
  void *vpkdata = NULL;                   // pointer to mmap
  void *vpkarchivesection = NULL;         // pointer to archive section

  bool split;                             // is archive split
  int archives;                           // split into how many archives
  void **archivedata;                     // pointers to archive mmaps
  size_t *archivesizes;                   // size of archive mmaps

  std::map<std::string, Path> paths;      // root of loaded directory tree

  VPKHeader_v2 header;                    // copy of vpk header
  int archivemd5count;                    // number of archive md5 entries
  VPKArchiveMD5SectionEntry *archivemd5;  // location of archive md5 section
  VPKOtherMD5Section othermd5;            // copy of othermd5 section
//  VPKSignatureSection signature;          // should contain signature (unimplemented)
};

VPKArchive *loadFile(std::string filename);
void unloadFile(VPKArchive* archive);

Entry *findFile(std::string path, VPKArchive *archive);
Path *findPath(std::string path, VPKArchive *archive);
int getSize(Entry* entry);
int readFile(VPKArchive *archive, Entry *entry, char *buffer, size_t size, off_t offset);

#endif //FUSEVPK_VPK_H
