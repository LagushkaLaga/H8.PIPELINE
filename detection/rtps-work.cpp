#include "rtps-work.hpp"

std::vector< rtps::Camera > rtps::read_rtps(std::istream & in)
{
  std::vector< Camera > result;
  while (in)
  {
    Camera temp;
    in >> temp.id >> temp.name >> temp.address;
    result.push_back(temp);
  }
  return result;
}
