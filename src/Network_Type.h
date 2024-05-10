/*
 * This file is part of the FRED system.
 *
 * Copyright (c) 2010-2012, University of Pittsburgh, John Grefenstette, Shawn Brown, 
 * Roni Rosenfield, Alona Fyshe, David Galloway, Nathan Stone, Jay DePasse, 
 * Anuroop Sriram, and Donald Burke
 * All rights reserved.
 *
 * Copyright (c) 2013-2021, University of Pittsburgh, John Grefenstette, Robert Frankeny,
 * David Galloway, Mary Krauland, Michael Lann, David Sinclair, and Donald Burke
 * All rights reserved.
 *
 * FRED is distributed on the condition that users fully understand and agree to all terms of the 
 * End User License Agreement.
 *
 * FRED is intended FOR NON-COMMERCIAL, EDUCATIONAL OR RESEARCH PURPOSES ONLY.
 *
 * See the file "LICENSE" for more information.
 */

//
//
// File: Network_Type.h
//

#ifndef _FRED_Network_Type_H
#define _FRED_Network_Type_H

#include <spdlog/spdlog.h>

#include "Global.h"
#include "Group_Type.h"


namespace Network_Action {
  enum e { NONE, JOIN, ADD_EDGE_TO, ADD_EDGE_FROM, DELETE_EDGE_TO, DELETE_EDGE_FROM, RANDOMIZE, QUIT  };
};

/**
 * This class represents a specific type of Network.
 *
 * Unlike Place_Type, Network_Type will not be associated with more than one Network. Each 
 * network type has only one associated network. This class contains data on that network, 
 * as well as static methods to manage the different networks and network types.
 *
 * This class inherits from Group_Type.
 */
class Network_Type : public Group_Type {
public:

  Network_Type(int _id, std::string _name);
  ~Network_Type();

  void get_properties();

  void prepare();

  // access methods

  /**
   * Gets this network type's associated Network.
   *
   * @return the network
   */ 
  Network* get_network() {
    return this->network;
  }

  /**
   * Checks if this network type is undirected. An undirected network does not have separate inward and outward degrees; 
   * each edge points both ways.
   *
   * @return if the network is undirected
   */
  bool is_undirected() {
    return this->undirected;
  }

  // static methods

  static void get_network_type_properties();

  static void prepare_network_types();

  /**
   * Gets the Network_Type with the specified name.
   *
   * @param name the network type name
   * @return the network type
   */
  static Network_Type* get_network_type(std::string name) {
    return static_cast<Network_Type*>(Group_Type::get_group_type(name));
  }

  /**
   * Gets the Network_Type with the specified ID.
   *
   * @param type_id the network type ID
   * @return the network type
   */
  static Network_Type* get_network_type(int type_id) {
    return static_cast<Network_Type*>(Group_Type::get_group_type(type_id));
  }

  /**
   * Gets the Network_Type at the specified index.
   *
   * @param index the index
   * @return the network type
   */
  static Network_Type* get_network_type_number(int index) {
    if(0 <= index && index < Network_Type::get_number_of_network_types()) {
      return Network_Type::network_types[index];
    } else {
      return nullptr;
    }
  }

  /**
   * Gets the Network of the Network_Type at the specified index.
   *
   * @param index the index
   * @return the network
   */
  static Network* get_network_number(int index) {
    if(0 <= index && index < Network_Type::get_number_of_network_types()) {
      return Network_Type::network_types[index]->get_network();
    } else {
      return nullptr;
    }
  }

  /**
   * Gets the Network of the Network_Type with the specified ID.
   *
   * @param type_id the network type ID
   * @return the network
   */
  static Network* get_network(int type_id) {
    Network_Type* network_type = Network_Type::get_network_type(type_id);
    if(network_type == nullptr) {
      return nullptr;
    } else {
      return network_type->get_network();
    }
  }

  /**
   * Gets the index of the Network_Type with the specified name.
   *
   * @param name the name
   * @return the index
   */
  static int get_network_index(std::string name) {
    int size = Network_Type::names.size();
    for(int i = 0; i < size; ++i) {
      if(name == Network_Type::names[i]) {
        return i;
      }
    }
    return -1;
  }

  /**
   * Adds the given network type name to the static network type names vector if it is not already included.
   *
   * @param name the name
   */
  static void include_network_type(std::string name) {
    if(!Network_Type::is_log_initialized) {
      Network_Type::setup_logging();
    }
    int size = Network_Type::names.size();
    for(int i = 0; i < size; ++i) {
      if(name == Network_Type::names[i]) {
        Network_Type::network_type_logger->info("INCLUDE_NETWORK {:s} found at network pos {:d}", name.c_str(),i);
        return;
      }
    }
    Network_Type::names.push_back(name);
    Network_Type::network_type_logger->info("INCLUDE_NETWORK {:s} added as network pos {:d}", name.c_str(), static_cast<int>(Network_Type::names.size()) - 1);
  }

  /**
   * Removes the given network type name from the static network type names vector if it is included.
   *
   * @param name the name
   */
  static void exclude_network_type(std::string name) {
    int size = Network_Type::names.size();
    for(int i = 0; i < size; ++i) {
      if(name == Network_Type::names[i]) {
        for(int j = i + 1; j < size; ++j) {
          Network_Type::names[j - 1] = Network_Type::names[j];
        }
        Network_Type::names.pop_back();
      }
    }
  }

  /**
   * Gets the number of network types.
   *
   * @return the number of network types
   */
  static int get_number_of_network_types() { 
    return Network_Type::network_types.size();
  }

  static void print_network_types(int day);
  static void finish_network_types();
  static void setup_logging();

private:

  // index in this vector of network types
  int index;

  // type id
  int id;
  bool undirected;

  // each network type has one network
  Network* network;

  // print interval (days)
  int print_interval;
  int next_print_day;

  // lists of network types
  static std::vector <Network_Type*> network_types;
  static std::vector<std::string> names;
  static int number_of_place_types;

  static bool is_log_initialized;
  static std::string network_type_log_level;
  static std::unique_ptr<spdlog::logger> network_type_logger;
};

#endif // _FRED_Network_Type_H
