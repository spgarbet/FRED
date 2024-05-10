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

#ifndef _FRED_NATURAL_HISTORY_H
#define _FRED_NATURAL_HISTORY_H

#include <spdlog/spdlog.h>

#include "Condition.h"
#include "Expression.h"
#include "Global.h"
#include "Person.h"
#include "Preference.h"
#include "Rule.h"
#include "State_Space.h"

typedef long long int longint;

/**
 * This class represents the natural history of an infection throughout the simulation.
 *
 * Natural_History is associated with a specific Condition, that condition's Epidemic model, 
 * and a State_Space model. A natural history tracks data on the course an infection takes during 
 * its spread thruoghout a population. Specifically, this class contains data corresponding 
 * to different states in the associated condition, as well as the way that condition spreads.
 */
class Natural_History {

public:

  Natural_History();

  ~Natural_History();

  void setup(Condition *condition);

  void get_properties();

  void setup(int phase); // _UNUSED_

  void prepare();

  void prepare_rules();

  void compile_rules(); // _UNUSED_

  void print();

  /**
   * Gets the edge expression for the specified condition state.
   *
   * @param state the condition state
   * @return the edge expression
   */
  Expression* get_edge_expression(int state) {
    return this->edge_expression[state];
  }

  char* get_name();

  /**
   * Gets the number of condition states.
   *
   * @return the number of states
   */
  int get_number_of_states() {
    return this->number_of_states;
  }

  std::string get_state_name(int i);

  int get_next_state(Person* person, int state);

  int select_next_state(int state, double* transition_prob);

  int get_next_transition_step(Person* person, int state, int day, int hour);

  /**
   * Gets the exposed condition state.
   *
   * @return the exposed state
   */
  int get_exposed_state() {
    return this->exposed_state;
  }

  /**
   * Checks if the specified state is a maternity state.
   *
   * @param state the condition state
   * @return if the state is a maternity state
   */
  bool is_maternity_state(int state) {
    return this->maternity_state[state];
  }

  /**
   * Checks if the specified state is a fatal state.
   *
   * @param state the condition state
   * @return if the state is a fatal state
   */
  bool is_fatal_state(int state) {
    return this->fatal_state[state];
  }

  /**
   * Checks if the specified state is a dormant state.
   *
   * @param state the condition state
   * @return if the state is a dormant state
   */
  bool is_dormant_state(int state) {
    return (this->state_is_dormant[state] == 1);
  }

  /**
   * Gets the transmissibility.
   *
   * @return the transmissibility
   */
  double get_transmissibility() {
    return this->transmissibility;
  }

  /**
   * Gets the Condition to transmit for the specified state.
   *
   * @param state the condition state
   * @return the condition index
   */
  int get_condition_to_transmit(int state) {
    return this->condition_to_transmit[state];
  }

  /**
   * Checks if external update is enabled.
   *
   * @return if external update is enabled
   */
  bool is_external_update_enabled() {
    return this->enable_external_update;
  }

  /**
   * Checks if the specified state is set to get external updates.
   *
   * @param state the condition state
   * @return if the state gets external updates
   */
  bool state_gets_external_updates(int state) {
    return this->update_vars_externally[state];
  }

  /**
   * Gets the Place_Type to join for the specified state.
   *
   * @param state the condition state
   * @return the place type index
   */
  int get_place_type_to_join(int state) {
    return this->place_type_to_join[state];
  }

  /**
   * Gets the Place_Type to quit for the specified state.
   *
   * @param state the condition state
   * @return the place type index
   */
  int get_place_type_to_quit(int state) {
    return this->place_type_to_quit[state];
  }

  /**
   * Gets the action rules for a specified state.
   *
   * @param state the condition state
   * @return the action rules
   */
  rule_vector_t get_action_rules(int state) {
    return this->action_rules[state];
  }

  /**
   * Gets the Network_Type for a specified conditon state.
   *
   * @param state the condition state
   * @return the network type index
   */
  int get_network_type(int state) {
    return this->network_type[state];
  }

  /**
   * Gets the Network mean degree for a specified condition state.
   *
   * @param state the condition state
   * @return the network mean degree
   */
  double get_network_mean_degree(int state) {
    return this->network_mean_degree[state];
  }

  /**
   * Gets the Network max degree for a specified condition state.
   *
   * @param state the condition state
   * @return the network max degree
   */
  int get_network_max_degree(int state) {
    return this->network_max_degree[state];
  }

  /**
   * Checks if the specified condition state should start hosting.
   *
   * @param state the condition state
   * @return if the specified state should start hosting
   */
  int should_start_hosting(int state) {
    return this->start_hosting[state];
  }

  /**
   * Gets the Place_Type to transmit.
   *
   * @return the place type
   */
  int get_place_type_to_transmit() {
    return this->place_type_to_transmit;
  }

  /**
   * Gets the import start state.
   *
   * @return the import start state
   */
  int get_import_start_state() {
    return this->import_start_state;
  }

  /**
   * Gets the admin start state.
   *
   * @return the admin start state
   */
  int get_admin_start_state() {
    return this->admin_start_state;
  }

  /**
   * Gets the import count for a specified condition state.
   *
   * @param state the condition state
   * @return the import count
   */
  int get_import_count(int state) {
    return this->import_count[state];
  }

  /**
   * Gets the import per capita transmissions for a specified condition state.
   *
   * @param state the condition state
   * @return the import per capita transmissions
   */
  double get_import_per_capita_transmissions(int state) {
    return this->import_per_capita_transmissions[state];
  }

  /**
   * Gets the import latitude for the specified condition state.
   *
   * @param state the condition state
   * @return the import latitude
   */
  double get_import_latitude(int state) {
    return this->import_latitude[state];
  }

  /**
   * Gets the import longitude for the specified condition state.
   *
   * @param state the condition state
   * @return the import longitude
   */
  double get_import_longitude(int state) {
    return this->import_longitude[state];
  }

  /**
   * Gets the import radius for the specified condition state.
   *
   * @param state the condition state
   * @return the import radius
   */
  double get_import_radius(int state) {
    return this->import_radius[state];
  }

  /**
   * Gets the import minimum age for the specified condition state.
   *
   * @param state the condition state
   * @return the import minimum age
   */
  double get_import_min_age(int state) {
    return this->import_min_age[state];
  }

  /**
   * Gets the import maximum age for the specified condition state.
   *
   * @param state the condition state
   * @return the import maximum age
   */
  double get_import_max_age(int state) {
    return this->import_max_age[state];
  }

  /**
   * Gets the import admin code for the specified condition state.
   *
   * @param state the condition state
   * @return the import admin code
   */
  longint get_import_admin_code(int state) {
    return this->import_admin_code[state];
  }

  bool is_absent(int state, int group_type_id);

  bool is_closed(int state, int group_type_id);

  /**
   * Gets the import count rule for the specified condition state.
   *
   * @param state the condition state
   * @return the import count rule
   */
  Rule* get_import_count_rule(int state) {
    return this->import_count_rule[state];
  }

  /**
   * Gets the import per capita rule for the specified condition state.
   *
   * @param state the condition state
   * @return the import per capita rule
   */
  Rule* get_import_per_capita_rule(int state) {
    return this->import_per_capita_rule[state];
  }

  /**
   * Gets the import ages rule for the specified condition state.
   *
   * @param state the condition state
   * @return the import ages rule
   */
  Rule* get_import_ages_rule(int state) {
    return this->import_ages_rule[state];
  }

  /**
   * Gets the import location rule for the specified condition state.
   *
   * @param state the condition state
   * @return the import location rule
   */
  Rule* get_import_location_rule(int state) {
    return this->import_location_rule[state];
  }

  /**
   * Gets the import admin code rule for the specified condition state.
   *
   * @param state the condition state
   * @return the import admin code rule
   */
  Rule* get_import_admin_code_rule(int state) {
    return this->import_admin_code_rule[state];
  }

  /**
   * Gets the import list rule for the specified condition state.
   *
   * @param state the condition state
   * @return the import list rule
   */
  Rule* get_import_list_rule(int state) {
    return this->import_list_rule[state];
  }

  /**
   * Checks if all import attempts were counted for the specified condition state.
   *
   * @param state the condition state
   * @return if all import attempts were counted
   */
  bool all_import_attempts_count(int state) {
    return this->count_all_import_attempts[state];
  }

  /**
   * Checks if this natural history has made a daily report. This will be true if 
   * this natural history's daily_report equates to true.
   *
   * @return if there is a daily report
   */
  bool make_daily_report() {
    return this->daily_report;
  }
  
  static void setup_logging();

protected:

  Condition* condition;
  char name[FRED_STRING_SIZE];
  int id;

  // STATE MODEL
  State_Space* state_space;
  int number_of_states;

  // RULES
  rule_vector_t* action_rules;
  rule_vector_t* wait_rules;
  rule_vector_t** next_rules;
  Rule** default_rule;

  // STATE SIDE EFFECTS
  Rule** susceptibility_rule;
  Rule** transmissibility_rule;
  Expression** edge_expression;
  int* condition_to_transmit;
  int* place_type_to_join;
  int* place_type_to_quit;
  int* network_action;
  int* network_type;
  double* network_mean_degree;
  int* network_max_degree;
  int* start_hosting;
  bool* maternity_state;
  bool* fatal_state;

  // PERSONAL VARIABLES
  bool* update_vars;
  bool* update_vars_externally;
  bool enable_external_update;

  // IMPORT STATE
  int import_start_state;
  int* import_count;
  double* import_per_capita_transmissions;
  double* import_latitude;
  double* import_longitude;
  double* import_radius;
  longint* import_admin_code;
  double* import_min_age;
  double* import_max_age;
  Rule** import_count_rule;
  Rule** import_per_capita_rule;
  Rule** import_ages_rule;
  Rule** import_location_rule;
  Rule** import_admin_code_rule;
  Rule** import_list_rule;
  bool* count_all_import_attempts;

  // STATE CONTACT RESTRICTIONS
  bool** absent_groups;
  bool** close_groups;

  // TRANSMISSIBILITY
  double transmissibility;
  double R0;
  double R0_a;
  double R0_b;
  int place_type_to_transmit;
  int exposed_state;
  Rule* exposure_rule;

  // TRANSITION MODEL
  Expression*** duration_expression;
  int** transition_day;
  std::string** transition_date;
  int** transition_days;
  int** transition_hour;

  int* default_next_state;
  int* state_is_dormant;

  // ADMIN START STATE
  static std::string global_admin_start_state;
  int admin_start_state;

  // REPORTIMG STATUS
  int daily_report;

private:
  static bool is_log_initialized;
  static std::string natural_history_log_level;
  static std::unique_ptr<spdlog::logger> natural_history_logger;
};

#endif
