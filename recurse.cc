#include "attrjager.hh"

#include <unordered_set>
#include <sys/types.h>
#include <sys/stat.h>
#include <attr/xattr.h>
#include <dirent.h>
#include <string.h>
#include <regex>

namespace {
  std::unordered_set<decltype(dirent::d_ino)> seen_inodes;
  std::regex ents_to_skip("^\\.\\.?$"); // matches "." and ".."
  std::regex attr_belongs_to_rsync("^user\\.rsync\\..*");
  bool handle_file(const std::string& path, bool& is_directory) {
    static std::vector<char> listbuffer;
    struct stat st;
    if(lstat(path.c_str(), &st)) {
      perror(path.c_str());
      return false;
    }
    ssize_t list_size = llistxattr(path.c_str(), nullptr, 0);
    if(list_size < 0) {
      perror(path.c_str());
      return false;
    }
    if(list_size > ssize_t(listbuffer.size()))
      listbuffer.resize(list_size);
    if(llistxattr(path.c_str(), listbuffer.data(), listbuffer.size())
       != list_size) {
      perror(path.c_str());
      return false;
    }
    bool handled = true;
    ssize_t start = 0;
    while(start < list_size) {
      ssize_t end = start + strlen(listbuffer.data() + start);
      std::string attr(listbuffer.data()+start, listbuffer.data()+end);
      if(std::regex_match(attr, attr_belongs_to_rsync)) {
        if(!handle_attr(path, attr, st))
          handled = false;
      }
      start = end + 1;
    }
    is_directory = S_ISDIR(st.st_mode);
    return handled;
  }
  bool recursively_handle_file(std::string path) {
    bool handled = true;
    bool is_directory = false;
    if(!handle_file(path, is_directory)) handled = false;
    if(is_directory) {
      DIR* d = opendir(path.c_str());
      if(!d) {
        perror(path.c_str());
        return false;
      }
      std::vector<std::string> paths;
      struct dirent* ent;
      while((ent = readdir(d)) != nullptr) {
        if(std::regex_match(ent->d_name, ents_to_skip))
          continue; // skip . and ..
        if(seen_inodes.find(ent->d_ino) == seen_inodes.end()) {
          std::string q;
          q.reserve(strlen(ent->d_name) + path.length() + 1);
          q += path;
          q += '/';
          q += ent->d_name;
          paths.emplace_back(std::move(q));
          seen_inodes.emplace(ent->d_ino);
        }
      }
      closedir(d);
      for(auto&& path : paths) {
        if(!recursively_handle_file(path))
          handled = false;
      }
    }
    return handled;
  }
}

bool recurse() {
  bool handled = true;
  for(auto&& path : paths) {
    if(!recursively_handle_file(path))
      handled = false;
  }
  return handled;
}
