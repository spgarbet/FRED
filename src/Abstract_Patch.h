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

#ifndef _FRED_ABSTRACT_PATCH_H
#define _FRED_ABSTRACT_PATCH_H

#include <string>

/**
 * This class represents an abstract patch in a grid.
 *
 * A patch is an area of the simulation, with varying sizes dependent on the patch size. 
 * A patch acts as a single cell in it's corresponding grid / layer.
 *
 * This class is inherited by Neighborhood_Patch, Regional_Patch, and Visualization_Patch.
 */
class Abstract_Patch {

public:
  /**
   * Gets the row of the patch.
   *
   * @return the row
   */
  int get_row() {
    return this->row;
  }

  /**
   * Gets the column of the patch.
   *
   * @return the column
   */
  int get_col() {
    return this->col;
  }

  /**
   * Gets the minimum global x value of the patch.
   *
   * @return the minimum global x
   */
  double get_min_x() {
    return this->min_x;
  }

  /**
   * Gets the maximum global x value of the patch.
   *
   * @return the maximum global x
   */
  double get_max_x() {
    return this->max_x;
  }

  /**
   * Gets the minimum global y value of the patch.
   *
   * @return the minimum global y
   */
  double get_min_y() {
    return this->min_y;
  }

  /**
   * Gets the maximum global y value of the patch.
   *
   * @return the maximum global y
   */
  double get_max_y() {
    return this->max_y;
  }

  /**
   * Gets the global x value of the center of the patch.
   *
   * @return the global x value of the center
   */
  double get_center_x() {
    return this->center_x;
  }

  /**
   * Gets the global y value of the center of the patch.
   *
   * @return the global y value of the center
   */
  double get_center_y() {
    return this->center_y;
  }

  std::string to_string();

protected:
  int row;
  int col;
  double min_x;
  double max_x;
  double min_y;
  double max_y;
  double center_x;
  double center_y;
};

#endif
