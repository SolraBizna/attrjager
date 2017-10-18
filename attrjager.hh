#ifndef ATTRJAGERHH
#define ATTRJAGERHH

#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>

extern std::string name;
extern std::vector<std::string> paths;
extern bool dry_run, verbose;
extern std::unordered_set<std::string> unhandled_attributes;
extern std::vector<std::string> unhandled_attribute_info;

/* true if we handled everything we encountered */
bool recurse();
/* true if we handled this attribute */
bool handle_attr(const std::string& path,
                 const std::string& attrname,
                 struct stat& st);

struct parsed_stat_attr {
  mode_t mode;
  dev_t rdev;
  uid_t uid;
  gid_t gid;
};
bool parse_stat_attr(const std::string& value, parsed_stat_attr& out);

#endif
