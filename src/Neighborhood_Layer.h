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
// File: Neighborhood_Layer.h
//

#ifndef _FRED_NEIGHBORHOOD_LAYER_H
#define _FRED_NEIGHBORHOOD_LAYER_H

#include <vector>

#include <spdlog/spdlog.h>

#include "Abstract_Grid.h"

typedef std::vector<int> offset_t;
typedef std::vector<double> gravity_cdf_t;
typedef std::vector<Place*> place_vector_t;

class Neighborhood_Patch;
class Neighborhood;
class Place;

/**
 * This class represents a grid of Neighborhood_Patch objects.
 *
 * The Neighborhood_Layer extends throughout the global simulation region, and 
 * contains data on specific patches in the region that relates to neighborhoods 
 * and population.
 *
 * This class inherits from Abstract_Grid.
 */
class Neighborhood_Layer : public Abstract_Grid {
 public:

  Neighborhood_Layer();
  ~Neighborhood_Layer() {}
  void setup();
  void setup(int phase);
  void prepare();
  Neighborhood_Patch* get_patch(int row, int col);
  Neighborhood_Patch* get_patch(fred::geo lat, fred::geo lon);
  Neighborhood_Patch* get_patch(Place* place);
  void quality_control();
  void quality_control(double min_x, double min_y);
  void record_activity_groups();
  int get_number_of_neighborhoods();
  void setup_gravity_model();
  void setup_null_gravity_model();
  void print_gravity_model();
  void print_distances();
  Place* select_destination_neighborhood(Place* src_neighborhood);
  void add_place(Place *place);
  
  static void setup_logging();

 private:

  Neighborhood_Patch** grid;     // Rectangular array of patches

  // data used by neighborhood gravity model
  offset_t** offset;
  gravity_cdf_t** gravity_cdf;
  int max_offset;
  std::vector<std::pair<double, int>> sort_pair;

  // runtime properties for neighborhood gravity model
  double max_distance;
  double min_distance;
  int max_destinations;
  double pop_exponent;
  double dist_exponent;

  static bool is_log_initialized;
  static std::string neighborhood_layer_log_level;
  static std::unique_ptr<spdlog::logger> neighborhood_layer_logger;
};

#endif // _FRED_NEIGHBORHOOD_LAYER_H
