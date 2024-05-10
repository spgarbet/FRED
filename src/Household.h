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
// File: Household.h
//
#ifndef _FRED_HOUSEHOLD_H
#define _FRED_HOUSEHOLD_H

#include <spdlog/spdlog.h>

#include "Global.h"
#include "Hospital.h"
#include "Place.h"

class Person;

/**
 * This class represents a household location.
 *
 * A Household contains specific information related to the inhabitants of the household, 
 * the relationship between them, and their activities.
 *
 * This class inherits from Place.
 */
class Household : public Place {
public:

  Household(); // _UNUSED_
  Household(const char* lab, char _subtype, fred::geo lon, fred::geo lat);

  /**
   * Default destructor.
   */
  ~Household() {}

  static void get_properties();

  /**
   * Gets the members of this household as a vector of persons.
   *
   * @return the members
   */
  person_vector_t get_inhabitants() {
    return this->members;
  }

  /**
   * Sets the race of this household.
   *
   * @param _race the new race
   */
  void set_household_race(int _race) {
    this->race = _race;
  }

  /**
   * Gets the race of this household.
   *
   * @return the race
   */
  int get_household_race() {
    return this->race;
  }

  /**
   * Checks if this household should be open.
   *
   * @param day _UNUSED_
   * @param condition _UNUSED_
   * @return if this household should be open
   */
  bool should_be_open(int day, int condition) {
    return true;
  }

  /**
   * Sets the group quarters units.
   *
   * @param units the new units
   */
  void set_group_quarters_units(int units) {
    this->group_quarters_units = units;
  }

  /**
   * Gets the group quarters units.
   *
   * @return the units
   */
  int get_group_quarters_units() {
    return this->group_quarters_units;
  }

  /**
   * Sets the group quarter's workplace.
   *
   * @param p the workplace
   */
  void set_group_quarters_workplace(Place* p) {
    this->group_quarters_workplace = p;
  }

  /**
   * Gets the group quarter's workplace.
   *
   * @return the workplace
   */
  Place* get_group_quarters_workplace() {
    return this->group_quarters_workplace;
  }

  /**
   * Sets the migration admin code of this household.
   *
   * @param mig_admin_code the migration admin code
   */
  void set_migration_admin_code(int mig_admin_code) {
    this->migration_admin_code = mig_admin_code;
  }

  /**
   * Clears the household's migration admin code.
   */
  void clear_migration_admin_code() {
    this->migration_admin_code = 0;  // 0 means not planning to migrate or done migrating
  }

  /**
   * Gets the migration admin code of this household.
   *
   * @return the migration admin code
   */
  int get_migration_admin_code() {
    return this->migration_admin_code;
  }

  /**
   * Gets the original household structure.
   *
   * @return the original household structure
   */
  int get_orig_household_structure() {
    return this->orig_household_structure;
  }

  /**
   * Gets the current household structure.
   *
   * @return the current household structure
   */
  int get_household_structure() {
    return this->household_structure;
  }

  void set_household_structure();

  /**
   * Sets the original household structure as this household's current structure.
   */
  void set_orig_household_structure() {
    this->orig_household_structure = this->household_structure;
    strcpy(this->orig_household_structure_label, this->household_structure_label);
  }

  /**
   * Gets the household structure label.
   *
   * @return the household structure label
   */
  char* get_household_structure_label() {
    return this->household_structure_label;
  }

  /**
   * Gets the original household structure label.
   *
   * @return the original household structure label
   */
  char* get_orig_household_structure_label() {
    return this->orig_household_structure_label;
  }

  void set_household_vaccination();

  /**
   * Checks if this household is in a low vaccination school.
   *
   * @return if this household is in a low vaccination school
   */
  bool is_in_low_vaccination_school() {
    return this->in_low_vaccination_school;
  }

  /**
   * Checks if this household refuses vaccines.
   *
   * @return if this household refuses vaccines
   */
  bool refuses_vaccines() {
    return this->refuse_vaccine;
  }
  
  static void setup_logging();


private:
 static bool is_log_initialized;
 static std::string household_log_level;
 static std::unique_ptr<spdlog::logger> household_logger;

  // household structure types
#define HTYPES 21

  enum htype_t {
    SINGLE_FEMALE,
    SINGLE_MALE,
    OPP_SEX_SIM_AGE_PAIR,
    OPP_SEX_DIF_AGE_PAIR,
    OPP_SEX_TWO_PARENTS,
    SINGLE_PARENT,
    SINGLE_PAR_MULTI_GEN_FAMILY,
    TWO_PAR_MULTI_GEN_FAMILY,
    UNATTENDED_KIDS,
    OTHER_FAMILY,
    YOUNG_ROOMIES,
    OLDER_ROOMIES,
    MIXED_ROOMIES,
    SAME_SEX_SIM_AGE_PAIR,
    SAME_SEX_DIF_AGE_PAIR,
    SAME_SEX_TWO_PARENTS,
    DORM_MATES,
    CELL_MATES,
    BARRACK_MATES,
    NURSING_HOME_MATES,
    UNKNOWN,
  };

  htype_t orig_household_structure;
  htype_t household_structure;

  char orig_household_structure_label[64];
  char household_structure_label[64];

  std::string htype[HTYPES] = {
    "single-female",
    "single-male",
    "opp-sex-sim-age-pair",
    "opp-sex-dif-age-pair",
    "opp-sex-two-parent-family",
    "single-parent-family",
    "single-parent-multigen-family",
    "two-parent-multigen-family",
    "unattended-minors",
    "other-family",
    "young-roomies",
    "older-roomies",
    "mixed-roomies",
    "same-sex-sim-age-pair",
    "same-sex-dif-age-pair",
    "same-sex-two-parent-family",
    "dorm-mates",
    "cell-mates",
    "barrack-mates",
    "nursing-home-mates",
    "unknown",
  };

  Place* group_quarters_workplace;

  int group_quarters_units;
  int race;

  double vaccination_probability;
  int vaccination_decision;
  bool in_low_vaccination_school;
  bool refuse_vaccine;

  int migration_admin_code;  //household preparing to do county-to-county migration

};

#endif // _FRED_HOUSEHOLD_H
