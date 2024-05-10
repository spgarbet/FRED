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
// File: Condition.h
//

#ifndef _FRED_Condition_H
#define _FRED_Condition_H

#include <spdlog/spdlog.h>

#include "Global.h"

class Epidemic;
class Expression;
class Natural_History;
class Network;
class Person;
class Preference;
class Place;
class Rule;
class Transmission;

/**
 * This class represents a condition that a person may have, usually an infectious condition.
 *
 * Every condition has an associated Epidemic, Natural_History, Transmission, and 
 * Transmission_Network. Conditions can be divided into different condition states, 
 * which represent the state of the condition that the host is in. Many Condition 
 * methods are simply calls to the Epidemic model's method counterparts.
 */
class Condition {
public:

  Condition();
  ~Condition();

  void get_properties(int condition, std::string name);

  void setup();
  void setup(int phase); // _UNUSED_
  void prepare();

  static void initialize_person(Person* person);
  void initialize_person(Person* person, int day);

  /**
   * Gets the ID of this condition.
   *
   * @return the ID
   */
  int get_id() {
    return this->id;
  }

  /**
   * Gets the transmissibility of this condition.
   *
   * @return the transmissibility
   */
  double get_transmissibility() {
    return this->transmissibility;
  }

  /**
   * Sets the transmissibility of this condition.
   *
   * @param value the new transmissability
   */
  void set_transmissibility(double value) {
    this->transmissibility = value;
  }

  double get_attack_rate();
  void report(int day);

  /**
   * Gets the Epidemic model with which this condition is associated.
   *
   * @return the epidemic
   */
  Epidemic* get_epidemic() {
    return this->epidemic;
  }

  void increment_group_state_count(int group_type_id, Group* place, int state);
  void decrement_group_state_count(int pgroup_type_id, Group* place, int state);

  static double get_prob_stay_home(); // _UNUSED_
  static void set_prob_stay_home(double); // _UNUSED_

  void increment_cohort_host_count(int day);

  void update(int day, int hour);

  void terminate_person(Person* person, int day);

  /**
   * Gets the name of this condition.
   *
   * @return the name
   */
  char* get_name() {
    return this->condition_name;
  }

  /**
   * Gets the Natural_History of this condition.
   *
   * @return the natural history
   */
  Natural_History* get_natural_history() {
    return this->natural_history;
  }

  bool is_external_update_enabled();

  int get_condition_to_transmit(int state);

  bool state_gets_external_updates(int state);

  // transmission mode

  /**
   * Gets the transmission mode of this condition.
   *
   * @return the transmission mode
   */
  char* get_transmission_mode() {
    return this->transmission_mode;
  }

  /**
   * Gets the Transmission of this condition.
   *
   * @return the transmission
   */
  Transmission* get_transmission() {
    return this->transmission;
  }

  int get_number_of_states();

  std::string get_state_name(int i);

  int get_state_from_name(std::string state_name);

  void finish();

  void track_group_state_counts(int type_id, int state);

  int get_incidence_group_state_count(Group* group, int state) { return 0; } // _UNUSED_
  int get_current_group_state_count(Group* group, int state);
  int get_total_group_state_count(Group* group, int state);

  int get_incidence_count(int state);
  int get_current_count(int state);
  int get_total_count(int state);

  int get_place_type_to_transmit();

  bool health_records_are_enabled();

  /**
   * Gets the transmission Network of this condition.
   *
   * @return the transmission network
   */
  Network* get_transmission_network() {
    return this->transmission_network;
  }

  rule_vector_t get_action_rules(int state); // _UNUSED_

  bool is_absent(int state, int group_type_id);
  bool is_closed(int state, int group_type_id);

  bool make_daily_report();

  /**
   * Adds the specified condition name to the static condition names vector,
   * if it is not already included.
   *
   * @param cond the condition name
   */
  static void include_condition(std::string cond) {
    int size = Condition::condition_names.size();
    for(int i = 0; i < size; ++i) {
      if(cond == Condition::condition_names[i]) {
        return;
      }
    }
    Condition::condition_names.push_back(cond);
  }

  /**
   * Removes the specified condition name from the static condition names vector, 
   * if it is not already excluded.
   *
   * @param cond the condition name
   */
  static void exclude_condition(std::string cond) {
    int size = Condition::condition_names.size();
    for(int i = 0; i < size; ++i) {
      if(cond == Condition::condition_names[i]) {
        for(int j = i + 1; j < size; ++j) {
          Condition::condition_names[j - 1] = Condition::condition_names[j];
        }
        Condition::condition_names.pop_back();
      }
    }
  }

  static void get_condition_properties();

  static void setup_conditions();
  static void setup_logging();

  /**
   * Gets the Condition with the specified ID.
   *
   * @param condition_id the ID
   * @return the condition
   */
  static Condition* get_condition(int condition_id) {
    return Condition::conditions[condition_id];
  }

  static Condition* get_condition(std::string condition_name);

  static int get_condition_id(std::string condition_name);

  /**
   * Gets the name of the Condition with the specified ID.
   *
   * @param condition_id the ID
   * @return the condition name
   */
  static std::string get_name(int condition_id) {
    return Condition::condition_names[condition_id];
  }

  /**
   * Gets the number of Condition objects active in the simulation.
   *
   * @return the number of conditions
   */
  static int get_number_of_conditions() {
    return Condition::number_of_conditions;
  }

  static void prepare_conditions();

  static void prepare_to_track_group_state_counts();

  static void finish_conditions();

private:

  // condition identifiers
  int id;
  char condition_name[FRED_STRING_SIZE];

  // the course of the condition within a host
  Natural_History* natural_history;

  // how the condition spreads
  char transmission_mode[FRED_STRING_SIZE];
  Transmission* transmission;

  // the transmission network
  char transmission_network_name[FRED_STRING_SIZE];
  Network* transmission_network;

  // contagiousness
  double transmissibility;

  // the course of infection at the population level
  Epidemic* epidemic;

  static std::vector <Condition*> conditions;
  static std::vector<std::string> condition_names;
  static int number_of_conditions;

  static bool is_log_initialized;
  static std::string condition_log_level;
  static std::unique_ptr<spdlog::logger> condition_logger;
};

#endif // _FRED_Condition_H
