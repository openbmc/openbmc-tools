/*******************************************************************************

   fruid-util.hpp - entrypoints for FRU utility.                     [interface]

   Copyright (c) 2019, Facebook Corporation. All rights reserved.

   This utility retrieves and outputs all FRU information based on what it can
   see on the system D-Bus.

   License: Apache 2.0

*******************************************************************************/
#ifndef __FRUIDUTIL_HPP__
#define __FRUIDUTIL_HPP__

// standard library definitions

#include <iostream>
#include <iomanip>

// Boost library definitiions

#include <boost/container/flat_map.hpp>
#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

// SDBus+ library definitions

#include <sdbusplus/bus.hpp>

// exit codes

#define EXITCODE_SUCCESS (0)
#define EXITCODE_FAILURE (1)
#define EXITCODE_INVALID_USAGE (2)
#define EXITCODE_CANT_GET_MANAGED_OBJECTS (3)

// The following definitions allow decoding of results from the D-Bus
// GetManagedObjects call

using AnyType =
    sdbusplus::message::variant<std::string, bool, uint8_t, int16_t, uint16_t,
                                int32_t, uint32_t, int64_t, uint64_t, double,
                                std::vector<std::string>>;

using AnyTypeMap = boost::container::flat_map< std::string, AnyType>;
using NamedArrayOfAnyTypeMaps = std::vector<std::pair<std::string, AnyTypeMap>>;
using ArrayOfObjectPathsAndTieredAnyTypeMaps =
      std::vector<std::pair<sdbusplus::message::object_path, NamedArrayOfAnyTypeMaps>>; 

#endif  // __FRUIDUTIL_HPP__