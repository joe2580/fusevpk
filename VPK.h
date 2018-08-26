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
  std::string filepath;



  size_t vpksize;
  void *vpkdata = NULL;
  void *vpkarchivesection = NULL;

  bool split;
  int archives;
  void **archivedata;
  size_t *archivesizes;

  std::map<std::string, Path> paths;

  VPKHeader_v2 header;
  int archivemd5count;
  VPKArchiveMD5SectionEntry *archivemd5;
  VPKOtherMD5Section othermd5;
  VPKSignatureSection signature;
};

VPKArchive *loadFile(std::string filename);
void unloadFile(VPKArchive* archive);

Entry *findFile(std::string path, VPKArchive *archive);
Path *findPath(std::string path, VPKArchive *archive);
int getSize(Entry* entry);
int readFile(VPKArchive *archive, Entry *entry, char *buffer, size_t size, off_t offset);

#endif //FUSEVPK_VPK_H
