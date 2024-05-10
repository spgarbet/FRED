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
// File: County.h
//

#ifndef _FRED_COUNTY_H
#define _FRED_COUNTY_H

#include <algorithm>
#include <random>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

#include "Admin_Division.h"
#include "Demographics.h"
#include "Household.h"
#include "Person.h"
#include "Place.h"
//class Household;
//class Person;
//class Place;

// 2-d array of lists
typedef std::vector<int> HouselistT;

// number of age groups: 0-4, 5-9, ... 85+
#define AGE_GROUPS 18

// number of target years: 2010, 2015, ... 2040
#define TARGET_YEARS 7

/**
 * This class represents a county division, which is a subdivision of a State and a higher 
 * division of a Census_Tract.
 *
 * Counties track population data, including data specific to age, sex, and housing, and also 
 * are capable tasks such as tracking workplace and school counts and building probability 
 * distributions for schools and workplaces, similar to census tracts. Counties are also 
 * responsible for ensuring the population stays accurate to synthetic population 
 * data. This is done by moving its population in and out of different housing environments, such as 
 * schools, prisons, and nursing homes, as well as performing migrations between residents of the 
 * county or other counties to distribute population accurately.
 *
 * This class inherits from Admin_Division.
 */
class County : public Admin_Division {
  
public:
  County(int _admin_code);
  ~County();

  void setup();

  /**
   * Gets the current total population size.
   *
   * @return the population size
   */
  int get_current_popsize() {
    return this->tot_current_popsize;
  }

  /**
   * Gets the total female population size.
   *
   * @return the population size
   */
  int get_tot_female_popsize() {
    return this->tot_female_popsize;
  }

  /**
   * Gets the total male population size.
   *
   * @return the population size
   */
  int get_tot_male_popsize() {
    return this->tot_male_popsize;
  }

  /**
   * Gets the current population size at the specified age.
   *
   * @param age the age
   * @return the population size
   */
  int get_current_popsize(int age) {
    if(age > Demographics::MAX_AGE) {
      age = Demographics::MAX_AGE;
    }
    if(age >= 0) {
      return this->female_popsize[age] + this->male_popsize[age];
    }
    return -1;
  }

  /**
   * Gets the current population for the specified age and sex.
   *
   * @param age the age
   * @param sex the sex
   * @return the population size
   */
  int get_current_popsize(int age, char sex) {
    if(age > Demographics::MAX_AGE) {
      age = Demographics::MAX_AGE;
    }
    if(age >= 0) {
      if(sex == 'F') {
        return this->female_popsize[age];
      } else if(sex == 'M') {
        return this->male_popsize[age];
      }
    }
    return -1;
  }

  int get_current_popsize(int age_min, int age_max, char sex);
  bool increment_popsize(Person* person);
  bool decrement_popsize(Person* person);
  void recompute_county_popsize();

  void update(int day);
  void get_housing_imbalance(int day);
  int fill_vacancies(int day);
  void update_housing(int day);
  void move_college_students_out_of_dorms(int day);
  void move_college_students_into_dorms(int day);
  void move_military_personnel_out_of_barracks(int day);
  void move_military_personnel_into_barracks(int day);
  void move_inmates_out_of_prisons(int day);
  void move_inmates_into_prisons(int day);
  void move_patients_into_nursing_homes(int day);
  void move_young_adults(int day);
  void move_older_adults(int day);
  void report_ages(int day, int house_id);
  void swap_houses(int day);
  int find_admin_code(int n); // _UNUSED_
  void get_housing_data();
  void report_household_distributions();
  void report_county_population();

  /**
   * Gets the mortality rate for the specified age and sex.
   *
   * @param age the age
   * @param sex the sex
   * @return the mortality rate
   */
  double get_mortality_rate(int age, char sex) {
    if(sex == 'F') {
      if(age > Demographics::MAX_AGE) {
        return this->female_mortality_rate[Demographics::MAX_AGE];
      } else {
        return this->female_mortality_rate[age];
      }
    } else {
      if(age > Demographics::MAX_AGE) {
        return this->male_mortality_rate[Demographics::MAX_AGE];
      } else {
        return this->male_mortality_rate[age];
      }
    }
  }

  // for selecting new workplaces
  void set_workplace_probabilities();
  Place* select_new_workplace();
  void report_workplace_sizes();

  // for selecting new schools
  void set_school_probabilities();
  Place* select_new_school(int grade);
  Place* select_new_school_in_county(int grade);
  void report_school_sizes();

  // county migration
  void read_population_target_properties();
  void county_to_county_migration();
  void migrate_household_to_county(Place* house, int dest);
  Place* select_new_house_for_immigrants(int hszie);
  void select_migrants(int day, int migrants, int lower_age, int upper_age, char sex, int dest_admin_code);
  void add_immigrant(Person* person);
  void add_immigrant(int age, char sex);
  void migration_swap_houses();
  void migrate_to_target_popsize();
  void read_migration_properties();
  double get_migration_rate(int sex, int age, int src, int dst);
  void group_population_by_sex_and_age(int reset);
  void report();
  void move_students();

  Household* get_hh(int i);

  /**
   * Gets the number of County objects in the static counties vector.
   *
   * @return the number of counties
   */
  static int get_number_of_counties() {
    return County::counties.size();
  }

  /**
   * Gets the County at the specified index in the static counties vector.
   *
   * @param n the index
   * @return the county
   */
  static County* get_county_with_index(int n) {
    return County::counties[n];
  }

  static County* get_county_with_admin_code(int county_admin_code);

  static void setup_counties();
  static void move_students_in_counties();
  static void setup_logging();

 private:

  int tot_current_popsize;
  int male_popsize[Demographics::MAX_AGE + 2];
  int tot_male_popsize;
  int female_popsize[Demographics::MAX_AGE + 2];
  int tot_female_popsize;

  double male_mortality_rate[Demographics::MAX_AGE + 2];
  double female_mortality_rate[Demographics::MAX_AGE + 2];
  int* beds;
  int* occupants;
  int max_beds;
  std::vector< std::pair<Person*, int> > ready_to_move;
  int target_males[AGE_GROUPS][TARGET_YEARS];
  int target_females[AGE_GROUPS][TARGET_YEARS];
  person_vector_t males_of_age[Demographics::MAX_AGE+1];
  person_vector_t females_of_age[Demographics::MAX_AGE+1];
  int number_of_households;

  // pointers to nursing homes
  std::vector<Household*> nursing_homes;
  int number_of_nursing_homes;

  // schools attended by people in this county, with probabilities
  place_vector_t schools_attended[Global::GRADES];
  std::vector<double> school_probabilities[Global::GRADES];

  // workplaces attended by people in this county, with probabilities
  place_vector_t workplaces_attended;
  std::vector<double> workplace_probabilities;

  std::vector<int> migration_households;  //vector of household IDs for migration

  // static vars
  static bool is_initialized;
  static double college_departure_rate;
  static double military_departure_rate;
  static double prison_departure_rate;
  static double youth_home_departure_rate;
  static double adult_home_departure_rate;

  static bool enable_migration_to_target_popsize;
  static bool enable_county_to_county_migration;
  static bool enable_within_state_school_assignment;
  static bool enable_within_county_school_assignment;
  static std::vector<int> migration_admin_code;
  static double**** migration_rate;
  static int migration_properties_read;
  static int population_target_properties_read;
  static int*** male_migrants;
  static int*** female_migrants;
  static std::string projection_directory;
  static std::string default_mortality_rate_file;
  static char county_migration_file[FRED_STRING_SIZE];
  static char migration_file[FRED_STRING_SIZE];
  static std::random_device rd;
  static std::mt19937_64 mt_engine;

  static std::vector<County*> counties;
  static std::unordered_map<int, County*> lookup_map;
    
  static bool is_log_initialized;
  static std::string county_log_level;
  static std::unique_ptr<spdlog::logger> county_logger;
};

#endif // _FRED_COUNTY_H
