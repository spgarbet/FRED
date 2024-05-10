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
// File: Network_Transmission.cc
//

#include "Network_Transmission.h"
#include "Condition.h"
#include "Epidemic.h"
#include "Network.h"
#include "Parser.h"
#include "Person.h"
#include "Random.h"
#include "Utils.h"

bool Network_Transmission::is_log_initialized = false;
std::string Network_Transmission::network_transmission_log_level;
std::unique_ptr<spdlog::logger> Network_Transmission::network_transmission_logger = nullptr;

//////////////////////////////////////////////////////////
//
// NETWORK TRANSMISSION MODEL
//
//////////////////////////////////////////////////////////

/**
 * Default constructor
 */
Network_Transmission::Network_Transmission() : Transmission() {
}

/**
 * Performs a network transmission at the given day and hour. The specified Condition will be randomly transmitted 
 * throughout the given Group, or Network. The given time block specifies the time block over which transmissions occur; 
 * a larger time block will result in more transmissions.
 *
 * @param day the day
 * @param hour the hour
 * @param condition_id the condition ID
 * @param group the group, or network
 * @param time_block the time block
 */
void Network_Transmission::transmission(int day, int hour, int condition_id, Group* group, int time_block) {

  Network_Transmission::network_transmission_logger->info("network_transmission: day {:d} hour {:d} network {:s} time_block  = {:d}",
      day, hour, group ? group->get_label() : "NULL", time_block);

  if(group == nullptr || group->is_a_network() == false) {
    return;
  }

  Network* network = static_cast<Network*>(group);

  Condition* condition = Condition::get_condition(condition_id);
  double beta = condition->get_transmissibility();
  if(beta == 0.0) {
    Network_Transmission::network_transmission_logger->debug("no transmission beta {:f}", beta);
    return;
  }

  int new_exposures = 0;

  person_vector_t* transmissible = group->get_transmissible_people(condition_id);
  int number_of_transmissibles = transmissible->size();

  Network_Transmission::network_transmission_logger->debug("network_transmission: day {:d} hour {:d} network {:s} transmissibles {:d}",
      day, hour, group->get_label(), static_cast<int>(transmissible->size()));

  // randomize the order of processing the transmissible list
  std::vector<int> shuffle_index;
  shuffle_index.clear();
  shuffle_index.reserve(number_of_transmissibles);
  for(int i = 0; i < number_of_transmissibles; ++i) {
    shuffle_index.push_back(i);
  }
  FYShuffle<int>(shuffle_index);

  for(int n = 0; n < number_of_transmissibles; ++n) {
    int source_pos = shuffle_index[n];
    // transmissible visitor
    Person* source = (*transmissible)[source_pos];
    Network_Transmission::network_transmission_logger->debug("source id {:d}", source->get_id());

    if(source->is_transmissible(condition_id) == false) {
      Network_Transmission::network_transmission_logger->warn("source id {:d} not transmissible!", source->get_id());
      continue;
    }

    // get the other agents connected to the source
    person_vector_t other = source->get_outward_edges(network, 1);
    int others = other.size();
    Network_Transmission::network_transmission_logger->debug("source id {:d} has {:d} out_links", source->get_id(), others);
    if(others == 0) {
      Network_Transmission::network_transmission_logger->debug("no available others");
      continue;
    }

    // determine how many contacts to attempt
    double real_contacts = 0.0;
    if(group->get_contact_count(condition_id) > 0) {
      real_contacts = group->get_contact_count(condition_id);
    } else {
      real_contacts = group->get_contact_rate(condition_id) * others;
    }
    real_contacts *= time_block * beta * source->get_transmissibility(condition_id);
    
    // find integer contact count
    int contact_count = real_contacts;
    
    // randomly round off based on fractional part
    double fractional = real_contacts - contact_count;
    double r = Random::draw_random();
    if(r < fractional) {
      ++contact_count;
    }
    
    if(contact_count == 0) {
      continue;
    }

    int condition_to_transmit = condition->get_condition_to_transmit(source->get_state(condition_id));

    std::vector<int> shuffle_index;
    if(group->use_deterministic_contacts(condition_id)) {
      // randomize the order of contacts among others
      shuffle_index.clear();
      shuffle_index.reserve(others);
      for(int i = 0; i < others; ++i) {
        shuffle_index.push_back(i);
      }
      FYShuffle<int>(shuffle_index);
    }

    // get a destination for each contact
    for(int count = 0; count < contact_count; ++count) {
      Person* host = nullptr;
      int pos = 0;
      if(group->use_deterministic_contacts(condition_id)) {
        // select an agent from among the others without replacement
        pos = shuffle_index[count % others];
      } else {
        // select an agent from among the others with replacement
        pos = Random::draw_random_int(0, others - 1);
      }

      host = other[pos];
      Network_Transmission::network_transmission_logger->debug("source id {:d} target id {:d}", source->get_id(), host->get_id());
      // If the host is already deceased, go to the next one
      if(host->is_deceased()) {
        continue;
      }
      
      // If the host is not present in the group today, go to the next one
      host->update_activities(day);
      if(host->is_present(day, group) == false) {
        continue;
      }

      // only proceed if person is susceptible
      if(host->is_susceptible(condition_to_transmit) == false) {
        Network_Transmission::network_transmission_logger->debug("host person {:d} is not susceptible", host->get_id());
        continue;
      }

      // attempt transmission
      double transmission_prob = 1.0;
      if(Transmission::attempt_transmission(transmission_prob, source, host, condition_id, condition_to_transmit, day, hour, group)) {
        ++new_exposures;
      } else {
        Network_Transmission::network_transmission_logger->debug("no exposure");
      }
    } // end contact loop
  } // end transmissible list loop

  if(new_exposures > 0) {
    Network_Transmission::network_transmission_logger->debug("network_transmission day {:d} hour {:d} network {:s} gives {:d} new_exposures",
        day, hour, group->get_label(), new_exposures);
  }

  Network_Transmission::network_transmission_logger->info("transmission finished day {:d} condition {:d} network {:s}",
      day, condition_id, network->get_label());

  return;
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Network_Transmission::setup_logging() {
  if(Network_Transmission::is_log_initialized) {
    return;
  }
    
  if(Parser::does_property_exist("network_transmission_log_level")) {
    Parser::get_property("network_transmission_log_level", &Network_Transmission::network_transmission_log_level);
  } else {
    Network_Transmission::network_transmission_log_level = "OFF";
  }
    
  // Get the log level for Network_Transmission.cc from the properties
  Parser::get_property("network_transmission_log_level", &Network_Transmission::network_transmission_log_level);

  try {
    spdlog::sinks_init_list sink_list = { Global::StdoutSink, Global::ErrorFileSink, Global::DebugFileSink, Global::TraceFileSink };
    Network_Transmission::network_transmission_logger =
          std::make_unique<spdlog::logger>("network_transmission_logger", sink_list.begin(), sink_list.end());
    Network_Transmission::network_transmission_logger->set_level(
          Utils::get_log_level_from_string(Network_Transmission::network_transmission_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }
  Network_Transmission::is_log_initialized = true;
}
