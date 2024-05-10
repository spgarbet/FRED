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
// File: Travel.h
//

#ifndef _FRED_TRAVEL_H
#define _FRED_TRAVEL_H

#include <spdlog/spdlog.h>

typedef std::vector <Person*> pvec;		// vector of person ptrs

// travel hub record:
typedef struct hub_record {
  int id;
  double lat;
  double lon;
  pvec users;
  int pop;
  int pct;
} hub_t;


class Events;
class Person;
class Age_Map;

/**
 * This class models travel between agents in the simulation.
 *
 * The Travel class includes functionality relating to travel throughout the simulation 
 * region in order to accurately model real life. Predefined data relating to travel 
 * hubs and volume is read into the simulation, and the Event class is utilized to 
 * create travel events.
 */
class Travel {
public:
  static void get_properties();
  static void setup(char* directory);
  static void read_hub_file();
  static void read_trips_per_day_file();
  static void setup_travelers_per_hub();
  static void setup_travel_lists(); // _UNUSED_
  static void update_travel(int day);
  static void find_returning_travelers(int day);
  static void old_find_returning_travelers(int day); // _UNUSED_
  static void quality_control(char* directory);
  static void terminate_person(Person* per);
  static void add_return_event(int day, Person* person);
  static void delete_return_event(int day, Person* person);
  static void setup_logging();

private:
  static Events * return_queue;

  static bool is_log_initialized;
  static std::string travel_log_level;
  static std::unique_ptr<spdlog::logger> travel_logger;
};

#endif // _FRED_TRAVEL_H
