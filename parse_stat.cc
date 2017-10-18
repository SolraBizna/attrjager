#include "attrjager.hh"

#include <regex>

bool parse_stat_attr(const std::string& value, parsed_stat_attr& out) {
  static std::regex stat_parse_regex("^(0?[0-7]*) ([0-9]+),([0-9]+) ([0-9]+):([0-9]+)$");
  std::smatch result;
  if(!std::regex_match(value, result, stat_parse_regex)) return false;
  out.mode = std::stoul(result[1], nullptr, 8);
  out.rdev = makedev(std::stoul(result[2], nullptr, 10),
                     std::stoul(result[3], nullptr, 10));
  out.uid = std::stoul(result[4], nullptr, 10);
  out.gid = std::stoul(result[5], nullptr, 10);
  return true;
}

