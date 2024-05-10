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
// File: Date.h
//

#ifndef DATE_H_
#define DATE_H_

#include <cstdio>
#include <map>
#include <string>
#include <unordered_map>

#include <spdlog/spdlog.h>

#include "Global.h"

#define MAX_DATES (250 * 366)


struct date_t {
  int year;
  int month;
  int day_of_month;
  int day_of_week;
  int day_of_year;
  int epi_week;
  int epi_year;
};

/**
 * This class contains static methods which track the current date in the simulation, as well as 
 * offer the ability to get a variety of data on both the current date as well as a specified date.
 */
class Date {
 public:
  static const int SUNDAY = 0;
  static const int MONDAY = 1;
  static const int TUESDAY = 2;
  static const int WEDNESDAY = 3;
  static const int THURSDAY = 4;
  static const int FRIDAY = 5;
  static const int SATURDAY = 6;
  static const int JANUARY = 1;
  static const int FEBRUARY = 2;
  static const int MARCH = 3;
  static const int APRIL = 4;
  static const int MAY = 5;
  static const int JUNE = 6;
  static const int JULY = 7;
  static const int AUGUST = 8;
  static const int SEPTEMBER = 9;
  static const int OCTOBER = 10;
  static const int NOVEMBER = 11;
  static const int DECEMBER = 12;
  static const int INVALID = -1;

  static char Start_date[];
  static char End_date[];

  Date();

  /**
   * Default destructor.
   */
  ~Date(){};

  // static void setup_dates(char * date_string); // _UNUSED_
  static void setup_dates();

  /**
   * Increments the current day, along with properties on the current date.
   */
  static void update() {
    ++Date::today;
    Date::year = Date::date[today].year;
    Date::month = Date::date[today].month;
    Date::day_of_month = Date::date[today].day_of_month;
    Date::day_of_week = Date::date[today].day_of_week;
    Date::day_of_year = Date::date[today].day_of_year;
    Date::epi_week = Date::date[today].epi_week;
    Date::epi_year = Date::date[today].epi_year;
  }

  /**
   * Gets the current year of the date.
   *
   * @return the year
   */
  static int get_year() {
    return Date::year;
  }

  /**
   * Gets the year of a specified simulation day.
   *
   * @param sim_day the numbered simulation day in the range of [0, Global::Simulation_Days)
   * @return the year
   */
  static int get_year(int sim_day) {
    return sim_start_index + sim_day < Date::max_days ? Date::date[sim_start_index + sim_day].year : -1;
  }

  /**
   * Gets the current month of the date.
   *
   * @return the month
   */
  static int get_month() {
    return Date::month;
  }

  /**
   * Gets the month of a specified simulation day.
   *
   * @param sim_day the numbered simulation day in the range of [0, Global::Simulation_Days)
   * @return the month
   */
  static int get_month(int sim_day) {
    return sim_start_index + sim_day < Date::max_days ? Date::date[sim_start_index + sim_day].month : -1;
  }

  /**
   * Gets the current day of the month.
   *
   * @return the day of the month
   */
  static int get_day_of_month() {
    return Date::day_of_month;
  }

  /**
   * Gets the day of the month of a specified simulation day.
   *
   * @param sim_day the numbered simulation day in the range of [0, Global::Simulation_Days)
   * @return the day of the month
   */
  static int get_day_of_month(int sim_day) {
    return sim_start_index + sim_day < Date::max_days ? Date::date[sim_start_index + sim_day].day_of_month : -1;
  }

  /**
   * Gets the current day of the week.
   *
   * @return the day of the week
   */
  static int get_day_of_week() {
    return Date::day_of_week;
  }

  /**
   * Gets the day of the week of a specified simulation day.
   *
   * @param sim_day the numbered simulation day in the range of [0, Global::Simulation_Days)
   * @return the day of the week
   */
  static int get_day_of_week(int sim_day) {
    return sim_start_index + sim_day < Date::max_days ? Date::date[sim_start_index + sim_day].day_of_week : -1;
  }

  /**
   * Gets the current day of the year.
   *
   * @return the day of the year
   */
  static int get_day_of_year() {
    return Date::day_of_year;
  }

  /**
   * Gets the day of the year of a specified simulation day.
   *
   * @param sim_day the numbered simulation day in the range of [0, Global::Simulation_Days)
   * @return the day of the year
   */
  static int get_day_of_year(int sim_day) {
    return sim_start_index + sim_day < Date::max_days ? Date::date[sim_start_index + sim_day].day_of_year : -1;
  }

  /**
   * Gets the current date code.
   *
   * @return the date code
   */
  static int get_date_code() {
    return 100 * Date::get_month() + get_day_of_month();
  }

  /**
   * Gets the date code of a specified month and day of the month.
   *
   * @param month the month
   * @param day_of_month the day of the month
   * @return the date code
   */
  static int get_date_code(int month, int day_of_month) {
    return 100 * month + day_of_month;
  }

  /**
   * Gets the date code of a specified month and day of the month.
   *
   * @param month_str the month as a string
   * @param day_of_month the day of the month
   * @return the date code
   */
  static int get_date_code(std::string month_str, int day_of_month) {
    return 100 * Date::get_month_from_name(month_str) + day_of_month;
  }

  /**
   * Gets the date code of a specified date.
   *
   * @param date_str the date as a string
   * @return the date code
   */
  static int get_date_code(std::string date_str) {
    std::string month_str = date_str.substr(0,3);
    std::string day_string = date_str.substr(4);
    int day = -1;
    day = static_cast<int>(strtod(day_string.c_str(), nullptr));
    if(day < 0) {
      return -1;
    } else {
      return 100 * Date::get_month_from_name(month_str) + day;
    }
  }

  /**
   * Gets the current EPI week.
   *
   * @return the EPI week
   */
  static int get_epi_week() {
    return Date::epi_week;
  }

  /**
   * Gets the EPI week of a specified simulation day.
   *
   * @param sim_day the numbered simulation day in the range of [0, Global::Simulation_Days)
   * @return the EPI week
   */
  static int get_epi_week(int sim_day) {
    return sim_start_index + sim_day < Date::max_days ? Date::date[sim_start_index + sim_day].epi_week : -1;
  }

  /**
   * Gets the current EPI year.
   *
   * @return the EPI year
   */
  static int get_epi_year() {
    return Date::epi_year;
  }

  /**
   * Gets the EPI year of a specified simulation day.
   *
   * @param sim_day the numbered simulation day in the range of [0, Global::Simulation_Days)
   * @return the EPI year
   */
  static int get_epi_year(int sim_day) {
    return sim_start_index + sim_day < Date::max_days ? Date::date[sim_start_index + sim_day].epi_year : -1;
  }

  /**
   * Checks if the current day is on a weekend.
   *
   * @return if the current day is on a weekend
   */
  static bool is_weekend() {
    int day = Date::get_day_of_week();
    return (day == Date::SATURDAY || day == Date::SUNDAY);
  }

  /**
   * Checks if a specified simulation day is on a weekend.
   *
   * @param sim_day the numbered simulation day in the range of [0, Global::Simulation_Days)
   * @return if the day is on a weekend
   */
  static bool is_weekend(int sim_day) {
    int day = Date::get_day_of_week(sim_day);
    return (day == Date::SATURDAY || day == Date::SUNDAY);
  }

  /**
   * Checks if the current day is a weekday.
   *
   * @return if the current day is a weekday
   */
  static bool is_weekday() {
    return (is_weekend() == false);
  }

  /**
   * Checks if a specified simulation day is a weekday.
   *
   * @param sim_day the numbered simulation day in the range of [0, Global::Simulation_Days)
   * @return if the day is a weekday
   */
  static bool is_weekday(int sim_day) {
    return (Date::is_weekend(sim_day) == false);
  }

  /**
   * Checks if the current year is a leap year.
   *
   * @return if the current year is a leap year
   */
  static bool is_leap_year() {
    return Date::is_leap_year(Date::year);
  }

  static bool is_leap_year(int year);

  /**
   * Gets the date as a string for the current date.
   *
   * @return the current date string
   */
  static std::string get_date_string() {
    char str[12];
    snprintf(str, 12, "%04d-%02d-%02d", Date::year, Date::month, Date::day_of_month);
    std::string cppstr(str);
    return cppstr;
  }

  /**
   * Gets the date as a string for a specified simulation day.
   *
   * @param sim_day the numbered simulation day in the range of [0, Global::Simulation_Days)
   * @return the date string
   */
  static std::string get_date_string(int sim_day) {
    char str[12];
    snprintf(str, 12, "%04d-%02d-%02d", Date::get_year(sim_day), Date::get_month(sim_day), Date::get_day_of_month(sim_day));
    std::string cppstr(str);
    return cppstr;
  }

  /**
   * Gets the current day of the week as a string.
   *
   * @return the day of the week string
   */
  static std::string get_day_of_week_string() {
    return day_of_week_string[Date::get_day_of_week()];
  }

  static int get_sim_day(int y, int m, int d);
  static int get_sim_day(std::string date_str);

  /**
   * Gets the month as an integer from a string.
   *
   * @param name the name of the month
   * @return the month
   */
  static int get_month_from_name(std::string name) {
    auto search = Date::month_map.find(name);
    if(search != Date::month_map.end()) {
      return search->second;
    }
    return 0;
  }

  /**
   * Gets the current hour in the format of a 12 hour clock. 
   * Example outputs: 7am, 12am, 12pm.
   *
   * @return the current time
   */
  static std::string get_12hr_clock() {
    return Date::get_12hr_clock(Global::Simulation_Hour);
  }

  /**
   * Gets the time in the format of a 12 hour clock given an hour.
   *
   * @param hour the hour
   * @return the time
   */
  static std::string get_12hr_clock(int hour) {
    char oclock[5];
    hour = hour % 24;
    if(hour == 0) {
      snprintf(oclock, 5, "12am");
    } else if(hour == 12) {
      snprintf(oclock, 5, "12pm");
    } else if(hour < 12) {
      snprintf(oclock, 5, "%dam", hour);
    } else {
      snprintf(oclock, 5, "%dpm", hour - 12);
    }
    return std::string(oclock);
  }

  static int get_hours_until(int y, int m, int d, int h);
  static int get_hours_until(int m, int d, int h);
  static int get_hours_until(std::string date_str, int h);
  static void setup_logging();

private:
  static int year;
  static int month;
  static int day_of_month;
  static int day_of_week;
  static int day_of_year;
  static int epi_week;
  static int epi_year;
  static int today;	// index of Global::Simulation_Day in date array
  static int sim_start_index; // index of Global::Simulation_Day=0 in date array
  static int max_days;
  static date_t * date;
  static std::map<std::string, int> month_map;

  // names of days of week
  static const std::string day_of_week_string[7];

  // static const int EPOCH_START_YEAR = 1700;
  static const int day_table[2][13];
  static const int doomsday_month_val[2][13];

  /**
   * Gets the doomsday month given a month and year. This is used to calculate a future 
   * day of the week using the Doomsday rule.
   *
   * @param month the month
   * @param year the year
   * @return the doomsday month
   */
  static int get_doomsday_month(int month, int year) {
    return Date::doomsday_month_val[(Date::is_leap_year(year) ? 1 : 0)][month];
  }

  static int get_doomsday_century(int year);
  static int get_day_of_week(int year, int month, int day_of_month);
  static int get_days_in_month(int month, int year);

  static bool is_log_initialized;
  static std::string date_log_level;
  static std::unique_ptr<spdlog::logger> date_logger;
};

#endif /* DATE_H_ */
