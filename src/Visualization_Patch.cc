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
// File: Visualization_Patch.cc
//

#include <spdlog/spdlog.h>

#include "Parser.h"
#include "Utils.h"
#include "Visualization_Layer.h"
#include "Visualization_Patch.h"

bool Visualization_Patch::is_log_initialized = false;
std::string Visualization_Patch::visualization_patch_log_level = "";
std::unique_ptr<spdlog::logger> Visualization_Patch::visualization_patch_logger = nullptr;


/**
 * Sets up this visualization patch with the specified properties.
 *
 * @param i the row
 * @param j the column
 * @param patch_size the length of the patch sides in km
 * @param grid_min_x the minimum global x value of the grid
 * @param grid_min_y the minimum global y value of the grid
 */
void Visualization_Patch::setup(int i, int j, double patch_size, double grid_min_x, double grid_min_y) {
  this->row = i;
  this->col = j;
  this->min_x = grid_min_x + (this->col) * patch_size;
  this->min_y = grid_min_y + (this->row) * patch_size;
  this->max_x = grid_min_x + (this->col + 1)* patch_size;
  this->max_y = grid_min_y + (this->row + 1) * patch_size;
  this->center_y = (this->min_y + this->max_y) / 2.0;
  this->center_x = (this->min_x + this->max_x) / 2.0;
  reset_counts();
}

/**
 * _UNUSED_
 */
void Visualization_Patch::quality_control() {
  return;
}

/**
 * Gets the xy distance from the center of this patch to the center of the specified Visualization_Patch 
 * using the distance formula.
 *
 * @param p2 the other visualization patch
 * @return the xy distance
 */
double Visualization_Patch::distance_to_patch(Visualization_Patch* p2) {
  double x1 = this->center_x;
  double y1 = this->center_y;
  double x2 = p2->get_center_x();
  double y2 = p2->get_center_y();
  return sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
}

/**
 * Prints data about this patch.
 */
void Visualization_Patch::print() {
  Visualization_Patch::visualization_patch_logger->info("visualization_patch: {:d} {:d} {:d} {:d}", 
      row, col, count, popsize);
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Visualization_Patch::setup_logging() {
  if(Visualization_Patch::is_log_initialized) {
    return;
  }

  if(Parser::does_property_exist("visualization_patch_log_level")) {
    Parser::get_property("visualization_patch_log_level", &Visualization_Patch::visualization_patch_log_level);
  } else {
    Visualization_Patch::visualization_patch_log_level = "OFF";
  }

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink,
        Global::DebugFileSink, Global::TraceFileSink};
    Visualization_Patch::visualization_patch_logger = std::make_unique<spdlog::logger>("visualization_patch_logger",
        sink_list.begin(), sink_list.end());
    Visualization_Patch::visualization_patch_logger->set_level(
        Utils::get_log_level_from_string(Visualization_Patch::visualization_patch_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Visualization_Patch::visualization_patch_logger->trace("<{:s}, {:d}>: Visualization_Patch logger initialized",
      __FILE__, __LINE__  );
  Visualization_Patch::is_log_initialized = true;
}
