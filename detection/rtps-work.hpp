#pragma once

#include <string>
#include <istream>
#include <vector>

struct Camera
{
  unsigned id;
  std::string name;
  std::string address;
};
  
std::vector< Camera > read_rtps(std::istream & in);
