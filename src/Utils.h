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
// File: Utils.h
//

#ifndef UTILS_H_
#define UTILS_H_

#include <stack>

#include <spdlog/spdlog.h>

#include "Global.h"

// Conditional includes
#ifdef LINUX
  #include <stdlib.h>
  #include <stdio.h>
  #include <string.h>
#elif OSX
  #include <mach/mach.h>
#elif WIN32
  // Not implemented
#endif



////// LOGGING MACROS
////// gcc recognizes a signature without variadic args: (verbosity, format) as well as
////// with: (vebosity, format, arg_1, arg_2, ... arg_n).  Other preprocessors may not.
////// To ensure compatibility, always provide at least one varg (which may be an empty string,
////// eg: (vebosity, format, "")

// FRED_VERBOSE prints to the stdout using Utils::fred_verbose
#ifdef FREDVERBOSE
#define FRED_VERBOSE(verbosity, format, ...) {    \
    if(Global::Verbose > verbosity) {    \
      Utils::fred_verbose(verbosity, "FRED_VERBOSE: <%s, LINE:%d> " format, __FILE__, __LINE__, ## __VA_ARGS__);    \
    }    \
  }

#else
#define FRED_VERBOSE(verbosity, format, ...){}\

#endif

// FRED_STATUS prints to Global::Statusfp using Utils::fred_status
// If Global::Verbose == 0, then abbreviated output is produced
#ifdef FREDSTATUS
#define FRED_STATUS(verbosity, format, ...){    \
    if(verbosity == 0 && Global::Verbose <= 1) {    \
      Utils::fred_status(verbosity, format, ## __VA_ARGS__);    \
    } else if(Global::Verbose > verbosity) {    \
      Utils::fred_status(verbosity, "FRED_STATUS: <%s, LINE:%d> " format, __FILE__, __LINE__, ## __VA_ARGS__);     \
    }    \
  }

#else
#define FRED_STATUS(verbosity, format, ...){}\

#endif

// FRED_DEBUG prints to Global::Statusfp using Utils::fred_status
#ifdef FREDDEBUG
#define FRED_DEBUG(verbosity, format, ...){    \
    if(Global::Debug >= verbosity) {    \
      Utils::fred_status(verbosity, "FRED_DEBUG: <%s, LINE:%d> " format, __FILE__, __LINE__, ## __VA_ARGS__);    \
    }    \
  }

#else
#define FRED_DEBUG(verbosity, format, ...){}    \

#endif

// FRED_WARNING prints to both stdout and the Global::ErrorLogfp using Utils::fred_warning
#ifdef FREDWARNING
#define FRED_WARNING(format, ...){    \
    Utils::fred_warning("<%s, LINE:%d> " format, __FILE__, __LINE__, ## __VA_ARGS__);    \
  }

#else
#define FRED_WARNING(format, ...){}    \

#endif

namespace Utils {

//  namespace {
//    char error_filename[FRED_STRING_SIZE];
//    bool logfile_sinks_initialized = false;
//    std::chrono::high_resolution_clock::time_point start_timer;
//    std::chrono::high_resolution_clock::time_point fred_timer;
//    std::chrono::high_resolution_clock::time_point day_timer;
//    std::chrono::high_resolution_clock::time_point initialization_timer;
//    std::chrono::high_resolution_clock::time_point update_timer;
//    std::chrono::high_resolution_clock::time_point epidemic_timer;
//
//    std::map<std::string, spdlog::level::level_enum> log_level_map = boost::assign::map_list_of("TRACE", spdlog::level::trace)
//        ("DEBUG", spdlog::level::debug)
//        ("INFO", spdlog::level::info)
//        ("WARN", spdlog::level::warn)("WARNING", spdlog::level::warn)
//        ("ERR", spdlog::level::err)("ERROR", spdlog::level::err)
//        ("CRITICAL", spdlog::level::critical)("FATAL", spdlog::level::critical)
//        ("OFF", spdlog::level::off);
//
//    int parse_line(char* line);
//  }

  bool compare_id (Person* p1, Person* p2);
  std::string delete_spaces(std::string &str);
  string_vector_t get_string_vector(std::string str, char delim);
  string_vector_t get_top_level_parse(std::string str, char delim);
  bool is_number(std::string s);
  spdlog::level::level_enum get_log_level_from_string(std::string str);
  spdlog::level::level_enum get_log_level_from_string(const char* c_str);
  void fred_abort(const char* format, ...);
  void fred_warning(const char* format, ...);
  void fred_open_output_files();
  void fred_initialize_logging();
  void fred_make_directory(char* directory);
  void fred_end();
  void fred_print_wall_time(const char* format, ...);
  void fred_start_timer();
  void fred_start_timer(std::chrono::high_resolution_clock::time_point* lap_start_time);
  void fred_start_day_timer();
  void fred_print_day_timer(int day);
  void fred_start_initialization_timer();
  void fred_print_initialization_timer();
  void fred_start_epidemic_timer();
  void fred_print_epidemic_timer(std::string  msg);
  void fred_print_finish_timer();
  void fred_print_update_time(const char* format, ...);
  void fred_print_lap_time(const char* format, ...);
  void fred_print_lap_time(std::chrono::high_resolution_clock::time_point* start_lap_time, const char* format, ...);
  void fred_verbose(int verbosity, const char* format, ...);
  void fred_status(int verbosity, const char* format, ...);
  void fred_log(const char* format, ...);
  FILE *fred_open_file(char* filename);
  FILE *fred_write_file(char* filename);
  void get_fred_file_name(char* filename);
  void fred_print_resource_usage(int day);
  double get_daily_probability(double prob, int days);
  std::string str_tolower(std::string s);
  bool does_path_exist(const std::string &s);
  void print_error(const std::string &msg);
  void print_warning(const std::string &msg);
  double get_fred_phys_mem_usg_in_gb();
}

#endif /* UTILS_H_ */
