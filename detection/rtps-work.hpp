#ifndef RTPS_WORK_HPP
#define RTPS_WORK_HPP

#include <string>
#include <istream>
#include <vector>

namespace rtps
{
  struct Camera
  {
    unsigned id;
    std::string name;
    std::string address;
  };
  
  std::vector< Camera > read_rtps(std::istream & in);
}

#endif
