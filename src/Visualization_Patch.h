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
// File: Visualization_Patch.h
//

#ifndef _FRED_VISUALIZATION_PATCH_H
#define _FRED_VISUALIZATION_PATCH_H

#include <spdlog/spdlog.h>

#include "Abstract_Patch.h"
#include "Global.h"

class Visualization_Layer;

/**
 * This class represents a cell in the Visualization_Layer grid.
 *
 * A Visualization_Patch is an area of the simulation for which locational data 
 * is stored for a visualization of the simulation.
 *
 * This class inherits from Abstract_Patch.
 */
class Visualization_Patch : public Abstract_Patch {
public:
  /**
   * Creates a Visualization_Patch. Initializes default variables.
   */
  Visualization_Patch() {
    this->count = 0;
    this->popsize = 0;
  }

  /**
   * Default destructor.
   */
  ~Visualization_Patch() {}

  void setup(int i, int j, double patch_size, double grid_min_x, double grid_min_y);
  void quality_control();
  double distance_to_patch(Visualization_Patch* patch2);
  void print();

  /**
   * Resets this patch's count and popsize.
   */
  void reset_counts() {
    this->count = 0;
    this->popsize = 0;
  }

  /**
   * Adds the specified values to this patch's count and population size.
   *
   * @param n the value to add to the count
   * @param total the value to add to the population size
   */
  void update_patch_count(int n, int total) {
    this->count += n;
    this->popsize += total;
  }

  /**
   * Gets this patch's count.
   *
   * @return the count
   */
  int get_count() {
    return this->count;
  }

  /**
   * Gets this patch's population size.
   *
   * @return the population size
   */
  int get_popsize() {
    return this->popsize;
  }
  
  static void setup_logging();

protected:
  int count;
  int popsize;

private:
  static bool is_log_initialized;
  static std::string visualization_patch_log_level;
  static std::unique_ptr<spdlog::logger> visualization_patch_logger;
};

#endif // _FRED_VISUALIZATION_PATCH_H
