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
// File: Condition.cc
//

#include <spdlog/spdlog.h>

#include "Condition.h"
#include "Date.h"
#include "Epidemic.h"
#include "Global.h"
#include "Household.h"
#include "Natural_History.h"
#include "Neighborhood_Patch.h"
#include "Network.h"
#include "Network_Type.h"
#include "Parser.h"
#include "Person.h"
#include "Random.h"
#include "Rule.h"
#include "Transmission.h"

#define PI 3.14159265359

/**
 * Creates a Condition. Initializes default varibales.
 */
Condition::Condition() {
  this->id = -1;
  strcpy(this->condition_name, "");
  this->natural_history = nullptr;
  strcpy(this->transmission_mode, "");
  this->transmission = nullptr;
  strcpy(this->transmission_network_name, "");
  this->transmission_network = nullptr;
  this->transmissibility = -1.0;
  this->epidemic = nullptr;
}

/**
 * Deletes this condition's Natural_History, Transmission_Network, and associated Epidemic model 
 * upon deletion of this condition.
 */
Condition::~Condition() {
  delete this->natural_history;
  delete this->transmission_network;
  delete this->epidemic;
}

/**
 * Sets up and outputs condition ID and condition name, as well as other properties of this condition.
 *
 * @param condition_id the new ID
 * @param name the new condition name
 */
void Condition::get_properties(int condition_id, std::string name) {

  // set condition id
  this->id = condition_id;

  // set condition name
  strcpy(this->condition_name, name.c_str());

  Condition::condition_logger->info("condition {:d} {:s} read_properties entered", this->id, 
      this->condition_name);

  // optional properties:
  Parser::disable_abort_on_failure();

  // type of transmission mode
  strcpy(this->transmission_mode, "none");
  Parser::get_property(this->condition_name, "transmission_mode", this->transmission_mode);

  // transmission_network
  this->transmission_network = nullptr;
  strcpy(this->transmission_network_name, "none");
  Parser::get_property(this->condition_name, "transmission_network", this->transmission_network_name);
  if(strcmp(this->transmission_network_name, "none") != 0) {
    Network_Type::include_network_type(this->transmission_network_name);
  }

  Parser::set_abort_on_failure();

  Condition::condition_logger->info("condition {:d} {:s} read_properties finished", this->id, 
      this->condition_name);
}

/**
 * Initializes properties of this condition.
 */
void Condition::setup() {

  Condition::condition_logger->info("condition {:d} {:s} setup entered", this->id, 
      this->condition_name);

  // Initialize Natural History
  this->natural_history = new Natural_History;

  // read in properties and files associated with this natural history model:
  this->natural_history->setup(this);
  this->natural_history->get_properties();

  // contagiousness
  this->transmissibility = this->natural_history->get_transmissibility();

  if(this->transmissibility > 0.0) {
    // Initialize Transmission Model
    this->transmission = Transmission::get_new_transmission(this->transmission_mode);

    // read in properties and files associated with this transmission mode:
    this->transmission->setup(this);

  }

  // Initialize Epidemic model
  this->epidemic = Epidemic::get_epidemic(this);
  this->epidemic->setup();

  Condition::condition_logger->info("condition {:d} {:s} setup finished", this->id, 
      this->condition_name);
}

/**
 * Prepares properties of this condition.
 */
void Condition::prepare() {

  Condition::condition_logger->info("condition {:d} {:s} prepare entered", this->id, 
      this->condition_name);

  // get transmission_network if any
  if (strcmp(this->transmission_network_name,"none")!=0) {
    this->transmission_network = Network_Type::get_network_type(this->transmission_network_name)->get_network();
    assert(this->transmission_network != nullptr);
  }

  // final prep for natural history model
  this->natural_history->prepare();

  Condition::condition_logger->info("condition {:d} {:s} prepare finished", this->id, 
      this->condition_name);
}


/**
 * Initializes a specified Person for each Condition in the static conditions vector.
 *
 * @param person the person
 */
void Condition::initialize_person(Person* person) {
  for(int condition_id = 0; condition_id < Condition::number_of_conditions; ++condition_id) {
    Condition::conditions[condition_id]->initialize_person(person, Global::Simulation_Day);
  }
}

/**
 * Initializes a specified Person for this condition's associated Epidemic model at the specified day.
 *
 * @param person the person
 * @param day the day
 */
void Condition::initialize_person(Person* person, int day) {
  this->epidemic->initialize_person(person, day);
}

/**
 * Gets the attack rate of this condition's associated Epidemic model.
 *
 * @return the attack rate
 */
double Condition::get_attack_rate() {
  return this->epidemic->get_attack_rate();
}

/**
 * Calls the report method of this condition's associated Epidemic model for the specified day.
 *
 * @param day the day
 */
void Condition::report(int day) {
  this->epidemic->report(day);
}

/**
 * Increments the number of people infected by a cohort in this condition's associated 
 * Epidemic model for the specified day.
 *
 * @param day the day
 */
void Condition::increment_cohort_host_count(int day) {
  this->epidemic->increment_cohort_host_count(day);
}

/**
 * Updates this condition's associated Epidemic model for the specified day and hour.
 *
 * @param day the day
 * @param hour the hour
 */
void Condition::update(int day, int hour) {
  this->epidemic->update(day, hour);
}

/**
 * Terminates a given Person tracked by this condition's associated Epidemic model 
 * at the specified day.
 *
 * @param person the person
 * @param day the day
 */
void Condition::terminate_person(Person* person, int day) {
  this->epidemic->terminate_person(person, day);
}

/**
 * Finishes this condition's associated Epidemic model.
 */
void Condition::finish() {
  this->epidemic->finish();
}

/**
 * Calls this condition's Natural_History to make a daily report.
 *
 * @return the reporting status
 */
bool Condition::make_daily_report() {
  return this->natural_history->make_daily_report();
}

/**
 * Gets the number of conditon states in this condition's Natural_History.
 *
 * @return the number of states
 */
int Condition::get_number_of_states() {
  return this->natural_history->get_number_of_states();
}

/**
 * Gets the condition state name in this condition's Natural_History at a specified index.
 *
 * @param i the index
 * @return the state name
 */
std::string Condition::get_state_name(int i) {
  if(i < this->get_number_of_states()) {
    return this->natural_history->get_state_name(i);
  } else {
    return "UNDEFINED";
  }
}

/**
 * Enables this condition's associated Epidemic model to track group state counts 
 * for a specified condition state and Group_Type.
 *
 * @param type_id the group type ID
 * @param state the conditon state
 */
void Condition::track_group_state_counts(int type_id, int state) {
  this->epidemic->track_group_state_counts(type_id, state);
}

/**
 * Gets the current group state count in this condition's associated Epidemic model 
 * for a specified Group and condition state.
 *
 * @param group the group
 * @param state the conditon state
 * @return the current group state count
 */
int Condition::get_current_group_state_count(Group* group, int state){
  return this->epidemic->get_group_state_count(group, state);
}

/**
 * Gets the total group state count in this condition's associated Epidemic model 
 * for a specified Group and condition state.
 *
 * @param group the group
 * @param state the conditon state
 * @return the total group state count
 */
int Condition::get_total_group_state_count(Group* group, int state){
  return this->epidemic->get_total_group_state_count(group, state);
}

/**
 * Gets the condition state index from the specified condition names.
 *
 * @param state_name the name of the state
 * @return the conditon state
 */
int Condition::get_state_from_name(std::string state_name) {
  for(int i = 0; i < this->get_number_of_states(); ++i) {
    if(this->get_state_name(i) == state_name) {
      return i;
    }
  }
  return -1;
}

/**
 * Gets the condition to trasmit for the specified state. This will be the ID of a condition.
 *
 * @param state the conditon state
 * @return the condition to transmit
 */
int Condition::get_condition_to_transmit(int state) {
  return this->natural_history->get_condition_to_transmit(state);
}

/**
 * Gets the incidence count from the condition's associated Epidemic model for a specified 
 * conditon state. This is the number of people currently entering the state.
 *
 * @param state the conditon state
 * @return the incidence count
 */
int Condition::get_incidence_count(int state) {
  return this->epidemic->get_incidence_count(state);
}

/**
 * Gets the current count from the condition's associated Epidemic model for a specified 
 * condition state. This is the number of people currently in the state.
 *
 * @param state the conditon state
 * @return the current count
 */
int Condition::get_current_count(int state) {
  return this->epidemic->get_current_count(state);
}

/**
 * Gets the total count from the condition's associated Epidemic model for a specified 
 * condition state. This is the number of people who have ever entered the state.
 *
 * @param state the conditon state
 * @return the total count
 */
int Condition::get_total_count(int state) {
  return this->epidemic->get_total_count(state);
}

/**
 * Checks if external updates are enabled for this condition's Natural_History.
 *
 * @return if external updates are enabled
 */
bool Condition::is_external_update_enabled() {
  return this->natural_history->is_external_update_enabled();
}

/**
 * Checks if external updates are enabled for a specified state in this condition's Natural_History.
 *
 * @param state the conditon state
 * @return if external updates are enabled
 */
bool Condition::state_gets_external_updates(int state) {
  return this->natural_history->state_gets_external_updates(state);
}

/**
 * Checks if the specified condition state is absent from the specified Group_Type in
 * this condition's Natural_History.
 *
 * @param state the conditon state
 * @param group_type_id the group type ID
 * @return if the group type is absent
 */
bool Condition::is_absent(int state, int group_type_id) {
  return this->natural_history->is_absent(state, group_type_id);
}

/**
 * Checks if the specified Group_Type is closed from the specified state in
 * this condition's Natural_History.
 *
 * @param state the conditon state
 * @param group_type_id the group type ID
 * @return if the group type is closed
 */
bool Condition::is_closed(int state, int group_type_id) {
  return this->natural_history->is_closed(state, group_type_id);
}

//////////////////////////

std::vector <Condition*> Condition::conditions;
std::vector<std::string> Condition::condition_names;
int Condition::number_of_conditions = 0;

bool Condition::is_log_initialized = false;
std::string Condition::condition_log_level = "";
std::unique_ptr<spdlog::logger> Condition::condition_logger = nullptr;

/**
 * Gets and parses properties of the condition.
 */
void Condition::get_condition_properties() {

  // read in the list of condition names
  Condition::conditions.clear();
  char property_name[FRED_STRING_SIZE];
  char property_value[FRED_STRING_SIZE];

  // optional properties:
  Parser::disable_abort_on_failure();

  Parser::set_abort_on_failure();

  strcpy(property_name, "conditions");
  if(Parser::does_property_exist(property_name)) {
    Condition::condition_names.clear();
    Parser::get_property(property_name, property_value);

    // parse the property value into separate strings
    char* pch;
    pch = strtok(property_value," \t\n\r");
    while(pch != nullptr) {
      Condition::include_condition(pch);
      //Condition::condition_names.push_back(string(pch));
      pch = strtok(nullptr, " \t\n\r");
    }
  }

  Condition::number_of_conditions = Condition::condition_names.size();

  for(int condition_id = 0; condition_id < Condition::number_of_conditions; ++condition_id) {

    // create new Condition object
    Condition* condition = new Condition();

    // get its properties
    condition->get_properties(condition_id, Condition::condition_names[condition_id]);
    Condition::conditions.push_back(condition);
    Condition::condition_logger->info("condition {:d} = {:s}", condition_id, 
        Condition::condition_names[condition_id].c_str());
  }
}

/**
 * Calls the setup method for each condition.
 */
void Condition::setup_conditions() {
  for(int condition_id = 0; condition_id < Condition::number_of_conditions; ++condition_id) {
    Condition::conditions[condition_id]->setup();
  }
}

/**
 * Prepares the tracking of group state counts for each Condition's associated Epidemic model.
 */
void Condition::prepare_to_track_group_state_counts() {
  for(int condition_id = 0; condition_id < Condition::number_of_conditions; ++condition_id) {
    Condition *condition = Condition::conditions[condition_id];
    condition->get_epidemic()->prepare_to_track_counts();
  }
}

/**
 * Calls the prepare method for each Condition and it's associated Epidemic model.
 */
void Condition::prepare_conditions() {
  for(int condition_id = 0; condition_id < Condition::number_of_conditions; ++condition_id) {
    Condition *condition = Condition::conditions[condition_id];
    condition->prepare();
  }
  for(int condition_id = 0; condition_id < Condition::number_of_conditions; ++condition_id) {
    Condition *condition = Condition::conditions[condition_id];
    // final prep for epidemic
    condition->epidemic->prepare();
  }
}

/**
 * Gets the Condition with the specified name.
 *
 * @param condition_name the condition name
 * @return the condition
 */
Condition* Condition::get_condition(std::string condition_name) {
  for(int condition_id = 0; condition_id < Condition::number_of_conditions; ++condition_id) {
    if(strcmp(condition_name.c_str(), Condition::conditions[condition_id]->get_name()) == 0) {
      return Condition::conditions[condition_id];
    }
  }
  return nullptr;
}

/**
 * Gets the ID of the Condition with the specified name.
 *
 * @param condition_name the condition name
 * @return the ID
 */
int Condition::get_condition_id(std::string condition_name) {
  for(int condition_id = 0; condition_id < Condition::number_of_conditions; ++condition_id) {
    if(strcmp(condition_name.c_str(), Condition::conditions[condition_id]->get_name()) == 0) {
      return condition_id;
    }
  }
  return -1;
}

/**
 * Calls the finish method for each Condition.
 */
void Condition::finish_conditions() {
  for(int condition_id = 0; condition_id < Condition::number_of_conditions; ++condition_id) {
    Condition::conditions[condition_id]->finish();
  }
}

/**
 * Gets the Place_Type to transmit for this condition's Natural_History.
 *
 * @return the place type to transmit
 */
int Condition::get_place_type_to_transmit() {
  return this->natural_history->get_place_type_to_transmit();
}

/**
 * Checks if health records are enabled for the condition's associated Epidemic model.
 *
 * @return if health records are enabled
 */
bool Condition::health_records_are_enabled() {
  return this->epidemic->health_records_are_enabled();
}

/**
 * Increments the group state count for a specified Group, Group_Type, and condition state 
 * in this condition's associated Epidemic model.
 *
 * @param group_type_id the group type ID
 * @param group the group
 * @param state the condition state
 */
void Condition::increment_group_state_count(int group_type_id, Group* group, int state) {
  this->epidemic->increment_group_state_count(group_type_id, group, state);
}

/**
 * Decrements the group state count for a specified Group, Group_Type, and condition state 
 * in this condition's associated Epidemic model.
 *
 * @param group_type_id the group type ID
 * @param group the group
 * @param state the condition state
 */
void Condition::decrement_group_state_count(int group_type_id, Group* group, int state) {
  this->epidemic->decrement_group_state_count(group_type_id, group, state);
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Condition::setup_logging() {
  if(Condition::is_log_initialized) {
    return;
  }

  if(Parser::does_property_exist("condition_log_level")) {
    Parser::get_property("condition_log_level", &Condition::condition_log_level);
  } else {
    Condition::condition_log_level = "OFF";
  }
  
  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Condition::condition_logger = std::make_unique<spdlog::logger>("condition_logger", 
        sink_list.begin(), sink_list.end());
    Condition::condition_logger->set_level(
        Utils::get_log_level_from_string(Condition::condition_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Condition::condition_logger->trace("<{:s}, {:d}>: Condition logger initialized", 
      __FILE__, __LINE__  );
  Condition::is_log_initialized = true;
}
