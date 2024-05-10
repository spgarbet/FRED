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

#ifndef _FRED_STATE_SPACE_H
#define _FRED_STATE_SPACE_H

#include <string>
#include <vector>

#include <spdlog/spdlog.h>

/**
 * This class models data relating to condition states.
 *
 * A State_Space model is used in a Natural_History to model the differing condition states 
 * for that natural history's Condition. It contains functionality for managing the different 
 * condition states.
 */
class State_Space {

public:
  State_Space(char* model_name);

  ~State_Space();

  void get_properties();

  /**
   * Gets the name of this state space.
   *
   * @return the name
   */
  char* get_name() {
    return this->name;
  }

  /**
   * Gets the number of condition states in this state space.
   *
   * @return the number of states
   */
  int get_number_of_states() {
    return this->number_of_states;
  }

  /**
   * Gets the name of the specified condition state.
   *
   * @param state the condition state
   * @return the state name
   */
  std::string get_state_name(int state) {
    if(state < this->number_of_states) {
      return state_name[state];
    } else {
      return "";
    }
  }

  int get_state_from_name(std::string sname);
  
  static void setup_logging();

private:
  char name[256];
  int number_of_states;
  std::vector<std::string> state_name;

  static bool is_log_initialized;
  static std::string state_space_log_level;
  static std::unique_ptr<spdlog::logger> state_space_logger;
};

#endif
