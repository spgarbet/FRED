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
// File: Admin_Division.h
//

#ifndef _FRED_ADMIN_DIVISION_H
#define _FRED_ADMIN_DIVISION_H

#include <vector>
#include <unordered_map>

class Place;

/**
 * An admin division represents a division in the simulation of varying complexity.
 *
 * An admin division is identified by an admin code, and can contain households as well as 
 * track population sizes within the division. A division can be broken down into subdivisions, 
 * and can, in turn, have a higher division.
 *
 * This class is inherited by Block_Group, Census_Tract, County, and State.
 */
class Admin_Division {
public:

  Admin_Division(long long int _admin_code);
  virtual ~Admin_Division();

  virtual void setup();

  virtual void add_household(Place* h);

  /**
   * Sets the higher level division.
   *
   * @param high the higher division
   */
  void set_higher_division(Admin_Division* high) {
    this->higher = high;
  }

  /**
   * Gets the higher level divison.
   *
   * @return the higher division
   */
  Admin_Division* get_higher_division() {
    return this->higher;
  }

  /**
   * Adds the specified subdivision to the subdivisions vector.
   *
   * @param sub the subdvision
   */
  virtual void add_subdivision(Admin_Division* sub) {
    this->subdivisions.push_back(sub);
  }

  /**
   * Gets the number of subdivisions in the subdivisions vector.
   *
   * @return the number of subdivisions
   */
  virtual int get_number_of_subdivisions() {
    return this->subdivisions.size();;
  }

  /**
   * Gets the subdivision at the specified index in the subdivisions vector.
   *
   * @param i the index
   * @return the subdvision
   */
  Admin_Division* get_subdivision(int i) {
    return this->subdivisions[i];
  }

  /**
   * Gets the admin code of this admin division.
   *
   * @return the admin code
   */
  long long int get_admin_division_code() {
    return this->admin_code;
  }

  /**
   * Gets the number of Household objects in the households vector.
   *
   * @return the number of households
   */
  int get_number_of_households() {
    return static_cast<int>(this->households.size());
  }

  Place* get_household(int i);

  int get_population_size();

protected:
  long long int admin_code;

  // pointers to households
  std::vector<Place*> households;

  // pointer to higher level division
  Admin_Division* higher;

  // vector of subdivisions
  std::vector<Admin_Division*> subdivisions;

};

#endif // _FRED_ADMIN_DIVISION_H
