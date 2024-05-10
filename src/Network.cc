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
// File: Network.cc
//

#include <spdlog/spdlog.h>

#include "Condition.h"
#include "Network.h"
#include "Network_Type.h"
#include "Person.h"
#include "Parser.h"
#include "Random.h"

bool Network::is_log_initialized = false;
std::string Network::network_log_level = "";
std::unique_ptr<spdlog::logger> Network::network_logger = nullptr;

/**
 * Creates a Network with the specified properties. The label and type ID are passed into the Group 
 * constructor, while the specified Network_Type is set as this network's type.
 *
 * @param lab the label
 * @param _type_id the network type ID
 * @param net_type the network type
 */
Network::Network(const char* lab, int _type_id, Network_Type* net_type) : Group(lab, _type_id) {
  this->network_type = net_type;
}

/**
 * Reads and initializes edges throughout this network according to a file.
 */
void Network::read_edges() {
  edge_vector_t results = Parser::get_edges(this->get_label());
  for(int i = 0; i < static_cast<int>(results.size()); ++i) {
    Network::network_logger->info("{:s}.add_edge {:d} {:d} with weight {:0.2f}", 
        this->get_label(), results[i].from_idx, results[i].to_idx, results[i].weight);
    Person* person1 = Person::get_person(results[i].from_idx);
    Person* person2 = Person::get_person(results[i].to_idx);
    if(results[i].from_idx == results[i].to_idx) {
      person1->join_network(this);
    } else {
      person1->join_network(this);
      person2->join_network(this);

      person1->add_edge_to(person2, this);
      person1->set_weight_to(person2, this, results[i].weight);

      person2->add_edge_from(person1, this);
      person2->set_weight_from(person1, this, results[i].weight);
      if(this->is_undirected()) {
        person2->add_edge_to(person1, this);
        person2->set_weight_to(person1, this, results[i].weight);
        person1->add_edge_from(person2, this);
        person1->set_weight_from(person2, this, results[i].weight);
      }
    }
  }
}

/**
 * Gets properties of this network.
 */
void Network::get_properties() {

  // set optional properties
  Parser::disable_abort_on_failure();

  // restore requiring properties
  Parser::set_abort_on_failure();
}

/**
 * Outputs details on this network to a file.
 */
void Network::print() {
  int day = Global::Simulation_Day;
  char filename[64];
  snprintf(filename, 64, "%s/RUN%d/%s-%d.txt", Global::Simulation_directory, Global::Simulation_run_number, this->get_label(), day);
  FILE* fp = fopen(filename,"w");
  int size = this->get_size();

  for(int i = 0; i < size; ++i) {
    Person* person = this->get_member(i);
    int out_degree = person->get_out_degree(this);
    int in_degree = person->get_in_degree(this);
    if(in_degree == 0 && out_degree == 0) {
      fprintf(fp, "%s.add_edge = %d %d 0.0\n", this->get_label(), person->get_id(), person->get_id());
    } else {
      for(int j = 0; j < out_degree; ++j) {
        Person* person2 = person->get_outward_edge(j,this);
        fprintf(fp, "%s.add_edge = %d %d %f\n",
            this->get_label(), person->get_id(), person2->get_id(), person->get_weight_to(person2,this));
      }
    }
  }
  fclose(fp);

  snprintf(filename, 64, "%s/RUN%d/%s-%d.vna", Global::Simulation_directory, Global::Simulation_run_number, this->get_label(), day);
  fp = fopen(filename,"w");
  fprintf(fp, "*node data\n");
  fprintf(fp, "ID age sex race\n");
  for(int i = 0; i < size; ++i) {
    Person* person = this->get_member(i);
    fprintf(fp, "%d %d %c %d\n", person->get_id(), person->get_age(), person->get_sex(),person->get_race());
  }
  fprintf(fp, "*tie data\n");
  fprintf(fp, "from to weight\n");
  for(int i = 0; i < size; ++i) {
    Person* person = this->get_member(i);
    int out_degree = person->get_out_degree(this);
    for(int j = 0; j < out_degree; ++j) {
      Person* person2 = person->get_outward_edge(j,this);
      if(is_undirected()) {
        if(person->get_id() < person2->get_id()) {
          fprintf(fp, "%d %d %f\n", person->get_id(), person2->get_id(), person->get_weight_to(person2,this));
        }
      } else {
        fprintf(fp, "%d %d %f\n", person->get_id(), person2->get_id(), person->get_weight_to(person2,this));
      }
    }
  }
  fclose(fp);

  return;
}

/**
 * Checks if there exists an outward edge connecting the first given Person to the second in this network.
 *
 * @param p1 the first person
 * @param p2 the second person
 * @return if there is a connection to
 */
bool Network::is_connected_to(Person* p1, Person* p2) {
  return p1->is_connected_to(p2, this);
}

/**
 * Checks if there exists an inward edge connecting the first given Person from the second in this network.
 *
 * @param p1 the first person
 * @param p2 the second person
 * @return if there is a connection from
 */ 
bool Network::is_connected_from(Person* p1, Person* p2) {
  return p1->is_connected_from(p2, this);
}

/**
 * Gets the mean of the out-degrees of all members of this network.
 *
 * @return the mean
 */
double Network::get_mean_degree() {
  int size = get_size();
  int total_out = 0;
  for(int i = 0; i < size; ++i) {
    Person* person = get_member(i);
    int out_degree = person->get_out_degree(this);
    total_out += out_degree;
  }
  double mean = 0.0;
  if(size != 0) {
    mean = static_cast<double>(total_out) / static_cast<double>(size);
  }
  return mean;
}

/** _UNUSED_ */
void Network::test() {
  return;
}

/**
 * Checks if this network's associated Network_Type is undirected. An undirected network 
 * does not have separate inward and outward degrees; each edge points both ways.
 *
 * @return if the network is undirected
 */
bool Network::is_undirected() {
  return this->network_type->is_undirected();
}

/**
 * Gets the Network of the Network_Type with the specified name.
 *
 * @param name the name
 * @return the network
 */
Network* Network::get_network(std::string name) {
  Network_Type* network_type = Network_Type::get_network_type(name);
  if(network_type == nullptr) {
    return nullptr;
  } else {
    return network_type->get_network();
  }
}

/**
 * Gets the time block of this network's associated Network_Type at the given day and hour.
 *
 * @param day the day
 * @param hour the hour
 * @return the time block
 */
int Network::get_time_block(int day, int hour) {
  return this->network_type->get_time_block(day, hour);
}

/**
 * Randomizes the edges of this network according to the given mean and max degree. The mean degree specifies 
 * the mean degree of members of this network, while the max degree specifies the maximum degree a member 
 * of this network can have.
 *
 * @param mean_degree the mean degree
 * @param max_degree the max degree
 */
void Network::randomize(double mean_degree, int max_degree) {
  Network::network_logger->debug("RANDOMIZE entered (mean_degree = {:0.8f}, max_degree = {:d})", 
      mean_degree, max_degree);
  int size = this->get_size();
  if(size < 2) {
    Network::network_logger->debug("RANDOMIZE exited because size of network < 2: size = {:d}", 
        size);
    return;
  }
  for(int i = 0; i < size; ++i) {
    Person* person = get_member(i);
    person->clear_network(this);
  }
  // print_stdout();
  int number_edges = mean_degree * size + 0.5;
  Network::network_logger->debug("RANDOMIZE size = {:d}  edges = {:d}", size, number_edges);
  int edges = 0;
  bool isFound = true;
  while(edges < number_edges && isFound) {
    isFound = false;
    int pos1 = Random::draw_random_int(0, size - 1);
    Person* src = this->get_member(pos1);
    while(src->get_degree(this) >= max_degree) {
      Network::network_logger->debug("RANDOMIZE src degree = {:d} >= max_degree = {:d}", 
          src->get_degree(this), max_degree);
      pos1 = Random::draw_random_int(0, size - 1);
      src = this->get_member(pos1);
    }
    Network::network_logger->debug("RANDOMIZE src person {:d} sex {:c}", src->get_id(), src->get_sex());

    // get a qualified destination

    // shuffle the order of candidates
    std::vector<int> shuffle_index;
    shuffle_index.clear();
    shuffle_index.reserve(size);
    for(int i = 0; i < size; ++i) {
      shuffle_index.push_back(i);
    }
    FYShuffle<int>(shuffle_index);

    int index = 0;
    while(index < size && !isFound) {
      int pos = shuffle_index[index];
      Person* dest = get_member(pos);
      if(dest == src) {
        ++index;
        continue;
      }
      if(dest->get_degree(this) >= max_degree) {
        ++index;
        continue;
      }
      if(src->is_connected_to(dest, this)) {
        ++index;
        continue;
      }

      src->add_edge_to(dest, this);
      dest->add_edge_from(src, this);
      if(this->is_undirected()) {
        src->add_edge_from(dest, this);
        dest->add_edge_to(src, this);
      }
      ++edges;
      isFound = true;
    }
  }
  Network::network_logger->info("RANDOMIZE size = {:d}  found = {:d} edges = {:d}  mean_degree = {:f}", 
      size, isFound, edges, (edges * 1.0) / size);
}

/**
 * Gets the name of this network's Network_Type.
 *
 * @return the name
 */
const char* Network::get_name() {
  return this->network_type->get_name();
}

/**
 * Prints details on the degrees of the specified Person in this network to a given file.
 *
 * @param person the person
 * @param fp the file
 */
void Network::print_person(Person* person, FILE* fp) {
  int out_degree = person->get_out_degree(this);
  int in_degree = person->get_in_degree(this);
  fprintf(fp, "\nNETWORK %s day %d person %d in_deg %d out_deg %d:\n", this->get_label(), Global::Simulation_Day, person->get_id(), in_degree, out_degree);
  for(int j = 0; j < out_degree; ++j) {
    Person* person2 = person->get_outward_edge(j,this);
    fprintf(fp, "NETWORK %s day %d person %d age %d sex %c race %d ", this->get_label(),
        Global::Simulation_Day, person->get_id(), person->get_age(), person->get_sex(), person->get_race());
    fprintf(fp, "other %d age %d sex %c race %d\n",  person2->get_id(),
        person2->get_age(), person2->get_sex(), person2->get_race());
  }
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Network::setup_logging() {
  if(Network::is_log_initialized) {
    return;
  }

  if(Parser::does_property_exist("network_log_level")) {
    Parser::get_property("network_log_level", &Network::network_log_level);
  } else {
    Network::network_log_level = "OFF";
  }

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Network::network_logger = std::make_unique<spdlog::logger>("network_logger", 
        sink_list.begin(), sink_list.end());
    Network::network_logger->set_level(
        Utils::get_log_level_from_string(Network::network_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Network::network_logger->trace("<{:s}, {:d}>: Network logger initialized", 
      __FILE__, __LINE__  );
  Network::is_log_initialized = true;
}
