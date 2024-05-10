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
// File: Proximity_Transmission.cc
//
#include <algorithm>

#include <spdlog/spdlog.h>

#include "Condition.h"
#include "Global.h"
#include "Group.h"
#include "Parser.h"
#include "Person.h"
#include "Place.h"
#include "Proximity_Transmission.h"
#include "Random.h"

bool Proximity_Transmission::is_log_initialized = false;
std::string Proximity_Transmission::proximity_transmission_log_level = "";
std::unique_ptr<spdlog::logger> Proximity_Transmission::proximity_transmission_logger = nullptr;

/**
 * Default constructor.
 */
Proximity_Transmission::Proximity_Transmission() {
}

/**
 * Default destructor.
 */
Proximity_Transmission::~Proximity_Transmission() {
}

/**
 * _UNUSED_
 */
void Proximity_Transmission::setup(Condition* condition) {
}

/**
 *
 * Attempt an hourly proximity transition in a Place.
 *
 * This method is the REQUIRED ENTRY POINT TO TRANSMISSION MODELS. It is pure virtual in Transmission class, so it must be overridden by subclasses.
 *
 * Steps:
 * - Get the transmissibility of the Condition itself.
 *   - Here is an example of the baseline INFluenza Condition in FRED.
 *   - Notice the transmissibility value set to 1.0.
 * \code{.unparsed}
 *   condition INF {
 *     states = S E Is Ia R Import
 *     import_start_state = Import
 *     transmission_mode = proximity
 *     transmissibility = 1.0
 *     exposed_state = E
 *   }
 * \endcode
 *
 * - Get a vector of pointers to all transmissible agents in the Place
 * - Get the count of transmissible agents (i.e how many agents in the place are transmissible for the condition | size of the vector above)
 *
 * - Get the proximity contact rate from the Place object
 * - Multiply the contact rate by the transmissibility of the Condition
 * - Multiply the contact rate by the number of hours that this transmission event lasts
 *
 * - Create a randomized list of indexes to pull from the vector of transmissible agents
 *
 * - For each transmissible agent (in randomized order) do the following:
 *   - The selected agent is the source
 *   - Determine how many contacts THIS agent will have by multiplying the contact rate from above by this agent's transmissibility for the condition
 *   - Randomly select an agent other than this agent from the Place and put it into a vector
 *
 * - Now that we have a source agent and a vector of possible host agents, for each possible host agent do the following:
 *   - Start with a transmissibility of 1.0 (i.e. Assume that the transmission of the Condition from source agent to the selected host agent will succeed)
 *   - Decide if the host is susceptible to the Condition or not
 *   - Apply any age based transmission bias to the transmission probability
 *   - Attempt a transmission of the Condition from source agent to the selected host agent
 *
 * @param day the simulation day to attempt the transmission
 * @param hour the hour of the simulation day to attempt the transmission
 * @param condition_id the id of the condition that is attempting to be transmitted
 * @param group pointer to the mixing Group where the Transmission will occur
 * @param time_block how many hours will this continue
 *
 * @see Transmission::attempt_transmission
 */
void Proximity_Transmission::transmission(int day, int hour, int condition_id, Group* group, int time_block) {

  //Proximity_Transmission must occur on a Place type
  if(group == nullptr || group->is_a_place() == false) {
    return;
  }

  Place* place = static_cast<Place*>(group);

  Proximity_Transmission::proximity_transmission_logger->info(
      "transmission day {:d} condition {:d} place {:d} {:s}", 
      day, condition_id, place->get_id(), place->get_label());

  Condition* condition = Condition::get_condition(condition_id);

  // abort if transmissibility == 0
  if(condition->get_transmissibility() == 0.0) {
    Proximity_Transmission::proximity_transmission_logger->info("no transmission");
    return;
  }

  // have place record first and last day of possible transmission
  place->record_transmissible_days(day, condition_id);

  // need at least one potential susceptible
  if(place->get_size() == 0) {
    Proximity_Transmission::proximity_transmission_logger->info("no transmission size = 0");
    return;
  }

  if(place->use_density_transmission(condition_id)) {
    this->density_transmission(day, hour, condition_id, place, time_block);
    return;
  }

  int new_exposures = 0;

  person_vector_t* transmissibles = place->get_transmissible_people(condition_id);
  int number_of_transmissibles = transmissibles->size();

  Proximity_Transmission::proximity_transmission_logger->info(
      "default_transmission DAY {:d} PLACE {:s} size {:d} trans {:d}",
      day, place->get_label(), place->get_size(), number_of_transmissibles);

  // place-specific contact rate
  double contact_rate = place->get_proximity_contact_rate();

  // scale by transmissibility of condition
  contact_rate *= condition->get_transmissibility();

  // take into account number of hours in the time_block
  contact_rate *= time_block;

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
    Person* source = (*transmissibles)[source_pos];

    if(source->is_transmissible(condition_id) == false) {
      continue;
    }

    // get the actual number of contacts to attempt to infect
    double real_contacts = contact_rate * source->get_transmissibility(condition_id);

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

    std::vector<Person*> target;
    // get a target for each contact attempt (with replacement)
    int count = 0;
    while(count < contact_count) {
      int pos = Random::draw_random_int(0, place->get_size() - 1);
      Person* other = place->get_member(pos);
      if(source != other) {
        target.push_back(other);
        ++count;
      } else {
        if(place->get_size() > 1) {
          continue; // try again
        } else {
          break; // give up
        }
      }
    }

    int condition_to_transmit = condition->get_condition_to_transmit(source->get_state(condition_id));

    for(int count = 0; count < static_cast<int>(target.size()); ++count) {
      Person* host = target[count];

      host->update_activities(day);
      if(!host->is_present(day, place)) {
        continue;
      }

      // get the transmission probs for given source/host pair
      double transmission_prob = 1.0;
      if(Global::Enable_Transmission_Bias) {
        double age_s = source->get_real_age();
        double age_h = host->get_real_age();
        double diff = fabs(age_h - age_s);
        double bias = place->get_proximity_same_age_bias();
        transmission_prob = exp(-bias * diff);
      }

      // only proceed if person is susceptible
      if(host->is_susceptible(condition_to_transmit)==false) {
        continue;
      }

      if(Transmission::attempt_transmission(transmission_prob, source, host, condition_id, condition_to_transmit, day, hour, place)) {
        ++new_exposures;
      }
    } // end contact loop
  } // end transmissible list loop

  if(new_exposures > 0) {
    Proximity_Transmission::proximity_transmission_logger->info(
      "default_transmission DAY {:d} PLACE {:s} gives {:d} new_exposures", day, place->get_label(), 
      new_exposures);
  }

  Proximity_Transmission::proximity_transmission_logger->info(
      "transmission finished day {:d} condition {:d} place {:d} {:s}", day, condition_id, place->get_id(), 
      place->get_label());

  return;
}


/**
 * Try a density transmission of the given condition.
 *
 * @param day the simulation day to attempt the transmission
 * @param hour the hour of the simulation day to attempt the transmission
 * @param condition_id the id of the condition that is attempting to be transmitted
 * @param place pointer to the Place where the Transmission will occur
 * @param time_bloc how many hours will this continue
 */
void Proximity_Transmission::density_transmission(int day, int hour, int condition_id, Place* place, int time_block) {

  Proximity_Transmission::proximity_transmission_logger->info(
      "transmission day %d hour %d condition %d place %s\n", day, hour, condition_id, place->get_label());

  person_vector_t* transmissibles = place->get_transmissible_people(condition_id);
  int number_of_transmissibles = transmissibles->size();

  Condition* condition = Condition::get_condition(condition_id);

  double contact_prob = place->get_density_contact_prob(condition_id);
  contact_prob *= condition->get_transmissibility();
  if(contact_prob > 1.0) {
    contact_prob = 1.0;
  }
  if(contact_prob < 0.0) {
    contact_prob = 0.0;
  }
  
  int number_of_susceptibles = place->get_size();
      
  // each host's probability of infection
  double prob_exposure = 1.0 - pow((1.0 - contact_prob), time_block * number_of_transmissibles);
  
  // select a number of hosts to be infected
  double expected_exposures = number_of_susceptibles * prob_exposure;
  int number_of_exposures = floor(expected_exposures);
  double remainder = expected_exposures - number_of_exposures;
  if(Random::draw_random() < remainder) {
    ++number_of_exposures;
  }

  Proximity_Transmission::proximity_transmission_logger->info(
      "DENSITY place {:s} cont {:f} size {:d} prob_exp {:f} n_exposures {:d}",
      place->get_label(), contact_prob, place->get_size(), prob_exposure, number_of_exposures);
  
  int exposed_count[number_of_transmissibles];
  for(int i = 0; i < number_of_transmissibles; ++i) {
    exposed_count[i] = 0;
  }
    
  // randomize the order of processing the susceptible list
  std::vector<int> shuffle_index;
  shuffle_index.reserve(number_of_susceptibles);
  for(int i = 0; i < number_of_susceptibles; ++i) {
    shuffle_index[i] = i;
  }
  FYShuffle<int>(shuffle_index);

  int new_exposures = 0;

  for(int j = 0; j < number_of_exposures && j < number_of_susceptibles && 0 < number_of_transmissibles; ++j) {
    Person* host = place->get_member(shuffle_index[j]);
    Proximity_Transmission::proximity_transmission_logger->info(
        "selected host {:d} age {:d}", host->get_id(), host->get_age());

    // select a random source
    int source_pos = Random::draw_random_int(0, number_of_transmissibles - 1);
    Person* source = (*transmissibles)[source_pos];

    if(source->is_transmissible(condition_id) == false) {
      continue;
    }

    int condition_to_transmit = condition->get_condition_to_transmit(source->get_state(condition_id));

    // only proceed if person is susceptible and present
    if(host->is_susceptible(condition_to_transmit) && host->is_present(day, place)) {

      // get the transmission prob for source
      double transmission_prob = source->get_transmissibility(condition_id);

      if(Transmission::attempt_transmission(transmission_prob, source, host, condition_id, condition_to_transmit, day, hour, place)) {
        // successful transmission
        ++exposed_count[source_pos];
        ++new_exposures;
      }
    } else {
      Proximity_Transmission::proximity_transmission_logger->info(
          "host {:d} not susceptible or not presents", host->get_id());
    }
  }
  
  Proximity_Transmission::proximity_transmission_logger->info(
      "DENSITY place {:s} cont {:f} size {:d} prob_exp {:f} attempts {:d} actual {:d}",
      place->get_label(), contact_prob, place->get_size(), prob_exposure,
      number_of_exposures, new_exposures);
  
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Proximity_Transmission::setup_logging() {
  if(Proximity_Transmission::is_log_initialized) {
    return;
  }

  if(Parser::does_property_exist("proximity_transmission_log_level")) {
    Parser::get_property("proximity_transmission_log_level", &Proximity_Transmission::proximity_transmission_log_level);
  } else {
    Proximity_Transmission::proximity_transmission_log_level = "OFF";
  }

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Proximity_Transmission::proximity_transmission_logger = std::make_unique<spdlog::logger>("proximity_transmission_logger", 
        sink_list.begin(), sink_list.end());
    Proximity_Transmission::proximity_transmission_logger->set_level(
        Utils::get_log_level_from_string(Proximity_Transmission::proximity_transmission_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Proximity_Transmission::proximity_transmission_logger->trace("<{:s}, {:d}>: Proximity_Transmission logger initialized", 
      __FILE__, __LINE__  );
  Proximity_Transmission::is_log_initialized = true;
}
