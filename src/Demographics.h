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
// File: Demographics.h
//

#ifndef _FRED_DEMOGRAPHICS_H
#define _FRED_DEMOGRAPHICS_H

#include <map>
#include <vector>

#include <spdlog/spdlog.h>

#include "Global.h"

class Person;

/**
 * This class contains static methods which track current statistics on the demographics of the 
 * simulation population.
 */
class Demographics {
 public:

  /**
   * Default constructor.
   */
  Demographics() {}

  /**
   * Default destructor.
   */
  ~Demographics() {}

  static const int MAX_AGE = 110;
  static const int MIN_PREGNANCY_AGE = 12;
  static const int MAX_PREGNANCY_AGE = 60;
  static const double MEAN_PREG_DAYS;
  static const double STDDEV_PREG_DAYS;

  static void initialize_static_variables();
  static void update(int day);
  static void report(int day);
  static int find_admin_code(int n);

  /**
   * Gets the amount of births that occurred in the current year.
   *
   * @return the births year-to-date
   */
  static int get_births_ytd() {
    return Demographics::births_ytd;
  }

  /**
   * Gets the total number of births that occurred.
   *
   * @return the total births
   */
  static int get_total_births() {
    return Demographics::total_births;
  }

  /**
   * Gets the amount of deaths that occurred today.
   *
   * @return the deaths today
   */
  static int get_deaths_today() {
    return Demographics::deaths_today;
  }

  /**
   * Gets the amount of deaths that occured in the current year.
   *
   * @return the deaths year-to-date
   */
  static int get_deaths_ytd() {
    return Demographics::deaths_ytd;
  }

  /**
   * Gets the total number of deaths that occurred.
   *
   * @return the total deaths
   */
  static int get_total_deaths() {
    return Demographics::total_deaths;
  }

  static void terminate(Person* self);
  static void setup_logging();

private:
  static int births_today;
  static int births_ytd;
  static int total_births;
  static int deaths_today;
  static int deaths_ytd;
  static int total_deaths;
  static std::vector<int> admin_codes;

  static bool is_log_initialized;
  static std::string demographics_log_level;
  static std::unique_ptr<spdlog::logger> demographics_logger;
};

#endif // _FRED_DEMOGRAPHICS_H
