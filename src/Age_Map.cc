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
// File: Age_Map.cpp
//

#include <spdlog/spdlog.h>

#include "Age_Map.h"
#include "Parser.h"
#include "Utils.h"

bool Age_Map::is_log_initialized = false;
std::string Age_Map::age_map_log_level = "";
std::unique_ptr<spdlog::logger> Age_Map::age_map_logger = nullptr;

/**
 * Creates an age map.
 */
Age_Map::Age_Map() {
  ages.clear();
  values.clear();
}

/**
 * Prints the properties of the specified prefix.
 *
 * @param prefix the prefix
 */
void Age_Map::read_properties(std::string prefix) {
  this->name = prefix;
  this->ages.clear();
  this->values.clear();

  // make the following properties optional
  Parser::disable_abort_on_failure();

  char property_string[FRED_STRING_SIZE];

  snprintf(property_string, FRED_STRING_SIZE, "%s.age_groups", prefix.c_str());
  Parser::get_property_vector(property_string, this->ages);

  snprintf(property_string, FRED_STRING_SIZE, "%s.age_values", prefix.c_str());
  Parser::get_property_vector(property_string, this->values);

  // restore requiring properties
  Parser::set_abort_on_failure();

  if(quality_control() != true) {
    Utils::fred_abort("Bad input on age map %s", this->name.c_str());
  }
  return;
}

/**
 * Finds the value of the age range that the specified age falls under. Will return 0.0 if no matching age range is found.
 *
 * @param age the age
 * @return the value
 */
double Age_Map::find_value(double age) {

  for(unsigned int i = 0; i < this->ages.size(); ++i) {
    if(age < this->ages[i]) {
      return this->values[i];
    }
  }
  return 0.0;
}

/**
 * Perform validation on the age map, making sure the age groups are mutually exclusive.
 */
bool Age_Map::quality_control() const {

  // number of groups and number of values must agree
  if(this->ages.size() != this->values.size()) {
    Age_Map::age_map_logger->critical("Age_Map {:s} number of groups = {:u}", this->name.c_str(), 
        this->ages.size(), this->values.size());
    return false;
  }

  if(this->ages.size() > 0) {
    // age groups must have increasing upper bounds
    for(unsigned int i = 0; i < this->ages.size() - 1; ++i) {
      if(this->ages[i] > this->ages[i + 1]) {
        Age_Map::age_map_logger->critical("Age_Map {:s} upper bound {:d} {:f} > upper bound {:d} {:f}", 
            this->name.c_str(), i, this->ages[i], i + 1, this->ages[i + 1]);
        return false;
      }
    }
  }
  return true;
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Age_Map::setup_logging() {
  if(Age_Map::is_log_initialized) {
    return;
  }

  if(Parser::does_property_exist("age_map_log_level")) {
    Parser::get_property("age_map_log_level",  &Age_Map::age_map_log_level);
  } else {
    Age_Map::age_map_log_level = "OFF";
  }

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Age_Map::age_map_logger = std::make_unique<spdlog::logger>("age_map_logger", 
        sink_list.begin(), sink_list.end());
    Age_Map::age_map_logger->set_level(
        Utils::get_log_level_from_string(Age_Map::age_map_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Age_Map::age_map_logger->trace("<{:s}, {:d}>: Age_Map logger initialized", 
      __FILE__, __LINE__  );
  Age_Map::is_log_initialized = true;
}
