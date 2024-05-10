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
// File: Admin_Division.cc
//

#include "Admin_Division.h"
#include "Place.h"

/**
 * Default destructor
 */
Admin_Division::~Admin_Division() {
}

/**
 * Creates an Admin_Division with the specified admin code.
 *
 * @param _admin_code the admin code
 */
Admin_Division::Admin_Division(long long int _admin_code) {
  this->admin_code = _admin_code;
  this->households.clear();
  this->subdivisions.clear();
  this->higher = nullptr;
}

/**
 * _UNUSED_
 */
void Admin_Division::setup() {
}

/**
 * Adds the specified Household to the households vector of this division and all 
 * higher level divisions.
 *
 * @param hh the household
 */
void Admin_Division::add_household(Place* hh) {
  this->households.push_back(hh);
  if(this->higher != nullptr) {
    this->higher->add_household(hh);
  }
}

/**
 * Gets the Household at the specified index in the households vector.
 *
 * @param i the index
 * @return the household
 */
Place* Admin_Division::get_household(int i) {
  assert(0 <= i && i < static_cast<int>(this->households.size()));
  return this->households[i];
}

/**
 * Gets the total population size by adding the population of each Household in the households vector.
 *
 * @return the total population size
 */
int Admin_Division::get_population_size() {
  int size = this->households.size();
  int popsize = 0;
  for(int i = 0; i < size; ++i) {
    popsize += this->households[i]->get_size();
  }
  return popsize;
}
