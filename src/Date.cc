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
// File: Date.cc
//

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <math.h>

#include <spdlog/spdlog.h>

#include "Date.h"
#include "Global.h"
#include "Parser.h"
#include "Utils.h"


const int Date::day_table[2][13] = {
  {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
  {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}};

const int Date::doomsday_month_val[2][13] = {
  {0, 31, 28, 7, 4, 9, 6, 11, 8, 5, 10, 7, 12},
  {0, 32, 29, 7, 4, 9, 6, 11, 8, 5, 10, 7, 12}};

const std::string Date::day_of_week_string[7] = {
  "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
};

std::map<std::string,int> Date::month_map = {
  {"Jan", 1}, {"1", 1}, {"01", 1}, {"001", 1},
  {"Feb", 2}, {"2", 1}, {"02", 2}, {"002", 2},
  {"Mar", 3}, {"3", 3}, {"03", 3}, {"003", 3},
  {"Apr", 4}, {"4", 4}, {"04", 4}, {"004", 4},
  {"May", 5}, {"5", 5}, {"05", 5}, {"005", 5},
  {"Jun", 6}, {"6", 6}, {"06", 6}, {"006", 6},
  {"Jul", 7}, {"7", 7}, {"07", 7}, {"007", 7},
  {"Aug", 8}, {"8", 8}, {"08", 8}, {"008", 8},
  {"Sep", 9}, {"9", 9}, {"09", 9}, {"009", 9},
  {"Oct", 10}, {"10", 10}, {"010", 10},
  {"Nov", 11}, {"11", 11}, {"011", 11},
  {"Dec", 12}, {"12", 12}, {"012", 12}
};

int Date::year = 0;
int Date::month = 0;
int Date::day_of_month = 0;
int Date::day_of_week = 0;
int Date::day_of_year = 0;
int Date::epi_week = 0;
int Date::epi_year = 0;
int Date::today = 0;
int Date::sim_start_index = 0;
int Date::max_days = 0;
char Date::Start_date[FRED_STRING_SIZE];
char Date::End_date[FRED_STRING_SIZE];
date_t* Date::date = nullptr;

bool Date::is_log_initialized = false;
std::string Date::date_log_level = "";
std::unique_ptr<spdlog::logger> Date::date_logger = nullptr;

/**
 * Default constructor.
 */
Date::Date() {}

/**
 * Checks if a specified year is a leap year.
 *
 * @param year the year
 * @return if the year is a leap year
 */
bool Date::is_leap_year(int year) {
  if(year % 400 == 0) {
    return true;
  } else if(year % 100 == 0) {
    return false;
  } else if(year % 4 == 0) {
    return true;
  } else {
    return false;
  }
}

/**
 * Gets the doomsday century given a year. This is used to calculate a future 
 * day of the week using the Doomsday rule.
 *
 * @param year the year
 * @return the century
 */
int Date::get_doomsday_century(int year) {
  int century = year - (year % 100);
  int r = -1;
  switch(century % 400) {
    case 0:
      r = 2;
      break;
    case 100:
      r = 0;
      break;
    case 200:
      r = 5;
      break;
    case 300:
      r = 3;
      break;
  }
  return r;
}

/**
 * Gets the days in a specified month during a specified year.
 *
 * @param month the month
 * @param year the year
 * @return the days in the month
 */
int Date::get_days_in_month(int month, int year) {
  return Date::day_table[(Date::is_leap_year(year) ? 1 : 0)][month];
}

/**
 * Gets the day of the week given a year, month, and day of the month using the Doomsday Rule. 
 * See https://en.wikipedia.org/wiki/Doomsday_rule".
 *
 * @param year the year
 * @param month the month
 * @param day_of_month the day of the month
 * @result the day of the week
 */
int Date::get_day_of_week(int year, int month, int day_of_month) {
  int x = 0, y = 0;
  int weekday = -1;
  int ddcentury = -1;
  int ddmonth = Date::get_doomsday_month(month, year);
  int century = year - (year % 100);

  ddcentury = Date::get_doomsday_century(year);

  if(ddcentury < 0) {
    return -1;
  }
  if(ddmonth < 0) {
    return -1;
  }
  if(ddmonth > day_of_month) {
    weekday = (7 - ((ddmonth - day_of_month) % 7 ) + ddmonth);
  } else {
    weekday = day_of_month;
  }

  x = (weekday - ddmonth);
  x %= 7;
  y = ddcentury + (year - century) + (floor((year - century) / 4));
  y %= 7;
  weekday = (x + y) % 7;

  return weekday;
}

/**
 * Sets up dates for the simulation.
 */
void Date::setup_dates() {
  char msg[FRED_STRING_SIZE];

  Parser::disable_abort_on_failure();
  strcpy(Date::Start_date, "");
  Parser::get_property("start_date", Date::Start_date);
  strcpy(Date::End_date, "");
  Parser::get_property("end_date", Date::End_date);
  int set_days = 0;
  Parser::get_property("days", &set_days);
  Parser::set_abort_on_failure();


  // extract date from date strings
  int start_year = 0;
  int start_month = 0;
  int start_day_of_month = 0;
  if(sscanf(Date::Start_date, "%d-%d-%d", &start_year, &start_month, &start_day_of_month) != 3) {
    char tmp[8];
    char mon_str[8];
    if(sscanf(Date::Start_date, "%d-%s", &start_year, tmp)==2) {
      strncpy(mon_str,tmp,3);
      mon_str[3] = '\0';
      start_month = Date::month_map[std::string(mon_str)];
      sscanf(&tmp[4], "%d", &start_day_of_month);
    } else {
      snprintf(msg, FRED_STRING_SIZE, "Bad start_date = '%s'", Date::Start_date);
      Utils::print_error(msg);
      return;
    }
  }
  if(start_year < 1900 || start_year > 2200 || start_month < 1 || start_month > 12
     || start_day_of_month < 1 || start_day_of_month > Date::get_days_in_month(start_month, start_year)) {
    snprintf(msg, FRED_STRING_SIZE, "Bad start_date = '%s'", Date::Start_date);
    Utils::print_error(msg);
    return;
  }

  int end_year = 0;
  int end_month = 0;
  int end_day_of_month = 0;
  if(set_days == 0) {
    if(sscanf(Date::End_date, "%d-%d-%d", &end_year, &end_month, &end_day_of_month) != 3) {
      char tmp[8];
      char mon_str[8];
      if(sscanf(Date::End_date, "%d-%s", &end_year, tmp) == 2) {
        strncpy(mon_str, tmp, 3);
        mon_str[3] = '\0';
        end_month = Date::month_map[std::string(mon_str)];
        sscanf(&tmp[4], "%d", &end_day_of_month);
      } else {
        snprintf(msg, FRED_STRING_SIZE, "Bad end_date = '%s'", Date::End_date);
        Utils::print_error(msg);
        return;
      }
    }
    if(end_year < 1900 || end_year > 2200 || end_month < 1 || end_month > 12
       || end_day_of_month < 1 || end_day_of_month > Date::get_days_in_month(end_month, end_year)) {
      snprintf(msg, FRED_STRING_SIZE, "Bad end_date = '%s'", Date::End_date);
      Utils::print_error(msg);
      return;
    }

    int start_date_int = 10000 * start_year + 100 * start_month + start_day_of_month;
    int end_date_int = 10000 * end_year + 100 * end_month + end_day_of_month;
    if(end_date_int < start_date_int) {
      snprintf(msg, FRED_STRING_SIZE, "end_date %s is before start date %s", Date::End_date, Date::Start_date);
      Utils::print_error(msg);
      return;
    }
  } else {
    end_year = start_year + 1 + (set_days / 365);
  }

  int epoch_year = start_year - 120;
  int max_years = end_year - epoch_year + 1;
  Date::max_days = 366*max_years;

  Date::date = new date_t [ Date::max_days ];

  Date::date[0].year = epoch_year;
  Date::date[0].month = 1;
  Date::date[0].day_of_month = 1;
  Date::date[0].day_of_year = 1;

  // assign the right epi week number:
  int jan_1_day_of_week = Date::get_day_of_week(epoch_year, 1, 1);
  Date::date[0].day_of_week = jan_1_day_of_week;
  int dec_31_day_of_week = (jan_1_day_of_week + (Date::is_leap_year(epoch_year) ? 365 : 364)) % 7;
  bool short_week;
  if(jan_1_day_of_week < 3) {
    Date::date[0].epi_week = 1;
    Date::date[0].epi_year = epoch_year;
    short_week = false;
  } else {
    Date::date[0].epi_week = 52;
    Date::date[0].epi_year = epoch_year - 1;
    short_week = true;
  }

  for(int i = 0; i < Date::max_days; ++i) {
    int new_year = Date::date[i].year;
    int new_month = Date::date[i].month;
    int new_day_of_month = Date::date[i].day_of_month + 1;
    int new_day_of_year = Date::date[i].day_of_year + 1;
    int new_day_of_week = (Date::date[i].day_of_week + 1) % 7;
    if(new_day_of_month > Date::get_days_in_month(new_month, new_year)) {
      new_day_of_month = 1;
      if(new_month < 12) {
        ++new_month;
      } else {
        ++new_year;
        new_month = 1;
        new_day_of_year = 1;
      }
    }
    Date::date[i + 1].year = new_year;
    Date::date[i + 1].month = new_month;
    Date::date[i + 1].day_of_month = new_day_of_month;
    Date::date[i + 1].day_of_year = new_day_of_year;
    Date::date[i + 1].day_of_week = new_day_of_week;

    // set epi_week and epi_year
    if(new_month == 1 && new_day_of_month == 1) {
      jan_1_day_of_week = new_day_of_week;
      dec_31_day_of_week = (jan_1_day_of_week + (Date::is_leap_year(new_year) ? 365 : 364)) % 7;
      if(jan_1_day_of_week <= 3) {
        Date::date[i + 1].epi_week = 1;
        Date::date[i + 1].epi_year = new_year;
        short_week = false;
      } else {
        Date::date[i + 1].epi_week = Date::date[i].epi_week;
        Date::date[i + 1].epi_year = Date::date[i].epi_year;
        short_week = true;
      }
    } else {
      if((new_month == 1) && short_week && (new_day_of_month <= 7 - jan_1_day_of_week)) {
        Date::date[i + 1].epi_week = Date::date[i].epi_week;
        Date::date[i + 1].epi_year = Date::date[i].epi_year;
      } else {
        if((new_month == 12) && (dec_31_day_of_week < 3) && (31 - dec_31_day_of_week) <= new_day_of_month) {
          Date::date[i + 1].epi_week = 1;
          Date::date[i + 1].epi_year = new_year + 1;
        } else {
          Date::date[i + 1].epi_week = (short_week ? 0 : 1) + (jan_1_day_of_week + new_day_of_year - 1) / 7;
          Date::date[i + 1].epi_year = new_year;
        }
      }
    }

    // set offset
    if(Date::date[i].year == start_year && Date::date[i].month == start_month && Date::date[i].day_of_month == start_day_of_month) {
      Date::today = i;
    }

    if(set_days == 0 && Date::date[i].year == end_year && Date::date[i].month == end_month && Date::date[i].day_of_month == end_day_of_month) {
      Global::Simulation_Days = i - Date::today + 1;
    }
  }
  if(set_days > 0) {
    Global::Simulation_Days = set_days;
  }

  Date::year = Date::date[Date::today].year;
  Date::month = Date::date[Date::today].month;
  Date::day_of_month = Date::date[Date::today].day_of_month;
  Date::day_of_week = Date::date[Date::today].day_of_week;
  Date::day_of_year = Date::date[Date::today].day_of_year;
  Date::epi_week = Date::date[Date::today].epi_week;
  Date::epi_year = Date::date[Date::today].epi_year;
  Date::sim_start_index = Date::today;
}

/**
 * Gets a simulation day given a year, month and day.
 *
 * @param y the year
 * @param m the month
 * @param d the day
 * @return the numbered simulation day in the range of [0, Global::Simulation_Days)
 */
int Date::get_sim_day(int y, int m, int d) {
  assert(m >= 1 && m <= 12);
  int result = 999999;
  int maxy = Date::date[Date::max_days - 1].year;
  int maxm = Date::date[Date::max_days - 1].month;
  int maxd = Date::date[Date::max_days - 1].day_of_month;

  if((Date::is_leap_year(y) == false) && m == 2 && d == 29) {
    d = 28;
  }

  if(maxy < y) {
    return result;
  }

  if(maxy == y && maxm < m) {
    return result;
  }

  if(maxy == y && maxm == m && maxd < d) {
    return result;
  }

  // binary search
  int low = 0;
  int high = Date::max_days - 1;
  int pos = (high + low) / 2;
  while(Date::date[pos].year != y || Date::date[pos].month != m || Date::date[pos].day_of_month != d) {
    if(Date::date[pos].year < y || (Date::date[pos].year == y
       && (Date::date[pos].month < m || (Date::date[pos].month == m && Date::date[pos].day_of_month < d)))) {
      low = pos;
      pos = (high + low) / 2;
    } else {
      high = pos;
      pos = (high + low) / 2;
    }
  }
  result = pos - Date::sim_start_index;
  return result;
}

/**
 * Gets a simulation day given a date string.
 *
 * @param date_str the date string
 * @return the numbered simulation day in the range of [0, Global::Simulation_Days)
 */
int Date::get_sim_day(std::string date_str) {
  int y, m, d;
  if(date_str.substr(4, 1) == "-") {
    y = strtod(date_str.substr(0, 4).c_str(), nullptr);
    m = Date::get_month_from_name(date_str.substr(5, 3));
    d = strtod(date_str.substr(9).c_str(), nullptr);
    fflush(stdout);
    return Date::get_sim_day(y,m,d);
  }
  if(date_str.substr(3, 1) == "-") {
    m = Date::get_month_from_name(date_str.substr(0, 3));
    d = strtod(date_str.substr(4).c_str(), nullptr);
    y = Date::get_year();
    int today = Date::get_date_code(Date::get_month(), Date::get_day_of_month());
    if(Date::get_date_code(m,d) < today) {
      ++y;
    }
    return Date::get_sim_day(y, m, d);
  }
  return -1;
}

/**
 * Gets the hours until a specified date at a given year, month, day, and hour.
 *
 * @param y the year
 * @param m the month
 * @param d the day
 * @param h the hour
 * @return the hours until
 */
int Date::get_hours_until(int y, int m, int d, int h) {
  int sim_day = Date::get_sim_day(y,m,d);
  int result = -1;
  if(sim_day < Global::Simulation_Day) {
    result = -1;
  } else if(sim_day == Global::Simulation_Day && Global::Simulation_Hour == h) {
    result = -1;
  } else {
    result = 24 * (sim_day - Global::Simulation_Day) + (h - Global::Simulation_Hour);
  }
  return result;
}

/**
 * Gets the hours until the next date at a given month, day, and hour.
 *
 * @param m the month
 * @param d the day
 * @param h the hour
 * @return the hours until
 */
int Date::get_hours_until(int m, int d, int h) {
  int y = Date::year;
  int sim_day = Date::get_sim_day(y, m, d);
  if(sim_day < Global::Simulation_Day) {
    return Date::get_hours_until(y + 1, m, d, h);
  }
  if(sim_day == Global::Simulation_Day && h <= Global::Simulation_Hour) {
    return Date::get_hours_until(y + 1, m, d, h);
  }
  return Date::get_hours_until(y, m, d, h);
}

/**
 * Gets the hours until a specified date and hour.
 *
 * @param date_str the date string
 * @param h the hour
 * @return the hours until
 */
int Date::get_hours_until(std::string date_str, int h) {
  Date::date_logger->debug("get_hours_until {:s} hour {:d}\n", date_str.c_str(), h);
  if(date_str.substr(4, 1)=="-") {
    int y = strtod(date_str.substr(0, 4).c_str(), nullptr);
    int m = 0;
    size_t found = date_str.substr(5, 3).find("-");
    if(found == std::string::npos) {
      m = Date::get_month_from_name(date_str.substr(5, 3));
    } else {
      m = Date::get_month_from_name(date_str.substr(5, 3).substr(0, found));
    }
    int d = strtod(date_str.substr(9).c_str(), nullptr);
    Date::date_logger->debug("get_hours_until |{:d}| |{:d}| |{:d}| hour {:d}\n", y, m, d, h);
    return Date::get_hours_until(y,m,d,h);
  }
  if(date_str.substr(3,1)=="-") {
    int m = Date::get_month_from_name(date_str.substr(0, 3));
    int d = strtod(date_str.substr(4).c_str(), nullptr);
    Date::date_logger->debug("get_hours_until |{:d}| |{:d}| hour {:d}\n", m, d, h);
    return Date::get_hours_until(m, d, h);
  }
  return -1;
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Date::setup_logging() {
  if(Date::is_log_initialized) {
    return;
  }

  if(Parser::does_property_exist("date_log_level")) {
    Parser::get_property("date_log_level", &Date::date_log_level);
  } else {
    Date::date_log_level = "OFF";
  }

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Date::date_logger = std::make_unique<spdlog::logger>("date_logger", 
        sink_list.begin(), sink_list.end());
    Date::date_logger->set_level(
        Utils::get_log_level_from_string(Date::date_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Date::date_logger->trace("<{:s}, {:d}>: Date logger initialized", 
      __FILE__, __LINE__  );
  Date::is_log_initialized = true;
}
