/***********************************************************************************************************************************

   fruid-util.cpp - entrypoints for FRU utility                                                                     [implementation]

   Copyright (c) 2019, Facebook Corporation. All rights reserved.

   This utility retrieves and outputs all FRU information based on what it can see on the system D-Bus.

   License: Apache 2.0

***********************************************************************************************************************************/

#include "../includes/includes.hpp"

namespace bpo = boost::program_options;

int main( int cArguments, char** apszArguments ) {
   GetManagedObjectsResponse::ArrayOfObjectPathsAndTieredAnyTypeMaps gmoResult;

   // Retrieve FRUDevice GetManagedObjects (partial result is not possible)

   try {
      auto bus = sdbusplus::bus::new_default_system(); /// eradicate auto
      auto method = bus.new_method_call("xyz.openbmc_project.FruDevice",
                                        "/",
                                        "org.freedesktop.DBus.ObjectManager",
                                        "GetManagedObjects"); /// eradicate auto, literals to const
      auto response = bus.call(method); /// eradicate auto
      response.read( gmoResult );
      }
   catch( const sdbusplus::exception::SdBusError& e) {
      std::cerr << "Exception occurred" << std::endl << e.what() << std::endl;
      return EXITCODE_CANT_GET_MANAGED_OBJECTS;
      }
   catch( const std::exception& se) {std::cerr << "?Exception while parsing command line:" << se.what() << std::endl;}
   catch( ... ) { std::cerr << "?Unhandled exception while retrieving FRUDevice GetManagedObjects" << std::endl;}

   // Display results

   try {
      for(const auto& i: gmoResult ) {
         std::size_t                   lastSlash = std::string(i.first).find_last_of("/\\");
         std::string                   sComponent = std::string(i.first).substr(lastSlash+1);
         if( sComponent != "FruDevice") {
            std::cout << sComponent << std::endl;
            for(const auto& j: i.second) {
               for( const auto& k: j.second) {
                  std::cout << "\t" << k.first << ": ";
                  switch( k.second.index()) {
                     case 0: {std::string x = std::get<std::string>(k.second); std::cout << x << std::endl;} break;
                     case 6: {uint32_t u = std::get<uint32_t>(k.second); std::cout << u << std::endl;} break;
                     default: std::cout << "Unexpected type" << std::endl;
                     }
                  }
               }
            }
         }
      }
   catch( const std::exception& se) {std::cerr << "?Exception while displaying results:" << se.what() << std::endl;}
   catch( ... ) { std::cerr << "?Unhandled exception while displaying results" << std::endl;}
   }  



