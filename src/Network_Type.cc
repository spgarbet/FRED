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
// File: Network_Type.cc
//

#include <spdlog/spdlog.h>

#include "Parser.h"
#include "Network.h"
#include "Network_Type.h"
#include "Place_Type.h"
#include "Utils.h"


//////////////////////////
//
// STATIC VARIABLES
//
//////////////////////////

std::vector <Network_Type*> Network_Type::network_types;
std::vector<std::string> Network_Type::names;
int Network_Type::number_of_place_types = 0;

bool Network_Type::is_log_initialized = false;
std::string Network_Type::network_type_log_level = "";
std::unique_ptr<spdlog::logger> Network_Type::network_type_logger = nullptr;

/**
 * Creates a Network_Type with the specified ID and name. The name is passed into 
 * the Group_Type constructor. Default variables are initialized.
 *
 * @param type_id the network type ID
 * @param _name the name
 */
Network_Type::Network_Type(int type_id, std::string _name) : Group_Type(_name) {
  // generate one network for each network_type
  this->id = type_id;
  this->index = 0;
  this->network = new Network(_name.c_str(), type_id, this);
  this->print_interval = 0;
  this->undirected = false;
  this->next_print_day = 999999;
  Group_Type::add_group_type(this);
}

/**
 * Default destructor.
 */
Network_Type::~Network_Type() {
}

/**
 * Prepares this network type. Creates an administrator for the associated Network if needed.
 */
void Network_Type::prepare() {
  
  Network_Type::network_type_logger->info("network_type {:s} type_id {:d} prepare entered", 
      this->name.c_str(), this->id);

  if(this->has_admin) {
    this->network->create_administrator();
  }

  this->get_network()->read_edges();

  Network_Type::network_type_logger->info("network_type {:s} prepare finished", this->name.c_str());
}

//////////////////////////
//
// STATIC METHODS
//
//////////////////////////

/**
 * Creates a Network_Type object for each network type name and assigns it a group type ID. Then gets the properties 
 * of each network type created.
 */
void Network_Type::get_network_type_properties() {

  Network_Type::network_types.clear();

  Network_Type::number_of_place_types = Place_Type::get_number_of_place_types();

  for(int index = 0; index < static_cast<int>(Network_Type::names.size()); ++index) {

    std::string name = Network_Type::names[index];

    int type_id = index + Network_Type::number_of_place_types;

    // create new Network_Type object
    Network_Type* network_type = new Network_Type(type_id, name);

    // add to network_type list
    Network_Type::network_types.push_back(network_type);

    Network_Type::network_type_logger->info("CREATED_NETWORK_TYPE index {:d} type_id {:d} = {:s}", 
        index, type_id, name.c_str());

    // get its properties
    network_type->get_properties();
  }

}

/**
 * Gets properties of this network type.
 */
void Network_Type::get_properties() {

  // first get the base class properties
  Group_Type::get_properties();

  char property_name[FRED_STRING_SIZE];

  Network_Type::network_type_logger->info("network_type {:s} read_properties entered", 
      this->name.c_str());
  
  // optional properties:
  Parser::disable_abort_on_failure();

  snprintf(property_name, FRED_STRING_SIZE, "%s.is_undirected", this->name.c_str());
  int n = 0;
  Parser::get_property(property_name, &n);
  this->undirected = n;

  snprintf(property_name, FRED_STRING_SIZE, "%s.print_interval", this->name.c_str());
  Parser::get_property(property_name, &this->print_interval);
  if(this->print_interval > 0) {
    this->next_print_day = 0;
  }

  Parser::set_abort_on_failure();

  Network_Type::network_type_logger->info("network_type {:s} read_properties finished", 
      this->name.c_str());
}

/**
 * Prepares all network types.
 */
void Network_Type::prepare_network_types() {
  int n = Network_Type::get_number_of_network_types();
  Network_Type::network_type_logger->info("prepare_network_type entered types = {:d}", n);
  for(int index = 0; index < n; ++index) {
    Network_Type::network_types[index]->prepare(); 
  }
}

/**
 * Prints details of each Network_Type's associated network if it is time to do so given a day. 
 * It is time to do so if the print interval for the Network of the type has expired, in which case the 
 * print interval will be reset.
 *
 * @param day the day
 */
void Network_Type::print_network_types(int day) {
  for(int index = 0; index < Network_Type::get_number_of_network_types(); ++index) {
    Network_Type* net_type = Network_Type::network_types[index];
    if(net_type->next_print_day <= day) {
      net_type->get_network()->print();
      net_type->next_print_day += net_type->print_interval;
    }
  }
}

/**
 * Prints details of each Network_Type's associated Network whose print interval remained a positive value.
 */
void Network_Type::finish_network_types() {
  for(int index = 0; index < Network_Type::get_number_of_network_types(); ++index) {
    if(Network_Type::network_types[index]->print_interval > 0) {
      Network_Type::network_types[index]->get_network()->print();
    }
  }
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Network_Type::setup_logging() {
  if(Network_Type::is_log_initialized) {
    return;
  }

  if(Parser::does_property_exist("network_type_log_level")) {
    Parser::get_property("network_type_log_level", &Network_Type::network_type_log_level);
  } else {
    Network_Type::network_type_log_level = "OFF";
  }

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Network_Type::network_type_logger = std::make_unique<spdlog::logger>("network_type_logger", 
        sink_list.begin(), sink_list.end());
    Network_Type::network_type_logger->set_level(
        Utils::get_log_level_from_string(Network_Type::network_type_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Network_Type::network_type_logger->trace("<{:s}, {:d}>: Network_Type logger initialized", 
      __FILE__, __LINE__  );
  Network_Type::is_log_initialized = true;
}
