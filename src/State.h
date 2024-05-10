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
// File: State.h
//


#ifndef _FRED_STATE_H
#define _FRED_STATE_H

#include <vector>
#include <unordered_map>

#include "Admin_Division.h"

/**
 * This class represents a state division, which is a higher division of a County.
 *
 * States exist in the FRED simulation in order to accurately model the real world. 
 * Their functionality is lacking in comparison to other divisions, but they allow 
 * the logging of state specific data.
 *
 * This class inherits from Admin_Division.
 */
class State : public Admin_Division {
public:

  State(long long int _admin_code);

  ~State();

  /**
   * Gets the number of State objects in the static states vector.
   *
   * @return the number of states
   */
  static int get_number_of_states() {
    return State::states.size();
  }

  /**
   * Gets the State at the specified index in the static states vector.
   *
   * @param i the index
   * @return the state
   */
  static State* get_state_with_index(int i) {
    return State::states[i];
  }

  static State* get_state_with_admin_code(long long int state_code);

private:

  // static variables
  static std::vector<State*> states;
  static std::unordered_map<long long int, State*> lookup_map;

};

#endif // _FRED_STATE_H
