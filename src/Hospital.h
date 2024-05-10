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
// File: Hospital.h
//

#ifndef _FRED_HOSPITAL_H
#define _FRED_HOSPITAL_H

#include "Place.h"
#include "Person.h"

/**
 * This class represents a hospital location.
 *
 * A Hospital contains specific information related to health insurace, patients, employees, 
 * physicians, etc.
 *
 * This class inherits from Place.
 */
class Hospital : public Place {

public: 
  
  Hospital();
  Hospital(const char* lab, char _subtype, fred::geo lon, fred::geo lat);

  /**
   * Default destructor.
   */
  ~Hospital() { }

  static void get_properties();

  bool is_open(int sim_day);
  bool should_be_open(int sim_day);
  bool should_be_open(int sim_day, int condition);

  int get_bed_count(int sim_day);

  /**
   * Sets the bed count.
   *
   * @param _bed_count the new bed count
   */
  void set_bed_count(int _bed_count) {
    this->bed_count = _bed_count;
  }

  /**
   * Gets the employee count.
   *
   * @return the employee count
   */
  int get_employee_count() {
    return this->employee_count;
  }

  /**
   * Sets the employee count.
   *
   * @param _employee_count the new employee count
   */
  void set_employee_count(int _employee_count) {
    this->employee_count = _employee_count;
  }

  /**
   * Gets the physician count.
   *
   * @return the physician count
   */
  int get_physician_count() {
    return this->physician_count;
  }

  /**
   * Sets the physician count.
   *
   * @param _physician_count the new physician count
   */
  void set_physician_count(int _physician_count) {
    this->physician_count = _physician_count;
  }

  int get_daily_patient_capacity(int sim_day);

  /**
   * Sets the daily patient capacity.
   *
   * @param _capacity the new capacity
   */
  void set_daily_patient_capacity(int _capacity) {
    this->daily_patient_capacity = _capacity;
  }

  /**
   * Gets the current daily patient count.
   *
   * @return the count
   */
  int get_current_daily_patient_count() {
    return this->current_daily_patient_count;
  }

  /**
   * Increments the current daily patient count.
   */
  void increment_current_daily_patient_count() {
    this->current_daily_patient_count++;
  }

  /**
   * Resets the current daily patient count.
   */
  void reset_current_daily_patient_count() {
    this->current_daily_patient_count = 0;
  }

  /**
   * Gets the occupied bed count.
   */
  int get_occupied_bed_count() {
    return this->occupied_bed_count;
  }

  /**
   * Increments the occupied bed count.
   */
  void increment_occupied_bed_count() {
    this->occupied_bed_count++;
  }

  /**
   * Decrements the occupied bed count.
   */
  void decrement_occupied_bed_count() {
    this->occupied_bed_count--;
  }

  /**
   * Resets the occupied bed count.
   */
  void reset_occupied_bed_count() {
    this->occupied_bed_count = 0;
  }

  std::string to_string();


private:
  static std::vector<double> Hospital_health_insurance_prob;

  int bed_count;
  int occupied_bed_count;
  int daily_patient_capacity;
  int current_daily_patient_count;

  int employee_count;
  int physician_count;

  bool add_capacity;
  int open_day;
  int close_day;

};

#endif // _FRED_HOSPITAL_H
