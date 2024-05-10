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

#include <spdlog/spdlog.h>

#include "State_Space.h"
#include "Parser.h"
#include "Random.h"
#include "Utils.h"

bool State_Space::is_log_initialized = false;
std::string State_Space::state_space_log_level = "";
std::unique_ptr<spdlog::logger> State_Space::state_space_logger = nullptr;

/**
 * Creates a State_Space model with the specified model name. Initializes default 
 * variables.
 *
 * @param model_name the name
 */
State_Space::State_Space(char* model_name) {
  strcpy(this->name, model_name);
  this->number_of_states = -1;
  this->state_name.clear();
}

/**
 * Default destructor.
 */
State_Space::~State_Space() {
}

/**
 * Gets properties for the space state. Reads in the list of state names.
 */
void State_Space::get_properties() {

  State_Space::state_space_logger->info("State_Space({:s})::get_properties", this->name);

  // read optional properties
  Parser::disable_abort_on_failure();
  
  // read in the list of state names
  this->state_name.clear();
  char property_name[FRED_STRING_SIZE];
  char property_value[FRED_STRING_SIZE];
  strcpy(property_value, "");
  snprintf(property_name, FRED_STRING_SIZE, "%s.%s", this->name, "states");
  Parser::get_property(property_name, property_value);

  this->number_of_states = 0;
  string_vector_t parts = Utils::get_string_vector(property_value, ' ');

  this->state_name.push_back("Start");
  for(int i = 0; i < static_cast<int>(parts.size()); ++i) {
    if(parts[i] != "Start" && parts[i] != "Excluded") {
      int new_state = 1;
      for(int j = 0; j < i; ++j) {
        if(parts[i] == this->state_name[j]) {
          char msg[FRED_STRING_SIZE];
          snprintf(msg, FRED_STRING_SIZE, "Duplicate state %s found in Condition %s", parts[i].c_str(), this->name);
          Utils::print_warning(msg);
          new_state = 0;
        }
      }
      if(new_state) {
        this->state_name.push_back(parts[i]);
      }
    }
  }
  this->state_name.push_back("Excluded");
  this->number_of_states = this->state_name.size();
  
  State_Space::state_space_logger->info("state space {:s} number of states = {:d}", 
      this->name, this->number_of_states);
  
  // restore requiring properties
  Parser::set_abort_on_failure();

}

/**
 * Gets the condition state index from the specified state name.
 *
 * @param sname the state name
 * @return the condition state
 */
int State_Space::get_state_from_name(std::string sname) {
  for(int i = 0; i < this->number_of_states; ++i) {
    if(sname == this->state_name[i]) {
      return i;
    }
  }
  return -1;
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void State_Space::setup_logging() {
  if(State_Space::is_log_initialized) {
    return;
  }
  
  if(Parser::does_property_exist("state_space_log_level")) {
    Parser::get_property("state_space_log_level", &State_Space::state_space_log_level);
  } else {
    State_Space::state_space_log_level = "OFF";
  }

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    State_Space::state_space_logger = std::make_unique<spdlog::logger>("state_space_logger", 
        sink_list.begin(), sink_list.end());
    State_Space::state_space_logger->set_level(
        Utils::get_log_level_from_string(State_Space::state_space_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  State_Space::state_space_logger->trace("<{:s}, {:d}>: State_Space logger initialized", 
      __FILE__, __LINE__  );
  State_Space::is_log_initialized = true;
}
