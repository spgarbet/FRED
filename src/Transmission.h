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
// File: Transmission.h
//

#ifndef _FRED_TRANSMISSION_H
#define _FRED_TRANSMISSION_H

#include <spdlog/spdlog.h>

class Condition;
class Group;
class Person;
class Place;

/**
 * This class represents a transmission of a condition from one simulation agent to another.
 *
 * A Transmission includes a transmission method which preforms a transmission in a specific way. 
 * The different subclasses of Transmission all preform different types of transmissions.
 *
 * This class is inherited by Environmental_Transmission, Network_Transmission, Null_Transmission, 
 * and Proximity_Transmission.
 */
class Transmission {

public:
  
  /**
   * Default destructor.
   */
  virtual ~Transmission() {}
  static Transmission* get_new_transmission(char* transmission_mode);

  virtual void setup(Condition* condition) = 0;
  virtual void transmission(int day, int hour, int condition_id, Group* group, int time_block) = 0;
  bool attempt_transmission(double transmission_prob, Person* source, Person* host, int condition_id, int condition_to_transmit, int day, int hour, Group* group);
  
  static void setup_logging();

protected:

private:
  static bool is_log_initialized;
  static std::string transmission_log_level;
  static std::unique_ptr<spdlog::logger> transmission_logger;
};

/**
 * This Class is Null version of a Transmission subclass. All of  its methods do nothing.
 */
class Null_Transmission : public Transmission {
public:
  Null_Transmission() {};
  ~Null_Transmission() {};
  void setup(Condition* condition) {};
  void transmission(int day, int hour, int condition_id, Group* group, int time_block) {};
};


#endif // _FRED_TRANSMISSION_H
