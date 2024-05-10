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
// File: State.cc
//

#include "State.h"
#include "Census_Tract.h"

/**
 * Default constructor.
 */
State::~State() {
}

/**
 * Creates a State. Passes the given admin code into the Admin_Division constructor.
 *
 * @param admin_code the admin code
 */
State::State(long long int admin_code) : Admin_Division(admin_code) {
}

//////////////////////////////
//
// STATIC METHODS
//
//////////////////////////////


// static variables
std::vector<State*> State::states;
std::unordered_map<long long int, State*> State::lookup_map;

/**
 * Gets the State in the lookup map with the specified admin code. If there is no state found, creates a state with 
 * that admin code and adds it to the lookup map.
 *
 * @param state_admin_code the state admin code
 * @return the state
 */
State* State::get_state_with_admin_code(long long int state_admin_code) {
  State* state = nullptr;
  std::unordered_map<long long int, State*>::iterator itr;
  itr = State::lookup_map.find(state_admin_code);
  if (itr == State::lookup_map.end()) {
    state = new State(state_admin_code);
    State::states.push_back(state);
    State::lookup_map[state_admin_code] = state;
  } else {
    state = itr->second;
  }
  return state;
}
