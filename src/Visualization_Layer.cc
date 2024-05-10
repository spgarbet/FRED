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
// File: Visualization_Layer.cc
//

#include <list>
#include <string>
#include <utility>

#include "Global.h"
#include "Condition.h"
#include "Parser.h"
#include "Place.h"
#include "Random.h"
#include "Regional_Layer.h"
#include "Visualization_Layer.h"
#include "Visualization_Patch.h"
#include "Utils.h"

/**
 * Creates the Visualization_Layer. Initializes default variables.
 */
Visualization_Layer::Visualization_Layer() {
  // create visualization data directories
  snprintf(Global::Visualization_directory, 
      FRED_STRING_SIZE,
      "%s/RUN%d/VIS",
	    Global::Simulation_directory,
	    Global::Simulation_run_number);
  Utils::fred_make_directory(Global::Visualization_directory);

  // get optional properties:
  Parser::disable_abort_on_failure();
  this->period = 1;
  Parser::get_property("visualization_period", &this->period);
  Parser::set_abort_on_failure();

  this->rows = 0;
  this->cols = 0;
}

/**
 * Gets the Visualization_Patch at the specified row and column in the grid.
 *
 * @param row the row
 * @param col the column
 * @return the visualization patch
 */
Visualization_Patch* Visualization_Layer::get_patch(int row, int col) {
  if(row >= 0 && col >= 0 && row < this->rows && col < this->cols) {
    return &(this->grid[row][col]);
  } else {
    return nullptr;
  }
}

/**
 * Gets the Visualization_Patch at the given global coordinates.
 *
 * @param x the global x value
 * @param y the global y value
 * @return the visualization patch
 */
Visualization_Patch* Visualization_Layer::get_patch(double x, double y) {
  int row = this->get_row(y);
  int col = this->get_col(x);
  return this->get_patch(row,col);
}

/**
 * Updates the Visualization_Patch at the given global coordinates with the specified values.
 *
 * @param x the global x value
 * @param y the global y value
 * @param n the value to add to the patch's count
 * @param total the value to add to the patch's population size
 */
void Visualization_Layer::update_data(double x, double y, int count, int popsize) {
  Visualization_Patch* patch =this->get_patch(x, y);
  if(patch != nullptr) {
    patch->update_patch_count(count, popsize);
  }
}
