#include "attrjager.hh"

#include <unordered_map>
#include <functional>
#include <algorithm>
#include <sys/stat.h>

std::string name;
std::vector<std::string> paths;
bool dry_run = false, verbose = false;
std::unordered_set<std::string> unhandled_attributes;
std::vector<std::string> unhandled_attribute_info;

namespace {
  void print_usage() {
    std::cout << "Usage: " << name << " [options] <paths...>\n"
      "Options:\n"
      "  --dry-run: Don't actually modify anything. Just report on what we would\n"
      "and would not have been able to process.\n"
      "  --verbose: Output information about what we are doing (or, with --dry-run,"
      "would be doing) to each file\n";
  }
  bool parse_command_line(int argc, char* argv[]) {
    bool valid = true;
    name = argv[0];
    int n = 1;
    std::unordered_map<std::string, std::function<bool()>> options ={
      {"--dry-run", []{ dry_run = true; return true; }},
      {"--verbose", []{ verbose = true; return true; }},
    };
    while(n < argc) {
      std::string arg = argv[n++];
      if(arg.size() >= 1 && arg[0] == '-') {
        if(arg.size() <= 2 || arg[1] != '-') {
          std::cout << "There are no short options\n";
          valid = false;
        }
        else if(arg.size() == 2) break; // handle --
        else {
          auto option = options.find(arg);
          if(option != options.end())
            valid = valid && option->second();
          else {
            std::cout << "Unknown option: " << arg << "\n";
            valid = false;
          }
        }
      }
      else {
        paths.emplace_back(std::move(arg));
      }
    }
    while(n < argc) {
      paths.emplace_back(argv[n++]);
    }
    if(paths.empty()) {
      std::cout << "No paths given\n";
      valid = false;
    }
    if(!valid) print_usage();
    return valid;
  }
}

int main(int argc, char* argv[]) {
  umask(0);
  if(!parse_command_line(argc, argv)) return EXIT_FAILURE;
  bool success = recurse();
  if(!unhandled_attribute_info.empty()) {
    std::sort(unhandled_attribute_info.begin(),
              unhandled_attribute_info.end());
    for(auto&& attr : unhandled_attribute_info) {
      std::cout << "\n" << attr << "\n";
    }
    return EXIT_FAILURE;
  }
  else return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
