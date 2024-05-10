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
// File: Hospital.cc
//

#include <math.h>
#include <string>
#include <sstream>

#include "Condition.h"
#include "Global.h"
#include "Hospital.h"
#include "Parser.h"
#include "Person.h"
#include "Random.h"

//Private static variables that will be set by property lookups
std::vector<double> Hospital::Hospital_health_insurance_prob;

/**
 * Creates a Hospital using the Place constructor. Sets this place's Place_Type as a hospital, and 
 * initializes default properties.
 */
Hospital::Hospital() : Place() {
  this->type_id = Place_Type::get_type_id("Hospital");
  this->set_subtype(Place::SUBTYPE_NONE);
  this->bed_count = 0;
  this->occupied_bed_count = 0;
  this->physician_count = 0;
  this->employee_count = 0;
  this->daily_patient_capacity = -1;
  this->current_daily_patient_count = 0;
  this->add_capacity = false;
  this->open_day = -1;
  this->close_day = Global::Simulation_Days;
}

/**
 * Creates a Hosptial with the given properties. Passes properties into the Place constructor, setting this 
 * place's Place_Type as a hospital. Initializes default properties.
 *
 * @param lab the label
 * @param _subtype the subtype
 * @param lon the longitude
 * @param lat the latitude
 */
Hospital::Hospital(const char* lab, char _subtype, fred::geo lon, fred::geo lat) : Place(lab, Place_Type::get_type_id("Hospital"), lon, lat) {
  this->set_subtype(_subtype);
  this->bed_count = 0;
  this->occupied_bed_count = 0;
  this->physician_count = 0;
  this->employee_count = 0;
  this->daily_patient_capacity = -1;
  this->current_daily_patient_count = 0;
  this->add_capacity = false;
  this->open_day = -1;
  this->close_day = Global::Simulation_Days;
}

/**
 * Gets the properties of this hospital.
 */
void Hospital::get_properties() {
  return;
}

/**
 * Gets the bed count of this hospital given a day.
 *
 * @param sim_day the day
 * @return the bed count
 */
int Hospital::get_bed_count(int sim_day) {
  return this->bed_count;
}

/**
 * Gets the daily patient capacity of this hospital given a day.
 *
 * @param sim_day the day
 * @return the capacity
 */
int Hospital::get_daily_patient_capacity(int sim_day) {
  return this->daily_patient_capacity;
}

/**
 * Checks if this hospital is open on the specified day.
 *
 * @param sim_day the day
 * @return if this hospital is open
 */
bool Hospital::is_open(int sim_day) {
  bool open = (sim_day < this->close_day && this->open_day <= sim_day);
  return open;
}

/**
 * Checks if this hospital should be open on the specific day.
 *
 * @param sim_day the day
 */
bool Hospital::should_be_open(int sim_day) {
  return is_open(sim_day);
}

/**
 * @param condition _UNUSED_
 */
bool Hospital::should_be_open(int sim_day, int condition) {
  return is_open(sim_day);
}

/**
 * Converts the hospital to a string representation.
 */
std::string Hospital::to_string() {
  std::stringstream ss;
  ss << "Hospital[" << this->get_label() << "]: bed_count: " << this->bed_count
     << ", occupied_bed_count: " << this->occupied_bed_count
     << ", daily_patient_capacity: " << this->daily_patient_capacity
     << ", current_daily_patient_count: " << this->current_daily_patient_count
     << ", add_capacity: " << this->add_capacity
     << ", subtype: " << this->get_subtype();

  return ss.str();
}
