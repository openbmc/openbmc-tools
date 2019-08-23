/***********************************************************************************************************************************

   includes.hpp - master includes list for OpenBMC Utilities

**********************************************************************************************************************************/

#ifndef __INCLUDES_HPP__
#define __INCLUDES_HPP__

#include <iostream>
#include <iomanip>
#include <pthread.h>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/asio/sd_event.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/exception.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/timer.hpp>
#include <variant>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <errno.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <string>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/regex.hpp>
#include <chrono>
#include <ctime>
#include <fstream>
#include <future>
#include <iostream>
#include <regex>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <thread>
#include <variant>
#include <ncurses.h>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <chrono>
#include <ctime>
#include <iostream>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/asio/sd_event.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/exception.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/timer.hpp>
#include <variant>
#include <systemd/sd-bus.h>

#define EXITCODE_SUCCESS (0)
#define EXITCODE_FAILURE (1)
#define EXITCODE_INVALID_USAGE (2)
#define EXITCODE_CANT_GET_MANAGED_OBJECTS (3)
#define EXITCODE_CANT_OPEN_KEYWORD_MAPPING (4)
#define EXITCODE_CANT_READ_KEYWORD_MAPPING (5)
#define EXITCODE_CANT_CLOSE_KEYWORD_MAPPING (6)




using AnyType = sdbusplus::message::variant< std::string,
                                     bool,
                                     uint8_t,
                                     int16_t,
                                     uint16_t,
                                     int32_t,
                                     uint32_t,
                                     int64_t,
                                     uint64_t,
                                     double,
                                     std::vector<std::string>>;

using AnyTypeMap = boost::container::flat_map< std::string, AnyType>;
using NamedArrayOfAnyTypeMaps = std::vector<std::pair<std::string, AnyTypeMap>>;
using ArrayOfObjectPathsAndTieredAnyTypeMaps = std::vector<std::pair<sdbusplus::message::object_path, NamedArrayOfAnyTypeMaps>>; 


#endif  // __INCLUDES_HPP__

