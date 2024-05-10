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


#ifndef _ABSTRACT_GRID_H
#define _ABSTRACT_GRID_H

#include "Global.h"
#include "Geo.h"

/**
 * This class represents an abstract grid of patches.
 *
 * The grid is a layer of patches that extends throughout the area in which the simulation 
 * takes place. There is typically only one of each specific layer during a simulation.
 *
 * This class is inherited by Neighborhood_Layer, Regional_Layer, and Visualization_Layer.
 */
class Abstract_Grid {

 public:

  /**
   * Gets the number of rows in the grid.
   *
   * @return the number of rows
   */
  int get_rows() {
    return this->rows;
  }

  /**
   * Gets the number of columns in the grid.
   *
   * @return the number of columns
   */
  int get_cols() {
    return this->cols;
  }

  /**
   * Gets the number of patches in the grid.
   *
   * @return the number of patches
   */
  int get_number_of_patches() {
    return this->rows * this->cols;
  }

  /**
   * Gets the minimum latitude of the grid. This will be the latitude of the SW corner.
   *
   * @return the minimum latitude
   */
  fred::geo get_min_lat() {
    return this->min_lat;
  }

  /**
   * Gets the minimum longitude of the grid. This will be the longitude of the SW corner.
   *
   * @return the minimum longitude
   */
  fred::geo get_min_lon() {
    return this->min_lon;
  }

  /**
   * Gets the maximum latitude of the grid. This will be the latitude of the NE corner.
   *
   * @return the maximum latitude
   */
  fred::geo get_max_lat() {
    return this->max_lat;
  }

  /**
   * Gets the maximum longitude of the grid. This will be the longitude of the NE corner.
   *
   * @return the maximum longitude
   */
  fred::geo get_max_lon() {
    return this->max_lon;
  }

  /**
   * Gets the minimum global x value of the grid. This will be the global x value of the SW corner.
   *
   * @return the minimum global x
   */
  double get_min_x() {
    return this->min_x;
  }

  /**
   * Gets the maximum global x value of the grid. This will be the global x value of the NE corner.
   *
   * @return the maximum global x
   */
  double get_max_x() {
    return this->max_x;
  }

  /**
   * Gets the minimum global y value of the grid. This will be the global y value of the SW corner.
   *
   * @return the minimum global y
   */
  double get_min_y() {
    return this->min_y;
  }

  /**
   * Gets the maximum global y value of the grid. This will be the global x value of the NE corner.
   *
   * @return the maximum global y
   */
  double get_max_y() {
    return this->max_y;
  }

  /**
   * Gets the patch size. This will be length of one side of the patch, in kilometers.
   *
   * @return the patch size
   */
  double get_patch_size() {
    return this->patch_size;
  }

  /**
   * Gets a row of the grid from a global y value.
   *
   * @param y the global y value
   * @return the row
   */
  int get_row(double y) {
    return static_cast<int>(((y - this->min_y) / this->patch_size));
  }

  /**
   * Gets a column of the grid from a global x value.
   *
   * @param x the global x value
   * @return the column
   */
  int get_col(double x) {
    return static_cast<int>(((x - this->min_x) / this->patch_size));
  }

  /**
   * Gets a row of the grid from a latitude.
   *
   * @param lat the latitude
   * @return the row
   */
  int get_row(fred::geo lat) {
    double y = Geo::get_y(lat);
    return static_cast<int>(((y - this->min_y) / this->patch_size));
  }

  /**
   * Gets a column of the grid from a longitude
   *
   * @param lon the longitude
   * @return the column
   */
  int get_col(fred::geo lon) {
    double x = Geo::get_x(lon);
    return static_cast<int>(((x - this->min_x) / this->patch_size));
  }

  /**
   * Gets the minimum global row coordinate. This will be the global row coordinate of the south row.
   *
   * @return the minimum global row coordinate
   */
  int get_global_row_min() {
    return this->global_row_min;        // global row coord of S row
  }

  /**
   * Gets the minimum global column coordinate. This will be the global column coordinate of the west column.
   *
   * @return the minimum global column coordinate
   */
  int get_global_col_min() {
    return this->global_col_min;        // global col coord of W col
  }

  /**
   * Gets the maximum global row coordinate. This will be the global row coordinate of the north row.
   *
   * @return the maximum global row coordinate
   */
  int get_global_row_max() {
    return this->global_row_max;        // global row coord of N row
  }

  /**
   * Gets the maximum global column coordinate. This will be the global column coordinate of the east column.
   *
   * @return the maximum global column coordinate
   */
  int get_global_col_max() {
    return this->global_col_max;        // global col coord of E col
  }

 protected:

  int rows;              // number of rows
  int cols;              // number of columns
  double patch_size;     // km per side
  fred::geo min_lat;     // lat of SW corner
  fred::geo min_lon;     // lon of SW corner
  fred::geo max_lat;     // lat of NE corner
  fred::geo max_lon;     // lon of NE corner
  double min_x;          // global x of SW corner
  double min_y;          // global y of SW corner
  double max_x;          // global x of NE corner
  double max_y;          // global y of NE corner
  int global_row_min;    // global row coord of S row
  int global_col_min;    // global col coord of W col
  int global_row_max;    // global row coord of N row
  int global_col_max;    // global col coord of E col
};

#endif
