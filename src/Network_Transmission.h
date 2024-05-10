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
// File: Network_Transmission.h
//

#ifndef _FRED_NETWORK_TRANSMISSION_H
#define _FRED_NETWORK_TRANSMISSION_H

#include <spdlog/spdlog.h>

#include "Transmission.h"

class Condition;
class Group;
class Person;
class Network;

/**
 * This class represents a transmission through a network.
 *
 * Network_Transmission exists solely to preform the transmission method. See 
 * that method for more detail.
 *
 * This class inherits from Transmission.
 */
class Network_Transmission: public Transmission {


public:
  
  Network_Transmission();
  
  /**
   * Default destructor.
   */
  ~Network_Transmission() {}

  /**
   * _UNUSED_
   */
  void setup(Condition* condition) {}

  void transmission(int day, int hour, int condition_id, Group* group, int time_block);
  static void setup_logging();
  
private:
  static bool is_log_initialized;
  static std::string network_transmission_log_level;
  static std::unique_ptr<spdlog::logger> network_transmission_logger;
};

#endif // _FRED_NETWORK_TRANSMISSION_H
