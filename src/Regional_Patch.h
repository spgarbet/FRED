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
// File: Regional_Patch.h
//
#ifndef _FRED_REGIONAL_PATCH_H
#define _FRED_REGIONAL_PATCH_H

#include <vector>
#include <set>

#include <spdlog/spdlog.h>

#include "Person.h"
#include "Abstract_Patch.h"
#include "Global.h"
#include "Utils.h"
#include "Household.h"
#include "Place.h"
//#include "Regional_Layer.h"

class Regional_Layer;
/**
 * This class represents a cell in the Regional_Layer grid.
 *
 * A Regional_Patch is an area in the simulation which models a region. It includes 
 * functionality for workplaces, such as finding workplaces nearby a location and 
 * swapping county residents.
 *
 * This class inherits from Abstract_Patch.
 */
class Regional_Patch : public Abstract_Patch {

public:
  Regional_Patch(Regional_Layer* grd, int i, int j);
  Regional_Patch();

  /**
   * Default destructor.
   */
  ~Regional_Patch() {
  }

  void setup(Regional_Layer* grd, int i, int j);
  void quality_control();
  double distance_to_patch(Regional_Patch* patch2);

  /**
   * Adds the specified Person to this patch's person vector.
   *
   * @param p the person
   */
  void add_person_to_patch(Person* p) {
    this->person.push_back(p);
  }

  /**
   * Gets the population size of this patch.
   *
   * @return the population size
   */
  int get_popsize() {
    return this->popsize;
  }

  Person* select_random_person();
  Person* select_random_student(int age_);
  Person* select_random_worker();
  void set_max_popsize(int n);

  /**
   * Gets the maximum population size of this patch.
   *
   * @return the maximum population size
   */
  int get_max_popsize() {
    return this->max_popsize;
  }

  /**
   * Gets the population density of this patch.
   *
   * @return the population density
   */
  double get_pop_density() {
    return this->pop_density;
  }

  void end_membership(Person* pers);
  void add_workplace(Place* place);
  void add_hospital(Place* place);

  /**
   * Gets the hospitals place vector of this patch.
   *
   * @return the hospitals
   */
  place_vector_t get_hospitals() {
    return this->hospitals;
  }

  Place* get_nearby_workplace(Place* place, int staff);
  Place* get_closest_workplace(double x, double y, int min_size, int max_size, double* min_dist);

  /**
   * Gets the ID of this patch.
   *
   * @return the ID
   */
  int get_id() {
    return this->id;
  }

  void swap_county_people();
  
  static void setup_logging();

protected:
  Regional_Layer* grid;
  int popsize;
  person_vector_t person;
  std::set<int> counties;
  int max_popsize;
  double pop_density;
  int id;
  static int next_patch_id;
  place_vector_t workplaces;
  place_vector_t hospitals;
  person_vector_t students_by_age[100];
  person_vector_t workers;

private:
  static bool is_log_initialized;
  static std::string regional_patch_log_level;
  static std::unique_ptr<spdlog::logger> regional_patch_logger;
};

#endif // _FRED_REGIONAL_PATCH_H
