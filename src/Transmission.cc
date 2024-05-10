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
// File: Transmission.cc
//

#include <math.h>

#include <spdlog/spdlog.h>

#include "Condition.h"
#include "Environmental_Transmission.h"
#include "Epidemic.h"
#include "Group.h"
#include "Network_Transmission.h"
#include "Parser.h"
#include "Person.h"
#include "Proximity_Transmission.h"
#include "Random.h"
#include "Transmission.h"
#include "Utils.h"

bool Transmission::is_log_initialized = false;
std::string Transmission::transmission_log_level = "";
std::unique_ptr<spdlog::logger> Transmission::transmission_logger = nullptr;

/**
 * This static factory method is used to get an instance of a
 * Transmission object of the specified subclass.
 *
 * @param transmission_mode a string containing the requested Transmission mode type
 * @return a pointer to a specific Transmission object
 */
Transmission* Transmission::get_new_transmission(char* transmission_mode) {
  if(strcmp(transmission_mode, "respiratory") == 0 || strcmp(transmission_mode, "proximity") == 0) {
    Transmission::transmission_logger->info("new Proximity_Transmission");
    return new Proximity_Transmission();
  }
  
  if(strcmp(transmission_mode, "network") == 0) {
    Transmission::transmission_logger->info("new Network_Transmission");
    return new Network_Transmission();
  }

  if(strcmp(transmission_mode, "environmental") == 0) {
    Transmission::transmission_logger->info("new Environmental_Transmission");
    return new Environmental_Transmission();
  }
  
  if(strcmp(transmission_mode, "none") == 0) {
    Transmission::transmission_logger->info("new Null_Transmission");
    return new Null_Transmission();
  }

  Utils::fred_abort("Unknown transmission_mode (%s).\n", transmission_mode);
  return nullptr;
}

/**
 * Attempts a transmission of the specified Condition to transmit in the specified Group at the given day and hour.
 * The specified destination Person will become exposed from the specified source Person who has a specified 
 * source Condition. The probability of the infection happening is the product of the specified transmission 
 * probability and the susceptibility of the destination person to the condition to transmit.
 *
 * @param transmission_prob the probability of a transmission
 * @param source the source person
 * @param dest the destination person
 * @param condition_id the source condition ID
 * @param condition_to_transmit the condition to transmit ID
 * @param day the day
 * @param hour the hour
 * @param group the group
 * @return if the transmission was successful
 */
bool Transmission::attempt_transmission(double transmission_prob, Person* source, Person* dest, int condition_id,
    int condition_to_transmit, int day, int hour, Group* group) {
  
  assert(dest->is_susceptible(condition_to_transmit));
  Transmission::transmission_logger->debug("source {:d} -- dest {:d} is susceptible", 
      source->get_id(), dest->get_id());

  double susceptibility = dest->get_susceptibility(condition_to_transmit);

  Transmission::transmission_logger->trace("<{:s}, {:d}>: susceptibility = {:f}", __FILE__, __LINE__, susceptibility);

  double r = Random::draw_random();
  double infection_prob = transmission_prob * susceptibility;

  if(r < infection_prob) {
    // successful transmission; create a new infection in dest
    source->expose(dest, condition_id, condition_to_transmit, group, day, hour);

    // notify the epidemic
    Condition::get_condition(condition_to_transmit)->get_epidemic()->become_exposed(dest, day, hour);

    return true;
  } else {
    return false;
  }
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Transmission::setup_logging() {
  if(Transmission::is_log_initialized) {
    return;
  }

  if(Parser::does_property_exist("transmission_log_level")) {
    Parser::get_property("transmission_log_level", &Transmission::transmission_log_level);
  } else {
    Transmission::transmission_log_level = "OFF";
  }

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Transmission::transmission_logger = std::make_unique<spdlog::logger>("transmission_logger", 
        sink_list.begin(), sink_list.end());
    Transmission::transmission_logger->set_level(
        Utils::get_log_level_from_string(Transmission::transmission_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Transmission::transmission_logger->trace("<{:s}, {:d}>: Transmission logger initialized", 
      __FILE__, __LINE__  );
  Transmission::is_log_initialized = true;
}
