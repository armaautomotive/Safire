#ifndef HTTP_SERVER3_HEADER_HPP
#define HTTP_SERVER3_HEADER_HPP

#include <string>

namespace http {
namespace server3 {

struct header
{
  std::string name;
  std::string value;
};

} // namespace server3
} // namespace http

#endif // HTTP_SERVER3_HEADER_HPP

