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

#include <spdlog/spdlog.h>

#include "Condition.h"
#include "Date.h"
#include "Expression.h"
#include "Global.h"
#include "Household.h"
#include "Natural_History.h"
#include "Network_Type.h"
#include "Parser.h"
#include "Person.h"
#include "Place.h"
#include "Predicate.h"
#include "Preference.h"
#include "Random.h"
#include "Rule.h"
#include "State_Space.h"
#include "Utils.h"

std::string Natural_History::global_admin_start_state = "";

bool Natural_History::is_log_initialized = false;
std::string Natural_History::natural_history_log_level = "";
std::unique_ptr<spdlog::logger> Natural_History::natural_history_logger = nullptr;

/**
 * Creates a Natural_History with default variables.
 */
Natural_History::Natural_History() {
  this->id = -1;
  this->transition_day = nullptr;
  this->transition_date = nullptr;
  this->transition_days = nullptr;
  this->transition_hour = nullptr;
  this->state_space = nullptr;
  this->update_vars_externally = nullptr;
  this->enable_external_update = false;
  this->R0 = -1.0;
  this->R0_a = -1.0;
  this->R0_b = -1.0;
  this->action_rules = nullptr;
  this->wait_rules = nullptr;
  this->exposure_rule = nullptr;
  this->next_rules = nullptr;
  this->default_rule = nullptr;
  this->import_count_rule = nullptr;
  this->import_per_capita_rule = nullptr;
  this->import_ages_rule = nullptr;
  this->import_location_rule = nullptr;
  this->import_admin_code_rule = nullptr;
  this->import_min_age = nullptr;
  this->import_max_age = nullptr;
  this->import_latitude = nullptr;
  this->import_longitude = nullptr;
  this->import_radius = nullptr;
  this->import_list_rule = nullptr;
  this->import_count = nullptr;
  this->count_all_import_attempts = nullptr;
  this->import_start_state = -1;
  this->admin_start_state = -1;
  this->daily_report = 1;
  this->network_action = nullptr;
  this->place_type_to_join = nullptr;
  this->place_type_to_transmit = -1;
  this->place_type_to_quit = nullptr;
  this->condition = nullptr;
  this->default_next_state = nullptr;
  this->import_per_capita_transmissions = nullptr;
  this->fatal_state = nullptr;
  this->duration_expression = nullptr;
  this->network_type = nullptr;
  this->condition_to_transmit = nullptr;
  this->transmissibility_rule = nullptr;
  this->maternity_state = nullptr;
  this->import_admin_code = nullptr;
  this->update_vars = nullptr;
  this->state_is_dormant = nullptr;
  this->exposed_state = -1;
  this->susceptibility_rule = nullptr;
  this->network_mean_degree = nullptr;
  this->network_max_degree = nullptr;
  this->close_groups = nullptr;
  this->number_of_states = 0;
  this->start_hosting = nullptr;
  this->absent_groups = nullptr;
  this->edge_expression = nullptr;
  this->transmissibility = 0.0;

}

/**
 * Default destructor.
 */
Natural_History::~Natural_History() {
}

/**
 * Sets up the specified Condition as the associated condition of this natural history.
 *
 * @param _condition the condition
 */
void Natural_History::setup(Condition* _condition) {

  Natural_History::natural_history_logger->info("Natural_History::setup for condition {:s}",
      _condition->get_name());

  this->condition = _condition;
  this->id = this->condition->get_id();
  strcpy(this->name, this->condition->get_name());
  this->state_space = new State_Space(this->name);

    // read optional properties
  Parser::disable_abort_on_failure();
  
  char property_name[FRED_STRING_SIZE];
  char property_value[FRED_STRING_SIZE];
  strcpy(property_value, "Excluded");
  snprintf(property_name, FRED_STRING_SIZE, "admin_start_state");
  Parser::get_property(property_name, property_value);
  Natural_History::global_admin_start_state = property_value;

  // restore requiring properties
  Parser::set_abort_on_failure();
}

/**
 * Sets up and gets properties of this natural history.
 */
void Natural_History::get_properties() {
  char property_name[FRED_STRING_SIZE];

  Natural_History::natural_history_logger->info("Natural_History::get_properties for condition {:d}", 
      this->condition->get_id());

  // this sets the number and names of the states:
  this->state_space->get_properties();
  this->number_of_states = this->state_space->get_number_of_states();

  // read optional properties
  Parser::disable_abort_on_failure();

  ///////// DAILY REPORT?
  this->daily_report = 1;
  Parser::get_property(get_name(), "daily_report", &this->daily_report);

  ///////// TRANSMISSIBILITY

  this->transmissibility = 0.0;
  Parser::get_property(get_name(), "transmissibility", &this->transmissibility);

  // convenience R0 setting properties
  Parser::get_property(get_name(), "R0", &this->R0);
  Parser::get_property(get_name(), "R0_a", &this->R0_a);
  Parser::get_property(get_name(), "R0_b", &this->R0_b);

  if(this->R0 > 0) {
    this->transmissibility = this->R0_a * this->R0 * this->R0 + this->R0_b * this->R0;
    Natural_History::natural_history_logger->info("R0 = {:f} so setting transmissibility to {:f}", 
        this->R0, this->transmissibility);
  }

  // get state to enter upon exposure
  this->exposed_state = -1;
  char exp_state_name[FRED_STRING_SIZE];
  strcpy(exp_state_name, "");
  Parser::get_property(get_name(), "exposed_state", exp_state_name);
  if (strcmp(exp_state_name,"") != 0) {
    this->exposed_state = this->state_space->get_state_from_name(exp_state_name);
  }
  Natural_History::natural_history_logger->info("exposed state = {:d}", this->exposed_state);

  ///////// TRANSITION MODEL

  // The transition model determines how to decide whether to change
  // state, and which state to change to.

  // STATE ACTIONS
  this->transmissibility_rule = new Rule*[this->number_of_states];
  this->susceptibility_rule = new Rule*[this->number_of_states];
  this->edge_expression = new Expression*[this->number_of_states];
  this->condition_to_transmit = new int[this->number_of_states];
  this->place_type_to_join = new int[this->number_of_states];
  this->place_type_to_quit = new int[this->number_of_states];
  this->network_action = new int[this->number_of_states];
  this->network_type = new int[this->number_of_states];
  this->network_mean_degree = new double[this->number_of_states];
  this->network_max_degree = new int[this->number_of_states];
  this->start_hosting = new int[this->number_of_states];
  this->maternity_state = new bool[this->number_of_states];
  this->fatal_state = new bool[this->number_of_states];

  // STATE DURATION
  this->duration_expression = new Expression**[this->number_of_states];
  this->transition_day = new int*[this->number_of_states];
  this->transition_date = new std::string*[this->number_of_states];
  this->transition_days = new int*[this->number_of_states];
  this->transition_hour = new int*[this->number_of_states];

  // STATE CONTACT RESTRICTIONS
  this->absent_groups = new bool*[this->number_of_states];
  this->close_groups = new bool*[this->number_of_states];

  // IMPORT STATE
  this->import_count = new int[this->number_of_states];
  this->import_per_capita_transmissions = new double[this->number_of_states];
  this->import_latitude = new double[this->number_of_states];
  this->import_longitude = new double[this->number_of_states];
  this->import_radius = new double[this->number_of_states];
  this->import_admin_code = new longint[this->number_of_states];
  this->import_min_age = new double[this->number_of_states];
  this->import_max_age = new double[this->number_of_states];
  this->import_count_rule = new Rule*[this->number_of_states];
  this->import_per_capita_rule = new Rule* [this->number_of_states];
  this->import_ages_rule = new Rule*[this->number_of_states];
  this->import_location_rule = new Rule*[this->number_of_states];
  this->import_admin_code_rule = new Rule*[this->number_of_states];
  this->import_list_rule = new Rule*[this->number_of_states];
  this->count_all_import_attempts = new bool[this->number_of_states];

  // TRANSITIONS
  this->state_is_dormant = new int[this->number_of_states];
  this->default_next_state = new int[this->number_of_states];
  this->default_rule = new Rule*[this->number_of_states];

  // INITIALIZE
  for(int i = 0; i < this->number_of_states; ++i) {
    this->transmissibility_rule[i] = nullptr;
    this->susceptibility_rule[i] = nullptr;
    this->edge_expression[i] = nullptr;
    this->condition_to_transmit[i] = this->id;
    this->place_type_to_join[i] = -1;
    this->place_type_to_quit[i] = -1;
    this->network_action[i] = Network_Action::NONE;
    this->network_type[i] = -1;
    this->network_mean_degree[i] = 0;
    this->network_max_degree[i] = 999999;
    this->start_hosting[i] = 0;
    int n = Group_Type::get_number_of_group_types();
    this->absent_groups[i] = new bool [n];
    this->close_groups[i] = new bool [n];
    for(int j = 0; j < n; ++j) {
      this->absent_groups[i][j] = false;
      this->close_groups[i][j] = false;
    }
    this->state_is_dormant[i] = 0;
    this->default_next_state[i] = -1;
    this->default_rule[i] = nullptr;
    this->import_count[i] = 0;
    this->import_per_capita_transmissions[i] = 0;
    this->import_latitude[i] = 0;
    this->import_longitude[i] = 0;
    this->import_radius[i] = 0;
    this->import_admin_code[i] = 0;
    this->import_min_age[i] = 0;
    this->import_max_age[i] = 999;
    this->maternity_state[i] = false;
    this->fatal_state[i] = false;

    this->import_count_rule[i] = nullptr;
    this->import_per_capita_rule[i] = nullptr;
    this->import_ages_rule[i] = nullptr;
    this->import_location_rule[i] = nullptr;
    this->import_admin_code_rule[i] = nullptr;
    this->import_list_rule[i] = nullptr;
    this->count_all_import_attempts[i] = false;
  }

  // GET PROPERTY VALUES
  for(int i = 0; i < this->number_of_states; ++i) {

    int is_dormant = 0;
    snprintf(property_name, FRED_STRING_SIZE, "%s.%s.is_dormant", get_name(), get_state_name(i).c_str());
    if(Parser::does_property_exist(property_name)) {
      Parser::get_property(property_name, &is_dormant);
      this->state_is_dormant[i] = is_dormant;
    }
    
    // IMPORT STATE
    snprintf(property_name, FRED_STRING_SIZE, "%s.%s.import_max_cases", get_name(), get_state_name(i).c_str());
    this->import_count[i] = 0;
    Parser::get_property(property_name, &this->import_count[i]);
    if(this->import_count[i]) {
      Natural_History::natural_history_logger->info("SETTING {:s}.{:s}.import_max_cases = {:d}", 
          get_name(), get_state_name(i).c_str(), this->import_count[i]);
    }
    
    snprintf(property_name, FRED_STRING_SIZE, "%s.%s.import_per_capita", get_name(), get_state_name(i).c_str());
    this->import_per_capita_transmissions[i] = 0;
    Parser::get_property(property_name, &this->import_per_capita_transmissions[i]);
    if(this->import_per_capita_transmissions[i]) {
      Natural_History::natural_history_logger->info("SETTING {:s}.{:s}.import_per_capita = {:f}", 
          get_name(), get_state_name(i).c_str(), this->import_per_capita_transmissions[i]);
    }
    
    snprintf(property_name, FRED_STRING_SIZE, "%s.%s.import_latitude", get_name(), get_state_name(i).c_str());
    this->import_latitude[i] = 0;
    Parser::get_property(property_name, &this->import_latitude[i]);
    if(this->import_latitude[i]) {
      Natural_History::natural_history_logger->info("SETTING {:s}.{:s}.import_latitude = {:f}", 
          get_name(), get_state_name(i).c_str(), this->import_latitude[i]);
    }
    
    snprintf(property_name, FRED_STRING_SIZE, "%s.%s.import_longitude", get_name(), get_state_name(i).c_str());
    this->import_longitude[i] = 0;
    Parser::get_property(property_name, &this->import_longitude[i]);
    if(this->import_longitude[i]) {
      Natural_History::natural_history_logger->info("SETTING {:s}.{:s}.import_longitude = {:f}", 
          get_name(), get_state_name(i).c_str(), this->import_longitude[i]);
    }
    
    snprintf(property_name, FRED_STRING_SIZE, "%s.%s.import_radius", get_name(), get_state_name(i).c_str());
    this->import_radius[i] = 0;
    Parser::get_property(property_name, &this->import_radius[i]);
    if(this->import_radius[i]) {
      Natural_History::natural_history_logger->info("SETTING {:s}.{:s}.import_radius = {:f}", 
          get_name(), get_state_name(i).c_str(), this->import_radius[i]);
    }

    snprintf(property_name, FRED_STRING_SIZE, "%s.%s.import_min_age", get_name(), get_state_name(i).c_str());
    this->import_min_age[i] = 0;
    Parser::get_property(property_name, &this->import_min_age[i]);
    if(this->import_min_age[i]) {
      Natural_History::natural_history_logger->info("SETTING {:s}.{:s}.import_min_age = {:f}", 
          get_name(), get_state_name(i).c_str(), this->import_min_age[i]);
    }

    snprintf(property_name, FRED_STRING_SIZE, "%s.%s.import_max_age", get_name(), get_state_name(i).c_str());
    this->import_max_age[i] = 999;
    Parser::get_property(property_name, &this->import_max_age[i]);
    if(this->import_max_age[i]) {
      Natural_History::natural_history_logger->info("SETTING {:s}.{:s}.import_max_age = {:f}", 
          get_name(), get_state_name(i).c_str(), this->import_max_age[i]);
    }

    snprintf(property_name, FRED_STRING_SIZE, "%s.%s.import_admin_code", get_name(), get_state_name(i).c_str());
    this->import_admin_code[i] = 0;
    Parser::get_property(property_name, &this->import_admin_code[i]);
    if(this->import_admin_code[i]) {
      Natural_History::natural_history_logger->info("SETTING {:s}.{:s}.import_admin_code = {:d}", 
          get_name(), get_state_name(i).c_str(), this->import_admin_code[i]);
    }

  }

  // get start state for import
  this->import_start_state = -1;
  char import_state_name[FRED_STRING_SIZE];
  strcpy(import_state_name, "none");
  Parser::get_property(get_name(), "import_start_state", import_state_name);
  if (strcmp(import_state_name,"none") != 0) {
    this->import_start_state = this->state_space->get_state_from_name(import_state_name);
  }
  Natural_History::natural_history_logger->info("{:s}.import_start_state = {:s}", 
      get_name(), import_state_name);

  // get start state for admin agents
  char admin_state_name[FRED_STRING_SIZE];
  strcpy(admin_state_name, "none");
  Parser::get_property(get_name(), "admin_start_state", admin_state_name);
  if(strcmp(admin_state_name,"none")==0) {
    strcpy(admin_state_name,Natural_History::global_admin_start_state.c_str());
  }
  this->admin_start_state = this->state_space->get_state_from_name(admin_state_name);
  if(this->admin_start_state == -1) {
    Utils::print_error("Bad admin_start_state: " + std::string(this->name) + ".admin_start_state = " + std::string(admin_state_name));
  }
  Natural_History::natural_history_logger->info("{:s}.admin_start_state = {:s}", 
      get_name(), admin_state_name);

  // RULES

  this->action_rules = new rule_vector_t [this->number_of_states];
  for(int state = 0; state < this->number_of_states; ++state) {
    this->action_rules[state].clear();
  }
  this->wait_rules = new rule_vector_t [this->number_of_states];
  for(int state = 0; state < this->number_of_states; ++state) {
    this->wait_rules[state].clear();
  }
  this->next_rules = new rule_vector_t* [this->number_of_states];
  for(int state = 0; state < this->number_of_states; ++state) {
    this->next_rules[state] = new rule_vector_t [this->number_of_states];
    for(int state2 = 0; state2 < this->number_of_states; ++state2) {
      this->next_rules[state][state2].clear();
    }
  }

  // PERSONAL VARIABLES

  this->update_vars_externally = new bool [this->number_of_states];
  for(int state = 0; state < this->number_of_states; ++state) {
    this->update_vars_externally[state] = false;
  }
  this->update_vars = new bool [this->number_of_states];
  for(int state = 0; state < this->number_of_states; ++state) {
    this->update_vars[state] = false;
  }

  for(int state = 0; state < this->number_of_states; ++state) {

    // decide if we update vars externally in this state
    snprintf(property_name, FRED_STRING_SIZE, "%s.%s.update_vars_externally", get_name(), get_state_name(state).c_str());
    int check = 0;
    Parser::get_property(property_name, &check);
    this->update_vars_externally[state] = check;
    if(check) {
      this->enable_external_update = true;
      // set global external update flag
      Global::Enable_External_Updates = true;
    }
  }

  // restore requiring properties
  Parser::set_abort_on_failure();

  Natural_History::natural_history_logger->info("Natural_History::setup finished for condition {:s}",
      this->condition->get_name());
}

/**
 * Prepares this natural history.
 */
void Natural_History::prepare() {

  char property_name[FRED_STRING_SIZE];

  Natural_History::natural_history_logger->info("Natural_History::prepare entered for condition {:s}", 
      this->condition->get_name());

  prepare_rules();

  // read optional properties
  Parser::disable_abort_on_failure();

  for(int state = 0; state < this->number_of_states; ++state) {

    snprintf(property_name, FRED_STRING_SIZE, "%s.%s.condition_to_transmit", this->get_name(), this->get_state_name(state).c_str());
    std::string condition_name = std::string(this->get_name());
    Parser::get_property(property_name, condition_name);
    int cond_id = Condition::get_condition_id(condition_name);
    this->condition_to_transmit[state] = cond_id;
 

    snprintf(property_name, FRED_STRING_SIZE, "%s.%s.start_hosting", get_name(), get_state_name(state).c_str());
    this->start_hosting[state] = 0;
    Parser::get_property(property_name, &this->start_hosting[state]);
  }

  // transmitted place type
  char new_place_type [FRED_STRING_SIZE];
  snprintf(property_name, FRED_STRING_SIZE, "%s.place_type_to_transmit", get_name());
  strcpy(new_place_type, "");
  Parser::get_property(property_name, new_place_type);
  this->place_type_to_transmit = Place_Type::get_type_id(new_place_type);

  // restore requiring properties
  Parser::set_abort_on_failure();

  Natural_History::natural_history_logger->info("Natural_History::prepare finished for condition {:s}", 
      this->condition->get_name());
}

/**
 * Gets the name of the associated State_Space.
 *
 * @return the name
 */
char* Natural_History::get_name() {
  return this->state_space->get_name();
}

/**
 * Gets the name of the specified condition state in the associated State_Space.
 *
 * @param state the condition state
 * @return the state name
 */
std::string Natural_History::get_state_name(int state) {
  if (state < 0) {
    return "UNSET";
  }
  return this->state_space->get_state_name(state);
}

/**
 * Prints details about this natural history.
 */
void Natural_History::print() {
  Natural_History::natural_history_logger->info("NATURAL HISTORY OF {:s}", get_name());
  std::stringstream natural_history_states_str;
  natural_history_states_str << "NATURAL HISTORY " << get_name() << ".states = ";
  for(int i = 0; i < this->number_of_states; ++i) {
    natural_history_states_str << get_state_name(i).c_str() << " ";
  }
  Natural_History::natural_history_logger->info("{:s}", natural_history_states_str.str());
  Natural_History::natural_history_logger->info("NATURAL HISTORY {:s}.exposed_state = {:s}", 
      get_name(), this->exposed_state > 0 ? get_state_name(this->exposed_state).c_str() : "UNSET");
  Natural_History::natural_history_logger->info("NATURAL HISTORY {:s}.import_start_state = {:s}", 
      get_name(), get_state_name(this->import_start_state).c_str());
  Natural_History::natural_history_logger->info("NATURAL HISTORY {:s}.transmissibility = {:f}", 
      get_name(), this->transmissibility);

  Natural_History::natural_history_logger->info("number of states = {:d}", this->number_of_states);
  for (int i = 0; i < this->number_of_states; i++) {
    char sname[FRED_STRING_SIZE];
    strcpy(sname, get_state_name(i).c_str());

    // import state
    Natural_History::natural_history_logger->info("NATURAL HISTORY {:s}.{:s}.import_max_cases = {:d}", 
        get_name(), sname, this->import_count[i]);
    Natural_History::natural_history_logger->info("NATURAL HISTORY {:s}.{:s}.import_per_capita_transmissions = {:f}", 
        get_name(), sname, this->import_per_capita_transmissions[i]);
    Natural_History::natural_history_logger->info("NATURAL HISTORY {:s}.{:s}.import_latitude = {:f}", 
        get_name(), sname, this->import_latitude[i]);
    Natural_History::natural_history_logger->info("NATURAL HISTORY {:s}.{:s}.import_longitude = {:f}", 
        get_name(), sname, this->import_longitude[i]);
    Natural_History::natural_history_logger->info("NATURAL HISTORY {:s}.{:s}.import_radius = {:f}", 
        get_name(), sname, this->import_radius[i]);
    Natural_History::natural_history_logger->info("NATURAL HISTORY {:s}.{:s}.import_min_age = {:f}", 
        get_name(), sname, this->import_min_age[i]);
    Natural_History::natural_history_logger->info("NATURAL HISTORY {:s}.{:s}.import_max_age = {:f}", 
        get_name(), sname, this->import_max_age[i]);
    Natural_History::natural_history_logger->info("NATURAL HISTORY {:s}.{:s}.import_admin_code = {:l}ld", 
        get_name(), sname, this->import_admin_code[i]);

    // transition
    for(int j = 0; j < this->number_of_states; ++j) {
      for(int n = 0; n < static_cast<int>(this->next_rules[i][j].size()); ++n) {
        Natural_History::natural_history_logger->info("NATURAL HISTORY RULE[{:d}][{:d}][{:d}]: {:s}", 
            i, j, n, this->next_rules[i][j][n]->to_string());
      }
    }

    // transmission info
    std::string stmp = Condition::get_name(this->condition_to_transmit[i]);
    Natural_History::natural_history_logger->info("NATURAL HISTORY {:s}.{:s}.condition_to_transmit = {:s}", 
        get_name(), sname, stmp.c_str());

    // transitions
    Natural_History::natural_history_logger->info("NATURAL HISTORY {:s}.{:s}.state_is_dormant = {:d}", 
        get_name(), sname, this->state_is_dormant[i]);

  }

  Natural_History::natural_history_logger->info("NATURAL HISTORY {:s}.place_type_to_transmit = {:d} {:s}", 
      get_name(), this->place_type_to_transmit,
    this->place_type_to_transmit < 0 ? "NONE" : Place_Type::get_place_type(this->place_type_to_transmit)->get_name());

  for(int state = 0; state < this->number_of_states; ++state) {
    Natural_History::natural_history_logger->info("NATURAL HISTORY {:s}.{:s}.network_action = {:d}", 
        get_name(), get_state_name(state).c_str(), this->network_action[state]);
    Natural_History::natural_history_logger->info("NATURAL HISTORY {:s}.{:s}.network_type = {:d}", 
        get_name(), get_state_name(state).c_str(), this->network_type[state]);
    Natural_History::natural_history_logger->info("NATURAL HISTORY {:s}.{:s}.network_mean_degree = {:f}", 
        get_name(), get_state_name(state).c_str(), this->network_mean_degree[state]);
    Natural_History::natural_history_logger->info("NATURAL HISTORY {:s}.{:s}.network_max_degree = {:d}", 
        get_name(), get_state_name(state).c_str(), this->network_max_degree[state]);
  }
}

/**
 * Gets the simulation step that a specified person at a specified Condition state will transition to the next state 
 * given a day and hour. The duration of this transition will be extended based on the wait rules for the state.
 *
 * @param person the person
 * @param state the condition state
 * @param day the day
 * @param hour the hour
 * @return the transition_step
 */
int Natural_History::get_next_transition_step(Person* person, int state, int day, int hour) {
  int step = 24*day + hour;
  int transition_step = step;

  Natural_History::natural_history_logger->info(
      "get_next_transition_step entered person {:d} state {:s} day {:d} hour {:d}", 
      person->get_id(), get_state_name(state).c_str(), day, hour);

  if(state == 0) {
    // zero transition time for Start
    return transition_step;
  }

  if(state == this->number_of_states - 1) {
    // no transition time from Excluded
    return -1;
  }

  int nrules = this->wait_rules[state].size();
  if(nrules > 0) {
    for (int n = 0; n < nrules; ++n) {

      Rule* rule = this->wait_rules[state][n];
      if(rule->applies(person) == false) {
        continue;
      }

      if(this->duration_expression[state][n]) {
        double duration = this->duration_expression[state][n]->get_value(person);
        transition_step += round(duration);
        break;
      } else if(0 <= this->transition_days[state][n]) {
        transition_step += 24*this->transition_days[state][n] + (this->transition_hour[state][n]-hour);
        break;
      } else if(0 <= this->transition_day[state][n]) {
        int days = this->transition_day[state][n] - Date::get_day_of_week(day);
        if(days < 0) {
          days += 7;
        } else if(days == 0 && this->transition_hour[state][n] < hour) {
          days += 7;
        }
        transition_step += 24 * days + (this->transition_hour[state][n] - hour);
        break;
      } else if (this->transition_date[state][n]!="") {
        transition_step += Date::get_hours_until(this->transition_date[state][n],this->transition_hour[state][n]);
        break;
      }
    }
  } else {
    Natural_History::natural_history_logger->debug(
        "NO WAIT RULES get_next_transition_step person {:d} state {:s} num wait rules = {:d}",
        person->get_id(), get_state_name(state).c_str(), nrules);
  }
  
  Natural_History::natural_history_logger->info(
      "get_next_transition_step finished person {:d} state {:s} trans_step {:d}", 
      person->get_id(), get_state_name(state).c_str(), transition_step);
  return transition_step;
}

/**
 * Calculates the probability distribution of transitioning to the next state, then selects that state 
 * for the given person with the calculated probability distribution.
 *
 * @param person the person
 * @param state the condition state
 * @return the selected state
 */
int Natural_History::get_next_state(Person* person, int state) {

  double total = 0.0;
  double trans_prob [this->number_of_states];
  for(int next = 0; next < this->number_of_states; ++next) {

    trans_prob[next] = 0.0;
    int nrules = this->next_rules[state][next].size();
    if (nrules > 0) {
      // find maximum with_value for any rule that matches this agent
      double max_value = 0.0;
      for(int n = 0; n < nrules; ++n) {
        Rule* rule = this->next_rules[state][next][n];
        double value = rule->get_value(person);
        if(value > max_value) {
          max_value = value;
        }
      }
      // use max_value as transition prob
      trans_prob[next] = max_value;
    }

    // the following is needed to correct for round-off effects in "zero probability" logit computations
    if(trans_prob[next] < 1e-20) {
      trans_prob[next] = 0.0;
    }

    // accumulate total prob weight (will be normalized below)
    total += trans_prob[next];
  }

  if(0.999999999 <= total) {
    // normalize
    for(int next = 0; next < this->number_of_states; ++next) {
      trans_prob[next] /= total;
    }
  } else {
    // total of transition probs is less than 1.0.
    // In this case, the default next state is assigned the remaining probability mass.
    trans_prob[this->default_next_state[state]] += 1.0 - total;
  }

  // DEBUGGING
  if(0) {
    if(Global::Enable_Records) {
      fprintf(Global::Recordsfp, "HEALTH RECORD: person %d COND %s TRANSITION_PROBS: ", person->get_id(), get_name());
      for(int next = 0; next < this->number_of_states; ++next) {
        fprintf(Global::Recordsfp, "%d: %e |", next, trans_prob[next]);
      }
      fprintf(Global::Recordsfp,"\n");
    }
  }

  int next_state = select_next_state(state, trans_prob);

  assert(next_state > -1);

  return next_state;
}

/**
 * Selects the next condition state using the given transition probability distribution.
 *
 * @param state the current condition state
 * @param transition_prob the probability distribution
 * @return the next state
 */
int Natural_History::select_next_state(int state, double* transition_prob) {

  // at this point, we assume transition_prob is a probability distribution

  // check for deterministic transition
  for(int j = 0; j < this->number_of_states; ++j) {
    if(transition_prob[j] == 1.0) {
      return j;
    }
  }

  // draw a random uniform variate
  double r = Random::draw_random();

  // find transition corresponding to this draw
  double sum = 0.0;
  for(int j = 0; j < this->number_of_states; ++j) {
    sum += transition_prob[j];
    if(r < sum) {
      return j;
    }
  }

  // should never reach this point
  Utils::fred_abort("Natural_History::select_next_state: Help! Bad result: state = %d\n",
        state);
  return -1;
}

/**
 * Prepares the rules of this natural history.
 */
void Natural_History::prepare_rules() {

  int n = Rule::get_number_of_compiled_rules();
  for (int i = 0; i < n; i++) {
    Rule* rule = Rule::get_compiled_rule(i);

    Natural_History::natural_history_logger->info("NH: rule = |{:s}|  cond {:d} state {:s}", 
        rule->get_name().c_str(), rule->get_cond_id(), get_state_name(rule->get_state_id()).c_str());

    // ACTION RULE
    if(rule->is_action_rule() && rule->get_cond_id() == this->id) {
      Natural_History::natural_history_logger->info("ACTION RULE");
      int state = rule->get_state_id();
      if(0 <= state) {
        rule->mark_as_used();
        this->action_rules[state].push_back(rule);

        // CHECK FOR FATAL RULE
        if(rule->get_action_id()==Rule_Action::DIE || rule->get_action_id()==Rule_Action::DIE_OLD) {
          this->fatal_state[state] = true;
        }

        // CHECK FOR SUS RULES
        if(rule->get_action_id() == Rule_Action::SUS) {
          if(this->susceptibility_rule[state]!=nullptr) {
            this->susceptibility_rule[state]->mark_as_unused();
          }
          this->susceptibility_rule[state] = rule;
          rule->mark_as_used();
          Natural_History::natural_history_logger->info("SUSCEPTIBILITY RULE: {:s}", rule->to_string());
        }

        // CHECK FOR TRANS RULES
        if(rule->get_action_id() == Rule_Action::TRANS) {
          if(this->transmissibility_rule[state] != nullptr) {
            this->transmissibility_rule[state]->mark_as_unused();
          }
          this->transmissibility_rule[state] = rule;
          rule->mark_as_used();
          Natural_History::natural_history_logger->info("TRANSMISSIBILITY RULE: {:s}", rule->to_string());
        }

        // CHECK FOR IMPORT RULES
        if(rule->get_action_id() == Rule_Action::IMPORT_COUNT) {
          std::string property = rule->get_expression_str();
          if(this->import_count_rule[state] != nullptr) {
            this->import_count_rule[state]->mark_as_unused();
          }
          this->import_count_rule[state] = rule;
          rule->mark_as_used();
          Natural_History::natural_history_logger->info("IMPORT RULE: {:s}", rule->to_string()); 
        }

        if(rule->get_action_id() == Rule_Action::IMPORT_PER_CAPITA) {
          if(this->import_per_capita_rule[state]!=nullptr) {
            this->import_per_capita_rule[state]->mark_as_unused();
          }
          this->import_per_capita_rule[state] = rule;
          rule->mark_as_used();
          Natural_History::natural_history_logger->info("IMPORT RULE: {:s}", rule->to_string());
        }

        if(rule->get_action_id() == Rule_Action::IMPORT_AGES) {
          if(this->import_ages_rule[state] != nullptr) {
            this->import_ages_rule[state]->mark_as_unused();
          }
          this->import_ages_rule[state] = rule;
          rule->mark_as_used();
          Natural_History::natural_history_logger->info("IMPORT RULE: {:s}", rule->to_string()); 
        }

        if(rule->get_action_id() == Rule_Action::IMPORT_LOCATION) {
          if(this->import_location_rule[state] != nullptr) {
            this->import_location_rule[state]->mark_as_unused();
          }
          this->import_location_rule[state] = rule;
          rule->mark_as_used();
          Natural_History::natural_history_logger->info("IMPORT RULE: {:s}", rule->to_string()); 
        }

        if(rule->get_action_id() == Rule_Action::IMPORT_ADMIN_CODE) {
          if(this->import_admin_code_rule[state] != nullptr) {
            this->import_admin_code_rule[state]->mark_as_unused();
          }
          this->import_admin_code_rule[state] = rule;
          rule->mark_as_used();
          Natural_History::natural_history_logger->info("IMPORT RULE: {:s}", rule->to_string()); 
        }

        if(rule->get_action_id() == Rule_Action::IMPORT_LIST) {
          if(this->import_list_rule[state] != nullptr) {
            this->import_list_rule[state]->mark_as_unused();
          }
          this->import_list_rule[state] = rule;
          rule->mark_as_used();
          Natural_History::natural_history_logger->info("IMPORT RULE: {:s}", rule->to_string()); 
        }

        if(rule->get_action_id() == Rule_Action::COUNT_ALL_IMPORT_ATTEMPTS) {
          this->count_all_import_attempts[state] = true;
          rule->mark_as_used();
          Natural_History::natural_history_logger->info("IMPORT RULE: {:s}", rule->to_string()); 
        }

        // CHECK FOR SCHEDULE RULES
        if(rule->is_schedule_rule()) {
          std::string group_type_str = rule->get_expression_str();
          Natural_History::natural_history_logger->info(
              "COMPILE_RULES: {:s} group_type_str = |{:s}| ", this->name, group_type_str.c_str());
          string_vector_t group_type_vec = Utils::get_string_vector(group_type_str,',');
          Natural_History::natural_history_logger->info(
              "COMPILE_RULES: {:s} group_type_vec size {:d} ", this->name, group_type_vec.size());
          for(int k = 0; k < static_cast<int>(group_type_vec.size()); ++k) {
            std::string group_name = group_type_vec[k];
            int type_id = Group_Type::get_type_id(group_name);
            if(rule->get_action()=="absent") {
              this->absent_groups[state][type_id] = true;
            }
            if(rule->get_action()=="present") {
              this->absent_groups[state][type_id] = false;
            }
            if(rule->get_action()=="close") {
              this->close_groups[state][type_id] = true;
            }
            Natural_History::natural_history_logger->info(
                "COMPILE: cond {:s} state {:s} {:s} group_name {:s} type_id {:d}", this->name, 
                get_state_name(state).c_str(), rule->get_action().c_str(), group_name.c_str(), type_id);
          }
          // debugging:
          if(rule->get_action()=="present") {
            int n = Group_Type::get_number_of_group_types();
            for(int k = 0; k < n; ++k) {
              if(this->absent_groups[state][k]) {
                std::string group_name = Group_Type::get_group_type_name(k);
                Natural_History::natural_history_logger->info(
                    "COMPILE: cond {:s} state {:s} UPDATED ABSENT group_name {:s}", this->name, 
                    get_state_name(state).c_str(), group_name.c_str());
              }
            }
          }
        } ////////// SCHED
      }
    } // END ACTION RULES

    // WAIT RULES
    if(rule->is_wait_rule() && rule->get_cond_id() == this->id) {
      Natural_History::natural_history_logger->info("WAIT RULE");
      int state = rule->get_state_id();
      if(0 < state) {
        rule->mark_as_used();
        this->wait_rules[state].push_back(rule);
      }
    }

    // EXPOSURE RULE
    if(rule->is_exposure_rule() && rule->get_cond_id() == this->id) {
      Natural_History::natural_history_logger->info("EXPOSURE RULE");
      int next_state_id = rule->get_next_state_id();
      if(0 <= next_state_id) {
        if(this->exposure_rule != nullptr) {
          this->exposure_rule->set_hidden_by_rule(rule);
          this->exposure_rule->mark_as_unused();
        }
        this->exposure_rule = rule;
        this->exposure_rule->mark_as_used();
        this->exposed_state = next_state_id;
      }
    }

    // NEXT RULE
    if(rule->is_next_rule() && rule->get_cond_id() == this->id) {
      int state = rule->get_state_id();
      int next_state = rule->get_next_state_id();
      Natural_History::natural_history_logger->info("NEXT RULE cond {:d} state {:d} next_state {:d}", 
          this->id, state,next_state);
      if(0 <= state && 0 <= next_state) {
        rule->mark_as_used();
        this->next_rules[state][next_state].push_back(rule);
      }
    }

    // DEFAULT RULE
    if(rule->is_default_rule() && rule->get_cond_id() == this->id) {
      Natural_History::natural_history_logger->info("DEFAULT RULE");
      int state_id = rule->get_state_id();
      if(0 <= state_id) {
        int next_state_id = rule->get_next_state_id();
        if(0 <= next_state_id) {
          if(this->default_rule[state_id] != nullptr) {
            this->default_rule[state_id]->set_hidden_by_rule(rule);
            this->default_rule[state_id]->mark_as_unused();
          }
          this->default_rule[state_id] = rule;
          this->default_rule[state_id]->mark_as_used();
          this->default_next_state[state_id] = next_state_id;
        }
      }
    }
    Natural_History::natural_history_logger->info("RULE {:d} FINISHED", i);
  }

  Natural_History::natural_history_logger->info("EXPOSURE RULE:");
  if(this->exposure_rule) {
    Natural_History::natural_history_logger->info("{:s}", this->exposure_rule->to_string());
  }
  
  for(int i = 0; i < this->number_of_states; ++i) {

    Natural_History::natural_history_logger->info("ACTION RULES for state {:d}:", i);
    for(int n = 0; n < static_cast<int>(this->action_rules[i].size()); ++n) {
      Natural_History::natural_history_logger->info("{:s}", this->action_rules[i][n]->to_string());
    }

    Natural_History::natural_history_logger->info("WAIT RULES for state {:d}:", i);
    for(int n = 0; n < static_cast<int>(this->wait_rules[i].size()); ++n) {
      Natural_History::natural_history_logger->info("{:s}", this->wait_rules[i][n]->to_string());
    }
    if(this->wait_rules[i].size() == 0) {
      if(i > 0 && i < this->number_of_states-1) {
        Utils::print_error("No wait rule found for state " + std::string(this->name) + "." + this->get_state_name(i));
        Natural_History::natural_history_logger->error("No wait rule found for state {:d} {:s}.{:s}", 
            i, this->name, this->get_state_name(i).c_str());
      } else {
        // no wait rules are processed for Start and Excluded.
        // instead, Start always has a zero wait time and
        // Excluded has infinite wait time
      }
    }

    int trans_found = 0;
    for(int j = 0; j < this->number_of_states; ++j) {
      Natural_History::natural_history_logger->info("NEXT RULES for transition {:d} to {:d} = {:d}:",
          i, j, static_cast<int>(this->next_rules[i][j].size()));
      for(int n = 0; n < static_cast<int>(this->next_rules[i][j].size()); ++n) {
        Natural_History::natural_history_logger->info("{:s}", this->next_rules[i][j][n]->to_string());
        trans_found = 1;
      }
    }
  
    Natural_History::natural_history_logger->info("DEFAULT RULE for state {:d}:",i);
    if(this->default_rule[i]) {
      Natural_History::natural_history_logger->info("{:s}", this->default_rule[i]->to_string());
    }
    
    // special case for Start state
    if(i == 0 && this->default_next_state[i] == -1) {
      if(trans_found) {
        // if trans rules exist but no default, default to Excluded
        this->default_next_state[i] = this->number_of_states-1;
      } else {
        // if no trans rules or default rules exist default to first state
        this->default_next_state[i] = 1;
      }
    }

    if(this->default_next_state[i] == -1) {
      this->default_next_state[i] = i;
      if(trans_found == 0 && i < this->number_of_states - 1) { // no warning for Excluded
        Utils::print_warning("No transition rules found for state " + std::string(this->name) + "." + this->get_state_name(i) + " -- Will self-transition by default");
        Natural_History::natural_history_logger->warn(
            "No transition rules found for state {:s}.{:s}  -- Will self-transition by default", 
            this->name, this->get_state_name(i).c_str());
      }
    }

    // COMPILE WAIT RULES
    int nrules = this->wait_rules[i].size();
    if(nrules > 0) {
      this->duration_expression[i] = new Expression* [nrules];
      this->transition_day[i] = new int [nrules]; 
      this->transition_date[i] = new std::string [nrules]; 
      this->transition_days[i] = new int [nrules]; 
      this->transition_hour[i] = new int [nrules]; 
    } else {
      // Utils::print_error("No wait rules found for state " + string(this->name) + "." + get_state_name(i));
    }
    
    int unconditional = 0;
    for(int n = 0; n < nrules; ++n) {

      // initialization
      this->duration_expression[i][n] = nullptr;
      this->transition_day[i][n] = -1;
      this->transition_date[i][n] = "";
      this->transition_days[i][n] = -1;
      this->transition_hour[i][n] = 0;

      Rule* rule = this->wait_rules[i][n];
      rule->mark_as_used();

      if(rule->get_clause() == nullptr) {
        unconditional = 1;
      }

      int state = i;
      std::string action = rule->get_action();
      if(action == "wait") {
        this->duration_expression[state][n] = rule->get_expression();
      } else { // "wait_until" rule
        std::string ttime = rule->get_expression_str();
        if(ttime.find("Today") == 0) {
          this->transition_days[state][n] = 0;
        }
        if(ttime.find("today") == 0) {
          this->transition_days[state][n] = 0;
        }
        if(ttime.find("Tomorrow") == 0) {
          this->transition_days[state][n] = 1;
        }
        if(ttime.find("tomorrow") == 0) {
          this->transition_days[state][n] = 1;
        }
        if(ttime.find("_day") != std::string::npos) {
          int pos = ttime.find("_day");
          std::string dstr = ttime.substr(0, pos);
          int d = -1;
          sscanf(dstr.c_str(), "%d", &d);
          this->transition_days[state][n] = d;
        }
        if(this->transition_days[state][n] == -1) {
          if(ttime.find("Sun") == 0) {
            this->transition_day[state][n] = 0;
          } else if(ttime.find("Mon") == 0) {
            this->transition_day[state][n] = 1;
          } else if(ttime.find("Tue") == 0) {
            this->transition_day[state][n] = 2;
          } else if(ttime.find("Wed") == 0) {
            this->transition_day[state][n] = 3;
          } else if(ttime.find("Thu") == 0) {
            this->transition_day[state][n] = 4;
          } else if(ttime.find("Fri") == 0) {
            this->transition_day[state][n] = 5;
          } else if(ttime.find("Sat") == 0) {
            this->transition_day[state][n] = 6;
          } else {
            int pos = ttime.find("_at_");
            if(pos != static_cast<int>(std::string::npos)) {
              this->transition_date[state][n] = ttime.substr(0,pos);
            } else {
              this->transition_date[state][n] = ttime;
            }
          }
        }
        int hour = 0;
        int pos = ttime.find("_at_");
        if(pos != static_cast<int>(std::string::npos)) {
          std::string hstr = ttime.substr(pos + 4);
          sscanf(hstr.c_str(), "%d", &hour);
          if(hour == 12 && hstr.find("am") != std::string::npos) {
            hour = 0;
          }
          if(hour < 12 && hstr.find("pm") != std::string::npos) {
            hour += 12;
          }
        }
        this->transition_hour[state][n] = hour;
        Natural_History::natural_history_logger->info("transition_hour = {:d}", 
            this->transition_hour[state][n]);
      }
    }
    if(unconditional == 0 && i > 0 && i < this->number_of_states - 1) {
      Utils::print_error("No unconditional wait rules found for state " + std::string(this->name) + "." + this->get_state_name(i));
      Natural_History::natural_history_logger->error(
          "No unconditional wait rules found for state {:s}.{:s}", this->name, 
          this->get_state_name(i).c_str());
    }
  }
}

/**
 * Checks if the specified condition state is absent from the specified Group_Type.
 *
 * @param state the condition state
 * @param group_type_id the group type ID
 * @return if the state is absent
 */
bool Natural_History::is_absent(int state, int group_type_id) {
  if(state < 0) {
    return false;
  }
  return this->absent_groups[state][group_type_id];
}

/**
 * Checks if the specified Group_Type is closed from the specified state.
 *
 * @param state the condition state
 * @param group_type_id the group type ID
 * @return if the group type is closed
 */
bool Natural_History::is_closed(int state, int group_type_id) {
  if(state < 0) {
    return false;
  }
  return this->close_groups[state][group_type_id];
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Natural_History::setup_logging() {
  if(Natural_History::is_log_initialized) {
    return;
  }

  if(Parser::does_property_exist("natural_history_log_level")) {
    Parser::get_property("natural_history_log_level", &Natural_History::natural_history_log_level);
  } else {
    Natural_History::natural_history_log_level = "OFF";
  }

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Natural_History::natural_history_logger = std::make_unique<spdlog::logger>("natural_history_logger", 
        sink_list.begin(), sink_list.end());
    Natural_History::natural_history_logger->set_level(
        Utils::get_log_level_from_string(Natural_History::natural_history_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Natural_History::natural_history_logger->trace("<{:s}, {:d}>: Natural_History logger initialized", 
      __FILE__, __LINE__  );
  Natural_History::is_log_initialized = true;
}
