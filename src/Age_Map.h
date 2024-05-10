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
// File: Age_Map.h
//
// Age_Map is a class that holds a set of age-specific values
//

#ifndef _FRED_AGE_MAP_H
#define _FRED_AGE_MAP_H

#include <vector>
#include <string>
#include <spdlog/spdlog.h>

#include "Global.h"
#include "Parser.h"

/**
 * This class is used to map values to specific age ranges.
 *
 * The ages vector stores the upper bound of each predetermined age group, while 
 * the values vector maps the values for each of those age groups.
 */
class Age_Map {
public:

  /**
   * Default constructor
   */
  Age_Map();

  void read_properties(std::string prefix);
  double find_value(double age);
  
  static void setup_logging();


private:
  std::string name;
  std::vector<double> ages; // vector to hold the upper age for each age group
  std::vector<double> values; // vector to hold the values for each age range

  bool quality_control() const;

  static bool is_log_initialized;
  static std::string age_map_log_level;
  static std::unique_ptr<spdlog::logger> age_map_logger;
};

#endif
