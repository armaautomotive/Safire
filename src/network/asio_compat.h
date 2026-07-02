//
// asio_compat.h
// ~~~~~~~~~~~~~
//

#ifndef HTTP_SERVER3_ASIO_COMPAT_HPP
#define HTTP_SERVER3_ASIO_COMPAT_HPP

#include <boost/asio.hpp>
#include <boost/version.hpp>

namespace http {
namespace server3 {

#if BOOST_VERSION >= 106600
typedef boost::asio::io_context io_context_type;
#else
typedef boost::asio::io_service io_context_type;
#endif

} // namespace server3
} // namespace http

#endif // HTTP_SERVER3_ASIO_COMPAT_HPP
