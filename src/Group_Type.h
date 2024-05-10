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
// File: Group_Type.h
//

#ifndef _FRED_Group_Type_H
#define _FRED_Group_Type_H

#include <spdlog/spdlog.h>

#include "Global.h"

class Group;
class Person;

/**
 * This class represents a specific type of a Group, which enables different groups to share the same 
 * attributes, and allows easy access of different groups of the same type.
 *
 * A Group_Type can be identified by a name or, more commonly, an ID. This ID represents it's index 
 * in the static vector containing all group types. Groups of the same group type share attributes 
 * relating to the spread of infection throughout them, hours of operation, and admins.
 *
 * This class is inherited by Network_Type and Place_Type.
 */
class Group_Type {
public:

  Group_Type(std::string _name);

  ~Group_Type();

  enum GTYPE {
    UNKNOWN = -1,
    HOUSEHOLD,
    NEIGHBORHOOD,
    SCHOOL,
    CLASSROOM,
    WORKPLACE,
    OFFICE,
    HOSPITAL,
    HOSTED_GROUP = 1000000
  };

  void get_properties();

  /**
   * Gets the name of this group type.
   *
   * @return the name
   */
  const char* get_name() {
    return this->name.c_str();
  }

  bool is_open();

  int get_time_block(int day, int hour);

  /**
   * Gets the proximity contact rate of this group type.
   *
   * @return the proximity contact rate
   */
  double get_proximity_contact_rate() {
    return this->proximity_contact_rate;
  }

  /**
   * Gets the proximity same age bias of this group type.
   *
   * @return the proximity same age bias
   */
  double get_proximity_same_age_bias() {
    return this->proximity_same_age_bias;
  }

  /**
   * Gets the density contact probability of the specified Condition in this group type.
   *
   * @param condition_id the condition ID
   * @return the desnity contact probability
   */
  double get_density_contact_prob(int condition_id) {
    return this->density_contact_prob[condition_id];
  }

  /**
   * Checks if this group type can transmit the specified Condition.
   *
   * @param condition_id the condition ID
   * @return if this group type can transmit
   */
  bool can_transmit(int condition_id) {
    return this->can_transmit_cond[condition_id];
  }

  /**
   * Gets the contact count of the specified Condition in this group type.
   *
   * @param condition_id the condition ID
   * @return the contact count
   */
  int get_contact_count(int condition_id) {
    return this->contact_count_for_cond[condition_id];
  }

  /**
   * Gets the contact rate of the specified Condition in this group type.
   *
   * @param condition_id the condition ID
   * @return the contact rate
   */
  double get_contact_rate(int condition_id) {
    return this->contact_rate_for_cond[condition_id];
  }

  /**
   * Checks if the use of deterministic contacts is enabled for the specified Condition in this group type.
   *
   * @param condition_id the condition ID
   * @return if the use of deterministic contacts is enabled
   */
  bool use_deterministic_contacts(int condition_id) {
    return this->deterministic_contacts_for_cond[condition_id];
  }

  /**
   * Checks if the use of density transmission is enabled for the specified Condition in this group type.
   *
   * @param condition_id the condition ID
   * @return if the use of density transmission is enabled
   */
  bool use_density_transmission(int condition_id) {
    return this->density_transmission_for_cond[condition_id];
  }

  /**
   * Checks if this group type has an administrator.
   *
   * @return if this has an administrator
   */
  bool has_administrator() {
    return this->has_admin;
  }

  // static methods

  /**
   * Gets the Group_Type with the specified ID.
   *
   * @param type_id the group type ID
   * @return the group type
   */
  static Group_Type* get_group_type(int type_id) {
    if(0 <= type_id && type_id < Group_Type::get_number_of_group_types()) {
      return Group_Type::group_types[type_id];
    } else {
      return nullptr;
    }
  }

  /**
   * Gets the Group_Type with the specified name.
   *
   * @param name the group type name
   * @return the group type
   */
  static Group_Type* get_group_type(std::string name) {
    int type_id = Group_Type::get_type_id(name);
    if(type_id < 0) {
      return nullptr;
    } else {
      return Group_Type::get_group_type(Group_Type::get_type_id(name));
    }
  }

  /**
   * Gets the number of Group_Type objects in the static group types vector.
   *
   * @return the number of group types
   */
  static int get_number_of_group_types() {
    return Group_Type::group_types.size();
  }

  /**
   * Adds the specified Group_Type to the static group types vector.
   *
   * @param group_type the group type
   */
  static void add_group_type(Group_Type* group_type) {
    Group_Type::group_types.push_back(group_type);
  }

  static int get_type_id(std::string group_type_name);

  /**
   * Gets the name of the Group_Type with the specified ID.
   *
   * @param type_id the group type ID
   * @return the name
   */
  static std::string get_group_type_name(int type_id) {
    if(type_id < 0) {
      return "UNKNOWN";
    }
    if(type_id < Group_Type::get_number_of_group_types()) {
      return Group_Type::names[type_id];
    } else {
      return "UNKNOWN";
    }
  }

  static void add_group_hosted_by(Person* person, Group* group);
  static Group* get_group_hosted_by(Person* person);
  static void setup_logging();

protected:

  // group_type variables
  std::string name;

  // initialization
  int file_available;

  // proximity transmission properties
  int proximity_contact_count;
  double proximity_contact_rate;
  double proximity_same_age_bias;
  double* density_contact_prob;
  double global_density_contact_prob;

  // condition-specific transmission properties
  int* can_transmit_cond;
  int* contact_count_for_cond;
  double* contact_rate_for_cond;
  bool* deterministic_contacts_for_cond;
  bool* density_transmission_for_cond;

  // hours of operation
  int starts_at_hour[7][24];
  int open_at_hour[7][24];

  // administrator
  bool has_admin;

  // lists of group types
  static std::vector <Group_Type*> group_types;
  static std::vector<std::string> names;
  static std::unordered_map<std::string, int> group_name_map;

  // host lookup
  static std::unordered_map<Person*, Group*> host_group_map;

private:
  static bool is_log_initialized;
  static std::string group_type_log_level;
  static std::unique_ptr<spdlog::logger> group_type_logger;
};

#endif // _FRED_Group_Type_H
