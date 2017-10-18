#include "attrjager.hh"

#include <attr/xattr.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

namespace {
  void irreversible() {
    std::cout << "THIS ERROR RESULTED IN AN IRREVERSIBLE CHANGE! YOU MUST RESOLVE IT MANUALLY!\n";
  }
  void unhandle_attribute(const std::string& path,
                          const std::string& attr,
                          const std::string& explanation) {
    auto it = unhandled_attributes.find(attr+explanation);
    if(it != unhandled_attributes.end()) return;
    unhandled_attributes.insert(it, attr+explanation);
    unhandled_attribute_info.emplace_back(std::string()+
                                          "Unhandled attribute: "+attr+"\n"
                                          "First seen at path: "+path+"\n"
                                          "Reason: "+explanation+"\n");
  }
  std::string lgetxattrstr(const std::string& path,
                           const std::string& attr) {
    std::string ret;
    ssize_t size = lgetxattr(path.c_str(), attr.c_str(), nullptr, 0);
    if(size < 0) {
      perror(path.c_str());
      return "";
    }
    ret.resize(size);
    if(lgetxattr(path.c_str(), attr.c_str(), &ret[0], ret.size())
       != ssize_t(ret.size())) {
      perror(path.c_str());
      return ""; // this shouldn't happen but it might?
    }
    return std::move(ret);
  }
  void v_printf(const char* format, ...) {
    va_list arg;
    va_start(arg, format);
    if(verbose) vprintf(format, arg);
    va_end(arg);
  }
  bool recreate_as_symlink(const std::string& path) {
    v_printf("%s: recreating as symlink\n", path.c_str());
    if(!dry_run) {
      char buf[256];
      std::string linkpath;
      int fd = open(path.c_str(), O_RDONLY);
      if(fd < 0) {
        perror(path.c_str());
        return false;
      }
      ssize_t red;
      while((red = read(fd, buf, sizeof(buf))) > 0) {
        linkpath.insert(linkpath.end(), buf, buf + red);
      }
      if(red < 0) {
        perror(path.c_str());
        close(fd);
        return false;
      }
      close(fd);
      if(unlink(path.c_str())) {
        perror(path.c_str());
        return false;
      }
      if(symlink(linkpath.c_str(), path.c_str())) {
        perror(path.c_str());
        irreversible();
      }
    }
    return true;
  }
  bool recreate_as_device(const std::string& path,
                          mode_t mode, dev_t rdev, uid_t uid, gid_t gid) {
    v_printf("%s: recreating as device\n", path.c_str());
    if(!dry_run) {
      if(unlink(path.c_str())) {
        perror(path.c_str());
        return false;
      }
      if(mknod(path.c_str(), mode, rdev)) {
        perror(path.c_str());
        irreversible();
      }
      if(chown(path.c_str(), uid, gid)) {
        perror(path.c_str());
        return false;
      }
    }
    return true;
  }
  bool recreate_as_fifo(const std::string& path,
                        mode_t mode, uid_t uid, gid_t gid) {
    v_printf("%s: recreating as FIFO\n", path.c_str());
    if(!dry_run) {
      if(unlink(path.c_str())) {
        perror(path.c_str());
        return false;
      }
      if(mkfifo(path.c_str(), mode)) {
        perror(path.c_str());
        irreversible();
      }
      if(chown(path.c_str(), uid, gid)) {
        perror(path.c_str());
        return false;
      }
    }
    return true;
  }
  bool perform_chmod(const std::string& path, mode_t mode) {
    v_printf("%s: chmod(0%o)\n", path.c_str(), mode);
    if(!dry_run) {
      if(chmod(path.c_str(), mode)) {
        perror(path.c_str());
        return false;
      }
    }
    return true;
  }
  bool perform_chown(const std::string& path, uid_t uid, gid_t gid) {
    v_printf("%s: chown(%i:%i)\n", path.c_str(), uid, gid);
    if(!dry_run) {
      if(chown(path.c_str(), uid, gid)) {
        perror(path.c_str());
        return false;
      }
    }
    return true;
  }
}

bool handle_attr(const std::string& path,
                 const std::string& name,
                 struct stat& st) {
  if(name == "user.rsync.%stat") {
    std::string value = lgetxattrstr(path, name);
    if(value == "") {
      unhandle_attribute(path, name, "Empty or unretrievable attribute");
      return false;
    }
    parsed_stat_attr parsed;
    if(!parse_stat_attr(value, parsed)) {
      unhandle_attribute(path, name+"="+value, "Unexpected format for attribute");
      return false;
    }
    if(!(S_ISBLK(parsed.mode) || S_ISCHR(parsed.mode))
       && parsed.rdev != 0) {
      unhandle_attribute(path, name+"="+value, "Unexpected non-zero device ID for non-device file");
      return false;
    }
    if((parsed.mode&S_IFMT) != (st.st_mode&S_IFMT)) {
      bool did_handle_mode_change = false;;
      if(S_ISREG(st.st_mode)) switch(parsed.mode&S_IFMT) {
      case S_IFLNK:
        if(parsed.mode == 0120777) {
          if(!recreate_as_symlink(path)) return false;
          did_handle_mode_change = true;
        }
        break;
      case S_IFBLK:
      case S_IFCHR:
        if(!recreate_as_device(path, parsed.rdev, parsed.mode,
                               parsed.uid, parsed.gid)) return false;
        did_handle_mode_change = true;
        break;
      case S_IFIFO:
        if(!recreate_as_fifo(path, parsed.mode, parsed.uid, parsed.gid))
          return false;
        did_handle_mode_change = true;
        break;
      }
      if(!did_handle_mode_change) {
        unhandle_attribute(path, name+"="+value, "Not sure how to handle this mode change");
        return false;
      }
    }
    else if((S_ISBLK(parsed.mode) || S_ISCHR(parsed.mode))
            && parsed.rdev != st.st_rdev) {
      if(!recreate_as_device(path, parsed.rdev, parsed.mode,
                             parsed.uid, parsed.gid)) return false;      
    }
    else {
      if((parsed.mode&~S_IFMT) != (st.st_mode&~S_IFMT)) {
        if(!perform_chmod(path, parsed.mode&~S_IFMT)) return false;
      }
      if(parsed.uid != st.st_uid || parsed.gid != st.st_gid) {
        if(!perform_chown(path, parsed.uid, parsed.gid)) return false;
      }
    }
    if(!dry_run) {
      if(lstat(path.c_str(), &st)) {
        perror(path.c_str());
        return false;
      }
      if(parsed.mode != st.st_mode || parsed.rdev != st.st_rdev
         || parsed.uid != st.st_uid || parsed.gid != st.st_gid) {
        unhandle_attribute(path, name+"="+value, "Weren't able to make it stick");
        return false;
      }
      else if(lremovexattr(path.c_str(), name.c_str())) {
        if(errno != ENOATTR) {
          unhandle_attribute(path, name+"="+value, "We made it stick but couldn't delete the attribute");
          return false;
        }
      }
    }
    return true;
  }
  else {
    unhandle_attribute(path, name, "Don't know this attribute");
    return false;
  }
}
