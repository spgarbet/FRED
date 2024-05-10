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
// File: Place_Type.h
//

#ifndef _FRED_Place_Type_H
#define _FRED_Place_Type_H

#include <spdlog/spdlog.h>

#include "Global.h"
#include "Group_Type.h"


class Person;

/**
 * This class represents a specific type of a Place, which enables different places to share the same 
 * attributes, and allows easy access of different places of the same type.
 *
 * Place_Type adds functionality to Group_Type specific to places, such as tracking data like vaccination rates, elevation, 
 * income distribution, etc., which are all specific to places. This class also allows the setup of partition types, which 
 * are used when making partitions for a Place. These are simply places that are a subset of another place. Some examples 
 * would be a school and classroom, or a workplace and office. These are all specific, predefined place types. Other patitions 
 * can be created based on factors like a partition basis and capacity.
 *
 * This class inherits from Group_Type.
 */
class Place_Type : public Group_Type {
public:

  Place_Type(int _id, std::string _name);
  ~Place_Type();

  void get_properties();
  void prepare();
  void set_admin_list();

  void finish();

  void add_place(Place* place);

  /**
   * Gets the Place at the specified index in the places vector of places with this place type.
   *
   * @param n the index
   * @return the place
   */
  Place* get_place(int n) {
    if(n < static_cast<int>(this->places.size())) {
      return this->places[n];
    } else {
      return nullptr;
    }
  }

  /**
   * Gets the number of Place objects in the places vector of places with this place type.
   *
   * @return the number of places
   */
  int get_number_of_places() {
    return static_cast<int>(this->places.size());
  }

  Place* select_place(Person* person);

  void report_contacts_for_place_type();

  void setup_partitions();

  /**
   * Gets the partition type ID of this place type. This is the place type ID that partitions of 
   * places of this place type will have.
   *
   * @return the partition type ID
   */
  int get_partition_type_id() {
    return this->partition_type_id;
  }

  /**
   * Gets the partition name of this place type. This is the name that partitions of 
   * places of this place type will have.
   *
   * @return the partition name
   */
  std::string get_partition_name() {
    return this->partition_name;
  }

  /**
   * Gets this place type's partition type as a Place_Type.
   *
   * @return the partition
   */
  Place_Type* get_partition() {
    return get_place_type(this->partition_type_id);
  }

  /**
   * Gets the partition basis of this place type.
   *
   * @return the partition basis
   */
  std::string get_partition_basis() {
    return this->partition_basis;
  }

  /**
   * Gets the partition capacity of this place type.
   *
   * @return the partition capacity
   */
  int get_partition_capacity() {
    return this->partition_capacity;
  }

  void prepare_vaccination_rates();

  /**
   * Checks if vaccination rate is enabled for this place type.
   *
   * @return if vaccination rate is enabled
   */
  bool is_vaccination_rate_enabled() {
    return this->enable_vaccination_rates;
  }

  /**
   * Gets the default vaccination rate for this place type.
   *
   * @return the default vaccination rate
   */
  double get_default_vaccination_rate() {
    return this->default_vaccination_rate;
  }

  /**
   * Gets the medical vaccination exemption rate for this place type.
   *
   * @return the medical vaccination exemption rate
   */
  double get_medical_vacc_exempt_rate() {
    return this->medical_vacc_exempt_rate;
  }

  Place* get_new_place();

  Place* generate_new_place(Person* person);

  void report(int day);

  // cutoffs

  int get_size_quartile(int n);

  int get_size_quintile(int n);

  int get_income_quartile(int n);

  int get_income_quintile(int n);

  int get_elevation_quartile(double n);

  int get_elevation_quintile(double n);

  /**
   * Gets the income cutoff for the first quartile of income.
   *
   * @return the first quartile income
   */
  double get_income_first_quartile() {
    return this->income_cutoffs.first_quartile;
  }

  /**
   * Gets the income cutoff for the second quartile of income.
   *
   * @return the second quartile income
   */
  double get_income_second_quartile() {
    return this->income_cutoffs.second_quartile;
  }

  /**
   * Gets the income cutoff for the third quartile of income.
   *
   * @return the third quartile income
   */
  double get_income_third_quartile() {
    return this->income_cutoffs.third_quartile;
  }


  // static methods

  static void set_cutoffs(cutoffs_t* cutoffs, double* values, int size);

  static void get_place_type_properties();

  static void prepare_place_types();

  static void set_place_type_admin_lists();


  static void read_places(const char* pop_dir);

  static void report_place_size(int place_type_id);

  static void finish_place_types();

  /**
   * Gets the place type with the specified ID.
   *
   * @param type_id the place type ID
   * @return the place type
   */
  static Place_Type* get_place_type(int type_id) {
    return static_cast<Place_Type*>(Group_Type::get_group_type(type_id));
  }

  /**
   * Gets the place type with the specified name.
   *
   * @param name the place type name
   * @return the place type
   */
  static Place_Type* get_place_type(std::string name) {
    return static_cast<Place_Type*>(Group_Type::get_group_type(name));
  }

  /**
   * Gets the name of this place type.
   *
   * @return the name
   */
  const char* get_name() {
    return this->name.c_str();
  }

  /**
   * Gets the name of the Place_Type with the specified ID.
   *
   * @param type_id the place type ID
   * @return the name
   */
  static std::string get_place_type_name(int type_id) {
    if(type_id < 0) {
      return "UNKNOWN";
    }
    if(type_id < Place_Type::get_number_of_place_types())
    {
      return Place_Type::names[type_id];
    } else {
      return "UNKNOWN";
    }
  }

  /**
   * Gets the number of place types.
   *
   * @return the number of place types
   */
  static int get_number_of_place_types() {
    return static_cast<int>(Place_Type::place_types.size());
  }

  /**
   * Adds the given place type name to the static place type names vector if it is not already included.
   *
   * @param name the name
   */
  static void include_place_type(std::string name) {
    int size = static_cast<int>(Place_Type::names.size());
    for(int i = 0; i < size; ++i) {
      if(name == Place_Type::names[i]) {
        return;
      }
    }
    Place_Type::names.push_back(name);
  }

  static void exclude_place_type(std::string name) {
  }

  static bool is_predefined(std::string value);

  /**
   * Checks that the Place_Type with the specified value, or name, is not predefined. This will return true if
   * the place type has a user defined value, i.e. if it is not one of the default place types: household,
   * neighborhood, school, classroom, workplace, office, or hospital.
   *
   * @param value the value
   * @return if the place type is predefined
   */
  static bool is_not_predefined(std::string value) {
    return Place_Type::is_predefined(value) == false;
  }

  /**
   * Gets the Household Place_Type.
   *
   * @return the household place type
   */
  static Place_Type* get_household_place_type() {
    return Place_Type::place_types[Place_Type::get_type_id("Household")];
  }

  /**
   * Gets the neighborhood Place_Type.
   *
   * @return the neighborhood place type
   */
  static Place_Type* get_neighborhood_place_type() {
    return Place_Type::place_types[Place_Type::get_type_id("Neighborhood")];
  }

  /**
   * Gets the school Place_Type.
   *
   * @return the school place type
   */
  static Place_Type* get_school_place_type() {
    return Place_Type::place_types[Place_Type::get_type_id("School")];
  }

  /**
   * Gets the classroom Place_Type.
   *
   * @return the classroom place type
   */
  static Place_Type* get_classroom_place_type() {
    return Place_Type::place_types[Place_Type::get_type_id("Classroom")];
  }

  /**
   * Gets the workplace Place_Type.
   *
   * @return the workplace place type
   */
  static Place_Type* get_workplace_place_type() {
    return Place_Type::place_types[Place_Type::get_type_id("Workplace")];
  }

  /**
   * Gets the office Place_Type.
   *
   * @return the office place type
   */
  static Place_Type* get_office_place_type() {
    return Place_Type::place_types[Place_Type::get_type_id("Office")];
  }

  /**
   * Gets the Hospital Place_Type.
   *
   * @return the hospital place type
   */
  static Place_Type* get_hospital_place_type() {
    return Place_Type::place_types[Place_Type::get_type_id("Hospital")];
  }

  static void add_places_to_neighborhood_layer();

  static Place* select_place_of_type(int place_type_id, Person* person);

  static void report_contacts();

  static Place* generate_new_place(int place_type_id, Person* person);

  /**
   * Gets the Place hosted by the specified Person in the static host place map.
   *
   * @param person the person
   * @return the place
   */
  static Place* get_place_hosted_by(Person* person) {
    std::unordered_map<Person*,Place*>::const_iterator found = host_place_map.find(person);
    if(found != Place_Type::host_place_map.end()) {
      return found->second;
    } else {
      return nullptr;
    }
  }

  /**
   * Checks if the specified Person is the host of a Place in the static host place map.
   *
   * @return if the person is a host
   */
  static bool is_a_host(Person* person) {
    return (Place_Type::get_place_hosted_by(person) != nullptr);
  }

  /**
   * Gets the max distance. This is the radius around places of this type in which other places will be
   * considered for selection in travel.
   *
   * @return the max distance
   */
  double get_max_dist() {
    return this->max_dist;
  }

  /**
   * Gets the max size. This is the maximum amount of members who can belong to a place of this type.
   *
   * @return the max size
   */
  int get_max_size() {
    return this->max_size;
  }

  /**
   * Increments and returns the next SP ID for a place of this type.
   *
   * @return the next SP ID
   */
  long long int get_next_sp_id() {
    return this->next_sp_id++;
  }
  
  static void setup_logging();

private:

  // list of places of this type
  place_vector_t places;

  // place type features
  int max_size;
  double max_dist;
  std::string partition_name;
  int partition_type_id;
  std::string partition_basis;
  int partition_capacity;
  int min_age_partition;
  int max_age_partition;
  int base_type_id;

  // plotting and visualizatiom
  int enable_visualization;
  int report_size;

  // vaccination rates
  int enable_vaccination_rates;
  double default_vaccination_rate;
  int need_to_get_vaccination_rates;
  char vaccination_rate_file[FRED_STRING_SIZE];
  double medical_vacc_exempt_rate;

  // cutoffs for quintiles and quartiles
  cutoffs_t elevation_cutoffs;
  cutoffs_t size_cutoffs;
  cutoffs_t income_cutoffs;

  // next sp_id for this type;
  long long int next_sp_id;

  // lists of place types
  static std::vector <Place_Type*> place_types;
  static std::vector<std::string> names;

  // host lookup
  static std::unordered_map<Person*, Place*> host_place_map;

  static bool is_log_initialized;
  static std::string place_type_log_level;
  static std::unique_ptr<spdlog::logger> place_type_logger;
};

#endif // _FRED_Place_Type_H
