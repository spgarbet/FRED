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
// File: Network.h
//

#ifndef _FRED_NETWORK_H
#define _FRED_NETWORK_H

#include <spdlog/spdlog.h>

#include "Global.h"
#include "Group.h"

class Condition;
class Preference;
class Network_Type;

/**
 * This class represents a network in which people interact with each other.
 *
 * A Network is a type of Group in which the interaction of people is, unlike a Place, 
 * not tied to location. There is only one Network per Network_Type. Connections between 
 * people are defined as edges, which is explained in detail in the Link class. This class 
 * is responsible for modeling the network as a group, but links are where people are connected 
 * to one another throughout a network.
 *
 * This class inherits from Group.
 */
class Network : public Group {
 public:

  Network(const char* lab, int _type_id, Network_Type* net_type);

  /**
   * Default destructor.
   */
  ~Network() {}

  void get_properties();
  double get_transmission_prob(int condition_id, Person* i, Person* s);

  void print();

  bool is_connected_to(Person* p1, Person* p2);
  bool is_connected_from(Person* p1, Person* p2);
  double get_mean_degree();
  void test();
  void randomize(double mean_degree, int max_degree);
  void infect_random_nodes(double pct, Condition* condition);

  bool is_undirected();
  void read_edges();

  /**
   * Gets this network's associated Network_Type.
   *
   * @return the network type
   */
  Network_Type* get_network_type() {
    return this->network_type;
  }

  int get_time_block(int day, int hour);
  void add_rule(Rule* rule);
  bool is_qualified(Person* person, Person* other);
  const char* get_name();
  void print_person(Person* person, FILE* fp);

  static Network* get_network(std::string name);
  static void setup_logging();

 protected:
  Network_Type* network_type;

 private:
  static bool is_log_initialized;
  static std::string network_log_level;
  static std::unique_ptr<spdlog::logger> network_logger;
};

#endif // _FRED_NETWORK_H
