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
// File: Regional_Layer.cc
//
#include <list>
#include <string>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "Geo.h"
#include "Global.h"
#include "Household.h"
#include "Parser.h"
#include "Place.h"
#include "Random.h"
#include "Regional_Layer.h"
#include "Regional_Patch.h"
#include "Utils.h"

bool Regional_Layer::is_log_initialized = false;
std::string Regional_Layer::regional_layer_log_level = "";
std::unique_ptr<spdlog::logger> Regional_Layer::regional_layer_logger = nullptr;

/**
 * Creates a Regional_Layer with the given geographical bounds. Sets up the grid to cover 
 * the global simulation region.
 *
 * @param minlon the minimum longitude
 * @param minlat the minimum latitude
 * @param maxlon the maximum longitude
 * @param maxlat the maximum latitude
 */
Regional_Layer::Regional_Layer(fred::geo minlon, fred::geo minlat, fred::geo maxlon, fred::geo maxlat) {
  this->min_lon = minlon;
  this->min_lat = minlat;
  this->max_lon = maxlon;
  this->max_lat = maxlat;
  Regional_Layer::regional_layer_logger->debug("Regional_Layer min_lon = {:f}", this->min_lon);
  Regional_Layer::regional_layer_logger->debug("Regional_Layer min_lat = {:f}", this->min_lat);
  Regional_Layer::regional_layer_logger->debug("Regional_Layer max_lon = {:f}", this->max_lon);
  Regional_Layer::regional_layer_logger->debug("Regional_Layer max_lat = {:f}", this->max_lat);

  // read in the patch size for this layer
  Parser::get_property("regional_patch_size", &this->patch_size);

  // find the global x,y coordinates of SW corner of grid
  this->min_x = Geo::get_x(this->min_lon);
  this->min_y = Geo::get_y(this->min_lat);

  // find the global row and col in which SW corner occurs
  this->global_row_min = static_cast<int>(this->min_y / this->patch_size);
  this->global_col_min = static_cast<int>(this->min_x / this->patch_size);

  // align coords to global grid
  this->min_x = this->global_col_min * this->patch_size;
  this->min_y = this->global_row_min * this->patch_size;

  // compute lat,lon of SW corner of aligned grid
  this->min_lat = Geo::get_latitude(this->min_y);
  this->min_lon = Geo::get_longitude(this->min_x);

  // find x,y coords of NE corner of bounding box
  this->max_x = Geo::get_x(this->max_lon);
  this->max_y = Geo::get_y(this->max_lat);

  // find the global row and col in which NE corner occurs
  this->global_row_max = static_cast<int>(this->max_y / this->patch_size);
  this->global_col_max = static_cast<int>(this->max_x / this->patch_size);

  // align coords_y to global grid
  this->max_x = (this->global_col_max + 1) * this->patch_size;
  this->max_y = (this->global_row_max + 1) * this->patch_size;

  // compute lat,lon of NE corner of aligned grid
  this->max_lat = Geo::get_latitude(this->max_y);
  this->max_lon = Geo::get_longitude(this->max_x);

  // number of rows and columns needed
  this->rows = this->global_row_max - this->global_row_min + 1;
  this->cols = this->global_col_max - this->global_col_min + 1;
    
  if(Global::Compile_FRED && this->rows < 0) {
    this->rows = 1;
  }
    
  if(Global::Compile_FRED && this->cols < 0) {
    this->cols = 1;
  }

  Regional_Layer::regional_layer_logger->debug("Regional_Layer new min_lon = {:f}", this->min_lon);
  Regional_Layer::regional_layer_logger->debug("Regional_Layer new min_lat = {:f}", this->min_lat);
  Regional_Layer::regional_layer_logger->debug("Regional_Layer new max_lon = {:f}", this->max_lon);
  Regional_Layer::regional_layer_logger->debug("Regional_Layer new max_lat = {:f}", this->max_lat);
  Regional_Layer::regional_layer_logger->debug("Regional_Layer rows = {:d}  cols = {:d}", this->rows, this->cols);
  Regional_Layer::regional_layer_logger->debug("Regional_Layer min_x = {:f}  min_y = {:f}", this->min_x, this->min_y);
  Regional_Layer::regional_layer_logger->debug("Regional_Layer max_x = {:f}  max_y = {:f}", this->max_x, this->max_y);
  Regional_Layer::regional_layer_logger->debug("Regional_Layer global_col_min = {:d}  global_row_min = {:d}", this->global_col_min, this->global_row_min);

  this->grid = new Regional_Patch *[this->rows];
  for(int i = 0; i < this->rows; ++i) {
    this->grid[i] = new Regional_Patch[this->cols];
  }
  for(int i = 0; i < this->rows; ++i) {
    for(int j = 0; j < this->cols; ++j) {
      this->grid[i][j].setup(this, i, j);
      Regional_Layer::regional_layer_logger->trace(
          "<{:s}, {:d}>: grid[{:d}][{:d}]: {:s}", __FILE__, __LINE__, i, j, this->grid[i][j].to_string());
    }
  }
}

/**
 * Gets the Regional_Patch at the given row and column in the grid.
 *
 * @param row the row
 * @param col the column
 * @return the regional patch
 */
Regional_Patch* Regional_Layer::get_patch(int row, int col) {
  if(row >= 0 && col >= 0 && row < this->rows && col < this->cols) {
    return &this->grid[row][col];
  } else {
    return nullptr;
  }
}

/**
 * Gets the Regional_Patch at the given latitude and longitude in the grid. The latitude and longitude are converted to 
 * a row and column.
 *
 * @param lat the latitude
 * @param lon the longitude
 * @return the regional patch
 */
Regional_Patch* Regional_Layer::get_patch(fred::geo lat, fred::geo lon) {
  int row = get_row(lat);
  int col = get_col(lon);
  if(row >= 0 && col >= 0 && row < this->rows && col < this->cols) {
    return &this->grid[row][col];
  } else {
    return nullptr;
  }
}

/**
 * Gets the Regional_Patch at the latitude and longitude of the specified place in the grid.
 *
 * @param place the place
 * @return the regional patch
 */
Regional_Patch* Regional_Layer::get_patch(Place* place) {
  return get_patch(place->get_latitude(), place->get_longitude());
}

/**
 * Gets the Regional_Patch in the grid at the given global coordinates. The grid's minimum global coordinates are subtracted from 
 * the global coordinates to find coordinates local to the grid.
 *
 * @param row the global row
 * @param col the global col
 * @return the regional patch
 */
Regional_Patch* Regional_Layer::get_patch_with_global_coords(int row, int col) {
  return get_patch(row - this->global_row_min, col - this->global_col_min);
}

/**
 * Gets the Regional_Patch in the grid with the specified ID.
 *
 * @param id the ID
 * @return the regional patch
 */
Regional_Patch* Regional_Layer::get_patch_from_id(int id) {
  int row = id / this->cols;
  int col = id % this->cols;
  Regional_Layer::regional_layer_logger->trace(
      "<{:s}, {:d}>: patch lookup for id = {:d} ... calculated row = {:d}, col = {:d}, rows = {:d}, cols = {:d}", 
      __FILE__, __LINE__, id, row, col, rows, cols);
  assert(this->grid[row][col].get_id() == id);
  return &(this->grid[row][col]);
}

/**
 * Selects a random Regional_Patch from the grid.
 *
 * @return the regional patch
 */
Regional_Patch* Regional_Layer::select_random_patch() {
  int row = Random::draw_random_int(0, this->rows - 1);
  int col = Random::draw_random_int(0, this->cols - 1);
  return &this->grid[row][col];
}

/**
 * Performs quality control on the grid.
 */
void Regional_Layer::quality_control() {
  Regional_Layer::regional_layer_logger->info("grid quality control check");

  for(int row = 0; row < this->rows; ++row) {
    for(int col = 0; col < this->cols; ++col) {
      this->grid[row][col].quality_control();
    }
  }

  if(Global::Verbose > 1) {
    char filename[FRED_STRING_SIZE];
    snprintf(filename, FRED_STRING_SIZE, "%s/large_grid.dat", Global::Simulation_directory);
    FILE* fp = fopen(filename, "w");
    for(int row = 0; row < this->rows; ++row) {
      if(row % 2) {
        for(int col = this->cols - 1; col >= 0; --col) {
          double x = this->grid[row][col].get_center_x();
          double y = this->grid[row][col].get_center_y();
          fprintf(fp, "%f %f\n", x, y);
        }
      } else {
        for(int col = 0; col < this->cols; ++col) {
          double x = this->grid[row][col].get_center_x();
          double y = this->grid[row][col].get_center_y();
          fprintf(fp, "%f %f\n", x, y);
        }
      }
    }
    fclose(fp);
  }

  Regional_Layer::regional_layer_logger->info("grid quality control finished");
}

// Specific to Regional_Patch Regional_Layer:

/**
 * Adds all persons to their corresponding Regional_Patch.
 */
void Regional_Layer::set_population_size() {
  for(int p = 0; p < Person::get_population_size(); ++p) {
    Person* person = Person::get_person(p);
    Place* hh = person->get_household();
    if(hh == nullptr) {
      if(Global::Enable_Hospitals && person->person_is_hospitalized() && person->get_permanent_household() != nullptr) {
        hh = person->get_permanent_household();
      }
    }
    assert(hh != nullptr);
    int row = get_row(hh->get_latitude());
    int col = get_col(hh->get_longitude());
    Regional_Patch* patch = get_patch(row, col);
    patch->add_person_to_patch(person);
  }
}

/**
 * Adds the specified workplace to the Regional_Patch in which it is located.
 *
 * @param place the workplace
 */
void Regional_Layer::add_workplace(Place* place) {
  Regional_Patch* patch = this->get_patch(place);
  if(patch != nullptr) {
    patch->add_workplace(place);
  }
}

/**
 * Adds the specified Hospital to the Regional_Patch in which it is located.
 *
 * @param place the hospital
 */
void Regional_Layer::add_hospital(Place* place) {
  Regional_Patch* patch = this->get_patch(place);
  if(patch != nullptr) {
    patch->add_hospital(place);
  } else {
    Regional_Layer::regional_layer_logger->info(
        "REGIONAL LAYER NULL PATCH FOR HOSP {:s} lat {:f} lon {:f}", 
        place->get_label(), place->get_latitude(), place->get_longitude());
  }
}

/**
 * Gets the closest workplace with a staff size within the specified range to the specified geographical location within 
 * a specified Regional_Patch or a surrounding patch.
 *
 * @param row the row of the patch
 * @param col the column of the patch
 * @param x the longitude of the location
 * @param y the latitude of the location
 * @param min_staff the minimum staff size
 * @param max_staff the maximum staff size
 * @param min_dist initial maximum value; will be mutated to be the distance to the closest workplace
 */
Place* Regional_Layer::get_nearby_workplace(int row, int col, double x, double y, int min_staff, int max_staff, double* min_dist) {
  //find nearest workplace that has right number of employees
  Place* nearby_workplace = nullptr;
  *min_dist = 1e99;
  for(int i = row - 1; i <= row + 1; ++i) {
    for(int j = col - 1; j <= col + 1; ++j) {
      Regional_Patch * patch = get_patch(i, j);
      if(patch != nullptr) {
        Place* closest_workplace = patch->get_closest_workplace(x, y, min_staff, max_staff,min_dist);
        if(closest_workplace != nullptr) {
          nearby_workplace = closest_workplace;
        } else {
          Regional_Layer::regional_layer_logger->info("No nearby workplace in row {:d} col {:d}", i, j);
        }
      }
    }
  }
  return nearby_workplace;
}

/**
 * Searches for the specified amount of hospitals in Regional_Patch at the specified row and column. If the minimum 
 * hospitals are not found in that patch, expands the search to the surrounding patches. This process continues 
 * until the minimum amount of hospitals are found, and the hospitals are returned as a place vector.
 *
 * @param row the row
 * @param col the column
 * @param x _UNUSED_
 * @param y _UNUSED_
 * @param min_found the minimum amount of hospitals to find
 * @return the hospitals
 */
std::vector<Place*> Regional_Layer::get_nearby_hospitals(int row, int col, double x, double y, int min_found) {
  std::vector<Place*> ret_val;
  ret_val.clear();
  bool done = false;
  int search_dist = 1;
  while(!done) {
    for(int i = row - search_dist; i <= row + search_dist; ++i) {
      for(int j = col - search_dist; j <= col + search_dist; ++j) {
        Regional_Patch* patch = get_patch(i, j);
        if(patch != nullptr) {
          std::vector<Place*> hospitals = patch->get_hospitals();
          if(static_cast<int>(hospitals.size()) > 0) {
            for(std::vector<Place*>::iterator itr = hospitals.begin(); itr != hospitals.end(); ++itr) {
              ret_val.push_back(*itr);
            }
          }
        }
      }
    }

    //Try to expand the search if we don't have enough hospitals and we CAN
    if(static_cast<int>(ret_val.size()) < min_found) {
      if((row + search_dist + 1 < this->rows || col + search_dist + 1 < this->cols) ||
         (row - search_dist - 1 >= 0 || col - search_dist - 1 >= 0)) {
        //Expand the search
        ret_val.clear();
        ++search_dist;
      } else {
        done = true;
      }
    } else {
      done = true;
    }
  }
  return ret_val;
}

/**
 * Removes the specified Person from the Regional_Patch at the given latitude and longitude.
 *
 * @param lat the latitude
 * @param lon the longitude
 * @param person the person
 */
void Regional_Layer::end_membership(fred::geo lat, fred::geo lon, Person* person) {
  Regional_Patch* regional_patch = this->get_patch(lat, lon);
  if(regional_patch != nullptr) {
    regional_patch->end_membership(person);
  }
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Regional_Layer::setup_logging() {
  if(Regional_Layer::is_log_initialized) {
    return;
  }

  if(Parser::does_property_exist("regional_layer_log_level")) {
    Parser::get_property("regional_layer_log_level", &Regional_Layer::regional_layer_log_level);
  } else {
    Regional_Layer::regional_layer_log_level = "OFF";
  }

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Regional_Layer::regional_layer_logger = std::make_unique<spdlog::logger>("regional_layer_logger", 
        sink_list.begin(), sink_list.end());
    Regional_Layer::regional_layer_logger->set_level(
        Utils::get_log_level_from_string(Regional_Layer::regional_layer_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Regional_Layer::regional_layer_logger->trace("<{:s}, {:d}>: Regional_Layer logger initialized", 
      __FILE__, __LINE__  );
  Regional_Layer::is_log_initialized = true;
}
