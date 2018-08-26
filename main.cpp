#define FUSE_USE_VERSION 31
//#define _FILE_OFFSET_BITS  64
#include <fuse3/fuse.h>
#include <iostream>
#include <string>
#include <string.h>
#include <vector>
#include <zconf.h>

#include "VPK.h"

std::string archivename;
VPKArchive *archive;
struct stat vpkstat;

// called when filesystem starts, setup some fuse flags here
static void *vpk_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
  cfg->kernel_cache = true;
  return nullptr;
}

// host wants file/folder attributes
static int vpk_getattr(const char *path, struct stat *st, struct fuse_file_info *fi) {
  st->st_uid = getuid();
  st->st_gid = getgid();
  st->st_ctim = vpkstat.st_ctim;
  st->st_atim = vpkstat.st_atim;
  st->st_mtim = vpkstat.st_mtim;
  if (strcmp(path, "/") == 0) {
    std::cout << "Fuse getattr root: " << path << std::endl;
    st->st_mode = S_IFDIR | 0555;
    st->st_nlink = 2 + archive->paths.size();
    return 0;
  }
  Entry *packedfile = findFile(&path[1], archive);
  if (packedfile != nullptr) {
    std::cout << "Fuse getattr file: " << path << std::endl;
    st->st_mode = S_IFREG | 0444;
    st->st_nlink = 1;
    st->st_size = getSize(packedfile);
    return 0;
  }
  std::cout << "Fuse getattr path: " << path << std::endl;
  Path *packedpath = findPath(&path[1], archive);
  if (packedpath != nullptr) {
    st->st_mode = S_IFDIR | 0555;
    st->st_nlink = 2 + packedpath->directories.size() + packedpath->files.size();
    return 0;
  }
  return -ENOENT;
}

// host wants to open a file (use this to search tree)
static int vpk_open(const char *path, struct fuse_file_info *fi) {
  std::cout << "Fuse open: " << path << std::endl;
  Entry *packedfile = findFile(&path[1], archive);
  if (packedfile == nullptr) return -ENOENT;
  if (fi->flags & O_ACCMODE != O_RDONLY) return -EACCES;
  fi->fh = reinterpret_cast<uint64_t>(packedfile);
  return 0;
}

// host wants to read bytes from a file
static int vpk_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
  Entry *packedfile;
  if (fi != nullptr) packedfile = reinterpret_cast<Entry*>(fi->fh);
  else               packedfile = findFile(&path[1], archive);
  std::cout << "Fuse read: " << path << " index: " << offset << " size: " << size << std::endl;
  if (packedfile == nullptr) return -ENOENT;
  int read = readFile(archive, packedfile, buffer, size, offset);
  return read;
}

// hosts wants to list a folder
static int vpk_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags) {
  std::cout << "Fuse readdir: " << path << std::endl;
  if (strcmp(path, "/") == 0) { // list all top level folders
    filler(buffer, ".", nullptr, 0, (fuse_fill_dir_flags)0);
    filler(buffer, "..", nullptr, 0, (fuse_fill_dir_flags)0);
    for (std::pair<std::string, Path> subpath : archive->paths)
      filler(buffer, subpath.first.c_str(), nullptr, 0, (fuse_fill_dir_flags)0);
    return 0;
  } else { // list some subfolder
    Path *packedpath = findPath(&path[1], archive);
    if (packedpath == nullptr) return -ENOENT;
    filler(buffer, ".", nullptr, 0, (fuse_fill_dir_flags)0);
    filler(buffer, "..", nullptr, 0, (fuse_fill_dir_flags)0);
    for (std::pair<std::string, Path> subpath : packedpath->directories)
      filler(buffer, subpath.first.c_str(), nullptr, 0, (fuse_fill_dir_flags)0);
    for (std::pair<std::string, Entry> subfile : packedpath->files)
      filler(buffer, subfile.first.c_str(), nullptr, 0, (fuse_fill_dir_flags)0);
    return 0;
  }
}

static struct fuse_args vpk_args;

static struct fuse_operations vpk_ops = {
    .getattr = vpk_getattr,
    .open = vpk_open,
    .read = vpk_read,
    .readdir = vpk_readdir,
    .init = vpk_init,
};

// function to extract the name of the archive from the arguments
static int vpk_arg_parse(void *data, const char *arg, int key, struct fuse_args *outargs) {
  if (key == FUSE_OPT_KEY_NONOPT && archivename == "") {
    archivename = arg;
    return 0;
  }
  return 1;
}

// parses arguments, loads archive, hooks fuse
int main(int argc, char *argv[]) {

  std::cout << "FuseVPK starting" << std::endl;
  vpk_args = FUSE_ARGS_INIT(argc, argv);
  fuse_opt_parse(&vpk_args, nullptr, nullptr, vpk_arg_parse);

  std::cout << "FuseVPK loading vpk: " << archivename << std::endl;
  try {
    archive = loadFile(archivename);
    stat(archivename.c_str(), &vpkstat);
  } catch (std::runtime_error &e) {
    fuse_opt_free_args(&vpk_args);
    std::cerr << "Failed to load archive: " << e.what() << std::endl;
    return 1;
  }

  std::cout << "FuseVPK starting filesystem" << std::endl;
  int fuseret = fuse_main(vpk_args.argc, vpk_args.argv, &vpk_ops, nullptr);

  unloadFile(archive);
  fuse_opt_free_args(&vpk_args);

  std::cout << "FuseVPK shutting down" << std::endl;
}
