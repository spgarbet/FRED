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
// File: Group.h
//

#ifndef _FRED_GROUP_H_
#define _FRED_GROUP_H_

#include <iomanip>
#include <limits>

#include <spdlog/spdlog.h>

#include "Global.h"


class Person;
class Group_Type;

/**
 * This class represents a group in which agents in the simulation can interact with each other.
 *
 * Groups can take different forms; some examples are schools, households, workplaces, and social 
 * networks. It is through these groups that infection spreads. Groups can be identified by an ID 
 * or label, which are created as the program runs, or their Synthetic Population (SP) ID, which 
 * is predefined and completely unique. Groups track data on the members of the group and the spread 
 * of infection within the group.
 *
 * This class is inherited by Network and Place.
 */
class Group {
public:

  static char TYPE_UNSET;
  static char SUBTYPE_NONE;


  Group(const char* lab, int _type_id);
  ~Group();

  /**
   * Gets the ID of this group.
   *
   * @return the ID
   */
  int get_id() const {
    return this->id;
  }

  /**
   * Sets the ID of this group.
   *
   * @param _id the new ID
   */
  void set_id(int _id) {
    this->id = _id;
  }

  /**
   * Gets the index of this group. This is the index in it's Group_Type's vector of groups of that type.
   *
   * @return the index
   */
  int get_index() const {
    return this->index;
  }

  /**
   * Sets the index of this group. This is the index in it's Group_Type's vector of groups of that type.
   *
   * @param _index the new index
   */
  void set_index(int _index) {
    this->index = _index;
  }

  /**
   * Gets the ID of this group's Group_Type.
   *
   * @return the group type ID
   */
  int get_type_id() const {
    return this->type_id;
  }

  Group_Type* get_group_type();

  /**
   * Gets the subtype of this group.
   *
   * @return the subtype
   */
  char get_subtype() const {
    return this->subtype;
  }

  /**
   * Sets the subtype of this group.
   *
   * @param _subtype the new subtype
   */
  void set_subtype(char _subtype) {
    this->subtype = _subtype;
  }

  /**
   * Gets the label of this group.
   *
   * @return the label
   */
  char* get_label() {
    return this->label;
  }

  int get_adults();
  int get_children();

  /**
   * Gets the average income of members of this group.
   *
   * @return the average income
   */
  int get_income() {
    return this->income;
  }

  /**
   * Sets the median income of members of this group.
   *
   * @param _income the median income
   */
  void set_income(int _income) {
    this->income = _income;
  }

  int begin_membership(Person* per);
  void end_membership(int pos);

  double get_proximity_same_age_bias();
  double get_proximity_contact_rate();
  double get_density_contact_prob(int condition_id);

  bool can_transmit(int condition_id);
  double get_contact_rate(int condition_id);
  int get_contact_count(int condition_id);
  bool use_deterministic_contacts(int condition_id);
  bool use_density_transmission(int condition_id);

  /**
   * Gets the number of members in this group.
   *
   * @return the number of members
   */
  int get_size() {
    return this->members.size();
  }

  /**
   * Gets the original number of members in this group.
   *
   * @return the original number of members
   */
  int get_original_size() {
    return this->N_orig;
  }

  /**
   * Gets the members of this group as a person vector.
   *
   * @return the members
   */
  person_vector_t* get_members() {
    return &(this->members);
  }

  /**
   * Gets the transmissible people in this group's transmissible people person vector
   * with the specified Condition as a person vector.
   *
   * @param condition_id the condition ID
   * @return the transmissible people
   */
  person_vector_t* get_transmissible_people(int  condition_id) {
    return &(this->transmissible_people[condition_id]);
  }

  /**
   * Gets the member of this group at the specified index in the members person vector.
   *
   * @param i the index
   * @return the member
   */
  Person* get_member(int i) {
    return this->members[i];
  }

  void record_transmissible_days(int day, int condition_id);

  void print_transmissible(int condition_id);

  /**
   * Clears the transmissible people in this group's transmissible people person vector
   * who had the specified Condition.
   *
   * @param condition_id the condition ID
   */
  void clear_transmissible_people(int condition_id) {
    this->transmissible_people[condition_id].clear();
  }

  void add_transmissible_person(int condition_id, Person* person);

  /**
   * Gets the number of transmissible people in this group's transmissible people person vector
   * with the specified Condition.
   *
   * @param condition_id the condition ID
   * @return the number of transmissible people
   */
  int get_number_of_transmissible_people(int condition_id) {
    return static_cast<int>(this->transmissible_people[condition_id].size());
  }

  /**
   * Gets nth transmissible person in this group's transmissible people person vector
   * with the specified Condition.
   *
   * @param condition_id the condition ID
   * @param n the index
   * @return the transmissible person
   */
  Person* get_transmissible_person(int condition_id, int n) {
    assert(n < static_cast<int>(this->transmissible_people[condition_id].size()));
    return this->transmissible_people[condition_id][n];
  }

  /**
   * Checks if the specified Condition is transmissible in the group. This will be true if there exists
   * transmissible people in the group with the condition.
   *
   * @param condition_id the condition ID
   * @return if the condition is transmissible
   */
  bool is_transmissible(int condition_id) {
    return static_cast<int>(this->transmissible_people[(int)condition_id].size()) > 0;
  }

  /**
   * Sets a specified Person as the host of the group.
   *
   * @param person the person
   */
  void set_host(Person* person) {
    this->host = person;
  }

  /**
   * Gets the host of the group.
   *
   * @return the host
   */
  Person* get_host() {
    return this->host;
  }

  double get_sum_of_var(int var_id);

  double get_median_of_var(int var_id);

  void report_size(int day);

  int get_size_on_day(int day);

  /**
   * Enables this group to start reporting size.
   */
  void start_reporting_size() {
    this->reporting_size = true;
  }

  /**
   * Checks if this group is set to report it's size.
   *
   * @return if the group is reporting size
   */
  bool is_reporting_size() {
    return this->reporting_size;
  }

  void create_administrator();

  /**
   * Gets the administrator of this group.
   *
   * @return the admin
   */
  Person* get_administrator() {
    return this->admin;
  }

  bool is_open();

  bool has_admin_closure();

  /**
   * Gets the contact factor of this group.
   *
   * @return the contact factor
   */
  double get_contact_factor() {
    return this->contact_factor;
  }

  /**
   * Sets the contact factor of this group.
   *
   * @param factor the new contact factor
   */
  void set_contact_factor(double factor) {
    this->contact_factor = factor;
  }

  bool is_a_place();

  bool is_a_network();

  void set_sp_id(long long int value);

  /**
   * Gets the Synthetic Population ID of this group.
   *
   * @return the SP ID
   */
  long long int get_sp_id() {
    return this->sp_id;
  }

  static bool is_a_place(int type_id);

  static bool is_a_network(int type_id);

  /**
   * Gets the Group with the specified Synthetic Population ID.
   *
   * @param sp_id the SP ID
   * @return the group
   */
  static Group* get_group_from_sp_id(long long int sp_id) {
    if(Group::sp_id_exists(sp_id)) {
      return Group::sp_id_map.find(sp_id)->second;
    } else {
      return nullptr;
    }
  }

  /**
   * Checks if the specified Synthetic Population ID exists in the SP ID map.
   *
   * @param sp_id the SP ID
   * @return if the SP ID exists
   */
  static bool sp_id_exists(long long int sp_id) {
    return Group::sp_id_map.find(sp_id) != Group::sp_id_map.end();
  }
  
  static void setup_logging();

protected:
  int id; // id
  int index; // index of place of this type
  int type_id;
  char label[32]; // external id
  char subtype;
  int N_orig;     // orig number of members
  long long int sp_id;
  double contact_factor;

  // epidemic counters
  int* first_transmissible_day; // first day when visited by transmissible people
  int* first_transmissible_count; // number of transmissible people on first_transmissible_day
  int* first_susceptible_count; // number of susceptible people on first_transmissible_day
  int* last_transmissible_day; // last day when visited by transmissible people

  // lists of people
  person_vector_t members;
  person_vector_t* transmissible_people;
  Person* host;    // person hosting this group
  Person* admin;   // person administering this group

  std::vector<int> size_change_day;
  std::vector<int> size_on_day;

  // should report size?
  bool reporting_size;

  // ave income
  int income;

  // map to retrieve group object from sp_id (must be unique)
  static std::map<long long int, Group*> sp_id_map;

private:
  static bool is_log_initialized;
  static std::string group_log_level;
  static std::unique_ptr<spdlog::logger> group_logger;
};

#endif /* _FRED_GROUP_H_ */
