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
// File: Neighborhood_Patch.h
//

#ifndef _FRED_NEIGHBORHOOD_PATCH_H
#define _FRED_NEIGHBORHOOD_PATCH_H

#include <string.h>
#include <vector>

#include <spdlog/spdlog.h>

typedef std::vector<Place *> place_vector;

#include "Abstract_Patch.h"
#include "Global.h"
#include "Neighborhood_Layer.h"
#include "Place.h"

class Household;
class Neighborhood;
class Neighborhood_Layer;
class Person;

/**
 * This class represents a cell in the Neighborhood_Layer grid.
 *
 * A Neighborhood_Patch is an area of the simulation in which a specific neighborhood 
 * exists. Every neighborhood patch has an associated neighborhood, which is a Place 
 * object. This class allows integration between the neighborhood and the grid. It tracks 
 * data on the neighborhood, such as information of the residents of the neighborhood, as well 
 * as other places that the neighborhood contains, such as schools, workplaces, and households.
 *
 * This class inherits from Abstract_Patch.
 */
class Neighborhood_Patch : public Abstract_Patch {

public:
  Neighborhood_Patch();

  /**
   * Default destructor.
   */
  ~Neighborhood_Patch() {}

  void setup(Neighborhood_Layer* grd, int i, int j);

  void setup(int phase);
  void prepare();

  void quality_control();

  void make_neighborhood(int type);

  void record_activity_groups();

  Place* select_random_household();

  /**
   * Gets the number of households in the patch.
   *
   * @return the number of households
   */
  int get_houses() { 
    return this->get_number_of_households(); // redundant
  }

  /**
   * Gets this neighborhood patch's associated neighborhood.
   *
   * @return the neighborhood
   */
  Place* get_neighborhood() {
    return this->neighborhood;
  }

  /**
   * Adds the specified Person as a member of this neighborhood patch's associated neighborhood. 
   * Returns the index that the person was added at.
   *
   * @param per the person
   * @return the index
   */
  int begin_membership(Person* per) {
    return this->neighborhood->begin_membership(per);
  }

  /**
   * Gets the population size.
   *
   * @return the population size
   */
  int get_popsize() { 
    return this->popsize;
  }

  /**
   * Gets the number of households in the patch.
   *
   * @return the number of households
   */
  int get_number_of_households() {
    return static_cast<int>(this->places[Place_Type::get_type_id("Household")].size());
  }

  /**
   * Gets the Household at the specified index.
   *
   * @param i the index
   * @return the household
   */
  Place* get_household(int i) {
    return this->get_place(Place_Type::get_type_id("Household"), i);
  }
  
  /**
   * Gets the number of schools in the patch.
   *
   * @return the number of schools
   */
  int get_number_of_schools() {
    return static_cast<int>(this->places[Place_Type::get_type_id("School")].size());
  }
  
  /**
   * Gets the school at the specified index.
   *
   * @param i the index
   * @return the school
   */
  Place* get_school(int i) {
    return this->get_place(Place_Type::get_type_id("School"), i);
  }

  /**
   * Gets the number of workplaces in the patch.
   *
   * @return the number of workplaces
   */  
  int get_number_of_workplaces() {
    return static_cast<int>(this->places[Place_Type::get_type_id("Workplace")].size());
  }

  /**
   * Gets the workplace at the specified index.
   *
   * @param i the index
   * @return the workplace
   */
  Place* get_workplace(int i) {
    if(0 <= i && i < this->get_number_of_workplaces()) {
      return this->get_place(Place_Type::get_type_id("Workplace"), i);
    } else {
      return nullptr;
    }
  }

  /**
   * Gets the number of hospitals in the patch.
   *
   * @return the number of hospitals
   */  
  int get_number_of_hospitals() {
    return static_cast<int>(this->places[Place_Type::get_type_id("Hospital")].size());
  }

  /**
   * Gets the Hospital at the specified index.
   *
   * @param i the index
   * @return the hospital
   */
  Place* get_hospital(int i) {
    if(0 <= i && i < this->get_number_of_hospitals()) {
      return this->get_place(Place_Type::get_type_id("Hospital"), i);
    } else {
      return nullptr;
    }
  }

  /**
   * Gets the number of Place objects of the specified Place_Type in the patch.
   *
   * @param type_id the type ID
   * @return the number of places
   */  
  int get_number_of_places(int type_id) {
    if (0 <= type_id && type_id < Place_Type::get_number_of_place_types()) {
      return static_cast<int>(this->places[type_id].size());
    } else {
      return 0;
    }
  }

  /**
   * Gets the Place of the specified Place_Type at the specified index.
   *
   * @param type_id the type ID
   * @param i the index
   * @return the place
   */
  Place* get_place(int type_id, int i) {
    if(0 <= i && i < this->get_number_of_places(type_id)) {
      return this->places[type_id][i];
    } else {
      return nullptr;
    }
  }

  /**
   * Adds an elevation site at the given latitude and longitude with the specified elevation.
   *
   * @param lat the latitude
   * @param lon the longitude
   * @param elev the elevation
   */
  void add_elevation_site(double lat, double lon, double elev) {
    elevation_t *elevation_ptr = new elevation_t;
    elevation_ptr->lat = lat;
    elevation_ptr->lon = lon;
    elevation_ptr->elevation = elev;
    this->elevation_sites.push_back(elevation_ptr);
  }

  /**
   * Adds the specified elevation site to the patch's elevation sites.
   *
   * @param esite the elevation site
   */
  void add_elevation_site(elevation_t *esite) {
    this->elevation_sites.push_back(esite);
  }

  double get_elevation(double lat, double lon);

  void add_place(Place* place);

  /**
   * Gets the Place objects of the specified Place_Type in the patch as a place vector.
   *
   * @param type_id the type ID
   * @return the places
   */
  place_vector_t get_places(int type_id) {
    return this->places[type_id];
  }
  
  place_vector_t get_places_at_distance(int type_id, int dist);
  static void setup_logging();

private:
  Neighborhood_Layer* grid;
  Place* neighborhood;
  person_vector_t person;
  int popsize;
  long long int admin_code;
  std::vector<elevation_t*> elevation_sites;

  // lists of places by type
  place_vector_t schools_attended_by_neighborhood_residents;
  place_vector_t schools_attended_by_neighborhood_residents_by_age[Global::GRADES];
  place_vector_t workplaces_attended_by_neighborhood_residents;
  place_vector_t* places;

  static bool is_log_initialized;
  static std::string neighborhood_patch_log_level;
  static std::unique_ptr<spdlog::logger> neighborhood_patch_logger;
};

#endif // _FRED_NEIGHBORHOOD_PATCH_H
