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
// File: Census_Tract.h
//

/**
 * This class represents a census tract division, which is a subdivision of a County and a higher 
 * division of a Block_Group.
 *
 * Census tracts are capable of tracking workplace and school counts, as well as building 
 * probability distributions for schools and workplaces to ensure even distributiion 
 * during the simulation.
 *
 * This class inherits from Admin_Division.
 */
#ifndef _FRED_CENSUS_TRACT_H
#define _FRED_CENSUS_TRACT_H

#include <assert.h>
#include <unordered_map>
#include <vector>

#include "Admin_Division.h"
#include "Global.h"

// school attendance maps
typedef std::unordered_map<int, int> attendance_map_t;
typedef attendance_map_t::iterator attendance_map_itr_t;
typedef std::unordered_map<int, Place*> school_id_map_t;


class Census_Tract : public Admin_Division {
public:

  Census_Tract(long long int _admin_code);
  ~Census_Tract();

  void setup();

  void update(int day);

  void set_workplace_probabilities();
  Place* select_new_workplace();
  void report_workplace_sizes(); // _UNUSED_

  void set_school_probabilities();
  Place* select_new_school(int grade);
  void report_school_sizes(); // _UNUSED_

  /**
   * Whether or not the specified school is attended at a specified grade.
   *
   * @param school_id the id of the school
   * @param grade the grade
   * @return if the school is attended
   */
  bool is_school_attended(int school_id, int grade) {
    return (this->school_counts[grade].find(school_id) != this->school_counts[grade].end());
  }

  /**
   * Gets the number of Census_Tract objects in the census tracts vector.
   *
   * @return the number of census tracts
   */
  static int get_number_of_census_tracts() {
    return Census_Tract::census_tracts.size();
  }

  /**
   * Gets the Census_Tract at the specified index in the census tracts vector.
   *
   * @param n the index
   * @return the census tract
   */
  static Census_Tract* get_census_tract_with_index(int n) {
    return Census_Tract::census_tracts[n];
  }

  static Census_Tract* get_census_tract_with_admin_code(long long int census_tract_admin_code);

  static void setup_census_tracts();

  /**
   * Checks if the Census_Tract with the specified census tract admin code is in 
   * the County with the specified county admin code.
   *
   * @param census_tract_admin_code the census tract admin code
   * @param county_admin_code the county admin code
   * @return if the census tract is in the county
   */
  static bool is_in_county(long long int census_tract_admin_code, long long int county_admin_code) {
    return census_tract_admin_code / 1000000 == county_admin_code;
  }

  /**
   * Checks if the Census_Tract with the specified census tract admin code is in 
   * the State with the specified state admin code.
   *
   * @param census_tract_admin_code the census tract admin code
   * @param state_admin_code the state admin code
   * @return if the census tract is in the county
   */
  static bool is_in_state(long long int census_tract_admin_code, long long int state_admin_code) {
    return census_tract_admin_code / 1000000000 == state_admin_code;
  }
  
  static void setup_logging();

private:

  // schools attended by people in this census_tract, with probabilities
  std::vector<Place*> schools_attended[Global::GRADES];
  std::vector<double> school_probabilities[Global::GRADES];

  // list of schools attended by people in this county
  attendance_map_t school_counts[Global::GRADES];
  school_id_map_t school_id_lookup;

  // workplaces attended by people in this census_tract, with probabilities
  std::vector<Place*> workplaces_attended;
  std::vector<double> workplace_probabilities;

  static std::vector<Census_Tract*> census_tracts;
  static std::unordered_map<long long int, Census_Tract*> lookup_map;

  static bool is_log_initialized;
  static std::string census_tract_log_level;
  static std::unique_ptr<spdlog::logger> census_tract_logger;
};

#endif // _FRED_CENSUS_TRACT_H
