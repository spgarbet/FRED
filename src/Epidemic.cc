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
// File: Epidemic.cc
//
#include <stdlib.h>
#include <stdio.h>
#include <new>
#include <iostream>
#include <vector>
#include <map>
#include <set>

#include "Condition.h"
#include "Date.h"
#include "Epidemic.h"
#include "Events.h"
#include "Expression.h"
#include "Geo.h"
#include "Global.h"
#include "Hospital.h"
#include "Household.h"
#include "Natural_History.h"
#include "Neighborhood_Layer.h"
#include "Network.h"
#include "Network_Type.h"
#include "Parser.h"
#include "Person.h"
#include "Place.h"
#include "Place_Type.h"
#include "Random.h"
#include "Rule.h"
#include "Transmission.h"
#include "Utils.h"
#include "Visualization_Layer.h"

bool Epidemic::is_log_initialized = false;
std::string Epidemic::epidemic_log_level = "";
std::unique_ptr<spdlog::logger> Epidemic::epidemic_logger = nullptr;


/**
 * This static factory method is used to get an instance of a specific
 * Epidemic Model. Depending on the model, it will create a
 * specific Epidemic Model and return a pointer to it.
 */

Epidemic* Epidemic::get_epidemic(Condition* condition) {
  return new Epidemic(condition);
}

//////////////////////////////////////////////////////
//
// CONSTRUCTOR
//
//

/**
 * Creates an Epidemic model with the specified Condition. This epidemic's ID will 
 * be set to the condition's ID, and this epidemic's name will be set to the name 
 * of the condition's Natural_History. Default variables are initialized.
 *
 * @param _condition the condition
 */
Epidemic::Epidemic(Condition* _condition) {

  this->condition = _condition;
  this->id = _condition->get_id();
  strcpy(this->name, _condition->get_natural_history()->get_name());

  // these are total (among current population)
  this->total_cases = 0;

  // reporting
  this->report_generation_time = false;
  this->total_serial_interval = 0.0;
  this->total_secondary_cases = 0;
  this->enable_health_records = false;

  this->RR = 0.0;
  this->natural_history = nullptr;

  this->daily_cohort_size = new int[Global::Simulation_Days];
  this->number_infected_by_cohort = new int[Global::Simulation_Days];
  for(int i = 0; i < Global::Simulation_Days; ++i) {
    this->daily_cohort_size[i] = 0;
    this->number_infected_by_cohort[i] = 0;
  }

  this->new_exposed_people_list.clear();
  this->active_people_list.clear();
  this->transmissible_people_list.clear();
  this->group_state_count.clear();
  this->total_group_state_count.clear();
  this->susceptible_count = 0;

  this->vis_case_fatality_loc_list.clear();
  this->enable_visualization = false;

  this->visualize_state = nullptr;
  this->visualize_state_place_type = nullptr;
  this->track_counts_for_group_state = nullptr;
  this->incidence_count = nullptr;
  this->current_count = nullptr;
  this->total_count = nullptr;
  this->daily_incidence_count = nullptr;
  this->daily_current_count = nullptr;
  this->vis_dormant_loc_list = nullptr;
  this->number_of_states = 0;
  this->import_agent = nullptr;


}

/**
 * Clears this epidemic's group_state_count and total_group_state_count vectors before deletion.
 */
Epidemic::~Epidemic() {
  this->group_state_count.clear();
  this->total_group_state_count.clear();
}

//////////////////////////////////////////////////////
//
// SETUP
//
//

/**
 * Sets up the environment for the simulation.
 */
void Epidemic::setup() {
  // read optional properties
  Parser::disable_abort_on_failure();

  int temp = 0;
  Parser::get_property(this->name, "report_generation_time", &temp);
  this->report_generation_time = temp;

  temp = 0;
  Parser::get_property(this->name, "enable_health_records", &temp);
  this->enable_health_records = temp;

  // initialize state specific-variables here:
  this->natural_history = this->condition->get_natural_history();

  this->number_of_states = this->natural_history->get_number_of_states();

  //vectors of group state counts, size to number of states
  this->group_state_count.reserve(this->number_of_states);
  for(int i = 0; i < this->number_of_states; ++i) {
    group_counter_t new_counter;
    this->group_state_count.push_back(new_counter);
  }
  this->total_group_state_count.reserve(this->number_of_states);
  for(int i = 0; i < this->number_of_states; ++i) {
    group_counter_t new_counter;
    this->total_group_state_count.push_back(new_counter);
  }

  // initialize state counters
  this->incidence_count = new int[this->number_of_states];
  this->total_count = new int[this->number_of_states];
  this->current_count = new int[this->number_of_states];
  this->daily_incidence_count = new int*[this->number_of_states];
  this->daily_current_count = new int*[this->number_of_states];

  // visualization loc lists for dormant states
  this->vis_dormant_loc_list = new vis_loc_vec_t[this->number_of_states];
  for(int i = 0; i < this->number_of_states; ++i) {
    this->vis_dormant_loc_list[i].clear();
  }

  // read state specific properties
  visualize_state = new bool[this->number_of_states];
  visualize_state_place_type = new int[this->number_of_states];

  for(int i = 0; i < this->number_of_states; ++i) {
    char label[FRED_STRING_SIZE];
    this->incidence_count[i] = 0;
    this->total_count[i] = 0;
    this->current_count[i] = 0;

    this->daily_incidence_count[i] = new int[Global::Simulation_Days + 1];
    this->daily_current_count[i] = new int[Global::Simulation_Days + 1];
    for(int day = 0; day <= Global::Simulation_Days; ++day) {
      this->daily_incidence_count[i][day] = 0;
      this->daily_current_count[i][day] = 0;
    }
    size_t buffer_size = FRED_STRING_SIZE;
    snprintf(label, buffer_size, "%s.%s", this->natural_history->get_name(), this->natural_history->get_state_name(i).c_str());

    // do we collect data for visualization?
    int n = 0;
    Parser::get_property(label, "visualize", &n);
    this->visualize_state[i] = n;
    if(Global::Enable_Visualization_Layer && n > 0) {
      this->enable_visualization = true;
    }

    char type_name[FRED_STRING_SIZE];
    snprintf(type_name, buffer_size, "Household");
    Parser::get_property(label, "visualize_place_type", type_name);
    this->visualize_state_place_type[i] = Place_Type::get_type_id(type_name);
  }

  // read in transmissible networks if using network transmission
  if(strcmp(this->condition->get_transmission_mode(), "network") == 0) {
    this->transmissible_networks.clear();
    std::string nets = "";
    Parser::get_property(this->name, "transmissible_networks", &nets);
    string_vector_t net_vec = Utils::get_string_vector(nets, ' ');

    for(int i = 0; i < static_cast<int>(net_vec.size()); ++i) {
      Epidemic::epidemic_logger->info("transmissible network for {:s} is {:s}", this->name, 
          net_vec[i].c_str());
      Network* network = Network_Type::get_network_type(net_vec[i])->get_network();
      if(network != nullptr) {
        this->transmissible_networks.push_back(network);
      } else {
        Epidemic::epidemic_logger->error("Help: no network named {:s} found.", net_vec[i].c_str());
      }
    }
  }

  // restore requiring properties
  Parser::set_abort_on_failure();

  Epidemic::epidemic_logger->info("setup for epidemic condition {:s} finished", this->name);
}


//////////////////////////////////////////////////////
//
// FINAL PREP FOR THE RUN
//
//

/**
 * Initializes the tracking of group state counts for each condition state and Group_Type 
 * to false.
 */
void Epidemic::prepare_to_track_counts() {
  this->track_counts_for_group_state = new bool*[this->number_of_states];
  int number_of_group_types = Group_Type::get_number_of_group_types();
  for(int state = 0; state < this->number_of_states; ++state) {
    this->track_counts_for_group_state[state] = new bool[number_of_group_types];
    for(int type = 0; type < number_of_group_types; ++type) {
      this->track_counts_for_group_state[state][type] = false;
    }
  }
}

/**
 * Prepares the environment for the simulation.
 */
void Epidemic::prepare() {

  Epidemic::epidemic_logger->info("Epidemic::prepare epidemic {:s} started", this->name);

  // set maximum number of loops in update_state()
  if(Global::Max_Loops == -1) {
    Global::Max_Loops = Person::get_population_size();
  }
  Epidemic::epidemic_logger->info("Max_Loops {:d}", Global::Max_Loops);

  int number_of_group_types = Group_Type::get_number_of_group_types();
  for(int state = 0; state < this->number_of_states; ++state) {
    for(int type = 0; type < number_of_group_types; ++type) {
      if(this->track_counts_for_group_state[state][type]) {
        Epidemic::epidemic_logger->info("TRACKING state {:s}.{:s} for group type {:s}",  this->name, 
            this->natural_history->get_state_name(state).c_str(), 
            Group_Type::get_group_type_name(type).c_str());
      }
    }
  }

  // setup administrative agents
  int admin_agents = Person::get_number_of_admin_agents();
  for(int p = 0; p < admin_agents; ++p) {
    Person* admin_agent = Person::get_admin_agent(p);
    int new_state = this->natural_history->get_admin_start_state();
    if(0 <= new_state) {
      Epidemic::epidemic_logger->info("Epidemic::initialize {:s} admin_agent {:d} to state {:s}", 
          this->name, admin_agent->get_id(), this->natural_history->get_state_name(new_state).c_str());
    }
    this->update_state(admin_agent, 0, 0, new_state, 0);
  }

  // initialize the population
  int day = 0;
  int popsize = Person::get_population_size();
  for(int p = 0; p < popsize; ++p) {
    Person* person = Person::get_person(p);
    this->initialize_person(person, day);
  }

  // setup meta_agent responsible for exogeneous transmission
  this->import_agent = Person::get_import_agent();
  int new_state = this->natural_history->get_import_start_state();
  if(0 <= new_state) {
    Epidemic::epidemic_logger->info("Epidemic::initialize meta_agent {:s}", this->name);
    this->update_state(this->import_agent, 0, 0, new_state, 0);
  }

  // setup visualization directories
  if(this->enable_visualization) {
    this->create_visualization_data_directories();
    Epidemic::epidemic_logger->info("visualization directories created");
  }

  Epidemic::epidemic_logger->info("epidemic prepare finished");
}

/**
 * Initializes a given Person at a specified day in the simulation for this epidemic.
 *
 * @param person the person
 * @param day the day
 */
void Epidemic::initialize_person(Person* person, int day) {

  Epidemic::epidemic_logger->info("Epidemic::initialize_person %s started\n", this->name);

  int new_state = 0;
  int hour = 0;

  if(new_state == this->natural_history->get_exposed_state()) {
    // person is initially exposed
    person->become_exposed(this->id, Person::get_import_agent(), nullptr, day, hour);
    this->new_exposed_people_list.push_back(person);
  }

  // update epidemic counters
  this->update_state(person, day, 0, new_state, 0);

  Epidemic::epidemic_logger->info("Epidemic::initialize_person %s finished\n", this->name);
}


///////////////////////////////////////////////////////
//
// DAILY UPDATES
//
//

/**
 * Updates the simulation for a given day and hour. Handles transitions and transmissions.
 *
 * @param day the day
 * @param hour the hour
 */
void Epidemic::update(int day, int hour) {

  Epidemic::epidemic_logger->info("epidemic update for condition {:s} day {:d} hour {:d}", 
      this->name, day, hour);
  Utils::fred_start_epidemic_timer();

  int step = 24 * day + hour;
  Epidemic::epidemic_logger->info("epidemic update for condition {:s} day {:d} hour {:d} step {:d}", 
      this->name, day, hour, step);

  if(hour == 0) {
    this->prepare_for_new_day(day);
  }

  // handle scheduled transitions for import_agent
  int size = this->meta_agent_transition_event_queue.get_size(step);
  for(int i = 0; i < size; ++i) {
    Person* person = this->meta_agent_transition_event_queue.get_event(step, i);
    this->update_state(person, day, hour, -1, 0);
  }

  // handle scheduled transitions
  size = this->state_transition_event_queue.get_size(step);
  Epidemic::epidemic_logger->debug("TRANSITION_EVENT_QUEUE day {:d} {:s} hour {:d} cond {:s} size {:d}",
      day, Date::get_date_string().c_str(), hour, this->name, size);

  for(int i = 0; i < size; ++i) {
    Person* person = this->state_transition_event_queue.get_event(step, i);
    this->update_state(person, day, hour, -1, 0);
  }
  this->state_transition_event_queue.clear_events(step);

  if(this->condition->get_transmissibility() > 0.0) {
    Epidemic::epidemic_logger->debug("update transmissions for condition {:s} with transmissibility = {:f}\n",
        this->name, this->condition->get_transmissibility());

    if(strcmp(this->condition->get_transmission_mode(), "proximity") == 0) {
      this->update_proximity_transmissions(day, hour);
    }

    if(strcmp(this->condition->get_transmission_mode(), "respiratory") == 0) {
      this->update_proximity_transmissions(day, hour);
    }

    if(strcmp(this->condition->get_transmission_mode(), "network") == 0) {
      this->update_network_transmissions(day, hour);
    }
  }

  return;
}

/**
 * Spreads the infection in places attended by transmissible people at a given day and hour. 
 * Will only spread at place types that are open at the specified time.
 *
 * @param day the day
 * @param hour the hour
 */
void Epidemic::update_proximity_transmissions(int day, int hour) {

  int number_of_place_types = Place_Type::get_number_of_place_types();
  for(int type = 0; type < number_of_place_types; ++type) {
    Place_Type* place_type = Place_Type::get_place_type(type);
    int time_block = place_type->get_time_block(day, hour);
    if(time_block > 0) {
      Epidemic::epidemic_logger->debug("place_type {:s} opens at hour {:d} on {:s} for {:d} hours on {:s}",
          place_type->get_name(), hour, Date::get_day_of_week_string().c_str(), time_block, 
          Date::get_date_string().c_str());
      this->find_active_places_of_type(day, hour, type);
      this->transmission_in_active_places(day, hour, time_block);
    } else {
      Epidemic::epidemic_logger->debug("place_type {:s} does not open at hour {:d} on {:s} on {:s}", 
          place_type->get_name(), hour, Date::get_day_of_week_string().c_str(),
          Date::get_date_string().c_str());
    }
  }
}

/**
 * Spreads the infection in networks attended by transmissible people at a given day and hour. 
 * Will only spread through networks who have a transmissible member.
 *
 * @param day the day
 * @param hour the hour
 */
void Epidemic::update_network_transmissions(int day, int hour) {


  int number_of_networks = Network_Type::get_number_of_network_types();
  for(int i = 0; i < number_of_networks; ++i) {
    Network* network = Network_Type::get_network_number(i);
    if(network->can_transmit(this->id) == false) {
      continue;
    }
    int time_block = network->get_time_block(day, hour);
    if(time_block == 0) {
      continue;
    }
    if(network->has_admin_closure()) {
      Epidemic::epidemic_logger->debug("network {:s} has an admin closure on day {:d} hour {:d}", 
          network->get_label(), day, hour);
      continue;
    }
    Epidemic::epidemic_logger->debug("network {:s} is open at hour {:d} on {:s} for {:d} hours on {:s}", 
        network->get_label(), hour, Date::get_day_of_week_string().c_str(), time_block, 
        Date::get_date_string().c_str());

    // is this network active (does it have transmissible people attending?)
    bool active = false;
    for(person_set_iterator itr = this->transmissible_people_list.begin(); itr != this->transmissible_people_list.end(); ++itr) {
      Person* person = (*itr);
      assert(person != nullptr);
      if(person->is_member_of_network(network)) {
        person->update_activities(day);
        if(person->is_present(day, network)) {
          Epidemic::epidemic_logger->debug("FOUND transmissible person {:d} day {:d} network {:s}", 
              person->get_id(), day, network->get_label());
          network->add_transmissible_person(this->id, person);
          active = true;
        } else {
          Epidemic::epidemic_logger->debug(
              "FOUND transmissible person {:d} day {:d} NOT PRESENT network {:s}", 
              person->get_id(), day, network->get_label());
        }
      } else {
        Epidemic::epidemic_logger->debug(
            "FOUND transmissible person {:d} day {:d} NOT MEMBER OF network {:s}", 
            person->get_id(), day, network->get_label());
      }
    }
    if(active) {
      Epidemic::epidemic_logger->debug("network {:s} is active day {:d} transmissible_people = {:d}",
          network->get_label(), day, network->get_number_of_transmissible_people(this->id));
      this->condition->get_transmission()->transmission(day, hour, this->id, network, time_block);
      // prepare for next step
      network->clear_transmissible_people(this->id);
    } else {
      Epidemic::epidemic_logger->debug("network {:s} is not active day {:d} transmissible_people = {:d}", 
          network->get_label(), day, get_number_of_transmissible_people());
    }
  }
}

/**
 * Prepares for the next day by clearing dormant location lists.
 *
 * @param day the day
 */
void Epidemic::prepare_for_new_day(int day) {

  Epidemic::epidemic_logger->info("epidemic {:s} prepare for new day {:d}", this->name, day);

  if(day > 0) {
    for(int i = 0; i < this->number_of_states; ++i) {
      this->vis_dormant_loc_list[i].clear();
    }
  }
}

/**
 * Finds active places of a specified place type at a given day and hour. Active places are places 
 * where a transmissible person is present during the given time.
 *
 * @param day the day
 * @param hour the hour
 * @param place_type_id the place type ID
 */
void Epidemic::find_active_places_of_type(int day, int hour, int place_type_id) {

  Place_Type* place_type = Place_Type::get_place_type(place_type_id);
  Epidemic::epidemic_logger->info(
      "find_active_places_of_type {:s} day {:d} hour {:d} transmissible_people = {:d}",
      place_type->get_name(), day, hour, this->get_number_of_transmissible_people());

  this->active_places_list.clear();
  for(person_set_iterator itr = this->transmissible_people_list.begin(); itr != this->transmissible_people_list.end(); ++itr) {
    Person* person = (*itr);
    assert(person != nullptr);
    person->update_activities(day);
    Place* place = person->get_place_of_type(place_type_id);
    if(place == nullptr) {
      continue;
    }
    Epidemic::epidemic_logger->info("find_active_places_of_type {:d} day {:d} person {:d} place {:s}",
        place_type_id, day, person->get_id(), place ? place->get_label() : "NULL");
    if(place->has_admin_closure()) {
      Epidemic::epidemic_logger->debug("place {:s} has admin closure", place->get_label());
      continue;
    }
    if(person->is_present(day, place)) {
      Epidemic::epidemic_logger->debug("FOUND transmissible person {:d} day {:d} hour {:d} place {:s}", 
          person->get_id(), day, hour, place->get_label());
      place->add_transmissible_person(this->id, person);
      this->active_places_list.insert(place);
    }
  }
  if(this->active_places_list.size() > 0) {
    Epidemic::epidemic_logger->info("find_active_places_of_type {:s} found {:d}", 
        place_type->get_name(), this->active_places_list.size());
  }
}

/**
 * Performs transmission at a given day, hour, and time block for all active places. The transmission will be dependent 
 * on the transmission method of the epidemics associated condition.
 *
 * @param day the day
 * @param hour the hour
 * @param time_block the time block
 */
void Epidemic::transmission_in_active_places(int day, int hour, int time_block) {
  for(place_set_iterator itr = active_places_list.begin(); itr != this->active_places_list.end(); ++itr) {
    Place* place = (*itr);
    if(this->condition->get_transmission() != nullptr) {
      this->condition->get_transmission()->transmission(day, hour, this->id, place, time_block);
    }

    // prepare for next step
    place->clear_transmissible_people(this->id);
  }
  return;
}


//////////////////////////////////////////////////////////////
//
// HANDLING CHANGES TO AN INDIVIDUAL'S STATUS
//
//


// called from attempt_transmission():

/**
 * Adds a person to the exposed people list at a given day and hour.
 *
 * @param person the person
 * @param day the day
 * @param hour the hour
 */
void Epidemic::become_exposed(Person* person, int day, int hour) {
  int new_state = this->natural_history->get_exposed_state();
  this->update_state(person, day, hour, new_state, 0);
  this->new_exposed_people_list.push_back(person);
}

/**
 * Adds a person to the active people list at a given day. Increments the total cases count.
 *
 * @param person the person
 * @param day the day
 */
void Epidemic::become_active(Person* person, int day) {
  Epidemic::epidemic_logger->info("Epidemic::become_active day {:d} person {:d}", day, person->get_id());

  // add to active people list
  this->active_people_list.insert(person);

  // update epidemic counters
  ++this->total_cases;
}

/**
 * Inactivates a transmissible person at a given day and hour. They will be removed from both 
 * the transmissible and active people list.
 *
 * @param person the person
 * @param day the day
 * @param hour the hour
 */
void Epidemic::inactivate(Person* person, int day, int hour) {

  Epidemic::epidemic_logger->info("inactivate day {:d} person {:d}", day, person->get_id());

  person_set_iterator itr = this->transmissible_people_list.find(person);
  if(itr != this->transmissible_people_list.end()) {
    // delete from transmissible list
    this->transmissible_people_list.erase(itr);
  }

  itr = this->active_people_list.find(person);
  if(itr != this->active_people_list.end()) {
    // delete from active list
    Epidemic::epidemic_logger->debug("DELETE from ACTIVE_PEOPLE_LIST day {:d} person {:d}", 
        Global::Simulation_Day, person->get_id());
    this->active_people_list.erase(itr);
  }

  if(this->enable_visualization) {
    int state = person->get_state(this->id);
    if(this->visualize_state[state]) {
      Place* place = person->get_place_of_type(this->visualize_state_place_type[state]);
      if(place != nullptr) {
        double lat = place->get_latitude();
        double lon = place->get_longitude();
        VIS_Location* vis_loc = new VIS_Location(lat,lon);
        this->vis_dormant_loc_list[state].push_back(vis_loc);
      }
    }
  }

  Epidemic::epidemic_logger->info("inactivate day {:d} person {:d} finished", day, person->get_id());
}

/**
 * Terminates a person at a given day. That person will remain in their current 
 * health state. If that health state is fatal, the current count and daily current 
 * count for that state are decremented. That person is then removed from both the 
 * active and transmissible people list. If the person was ever exposed to this 
 * epidemic, the total cases of this epidemic are decremented.
 *
 * @param person the person
 * @param day the day
 */
void Epidemic::terminate_person(Person* person, int day) {

  Epidemic::epidemic_logger->info("EPIDEMIC {:s} TERMINATE person {:d} day {:d}", this->name, person->get_id(), day);

  // note: the person is still in its current health state
  int state = person->get_state(this->id);

  if(0 <= state && this->natural_history->is_fatal_state(state)==false) {
    current_count[state]--;
    daily_current_count[state][day]--;
    Epidemic::epidemic_logger->info("EPIDEMIC TERMINATE person {:d} day {:d} {:s} removed from state {:d}",
        person->get_id(), day, Date::get_date_string().c_str(), state);
  }

  this->delete_from_epidemic_lists(person);

  // update total case count
  if(person->was_ever_exposed(this->id)) {
    this->total_cases--;
  }

  // cancel any scheduled transition
  int transition_step = person->get_next_transition_step(this->id);
  if(24 * day <= transition_step) {
    this->state_transition_event_queue.delete_event(transition_step, person);
  }
  person->set_next_transition_step(this->id, -1);

  Epidemic::epidemic_logger->info("EPIDEMIC TERMINATE person {:d} finished", person->get_id());
}

//////////////////////////////////////////////////////////////
//
// REPORTING
//
//

/**
 * Generates a report of epidemic statistics for a given day.
 *
 * @param day the day
 */
void Epidemic::report(int day) {
  this->print_stats(day);
  Utils::fred_print_lap_time("day %d %s report", day, this->name);
  if(this->enable_visualization && (day % Global::Visualization->get_period() == 0)) {
    this->print_visualization_data(day);
    Utils::fred_print_lap_time("day %d %s print_visualization_data", day, this->name);
  }
}

/**
 * Prints statistics on the epidemic for a given day. Sets the daily cohort size 
 * and prepares the simulation for the next day.
 *
 * @param day the day
 */
void Epidemic::print_stats(int day) {
  Epidemic::epidemic_logger->info("epidemic print stats for condition {:d} day {:d}", id, day);

  // set daily cohort size
  this->daily_cohort_size[day] = this->new_exposed_people_list.size();

  if(this->report_generation_time || Global::Report_Serial_Interval) {
    this->report_serial_interval(day);
  }

  // prepare for next day
  this->new_exposed_people_list.clear();
  if(this->natural_history != nullptr) {
    for(int i = 0; i < this->number_of_states; ++i) {
      this->incidence_count[i] = 0;
      this->daily_current_count[i][day + 1] = this->daily_current_count[i][day];
    }
  }

  Epidemic::epidemic_logger->info("epidemic finished print stats for condition {:d} day {:d}", 
      id, day);
}

/**
 * Tracks the serial interval of secondary cases. The serial interval is the amount of days it took for a source person 
 * to infect another person. Secondary cases are cases that were transmitted from another person. Statistics on the 
 * serial interval are then logged for the given day.
 *
 * @param day the day
 */
void Epidemic::report_serial_interval(int day) {

  int people_exposed_today = this->new_exposed_people_list.size();
  for(int i = 0; i < people_exposed_today; ++i) {
    Person* host = this->new_exposed_people_list[i];
    Person* source = host->get_source(id);
    if(source != nullptr) {
      int serial_interval = host->get_exposure_day(this->id) - source->get_exposure_day(this->id);
      this->total_serial_interval += static_cast<double>(serial_interval);
      ++this->total_secondary_cases;
    }
  }

  double mean_serial_interval = 0.0;
  if(this->total_secondary_cases > 0) {
    mean_serial_interval = this->total_serial_interval / static_cast<double>(this->total_secondary_cases);
  }

  if(Global::Report_Serial_Interval) {
    //Write to log file
    Epidemic::epidemic_logger->info("day {:d} SERIAL_INTERVAL: ser_int {:.2f}", 
        day, mean_serial_interval);
  }
}

/**
 * Creates directories for visualization data if data visualization is enabled.
 */
void Epidemic::create_visualization_data_directories() {
  char vis_var_dir[FRED_STRING_SIZE];
  // create directories for each state
  for(int i = 0; i < this->number_of_states; ++i) {
    if(this->visualize_state[i]) {
      snprintf(vis_var_dir, 
          FRED_STRING_SIZE,
          "%s/%s.%s",
          Global::Visualization_directory,
          this->name,
          this->natural_history->get_state_name(i).c_str());
      Utils::fred_make_directory(vis_var_dir);
      snprintf(vis_var_dir,
          FRED_STRING_SIZE,
          "%s/%s.new%s",
          Global::Visualization_directory,
          this->name,
          this->natural_history->get_state_name(i).c_str());
      Utils::fred_make_directory(vis_var_dir);
      // add this variable name to visualization list
      char filename[FRED_STRING_SIZE];
      snprintf(filename, FRED_STRING_SIZE, "%s/VARS", Global::Visualization_directory);
      FILE* fp = fopen(filename, "a");
      if(fp != nullptr) {
        fprintf(fp, "%s.%s\n", this->name, this->natural_history->get_state_name(i).c_str());
        fprintf(fp, "%s.new%s\n", this->name, this->natural_history->get_state_name(i).c_str());
        fclose(fp);
      }
    }
  }
}

/**
 * Prints visualization data for a given day.
 *
 * @param day the day
 */
void Epidemic::print_visualization_data(int day) {
  char filename[FRED_STRING_SIZE];
  Person* person;
  double lat, lon;
  char location[FRED_STRING_SIZE];
  FILE* statefp[this->number_of_states];
  FILE* newstatefp[this->number_of_states];

  // open file pointers for each state
  for(int i = 0; i < this->number_of_states; ++i) {
    statefp[i] = nullptr;
    newstatefp[i] = nullptr;
    if(this->visualize_state[i]) {
      snprintf(filename,
          FRED_STRING_SIZE,
          "%s/%s.new%s/loc-%d.txt",
          Global::Visualization_directory,
          this->name,
          this->natural_history->get_state_name(i).c_str(),
          day);
      newstatefp[i] = fopen(filename, "w");
      snprintf(filename,
          FRED_STRING_SIZE,
          "%s/%s.%s/loc-%d.txt",
          Global::Visualization_directory,
          this->name,
          this->natural_history->get_state_name(i).c_str(),
          day);
      statefp[i] = fopen(filename, "w");
    }
  }

  for(person_set_iterator itr = this->active_people_list.begin(); itr != this->active_people_list.end(); ++itr) {
    person = (*itr);
    int state = person->get_state(this->id);
    assert(0 <= state);
    if(this->visualize_state[state]) {
      Place* place = person->get_place_of_type(this->visualize_state_place_type[state]);
      if(place != nullptr) {
        lat = place->get_latitude();
        lon = place->get_longitude();
        snprintf(location, FRED_STRING_SIZE, "%f %f\n", lat, lon);
        if(day == person->get_last_transition_step(this->id) / 24) {
          fputs(location, newstatefp[state]);
        }
        fputs(location, statefp[state]);
      }
    }
  }

  // print data for dormant people
  for(int state = 0; state < this->number_of_states; ++state) {
    if(this->visualize_state[state]) {
      if(this->natural_history->is_dormant_state(state)) {
        int size = this->vis_dormant_loc_list[state].size();
        for(int i = 0; i < size; ++i) {
          fprintf(newstatefp[state], "%f %f\n",
            this->vis_dormant_loc_list[state][i]->get_lat(),
            this->vis_dormant_loc_list[state][i]->get_lon());
          fprintf(statefp[state], "%f %f\n",
            this->vis_dormant_loc_list[state][i]->get_lat(),
            this->vis_dormant_loc_list[state][i]->get_lon());
        }
      }
    }
  }

  // print data for case_fatalities
  for(int state = 0; state < this->number_of_states; ++state) {
    if(this->natural_history->is_fatal_state(state)) {
      if(this->visualize_state[state]) {
        int size = this->vis_case_fatality_loc_list.size();
        for(int i = 0; i < size; ++i) {
          fprintf(newstatefp[state], "%f %f\n",
            this->vis_case_fatality_loc_list[i]->get_lat(),
            this->vis_case_fatality_loc_list[i]->get_lon());
          fprintf(statefp[state], "%f %f\n",
            this->vis_case_fatality_loc_list[i]->get_lat(),
            this->vis_case_fatality_loc_list[i]->get_lon());
        }
      }
    }
  }
  this->vis_case_fatality_loc_list.clear();

  // close file pointers for each state
  for(int i = 0; i < this->number_of_states; ++i) {
    if(statefp[i] != nullptr) {
      fclose(statefp[i]);
    }
    if(newstatefp[i] != nullptr) {
      fclose(newstatefp[i]);
    }
  }

}

/**
 * Deletes a terminated person from both the active and transmissible people lists.
 *
 * @param person the person
 */
void Epidemic::delete_from_epidemic_lists(Person* person) {

  // this only happens for terminated people
  Epidemic::epidemic_logger->info("deleting terminated person {:d} from active_people_list list", 
      person->get_id());

  person_set_iterator itr = this->active_people_list.find(person);
  if(itr != this->active_people_list.end()) {
    // delete from active list
    Epidemic::epidemic_logger->debug("DELETE from ACTIVE_PEOPLE_LIST day {:d} person {:d}", 
        Global::Simulation_Day, person->get_id());
    this->active_people_list.erase(itr);
  }

  itr = this->transmissible_people_list.find(person);
  if(itr != this->transmissible_people_list.end()) {
    // delete from transmissible list
    Epidemic::epidemic_logger->debug("DELETE from TRANSMISSIBLE_PEOPLE_LIST day {:d} person {:d}", 
        Global::Simulation_Day, person->get_id());
    this->transmissible_people_list.erase(itr);
  }

}

/**
 * Updates the condition state of a person to a new state at a given day and hour.
 *
 * @param person the person
 * @param day the day
 * @param hour the hour
 * @param new_state the new state
 * @param loop_counter counts the number of recursive loops
 */
void Epidemic::update_state(Person* person, int day, int hour, int new_state, int loop_counter) {

  int step = 24 * day + hour;
  int old_state = person->get_state(this->id);

  double age = person->get_real_age();

  Epidemic::epidemic_logger->info(
      "UPDATE_STATE ENTERED condition {:s} day {:d} hour {:d} person {:d} age {:.2f} old_state {:s} new_state {:s}",
      this->name, day, hour, person->get_id(), age, this->natural_history->get_state_name(old_state).c_str(), 
      this->natural_history->get_state_name(new_state).c_str());

  if(new_state < 0) {
    // this is a scheduled state transition
    new_state = this->natural_history->get_next_state(person, old_state);
    assert(0 <= new_state);

    // this handles a new exposure as the result of get_next_state()
    if(new_state == this->natural_history->get_exposed_state() && person->get_exposure_day(this->id) < 0) {
      person->become_exposed(this->id, Person::get_import_agent(), nullptr, day, hour);
    }
  } else {

    // the following applies iff we are changing state through a state
    // modification process or a cross-infection from another condition:

    // cancel any scheduled transition
    int transition_step = person->get_next_transition_step(this->id);
    if(step <= transition_step) {
      this->state_transition_event_queue.delete_event(transition_step, person);
    }
  }

  // at this point we should not have any transitions scheduled
  person->set_next_transition_step(this->id, -1);

  // get the next transition step
  int transition_step = this->natural_history->get_next_transition_step(person, new_state, day, hour);

  // DEBUGGING
  Epidemic::epidemic_logger->debug(
      "UPDATE_STATE condition {:s} day {:d} hour {:d} person {:d} age {:.2f} "
      "race {:d} sex {:c} old_state {:d} new_state {:d} next_transition_step {:d}",
      this->name, day, hour, person->get_id(), age, person->get_race(), person->get_sex(), 
      old_state, new_state, transition_step);

  if(transition_step > step) {

    Epidemic::epidemic_logger->debug(
        "UPDATE_STATE day {:d} hour {:d} adding person {:d} to state_transition_event_queue for step {:d}", 
        day, hour, person->get_id(), transition_step);

    if(person->is_meta_agent()) {
      Epidemic::epidemic_logger->debug(
          "UPDATE_STATE META cond {:s} day {:d} hour {:d} adding person {:d} with "
          "old_state {:d} new_state {:d} step {:d} to meta_agent_transition_event_queue for step {:d}",
          this->name, day, hour, person->get_id(), old_state, new_state, step, transition_step);
      this->meta_agent_transition_event_queue.add_event(transition_step, person);
    } else {
      this->state_transition_event_queue.add_event(transition_step, person);
    }

    person->set_next_transition_step(this->id, transition_step);
  }

  // does entering this state cause the import_agent to cause transmissions?
  if(person == this->import_agent) {
    if(0 <= new_state) {
      int max_imported = this->natural_history->get_import_count(new_state);
      double per_cap = this->natural_history->get_import_per_capita_transmissions(new_state);
      if(max_imported > 0 || per_cap > 0.0) {
        fred::geo lat = this->natural_history->get_import_latitude(new_state);
        fred::geo lon = this->natural_history->get_import_longitude(new_state);
        double radius = this->natural_history->get_import_radius(new_state);
        long long int admin_code = this->natural_history->get_import_admin_code(new_state);
        double min_age = this->natural_history->get_import_min_age(new_state);
        double max_age = this->natural_history->get_import_max_age(new_state);
        select_imported_cases(day, max_imported, per_cap, lat, lon, radius, admin_code, min_age, max_age, false);
      } else {
        int found_import_list_rule = 0;
        Rule* rule = this->natural_history->get_import_list_rule(new_state);
        if(rule != nullptr && rule->applies(person)) {
          get_imported_list(rule->get_expression()->get_list_value(person));
          found_import_list_rule = 1;

        }
        if(found_import_list_rule == 0) {
          int max_imported = 0;
          rule = this->natural_history->get_import_count_rule(new_state);
          if(rule != nullptr && rule->applies(person)) {
            max_imported = rule->get_expression()->get_value(person);
          }
          double per_cap = 0.0;
          rule = this->natural_history->get_import_per_capita_rule(new_state);
          if(rule != nullptr && rule->applies(person)) {
            per_cap = rule->get_expression()->get_value(person);

          }
          if(max_imported > 0 || per_cap > 0.0) {
            Epidemic::epidemic_logger->debug(
                "UPDATE_STATE day {:d} hour {:d} person {:d} IMPORT max_imported {:d} per_cap {:f}", 
                day, hour, person->get_id(), max_imported, per_cap );

            fred::geo lat = 0;
            fred::geo lon = 0;
            double radius = 0;
            long long int admin_code = 0;
            double min_age = 0;
            double max_age = 999;
            bool count_all = this->natural_history->all_import_attempts_count(new_state);

            Rule* rule = this->natural_history->get_import_location_rule(new_state);
            if(rule != nullptr && rule->applies(person)) {
              lat = rule->get_expression()->get_value(person);
              lon = rule->get_expression2()->get_value(person);
              radius = rule->get_expression3()->get_value(person);
            }

            rule = this->natural_history->get_import_admin_code_rule(new_state);
            if(rule != nullptr && rule->applies(person)) {
              admin_code = rule->get_expression()->get_value(person);
            }

            rule = this->natural_history->get_import_ages_rule(new_state);
            if(rule != nullptr && rule->applies(person)) {
              min_age = rule->get_expression()->get_value(person);
              max_age = rule->get_expression2()->get_value(person);
            }

            this->select_imported_cases(day, max_imported, per_cap, lat, lon, radius, admin_code, min_age, max_age, count_all);
          }
        }
      }
    }
  }
  if(new_state == 0 || old_state != new_state) {
    // update the epidemic variables for this person's new state
    if(new_state > 0 && 0 <= old_state) {
      if(this->current_count[old_state] > 0) {
        --this->current_count[old_state];
      }
      if(this->daily_current_count[old_state][day] > 0) {
        --this->daily_current_count[old_state][day];
      }
      this->dec_state_count(person, old_state);
    }

    if(0 <= new_state) {
      ++this->incidence_count[new_state];
      ++this->daily_incidence_count[new_state][day];
      ++this->total_count[new_state];
      ++this->current_count[new_state];
      ++this->daily_current_count[new_state][day];
      this->inc_state_count(person, new_state);
    }

    // note: person's health state is still old_state

    // this is true if person was exposed through a modification operator
    if(new_state == this->natural_history->get_exposed_state() && person->get_exposure_day(this->id) < 0) {
      person->become_exposed(this->id, Person::get_import_agent(), nullptr, day, hour);
    }

    if(this->natural_history->is_dormant_state(new_state) == false &&
        this->active_people_list.find(person) == this->active_people_list.end()) {
      this->become_active(person, day);
    }

    // now finally, update person's health state, transmissibility etc
    person->set_state(this->id, new_state, day);

    // show place counters for this condition for this agent
    Epidemic::epidemic_logger->debug(
        "UPDATE_STATE day {:d} person {:d} to state {:s} household count {:d} "
        "school count {:d} workplace count {:d} neighborhood count {:d}",
        day, person->get_id(), this->condition->get_state_name(new_state).c_str(), 
        this->get_group_state_count(person->get_household(), new_state),
        this->get_group_state_count(person->get_school(), new_state),
        this->get_group_state_count(person->get_workplace(), new_state),
        this->get_group_state_count(person->get_neighborhood(), new_state));

    // update person health record
    if(0 < new_state && this->enable_health_records && Global::Enable_Records) {
      if(this->natural_history->get_state_name(new_state) != "Excluded") {
        char tmp[FRED_STRING_SIZE];
        person->get_record_string(tmp);
        fprintf(Global::Recordsfp, "%s CONDITION %s CHANGES from %s to %s\n", tmp,
            this->name, old_state >= 0 ? this->natural_history->get_state_name(old_state).c_str() : "-1",
            new_state >= 0 ? this->natural_history->get_state_name(new_state).c_str() : "-1");
        fflush(Global::Recordsfp);
      }
    }

    if(this->natural_history->is_dormant_state(new_state)) {
      this->inactivate(person, day, hour);
    }

    // handle case fatalities
    if(this->natural_history->is_fatal_state(new_state) && person->is_meta_agent() == false) {

      // update person's health record
      person->become_case_fatality(this->id, day);

      if(this->enable_visualization && this->visualize_state[new_state]) {
        // save visualization data for case fatality
        Place* place = person->get_place_of_type(this->visualize_state_place_type[new_state]);
        if(place != nullptr) {
          double lat = place->get_latitude();
          double lon = place->get_longitude();
          VIS_Location* vis_loc = new VIS_Location(lat,lon);
          this->vis_case_fatality_loc_list.push_back(vis_loc);
        }
      }
      this->delete_from_epidemic_lists(person);
    }
    // end of change to a new state
  } else { // continue in same state
    // DEBUGGING
    if(0) {
      // update person health record
      if(this->enable_health_records && Global::Enable_Records) {
        char tmp[FRED_STRING_SIZE];
        person->get_record_string(tmp);
        fprintf(Global::Recordsfp, "%s CONDITION %s STATE %s STAYS %s\n", tmp,
            this->name, old_state >= 0 ? this->natural_history->get_state_name(old_state).c_str() : "-1",
            new_state >= 0 ? this->natural_history->get_state_name(new_state).c_str() : "-1");
        fflush(Global::Recordsfp);
      }
    } // end DEBUGGING
  } // end continue in same state

  // record old state of person
  bool was_susceptible = person->is_susceptible(this->id);
  bool was_transmissible = person->is_transmissible(this->id);

  // action rules
  person->run_action_rules(this->id, new_state, this->natural_history->get_action_rules(new_state));
  // new status
  bool is_now_susceptible = person->is_susceptible(this->id);
  bool is_now_transmissible = person->is_transmissible(this->id);

  // report change of status
  if(is_now_susceptible && !was_susceptible) {
    this->susceptible_count++;
  }
  if(!is_now_susceptible && was_susceptible) {
    this->susceptible_count--;
  }
  if(is_now_transmissible && !was_transmissible) {
    // add to transmissible_people_list
    this->transmissible_people_list.insert(person);
  }
  if(!is_now_transmissible && was_transmissible) {
    // delete from transmissible list
    person_set_iterator itr = this->transmissible_people_list.find(person);
    if(itr != this->transmissible_people_list.end()) {
      this->transmissible_people_list.erase(itr);
    }
  }

  // does entering this state cause agent to starting hosting?
  if(0 <= this->natural_history->get_place_type_to_transmit() &&
      this->natural_history->should_start_hosting(new_state)) {
    person->start_hosting(this->natural_history->get_place_type_to_transmit());
  }

  Epidemic::epidemic_logger->info(
      "UPDATE_STATE FINISHED person {:d} condition {:s} day {:d} hour {:d} "
      "old_state {:s} new_state {:s} loops {:d}",
      person->get_id(), this->name, day, hour,
      0 <= old_state ? this->natural_history->get_state_name(old_state).c_str() : "NONE",
      0 <= new_state ? this->natural_history->get_state_name(new_state).c_str() : "NONE",
      loop_counter);
  
  if(transition_step == step) {

    // counts loops
    if(old_state == new_state) {
      ++loop_counter;
    } else {
      loop_counter = 0;
    }

    Epidemic::epidemic_logger->debug(
        "UPDATE_STATE RECURSE person {:d} condition {:s} day {:d} hour {:d} "
        "old_state {:s} new_state {:s} loops {:d} max_loops {:d}",
        person->get_id(), this->name, day, hour, 
        0 <= old_state ? this->natural_history->get_state_name(old_state).c_str() : "NONE",
        0 <= new_state ? this->natural_history->get_state_name(new_state).c_str(): "NONE",
        loop_counter, Global::Max_Loops);

    // infinite loop protection
    if(loop_counter < Global::Max_Loops) {
      // recurse if this is an instant transition
      this->update_state(person, day, hour, -1, loop_counter);
    }
  }
}

/**
 * Generates a daily report and outputs to a file.
 */
void Epidemic::finish() {

  char dir[FRED_STRING_SIZE];
  char outfile[FRED_STRING_SIZE];
  FILE* fp;

  if(this->natural_history->make_daily_report() == false) {
    return;
  }

  snprintf(dir,
      FRED_STRING_SIZE,
      "%s/RUN%d/DAILY",
      Global::Simulation_directory,
      Global::Simulation_run_number);
  Utils::fred_make_directory(dir);

  for(int i = 0; i < this->number_of_states; ++i) {
    snprintf(outfile, FRED_STRING_SIZE, "%s/%s.new%s.txt", dir, this->name, this->natural_history->get_state_name(i).c_str());
    fp = fopen(outfile, "w");
    if(fp == nullptr) {
      Utils::fred_abort("Fred: can't open file %s\n", outfile);
    }
    for(int day = 0; day < Global::Simulation_Days; ++day) {
      fprintf(fp, "%d %d\n", day, this->daily_incidence_count[i][day]);
    }
    fclose(fp);

    snprintf(outfile, FRED_STRING_SIZE, "%s/%s.%s.txt", dir, this->name, this->natural_history->get_state_name(i).c_str());
    fp = fopen(outfile, "w");
    if(fp == nullptr) {
      Utils::fred_abort("Fred: can open file %s\n", outfile);
    }
    for(int day = 0; day < Global::Simulation_Days; ++day) {
      fprintf(fp, "%d %d\n", day, this->daily_current_count[i][day]);
    }
    fclose(fp);

    snprintf(outfile, FRED_STRING_SIZE, "%s/%s.tot%s.txt", dir, this->name, this->natural_history->get_state_name(i).c_str());
    fp = fopen(outfile, "w");
    if(fp == nullptr) {
      Utils::fred_abort("Fred: can open file %s\n", outfile);
    }
    int tot = 0;
    for(int day = 0; day < Global::Simulation_Days; ++day) {
      tot += this->daily_incidence_count[i][day];
      fprintf(fp, "%d %d\n", day, tot);
    }
    fclose(fp);
  }

  // reproductive rate
  snprintf(outfile, FRED_STRING_SIZE, "%s/%s.RR.txt", dir, this->name);
  fp = fopen(outfile, "w");
  if(fp == nullptr) {
    Utils::fred_abort("Fred: can open file %s\n", outfile);
  }
  for(int day = 0; day < Global::Simulation_Days; ++day) {
    double rr = 0.0;
    if(this->daily_cohort_size[day] > 0) {
      rr = static_cast<double>(this->number_infected_by_cohort[day]) / static_cast<double>(this->daily_cohort_size[day]);
    }
    fprintf(fp, "%d %f\n", day, rr);
  }
  fclose(fp);

  // create a csv file for this condition

  // this joins two files with same value in column 1, from
  // https://stackoverflow.com/questions/14984340/using-awk-to-process-input-from-multiple-files
  char awkcommand[FRED_STRING_SIZE];
  snprintf(awkcommand, FRED_STRING_SIZE, "awk 'FNR==NR{a[$1]=$2 FS $3;next}{print $0, a[$1]}' ");

  char command[FRED_STRING_SIZE];
  char dailyfile[FRED_STRING_SIZE];
  snprintf(outfile, FRED_STRING_SIZE, "%s/RUN%d/%s.csv", Global::Simulation_directory, Global::Simulation_run_number, this->name);

  snprintf(dailyfile, FRED_STRING_SIZE, "%s/%s.new%s.txt", dir, this->name, this->natural_history->get_state_name(0).c_str());
  snprintf(command, FRED_STRING_SIZE, "cp %s %s", dailyfile, outfile);
  system(command);

  for(int i = 0; i < this->number_of_states; ++i) {
    if(i > 0) {
      snprintf(dailyfile, FRED_STRING_SIZE, "%s/%s.new%s.txt", dir, this->name, this->natural_history->get_state_name(i).c_str());
      snprintf(command, FRED_STRING_SIZE, "%s %s %s > %s.tmp; mv %s.tmp %s",
          awkcommand, dailyfile, outfile, outfile, outfile, outfile);
      system(command);
    }

    snprintf(dailyfile, FRED_STRING_SIZE, "%s/%s.%s.txt", dir, this->name, this->natural_history->get_state_name(i).c_str());
    snprintf(command, FRED_STRING_SIZE, "%s %s %s > %s.tmp; mv %s.tmp %s",
        awkcommand, dailyfile, outfile, outfile, outfile, outfile);
    system(command);

    snprintf(dailyfile, FRED_STRING_SIZE, "%s/%s.tot%s.txt", dir, this->name, this->natural_history->get_state_name(i).c_str());
    snprintf(command, FRED_STRING_SIZE, "%s %s %s > %s.tmp; mv %s.tmp %s",
        awkcommand, dailyfile, outfile, outfile, outfile, outfile);
    system(command);
  }
  snprintf(dailyfile, FRED_STRING_SIZE, "%s/%s.RR.txt", dir, this->name);
  snprintf(command, FRED_STRING_SIZE, "%s %s %s > %s.tmp; mv %s.tmp %s", awkcommand, dailyfile, outfile, outfile, outfile, outfile);
  system(command);

  // create a header line for the csv file
  char headerfile[FRED_STRING_SIZE];
  snprintf(headerfile, FRED_STRING_SIZE, "%s/RUN%d/%s.header", Global::Simulation_directory, Global::Simulation_run_number, this->name);
  fp = fopen(headerfile, "w");
  fprintf(fp, "Day ");
  for(int i = 0; i < this->number_of_states; ++i) {
    fprintf(fp, "%s.new%s ", this->name, this->natural_history->get_state_name(i).c_str());
    fprintf(fp, "%s.%s ", this->name, this->natural_history->get_state_name(i).c_str());
    fprintf(fp, "%s.tot%s ", this->name, this->natural_history->get_state_name(i).c_str());
  }
  fprintf(fp, "%s.RR\n", this->name);
  fclose(fp);

  // concatenate header line
  snprintf(command, FRED_STRING_SIZE, "cat %s %s > %s.tmp; mv %s.tmp %s; unlink %s", headerfile, outfile, outfile, outfile, outfile, headerfile);
  system(command);

  // replace spaces with commas
  snprintf(command, FRED_STRING_SIZE, "sed -E 's/ +/,/g' %s | sed -E 's/,$//' > %s.tmp; mv %s.tmp %s", outfile, outfile, outfile, outfile);
  system(command);
}

/**
 * Gets the attack rate of the epidemic. This is the total cases divided by the population size.
 *
 * @return the attack rate
 */
double Epidemic::get_attack_rate() {
  return static_cast<double>(this->total_cases) / static_cast<double>(Person::get_population_size());
}

/**
 * Increments the group state count for the specified state in each group the given person 
 * is a member of.
 *
 * @param person the person
 * @param state the condition state
 */
void Epidemic::inc_state_count(Person* person, int state){

  for(int type_id = 0; type_id < Group_Type::get_number_of_group_types(); ++type_id) {
    if(this->track_counts_for_group_state[state][type_id]) {
      Group* group = person->get_group_of_type(type_id);
      if(group != nullptr) {
        if(this->group_state_count[state].find(group) == this->group_state_count[state].end()) {
          // not present so insert with count of one
          std::pair<Group*,int> new_count(group,1);
          this->group_state_count[state].insert(new_count);
          this->total_group_state_count[state].insert(new_count);
        } else { // present so increment count
          ++this->group_state_count[state][group];
          ++this->total_group_state_count[state][group];
        }
        Epidemic::epidemic_logger->debug(
            "inc_state_count person {:d} group {:s} cond {:s} state {:s} count {:d} total_count {:d}", 
            person->get_id(), group->get_label(), this->name, 
            this->natural_history->get_state_name(state).c_str(), 
            this->group_state_count[state][group], 
            this->total_group_state_count[state][group]);
      }
    }
  }
}

/**
 * Decrements the group state count for the specified state in each group the given person 
 * is a member of.
 *
 * @param person the person
 * @param state the condition state
 */
void Epidemic::dec_state_count(Person* person, int state){
  for(int type_id = 0; type_id < Group_Type::get_number_of_group_types(); ++type_id) {
    if(this->track_counts_for_group_state[state][type_id]) {
      Group* group = person->get_group_of_type(type_id);
      if(group != nullptr) {
        if(this->group_state_count[state].find(group) != this->group_state_count[state].end()) {
          --this->group_state_count[state][group];
          Epidemic::epidemic_logger->debug(
              "dec_state_count person {:d} group {:s} cond {:s} state {:s} count {:d}", 
              person->get_id(), group->get_label(), this->name, 
              this->natural_history->get_state_name(state).c_str(), 
              this->group_state_count[state][group]);
        }
      }
    }
  }
}

/**
 * Gets the current group state count at a specified place and state.
 *
 * @param place the group
 * @param state the condition state
 * @return the group state count
 */
int Epidemic::get_group_state_count(Group* place, int state) {
  int count = 0;
  if(this->group_state_count[state].find(place) != this->group_state_count[state].end()) {
    count = this->group_state_count[state][place];
  }
  return count;
}

/**
 * Gets the total group state count at a specified place and state.
 *
 * @param place the group
 * @param state the condition state
 * @return the total group state count
 */
int Epidemic::get_total_group_state_count(Group* place, int state) {
  int count = 0;
  if(this->total_group_state_count[state].find(place) != this->total_group_state_count[state].end()) {
    count = this->total_group_state_count[state][place];
  }
  return count;
}

/**
 * Gets a list of imported person IDs. Each person with an ID on the list will be exposed to 
 * this epidemic.
 *
 * @param id_list a list of IDs
 */
void Epidemic::get_imported_list(double_vector_t id_list) {
  Epidemic::epidemic_logger->info("GET_IMPORTED_LIST: id_list size = {:d}", id_list.size());
  if(Global::Compile_FRED) {
    return;
  }
  int day = Global::Simulation_Day;
  int hour = Global::Simulation_Hour;

  int imported_cases = 0;
  for(int n = 0; n < static_cast<int>(id_list.size()); ++n) {
    Person* person = Person::get_person_with_id(id_list[n]);
    if(person != nullptr) {
      person->become_exposed(this->id, Person::get_import_agent(), nullptr, day, hour);
      this->become_exposed(person, day, hour);
      ++imported_cases;
      Epidemic::epidemic_logger->debug(
          "IMPORT day {:d} exposure {:d} person {:d} age {:d} sex {:c} hh {:s}", 
          day, imported_cases, person->get_id(), person->get_age(), person->get_sex(), 
          person->get_household()->get_label());
    }
  }
  Epidemic::epidemic_logger->info("GET_IMPORTED_LIST: imported cases = {:d}", imported_cases);
}

/**
 * Selects new people to expose based on a variety of parameters. 
 * Longitude, latitude, radius, and admin code specify location restrictions; min_age and max_age 
 * specify age restrictions. If no location or age restrictions are provided, people are selected 
 * at random from the entire population. This method will attempt to expose people until 
 * the number of people exposed reaches the max_imported parameter.
 *
 * @param day the day
 * @param max_imported the maximum number of cases to import
 * @param per_cap the number of cases to import per capita
 * @param lat the center latitude of the area to import cases
 * @param lon the center longitude of the area to import cases
 * @param radius the radius of the area to import cases
 * @param admin_code the admin code of the census tract, county, or state to import cases
 * @param min_age the minimum age of the age range to import cases for
 * @param max_age the maximum age of the age range to import cases for
 * @param count_all whether or not to count all
 */
void Epidemic::select_imported_cases(int day, int max_imported, double per_cap, double lat, double lon, double radius, long long int admin_code, double min_age, double max_age, bool count_all) {

  Epidemic::epidemic_logger->info(
      "IMPORT SPEC for {:s} day {:d}: max = {:d} per_cap = {:f} lat = {:f} "
      "lon = {:f} rad = {:f} fips = {:d} min_age = {:f} max_age = {:f}",
      this->name, day, max_imported, per_cap, lat, lon, radius, admin_code, min_age, max_age);

  if(Global::Compile_FRED) {
    return;
  }

  int popsize = Person::get_population_size();
  if(popsize == 0) {
    return;
  }

  int hour = Global::Simulation_Hour;
  hour = 0;

  // number imported so far
  int imported_cases = 0;

  if(lat == 0.0 && lon == 0.0 && admin_code == 0 && min_age == 0 && max_age > 100 && (this->susceptible_count > 0.1 * popsize)) {

    // no location or age restriction -- select from entire population
    // using this optimization: select people from population at random,
    // and expose only the susceptibles ones.

    Epidemic::epidemic_logger->info("IMPORT OPTIMIZATION popsize = {:d} susceptible_count = {:d}", 
        popsize, this->susceptible_count);

    // determine the target number of susceptibles to expose
    int real_target = 0;
    if(per_cap == 0.0) {
      if(count_all) {
        real_target = max_imported * static_cast<double>(this->susceptible_count) / static_cast<double>(popsize);
      } else {
        real_target = max_imported;
      }
    } else {
      // per_cap overrides max_imported.
      // NOTE: per_cap target is the same whether or not count_all = true
      real_target = per_cap * this->susceptible_count;
    }

    // determine integer number of susceptibles to expose
    int target = static_cast<int>(real_target);
    double remainder = real_target - target;
    if(Random::draw_random(0, 1) < remainder) {
      ++target;
    }

    if(target == 0) {
      return;
    }

    // select people at random, and exposed if they are susceptible
    int tries = 0;
    while(imported_cases < target) {
      ++tries;
      Person* person = Person::select_random_person();
      if(person->is_susceptible(this->id)) {
        double susc = person->get_susceptibility(this->id);
        // give preference to people with higher susceptibility

        if(susc == 1.0 || Random::draw_random(0, 1) < susc) {
          // expose the candidate
          person->become_exposed(this->id, Person::get_import_agent(), nullptr, day, hour);
          become_exposed(person, day, hour);
          ++imported_cases;
          Epidemic::epidemic_logger->info(
              "IMPORT day {:d} exposure {:d} person {:d} age {:d} sex {:c} hh {:s}", 
              day, imported_cases, person->get_id(), person->get_age(), person->get_sex(),
              person->get_household()->get_label());
        }
      }
    } // end while loop import_cases

    if(tries > 0) {
      Epidemic::epidemic_logger->info(
          "day {:d} IMPORT: {:d} tries yielded {:d} imported cases of {:s}", 
          day, tries, imported_cases, this->name);
    }
  } else {

    Epidemic::epidemic_logger->info("Enter susceptible selection process");

    // list of susceptible people that qualify by distance and age
    person_vector_t people;

    // clear the list of candidates
    people.clear();

    // find households that qualify by location
    int hsize = Place::get_number_of_households();
    for(int i = 0; i < hsize; ++i) {
      Household* hh = Place::get_household(i);
      if(admin_code) {
        if(hh->get_census_tract_admin_code() != admin_code && Census_Tract::is_in_county(hh->get_census_tract_admin_code(), admin_code) == false &&
            Census_Tract::is_in_state(hh->get_census_tract_admin_code(), admin_code) == false) {
          continue;
        }
      } else {
        double dist = 0.0;
        if(radius > 0 || lat != 0 || lon != 0) {
          dist = Geo::xy_distance(lat, lon, hh->get_latitude(), hh->get_longitude());
          if(radius < dist) {
            continue;
          }
        }
      }
      // this household qualifies by location
      // find all susceptible Housemates who qualify by age.
      int size = hh->get_size();
      for(int j = 0; j < size; ++j) {
        Person* person = hh->get_member(j);
        if(person->is_susceptible(this->id)) {
          double age = person->get_real_age();
          if(min_age <= age && age < max_age) {
            people.push_back(person);
          }
        }
      }
    }

    int susceptibles = static_cast<int>(people.size());

    // determine the target number of susceptibles to expose
    double real_target = 0;
    if(per_cap == 0.0) {
      if(count_all) {
        real_target = max_imported * static_cast<double>(susceptibles) / static_cast<double>(popsize);
      } else {
        real_target = max_imported;
      }
    } else {
      // per_cap overrides max_imported.
      // NOTE: per_cap target is the same whether or not count_all = true
      real_target = per_cap * susceptibles;
    }

    // determine integer number of susceptibles to expose
    int target = static_cast<int>(real_target);
    double remainder = real_target - target;
    if(remainder > 0 && Random::draw_random(0, 1) < remainder) {
      ++target;
    }


    if(target == 0) {
      return;
    }

    // sort the candidates
    std::sort(people.begin(), people.end(), Utils::compare_id);

    if(target <= static_cast<int>(people.size())) {
      // we have at least the minimum number of candidates.
      for(int n = 0; n < target; ++n) {

        // pick a candidate without replacement
        int pos = Random::draw_random_int(0, people.size()-1);
        Person* person = people[pos];
        people[pos] = people[people.size() - 1];
        people.pop_back();

        // try to expose the candidate
        double susc = person->get_susceptibility(this->id);
        if(susc == 1.0 || Random::draw_random(0,1) < susc) {
          person->become_exposed(this->id, Person::get_import_agent(), nullptr, day, hour);
          this->become_exposed(person, day, hour);
          ++imported_cases;
          Epidemic::epidemic_logger->info(
              "IMPORT day {:d} exposure {:d} person {:d} age {:d} sex {:c} hh {:s}",
              day, imported_cases, person->get_id(), person->get_age(), person->get_sex(), 
              person->get_household()->get_label());
        }
      }
      Epidemic::epidemic_logger->info("IMPORT SUCCESS: day = {:d} imported {:d} cases of {:s}", 
          day, imported_cases, this->name);
    } else {
      // try to expose all the candidates
      for(int n = 0; n < static_cast<int>(people.size()); ++n) {
        Person* person = people[n];
        double susc = person->get_susceptibility(this->id);
        if(susc == 1.0 || Random::draw_random(0,1) < susc) {
          person->become_exposed(this->id, Person::get_import_agent(), nullptr, day, hour);
          this->become_exposed(person, day, hour);
          ++imported_cases;
          Epidemic::epidemic_logger->info(
              "IMPORT day {:d} exposure {:d} person {:d} age {:d} sex {:c} hh {:s}", 
              day, imported_cases, person->get_id(), person->get_age(), person->get_sex(), 
              person->get_household()->get_label());
        }
      }
    }
    if(imported_cases < target) {
      Epidemic::epidemic_logger->error("IMPORT FAILURE: only {:d} imported cases out of {:d}", 
          imported_cases, target);
    }
  } // end else -- select from susceptibles
}

/**
 * Increments the group state count for a specified group and state, if that group type 
 * has count tracking enabled.
 *
 * @param group_type_id the group type ID
 * @param group the group
 * @param state the state
 */
void Epidemic::increment_group_state_count(int group_type_id, Group* group, int state) {
  if(this->track_counts_for_group_state[state][group_type_id]) {
    if(group != nullptr) {
      if(this->group_state_count[state].find(group) == this->group_state_count[state].end()) {
        // not present so insert with count of one
        std::pair<Group*,int> new_count(group, 1);
        this->group_state_count[state].insert(new_count);
        this->total_group_state_count[state].insert(new_count);
      } else { // present so increment count
        ++this->group_state_count[state][group];
        ++this->total_group_state_count[state][group];
      }
      Epidemic::epidemic_logger->debug(
          "increment_group_state_count group {:s} cond {:s} state {:s} count {:d} total_count {:d}", 
          group->get_label(), this->name, this->natural_history->get_state_name(state).c_str(), 
          this->group_state_count[state][group], this->total_group_state_count[state][group]);
    }
  }
}

/**
 * Decrements the group state count for a specified group and state, if that group type 
 * has count tracking enabled.
 *
 * @param group_type_id the group type ID
 * @param group the group
 * @param state the state
 */
void Epidemic::decrement_group_state_count(int group_type_id, Group* group, int state) {
  if(this->track_counts_for_group_state[state][group_type_id]) {
    if(group != nullptr) {
      if(this->group_state_count[state].find(group) != this->group_state_count[state].end()) {
        --this->group_state_count[state][group];
        Epidemic::epidemic_logger->debug(
            "decrement_group_state_count group {:s} cond {:s} state {:s} count = {:d}", 
            group->get_label(), this->name, this->natural_history->get_state_name(state).c_str(), 
            this->group_state_count[state][group]);
      }
    }
  }
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Epidemic::setup_logging() {
  if(Epidemic::is_log_initialized) {
    return;
  }
  
  if(Parser::does_property_exist("epidemic_log_level")) {
    Parser::get_property("epidemic_log_level", &Epidemic::epidemic_log_level);
  } else {
    Epidemic::epidemic_log_level = "OFF";
  }
  
  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Epidemic::epidemic_logger = std::make_unique<spdlog::logger>("epidemic_logger", 
        sink_list.begin(), sink_list.end());
    Epidemic::epidemic_logger->set_level(
        Utils::get_log_level_from_string(Epidemic::epidemic_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Epidemic::epidemic_logger->trace("<{:s}, {:d}>: Epidemic logger initialized", 
      __FILE__, __LINE__  );
  Epidemic::is_log_initialized = true;
}
