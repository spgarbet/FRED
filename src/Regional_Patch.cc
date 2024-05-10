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
// File: Regional_Patch.cc
//
#include <algorithm>
#include <set>

#include <spdlog/spdlog.h>

#include "Global.h"
#include "Place.h"
#include "Regional_Patch.h"
#include "Regional_Layer.h"
#include "Geo.h"
#include "Random.h"
#include "Utils.h"
#include "Parser.h"

int Regional_Patch::next_patch_id = 0;

bool Regional_Patch::is_log_initialized = false;
std::string Regional_Patch::regional_patch_log_level = "";
std::unique_ptr<spdlog::logger> Regional_Patch::regional_patch_logger = nullptr;

/**
 * Creates a Regional_Patch with default variables.
 */
Regional_Patch::Regional_Patch() {
  this->grid = nullptr;
  this->popsize = 0;
  this->max_popsize = 0;
  this->pop_density = 0;
  this->id = -1;
}

/**
 * Creates a Regional_Patch and sets it up in the specified Regional_Layer 
 * at the specified row and column.
 *
 * @param grd the regional layer
 * @param i the row
 * @param j the column
 */
Regional_Patch::Regional_Patch(Regional_Layer* grd, int i, int j) {
  setup(grd, i, j);
}

/**
 * Sets up this patch in the specified Regional_Layer at the given row and column.
 *
 * @param grd the regional layer
 * @param i the row
 * @param j the column
 */
void Regional_Patch::setup(Regional_Layer* grd, int i, int j) {
  this->grid = grd;
  this->row = i;
  this->col = j;
  double patch_size = this->grid->get_patch_size();
  double grid_min_x = this->grid->get_min_x();
  double grid_min_y = this->grid->get_min_y();
  this->min_x = grid_min_x + (this->col) * patch_size;
  this->min_y = grid_min_y + (this->row) * patch_size;
  this->max_x = grid_min_x + (this->col + 1) * patch_size;
  this->max_y = grid_min_y + (this->row + 1) * patch_size;
  this->center_y = (this->min_y + this->max_y) / 2.0;
  this->center_x = (this->min_x + this->max_x) / 2.0;
  this->popsize = 0;
  this->max_popsize = 0;
  this->pop_density = 0;
  this->person.clear();
  this->counties.clear();
  this->workplaces.clear();
  this->id = Regional_Patch::next_patch_id++;
  for(int k = 0; k < 100; ++k){
    this->students_by_age[k].clear();
  }
  this->workers.clear();
  this->hospitals.clear();
}

/**
 * Performs quality control on the patch.
 */
void Regional_Patch::quality_control() {
  for(int i = 0; i < static_cast<int>(this->hospitals.size()); ++i) {
    Regional_Patch::regional_patch_logger->trace(
        "<{:s}, {:d}>: patch quality control row {:d} col {:d} hosp {:d} {:s}", 
        __FILE__, __LINE__, this->row, this->col, i, this->hospitals[i]->get_label());
  }
  return;
}

/**
 * Gets the xy distance from the center of this patch to the center of the specified Regional_Patch 
 * using the distance formula.
 *
 * @param p2 the other patch
 * @return the xy distance
 */
double Regional_Patch::distance_to_patch(Regional_Patch* p2) {
  double x1 = this->center_x;
  double y1 = this->center_y;
  double x2 = p2->get_center_x();
  double y2 = p2->get_center_y();
  return sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
}

/**
 * Selects a random Person from this patch's person vector.
 *
 * @return the person
 */
Person* Regional_Patch::select_random_person() {
  if(static_cast<int>(this->person.size()) == 0) {
    return nullptr;
  }
  int i = Random::draw_random_int(0, static_cast<int>(this->person.size()) - 1);

  return this->person[i];
}

/**
 * Sets the maximum population size of this patch. Also sets the population density, which is the current 
 * population size divided by the maximum.
 *
 * @param n the maximum population size
 */
void Regional_Patch::set_max_popsize(int n) {
  this->max_popsize = n;
  this->pop_density = static_cast<double>(this->popsize) / static_cast<double>(n);

  // the following reflects noise in the estimated population in the preprocessing routine
  if(this->pop_density > 0.8) {
    this->pop_density = 1.0;
  }

}

/**
 * Selects a random student at the specified age from this patch's students by age person vector.
 *
 * @param age_ the age
 * @return the student
 */
Person* Regional_Patch::select_random_student(int age_) {
  if(static_cast<int>(this->students_by_age[age_].size()) == 0) {
    return nullptr;
  }
  int i = Random::draw_random_int(0, (static_cast<int>(this->students_by_age[age_].size())) - 1);

  return this->students_by_age[age_][i];
}

/**
 * Selects a random worker from this patch's workers person vector.
 *
 * @return the worker
 */
Person* Regional_Patch::select_random_worker() {
  if(static_cast<int>(this->workers.size()) == 0) {
    return nullptr;
  }
  int i = Random::draw_random_int(0, (static_cast<int>(this->workers.size())) - 1);
  
  return this->workers[i];
}

/**
 * Removes the specified Person from this patch's person vector.
 *
 * @param per the person
 */
void Regional_Patch::end_membership(Person* per) {
  if(this->person.size() > 1) {
    person_vector_t::iterator iter;
    iter = std::find(this->person.begin(), this->person.end(), per);
    if(iter != this->person.end()) {
      std::swap((*iter), this->person.back());
      this->person.erase(this->person.end() - 1);
    }
    assert(std::find(this->person.begin(), this->person.end(), per) == this->person.end());
  } else {
    this->person.clear();
  }
}

/**
 * Gets a workplace nearby the specified Place with the specified number of staff, allowing a staff size variation 
 * of 25%. This will call this patch's Regional_Layer to find a workplace nearby.
 *
 * @param place the place
 * @param staff the staff size
 * @return the nearby place
 */
Place* Regional_Patch::get_nearby_workplace(Place* place, int staff) {
  Regional_Patch::regional_patch_logger->info("get_workplace_near_place entered");

  double x = Geo::get_x(place->get_longitude());
  double y = Geo::get_y(place->get_latitude());

  // allow staff size variation by 25%
  int min_staff = (int)(0.75 * staff);
  if(min_staff < 1) {
    min_staff = 1;
  }
  int max_staff = (int)(0.5 + 1.25 * staff);
  Regional_Patch::regional_patch_logger->debug("staff {:d} {:d} {:d}", min_staff, staff, max_staff);
  // find nearest workplace that has right number of employees
  double min_dist = 1e99;
  Place* nearby_workplace = this->grid->get_nearby_workplace(this->row, this->col, x, y, min_staff,
                   max_staff, &min_dist);
  if(nearby_workplace == nullptr) {
    Regional_Patch::regional_patch_logger->debug("nearby_workplace == nullptr");
    return nullptr;
  }
  assert(nearby_workplace != nullptr);
  double x2 = Geo::get_x(nearby_workplace->get_longitude());
  double y2 = Geo::get_y(nearby_workplace->get_latitude());
  Regional_Patch::regional_patch_logger->debug(
      "nearby workplace {:s} {:f} {:f} size {:d} target {:d} dist {:f}", nearby_workplace->get_label(), 
      x2, y2, nearby_workplace->get_size(), staff, min_dist);
  return nearby_workplace;
}

/**
 * Gets the workplace in this patch closest to a specified latitude and longitude, with a staff size in the given range.
 *
 * @param x the longitude
 * @param y the latitude
 * @param min_size the minimum staff size
 * @param max_size the maximum staff size
 * @param min_dist an initial maximum value; will be set to the distance to the closest workplace
 * @return the closest workplace
 **/
Place* Regional_Patch::get_closest_workplace(double x, double y, int min_size, int max_size, double* min_dist) {
  Regional_Patch::regional_patch_logger->info(
      "get_closest_workplace entered for patch {:d} {:d} min_size = {:d} max_size = {:d} min_dist = {:f}  workplaces in patch = {:d}",
      row, col, min_size, max_size, *min_dist, static_cast<int>(this->workplaces.size()));
  Place* closest_workplace = nullptr;
  int number_workplaces = static_cast<int>(this->workplaces.size());
  for(int j = 0; j < number_workplaces; ++j) {
    Place* workplace = this->workplaces[j];
    if(workplace->is_group_quarters()) {
      continue;
    }
    int size = workplace->get_size();
    if(min_size <= size && size <= max_size) {
      double x2 = Geo::get_x(workplace->get_longitude());
      double y2 = Geo::get_y(workplace->get_latitude());
      double dist = sqrt((x - x2) * (x - x2) + (y - y2) * (y - y2));
      if(dist < 20.0 && dist < *min_dist) {
        *min_dist = dist;
        closest_workplace = workplace;
        Regional_Patch::regional_patch_logger->debug("closer = {:s} size = {:d} min_dist = {:f}", 
            closest_workplace->get_label(), size, *min_dist);
      }
    }
  }
  return closest_workplace;
}

/**
 * Adds a specified workplace to this patch's workplaces place vector.
 *
 * @param workplace the workplace
 */
void Regional_Patch::add_workplace(Place* workplace) {
  this->workplaces.push_back(workplace);
}

/**
 * Adds a specified Hospital to this patch's hospitals place vector.
 *
 * @param hospital the hospital
 */
void Regional_Patch::add_hospital(Place* hospital) {
  Regional_Patch::regional_patch_logger->debug("REGIONAL PATCH row {:d} col {:d} ADD HOSP {:s}", 
      this->row, this->col, hospital->get_label());
  this->hospitals.push_back(hospital);
}

/**
 * Swaps 10% of this patch's population. For each Person to swap, a random person is selected. If that person is a student, 
 * a random student will be selected for the second person. If that person is a worker, a random worker will be selected for 
 * the second person. If the two people are from different counties, their school / workplace will be swapped.
 */
void Regional_Patch::swap_county_people(){
  if(static_cast<int>(this->counties.size()) > 1){
    double percentage = 0.1;
    int people_swapped = 0;
    int people_to_reassign_place = (int) (percentage * this->person.size());
    Regional_Patch::regional_patch_logger->debug("People to reassign : {:d}", people_to_reassign_place);
    for(int k = 0; k < people_to_reassign_place; ++k) {
      Person* p = this->select_random_person();
      Person* p2;
      if(p != nullptr) {
        if(p->is_student()) {
          int age_ = 0;
          age_ = p->get_age();
          if(age_ > 100) {
            age_ = 100;
          }
          if(age_ < 0) {
            age_ = 0;
          }
          p2 = select_random_student(age_);
          if(p2 != nullptr) {
            Place* s1 = p->get_school();
            Place* s2 = p2->get_school();
            Household* h1 =  p->get_household();
            int h1_county = h1->get_county_admin_code();
            Household* h2 = p2->get_household();
            int h2_county = h2->get_county_admin_code();
            if(h1_county != h2_county) {
              p->change_school(s2);
              p2->change_school(s1);
              Regional_Patch::regional_patch_logger->info(
                  "SWAPSCHOOLS\t{:d}\t{:d}\t{:s}\t{:s}\t{:g}\t{:g}\t{:g}\t{:g}",
                  p->get_id(), p2->get_id(), p->get_school()->get_label(), p2->get_school()->get_label(), 
                  p->get_school()->get_latitude(), p->get_school()->get_longitude(), 
                  p2->get_school()->get_latitude(), p2->get_school()->get_longitude());
              ++people_swapped;
            }
          }
        } else if(p->get_workplace() != nullptr) {
          p2 = select_random_worker();
          if(p2 != nullptr) {
            Place* w1 = p->get_workplace();
            Place* w2 = p2->get_workplace();
            Household* h1 = p->get_household();
            int h1_county = h1->get_county_admin_code();
            Household* h2 = p2->get_household();
            int h2_county = h2->get_county_admin_code();
            if(h1_county != h2_county) {
              p->change_workplace(w2);
              p2->change_workplace(w1);
              Regional_Patch::regional_patch_logger->info(
                  "SWAPWORKS\t{:d}\t{:d}\t{:s}\t{:s}\t{:g}\t{:g}\t{:g}\t{:g}",
                  p->get_id(), p2->get_id(), p->get_workplace()->get_label(), p2->get_workplace()->get_label(),
                  p->get_workplace()->get_latitude(), p->get_workplace()->get_longitude(),
                  p2->get_workplace()->get_latitude(), p2->get_workplace()->get_longitude());
              ++people_swapped;
            }
          }
        }
      }
    }
    Regional_Patch::regional_patch_logger->info("People Swapped:: {:d} out of {:d}", 
        people_swapped, people_to_reassign_place);
  }
  return;
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Regional_Patch::setup_logging() {
  if(Regional_Patch::is_log_initialized) {
    return;
  }

  if(Parser::does_property_exist("regional_patch_log_level")) {
    Parser::get_property("regional_patch_log_level", &Regional_Patch::regional_patch_log_level);
  } else {
    Regional_Patch::regional_patch_log_level = "OFF";
  }

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Regional_Patch::regional_patch_logger = std::make_unique<spdlog::logger>("regional_patch_logger", 
        sink_list.begin(), sink_list.end());
    Regional_Patch::regional_patch_logger->set_level(
        Utils::get_log_level_from_string(Regional_Patch::regional_patch_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Regional_Patch::regional_patch_logger->trace("<{:s}, {:d}>: Regional_Patch logger initialized", 
      __FILE__, __LINE__  );
  Regional_Patch::is_log_initialized = true;
}
