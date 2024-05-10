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
// File: Utils.cc
//

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/assign/list_of.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "County.h"
#include "Expression.h"
#include "Global.h"
#include "Household.h"
#include "Person.h"
#include "Network_Transmission.h"
#include "Utils.h"

namespace Utils
{
  namespace {
    char error_filename[FRED_STRING_SIZE];
    FILE* error_logfp = nullptr;
    bool logs_initialized = false;
    std::unique_ptr<spdlog::logger> stdout_logger = nullptr;
    std::unique_ptr<spdlog::logger> error_logger = nullptr;
    std::unique_ptr<spdlog::logger> warning_logger = nullptr;
    std::chrono::high_resolution_clock::time_point start_timer;
    std::chrono::high_resolution_clock::time_point fred_timer;
    std::chrono::high_resolution_clock::time_point day_timer;
    std::chrono::high_resolution_clock::time_point initialization_timer;
    std::chrono::high_resolution_clock::time_point update_timer;
    std::chrono::high_resolution_clock::time_point epidemic_timer;

    std::map<std::string, spdlog::level::level_enum> log_level_map = boost::assign::map_list_of("TRACE", spdlog::level::trace)
        ("DEBUG", spdlog::level::debug)
        ("INFO", spdlog::level::info)
        ("WARN", spdlog::level::warn)("WARNING", spdlog::level::warn)
        ("ERR", spdlog::level::err)("ERROR", spdlog::level::err)
        ("CRITICAL", spdlog::level::critical)("FATAL", spdlog::level::critical)
        ("OFF", spdlog::level::off);

    int parse_line(char* line) {
      // This assumes that a digit will be found and the line ends in " Kb".
      int i = strlen(line);
      const char* p = line;
      while(*p <'0' || *p > '9') {
        ++p;
      }
      line[i - 3] = '\0';
      i = atoi(p);
      return i;
    }
  }

  /**
   * Compares the IDs of two specified persons. Checks if the first person's ID is less than the second person' ID.
   *
   * @param p1 the first person
   * @param p2 the second person
   * @return the result of the comparison
   */
  bool compare_id (Person* p1, Person* p2) {
    return p1->get_id() < p2->get_id();
  }

  /**
   * Deletes spaces from the specified string.
   *
   * @param str the string
   * @return the new string
   */
  std::string delete_spaces(std::string &str) {
    str.erase(std::remove(str.begin(), str.end(), ' '), str.end());
    return str;
  }

  /**
   * Splits the specified string at the specified delimiter to get a vector of separate strings.
   *
   * @param str the string
   * @param delim the delimiter
   * @return the string vector
   */
  string_vector_t get_string_vector(std::string str, char delim) {
    string_vector_t result;
    std::stringstream ss(str);
    while(ss.good()) {
      std::string substr;
      getline(ss, substr, delim);
      if(substr != "") {
        result.push_back(Utils::delete_spaces(substr));
      }
    }
    return result;
  }

  /**
   * Checks if the specified string can be converted to a number.
   *
   * @param str the string to be converted
   * @return if the string is a number
   */
  bool is_number(std::string str) {
    char* p;
    strtod(str.c_str(), &p);
    return *p == 0;
  }

  /**
   * Convert a string value into the actual logging level enumerated value
   * Defaults to a logging level of critical if the incoming string does not match any of the expected values
   *
   * @param str the string to be looked up
   * @return the logging level enum that matches the string
   */
  spdlog::level::level_enum get_log_level_from_string(std::string str) {
    // Convert the incoming string to all caps and trim whitespace
    boost::to_upper(str);
    boost::algorithm::trim(str);
      
    auto itr = Utils::log_level_map.find(str);
    if(itr != Utils::log_level_map.end()) {
      return itr->second;
    } else {
      return spdlog::level::off;
    }
  }

  /**
   * Convert a const char pointer (c-string literal) value into the actual logging level enumerated value
   * Defaults to a logging level of critical if the incoming string does not match any of the expected values
   *
   * @param c_str the string to be looked up
   * @return the logging level enum that matches the string
   */
  spdlog::level::level_enum get_log_level_from_string(const char* c_str) {
    return Utils::get_log_level_from_string(std::string(c_str));
  }

  string_vector_t get_top_level_parse(std::string str, char delim) {
    string_vector_t result;
    str = Utils::delete_spaces(str);
    char cstr[FRED_STRING_SIZE];
    char tmp[FRED_STRING_SIZE];
    strcpy(cstr, str.c_str());
    // FRED_VERBOSE(0, "input string = |%s|\n", cstr);
    int inside = 0;
    char* s = cstr;
    char* t = tmp;
    *t = '\0';
    while(*s != '\0') {
      // printf("*s = |%c| tmp = |%s|\n", *s, tmp); fflush(stdout);
      if(*s  ==  delim && !inside) {
        // FRED_VERBOSE(0, "push_back |%s|\n", tmp);
        result.push_back(std::string(tmp));
        t = tmp;
        *t = '\0';
      } else {
        if(*s == '(') {
          ++inside;
        }
        if(*s == ')') {
          --inside;
        }
        *t++ = *s;
        *t = '\0';
      }
      ++s;
    }
    result.push_back(std::string(tmp));
    return result;
  }

  void fred_abort(const char* format, ...) {
    
    // Only use the error_filename if the logs aren't initialized
    if(Utils::logs_initialized) {
      char buffer[FRED_STRING_SIZE];
      va_list ap;
      va_start(ap, format);
      vsnprintf(buffer, FRED_STRING_SIZE, format, ap);
      va_end(ap);
      
      Utils::stdout_logger->error("FRED ERROR: {:s}", buffer);
      Utils::error_logger->error("{:s}", buffer);
    } else {
      // open ErrorLog file if it doesn't exist
      if(Utils::error_logfp == nullptr) {
        Utils::error_logfp = fopen(Utils::error_filename, "w");
        if(Utils::error_logfp == nullptr) {
          // output to stdout
          printf("\nFRED ERROR: Can't open errorfile %s\n", Utils::error_filename);
          // current error message:
          va_list ap;
          va_start(ap,format);
          printf("\nFRED ERROR: ");
          vprintf(format, ap);
          va_end(ap);
          fflush(stdout);
          Utils::fred_end();
          abort();
        }
      }
      // output to error file
      va_list ap;
      va_start(ap, format);
      fprintf(Utils::error_logfp, "\nFRED ERROR: ");
      vfprintf(Utils::error_logfp, format, ap);
      va_end(ap);
      fflush(Utils::error_logfp);

      // output to stdout
      va_start(ap, format);
      printf("\nFRED ERROR: ");
      vprintf(format, ap);
      va_end(ap);
      fflush(stdout);
    }
    Utils::fred_end();
    abort();
  }

  void fred_warning(const char* format, ...) {

    // Only use the error_filename if the logs aren't initialized
    if(Utils::logs_initialized) {
      
      char buffer[FRED_STRING_SIZE];
      va_list ap;
      va_start(ap, format);
      vsnprintf(buffer, FRED_STRING_SIZE, format, ap);
      va_end(ap);
      
      Utils::stdout_logger->warn("FRED WARNING: {:s}", buffer);
      Utils:warning_logger->warn("{:s}", buffer);
    } else {
      // open ErrorLog file if it doesn't exist
      if(Utils::error_logfp == nullptr) {
        Utils::error_logfp = fopen(Utils::error_filename, "w");
        if(Utils::error_logfp == nullptr) {
          // output to stdout
          printf("\nFRED ERROR: Can't open errorfile %s\n", Utils::error_filename);
          // current error message:
          va_list ap;
          va_start(ap,format);
          printf("\nFRED WARNING: ");
          vprintf(format, ap);
          va_end(ap);
          fflush(stdout);
          Utils::fred_end();
          abort();
        }
      }

      // output to error file
      va_list ap;
      va_start(ap, format);
      fprintf(Utils::error_logfp, "\nFRED WARNING: ");
      vfprintf(Utils::error_logfp, format, ap);
      va_end(ap);
      fflush(Utils::error_logfp);

      // output to stdout
      va_start(ap, format);
      printf("\nFRED WARNING: ");
      vprintf(format, ap);
      va_end(ap);
      fflush(stdout);
    }
  }

  void fred_open_output_files() {
    int run = Global::Simulation_run_number;
    char filename[FRED_STRING_SIZE];
    char directory[FRED_STRING_SIZE];
    size_t buffer_size = FRED_STRING_SIZE;
    snprintf(directory, buffer_size, "%s/RUN%d", Global::Simulation_directory, run);
    Utils::fred_make_directory(directory);

    // ErrorLog file is created at the first warning or error
    Utils::error_logfp = nullptr;
    snprintf(Utils::error_filename, buffer_size, "%s/err.txt", directory);

    Global::Recordsfp = nullptr;
    if(Global::Enable_Records > 0) {
      snprintf(filename, buffer_size, "%s/health_records.txt", directory);
      Global::Recordsfp = fopen(filename, "w");
      if(Global::Recordsfp == nullptr) {
        Utils::fred_abort("Can't open %s\n", filename);
      }
    }

    return;
  }

  /**
   * Called from the initial startup after Global::Simulation_directory and Global:Simulation_run_number have been set.
   *
   * Creates all of the file sinks and local logs for the logging
   */
  void fred_initialize_logging() {
    
    if(!Utils::logs_initialized) {
      
      int run = Global::Simulation_run_number;
      char filename[FRED_STRING_SIZE];
      char directory[FRED_STRING_SIZE];
      snprintf(directory, FRED_STRING_SIZE, "%s/RUN%d", Global::Simulation_directory, run);
      Utils::fred_make_directory(directory);

      // StdoutSink only displays the text that is to be logged, not any additional information
      // StdoutSink is set to informational logging
      Global::StdoutSink->set_pattern("%v");
      Global::StdoutSink->set_level(Utils::get_log_level_from_string("info"));

      snprintf(filename, FRED_STRING_SIZE, "%s/logs/error.log", directory);
      auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_st>(filename);
      Global::ErrorFileSink = std::make_shared<spdlog::sinks::basic_file_sink_st>(filename);
      Global::ErrorFileSink->set_level(Utils::get_log_level_from_string("err"));
    
      snprintf(filename, FRED_STRING_SIZE, "%s/logs/debug.log", directory);
      Global::DebugFileSink = std::make_shared<spdlog::sinks::basic_file_sink_st>(filename);
      Global::DebugFileSink->set_level(Utils::get_log_level_from_string("debug"));
    
      snprintf(filename, FRED_STRING_SIZE, "%s/logs/trace.log", directory);
      Global::TraceFileSink = std::make_shared<spdlog::sinks::basic_file_sink_st>(filename);
      Global::TraceFileSink->set_level(Utils::get_log_level_from_string("trace"));
      
      // Create the logs that can be written to from other classes (may not be necessary after we are done)
      Utils::stdout_logger = std::make_unique<spdlog::logger>("utils_status", Global::StdoutSink);
      Utils::stdout_logger->set_level(Utils::get_log_level_from_string("info"));
      
      Utils::error_logger = std::make_unique<spdlog::logger>("FRED ERROR", file_sink);
      Utils::error_logger->set_pattern("%n: %v");
      Utils::error_logger->set_level(Utils::get_log_level_from_string("error"));
      
      Utils::warning_logger = std::make_unique<spdlog::logger>("FRED WARNING", file_sink);
      Utils::warning_logger->set_pattern("%n: %v");
      Utils::warning_logger->set_level(Utils::get_log_level_from_string("warn"));
      
      Utils::logs_initialized = true;
    }
  }

  void fred_make_directory(char* directory) {
    struct stat info;
    if(stat(directory, &info) == 0) {
      // file already exists. verify that it is a directory
      if(info.st_mode & S_IFDIR) {
        // printf( "fred_make_directory: %s already exists\n", directory );
        return;
       } else {
        Utils::fred_abort("fred_make_directory: %s exists but is not a directory\n", directory);
      }
    }
    // try to create the directory:
    mode_t mask;        // the user's current umask
    mode_t mode = 0777; // as a start
    mask = umask(0);    // get the current mask, which reads and sets...
    umask(mask);        // so now we have to put it back
    mode ^= mask;       // apply the user's existing umask
    if(0 != mkdir(directory, mode) && EEXIST != errno) { // make it
      Utils::fred_abort("mkdir(%s) failed with %d\n", directory, errno); // or die
    }
  }

  void fred_end(void) {

    // This is a function that cleans up FRED and exits
    if(Global::Statusfp != nullptr) {
      fclose(Global::Statusfp);
    }

    if(Global::Birthfp != nullptr) {
      fclose(Global::Birthfp);
    }

    if(Global::Deathfp != nullptr) {
      fclose(Global::Deathfp);
    }

    if(Global::Recordsfp != nullptr) {
      fclose(Global::Recordsfp);
    }
    
    if(Utils::error_logfp != nullptr) {
      fclose(Utils::error_logfp);
    }
  }

  void fred_print_wall_time(const char* format, ...) {
    time_t clock;
    time(&clock);
    char buffer[FRED_STRING_SIZE];
    va_list ap;
    va_start(ap, format);
    vsnprintf(buffer, FRED_STRING_SIZE, format, ap);
    va_end(ap);
    
    
    if(Utils::logs_initialized) {
      Utils::stdout_logger->info("{:s} {:s}", buffer, ctime(&clock));
    } else {
      fprintf(Global::Statusfp, "%s %s", buffer, ctime(&clock));
      fflush(Global::Statusfp);
    }
  }

  void fred_start_timer() {
    Utils::fred_timer = std::chrono::high_resolution_clock::now();
    Utils::start_timer = Utils::fred_timer;
  }

  void fred_start_timer(std::chrono::high_resolution_clock::time_point* lap_start_time) {
    *lap_start_time = std::chrono::high_resolution_clock::now();
  }

  void fred_start_epidemic_timer() {
    Utils::epidemic_timer = std::chrono::high_resolution_clock::now();
  }

  void fred_print_epidemic_timer(std::string msg) {
    std::chrono::high_resolution_clock::time_point stop_timer = std::chrono::high_resolution_clock::now();
    double duration = 0.000001 * std::chrono::duration_cast<std::chrono::microseconds>(stop_timer - Utils::epidemic_timer).count();
    
    if(Utils::logs_initialized) {
      Utils::stdout_logger->info("{:s} took {:f} seconds", msg, duration);
    } else {
      fprintf(Global::Statusfp, "%s took %f seconds\n", msg.c_str(), duration);
      fflush(Global::Statusfp);
    }
    
    Utils::epidemic_timer = stop_timer;
  }

  void fred_start_initialization_timer() {
    Utils::initialization_timer = std::chrono::high_resolution_clock::now();
  }

  void fred_print_initialization_timer() {
    std::chrono::high_resolution_clock::time_point stop_timer = std::chrono::high_resolution_clock::now();
    double duration = 0.000001 * std::chrono::duration_cast<std::chrono::microseconds>(stop_timer - Utils::initialization_timer).count();
    
    if(Utils::logs_initialized) {
      Utils::stdout_logger->info("FRED initialization took {:f} seconds", duration);
      Utils::stdout_logger->info("");
    } else {
      fprintf(Global::Statusfp, "FRED initialization took %f seconds\n\n", duration);
      fflush(Global::Statusfp);
    }
  }

  void fred_start_day_timer() {
    Utils::day_timer = std::chrono::high_resolution_clock::now();
  }

  void fred_print_day_timer(int day) {
    std::chrono::high_resolution_clock::time_point stop_timer = std::chrono::high_resolution_clock::now();
    double duration = 0.000001 * std::chrono::duration_cast<std::chrono::microseconds>(stop_timer - Utils::day_timer).count();
    
    if(Utils::logs_initialized) {
      Utils::stdout_logger->info("DAY_TIMER day {:d} took {:f} seconds", day, duration);
      Utils::stdout_logger->info("");
    } else {
      fprintf(Global::Statusfp, "DAY_TIMER day %d took %f seconds\n\n", day, duration);
      fflush(Global::Statusfp);
    }
  }

  void fred_print_finish_timer() {
    std::chrono::high_resolution_clock::time_point stop_timer = std::chrono::high_resolution_clock::now();
    double duration = 0.000001 * std::chrono::duration_cast<std::chrono::microseconds>(stop_timer - Utils::fred_timer).count();
    
    if(Utils::logs_initialized) {
      Utils::stdout_logger->info("FRED took {:f} seconds", duration);
    } else {
      fprintf(Global::Statusfp, "FRED took %f seconds\n", duration);
      fflush(Global::Statusfp);
    }
  }

  void fred_print_lap_time(const char* format, ...) {
    std::chrono::high_resolution_clock::time_point stop_timer = std::chrono::high_resolution_clock::now();
    double duration = 0.000001 * std::chrono::duration_cast<std::chrono::microseconds>(stop_timer - Utils::start_timer).count();
    char buffer[FRED_STRING_SIZE];
    va_list ap;
    va_start(ap, format);
    vsnprintf(buffer, FRED_STRING_SIZE, format, ap);
    va_end(ap);
    
    if(Utils::logs_initialized) {
      Utils::stdout_logger->info("{:s} took {:f} seconds", buffer, duration);
    } else {
      fprintf(Global::Statusfp, "%s took %f seconds\n", buffer, duration);
      fflush(Global::Statusfp);
    }
    Utils::start_timer = stop_timer;
  }

  void fred_print_lap_time(std::chrono::high_resolution_clock::time_point* start_lap_time, const char* format, ...) {
    std::chrono::high_resolution_clock::time_point stop_timer = std::chrono::high_resolution_clock::now();
    double duration = 0.000001 * std::chrono::duration_cast<std::chrono::microseconds>(stop_timer - (*start_lap_time)).count();
    char buffer[FRED_STRING_SIZE];
    va_list ap;
    va_start(ap, format);
    vsnprintf(buffer, FRED_STRING_SIZE, format, ap);
    va_end(ap);
    
    if(Utils::logs_initialized) {
      Utils::stdout_logger->info("{:s} took {:f} seconds", buffer, duration);
    } else {
      fprintf(Global::Statusfp, "%s took %f seconds\n", buffer, duration);
      fflush(Global::Statusfp);
    }
  }

  void fred_verbose(int verbosity, const char* format, ...) {
    if(Global::Verbose > verbosity) {
      va_list ap;
      va_start(ap, format);
      vprintf(format, ap);
      va_end(ap);
      fflush(stdout);
    }
  }

  void fred_status(int verbosity, const char* format, ...) {
    if(Global::Verbose > verbosity) {
      va_list ap;
      va_start(ap, format);
      vfprintf(Global::Statusfp, format, ap);
      va_end(ap);
      fflush(Global::Statusfp);
    }
  }

  void fred_log(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    vfprintf(Global::Statusfp, format, ap);
    va_end(ap);
    fflush(Global::Statusfp);
  }

  FILE* fred_open_file(char* filename) {
    FILE* fp;
    Utils::get_fred_file_name(filename);
    if(Utils::logs_initialized) {
      Utils::stdout_logger->info("fred_open_file: opening file {:s} for reading", filename);
    } else {
      printf("fred_open_file: opening file %s for reading\n", filename);
    }
    fp = fopen(filename, "r");
    return fp;
  }

  FILE* fred_write_file(char* filename) {
    FILE* fp;
    Utils::get_fred_file_name(filename);
    
    if(Utils::logs_initialized) {
      Utils::stdout_logger->info("fred_open_file: opening file {:s} for writing", filename);
    } else {
      printf("fred_open_file: opening file %s for writing\n", filename);
    }
    fp = fopen(filename, "w");
    return fp;
  }

  void get_fred_file_name(char* filename) {
    std::string str;
    str.assign(filename);
    if(str.compare(0, 10, "$FRED_HOME") == 0) {
      char* fred_home = getenv("FRED_HOME");
      if(fred_home != nullptr) {
        str.erase(0, 10);
        str.insert(0, fred_home);
        strcpy(filename, str.c_str());
      } else {
        Utils::fred_abort("get_fred_file_name: the FRED_HOME environmental variable cannot be found\n");
      }
    }
  }

#include <sys/resource.h>
/*
  NOTE: FROM sys/resource.h ...

  #define   RUSAGE_SELF     0
  #define   RUSAGE_CHILDREN     -1

  struct rusage {
  struct timeval ru_utime; // user time used
  struct timeval ru_stime; // system time used
  long ru_maxrss;          // integral max resident set size
  long ru_ixrss;           // integral shared text memory size
  long ru_idrss;           // integral unshared data size
  long ru_isrss;           // integral unshared stack size
  long ru_minflt;          // page reclaims
  long ru_majflt;          // page faults
  long ru_nswap;           // swaps
  long ru_inblock;         // block input operations
  long ru_oublock;         // block output operations
  long ru_msgsnd;          // messages sent
  long ru_msgrcv;          // messages received
  long ru_nsignals;        // signals received
  long ru_nvcsw;           // voluntary context switches
  long ru_nivcsw;          // involuntary context switches
  };
*/

  void fred_print_resource_usage(int day) {
    rusage r_usage;
    getrusage(RUSAGE_SELF, &r_usage);
    if(Utils::logs_initialized) {
      Utils::stdout_logger->info("day {:d} maxrss {:d}", day, r_usage.ru_maxrss);
      Utils::stdout_logger->info("day {:d} cur_phys_mem_usage_gbs {:0.4f}", day, Utils::get_fred_phys_mem_usg_in_gb());
    } else {
      printf("day %d maxrss %ld\n", day, r_usage.ru_maxrss);
      printf("day %d cur_phys_mem_usage_gbs %0.4f\n", day, Utils::get_fred_phys_mem_usg_in_gb());
      fflush(stdout);
    }
  }

  double get_daily_probability(double prob, int days) {
    // p = total prob; d = daily prob; n = days
    // prob of survival = 1-p = (1-d)^n
    // log(1-p) = n*log(1-d)
   // (1/n)*log(1-p) = log(1-d)
    // (1-p)^(1/n) = 1-d
    // d = 1-(1-p)^(1/n)
    double daily = 1.0 - pow((1.0 - prob), (1.0 / days));
    return daily;
  }

  std::string str_tolower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
      return std::tolower(c);
    });
    return s;
  }

  bool does_path_exist(const std::string &s) {
    char filename[FRED_STRING_SIZE];
    snprintf(filename, FRED_STRING_SIZE, "%s", s.c_str());
    Utils::get_fred_file_name(filename);
    struct stat buffer;
    return (stat (filename, &buffer) == 0);
  }

  void print_error(const std::string &msg) {
    char error_file[FRED_STRING_SIZE];
    snprintf(error_file, FRED_STRING_SIZE, "%s/errors.txt", Global::Simulation_directory);
    FILE* fp = fopen(error_file, "a");
    fprintf(fp, "\nFRED Error (file %s) %s\n", Global::Model_file, msg.c_str());
    fclose(fp);
    printf("Error message: %s\n", msg.c_str());
    Global::Error_found = true;
  }

  void print_warning(const std::string &msg) {
    char warning_file[FRED_STRING_SIZE];
    snprintf(warning_file, FRED_STRING_SIZE, "%s/warnings.txt", Global::Simulation_directory);
    FILE* fp = fopen(warning_file, "a");
    fprintf(fp, "\nFRED Warning (file %s) %s\n", Global::Model_file, msg.c_str());
    fclose(fp);
  }

  double get_fred_phys_mem_usg_in_gb() { //Note: this value is in GB!
    double result = -1.0;

  #ifdef LINUX

    FILE* file = fopen("/proc/self/status", "r");
    char line[128];

    while(fgets(line, 128, file) != nullptr) {
      if(strncmp(line, "VmRSS:", 6) == 0) {
        result = static_cast<double>(Utils::parse_line(line)); //KB
        result /= 1024.0; //MB
        result /= 1024.0; //GB
        break;
      }
    }
    fclose(file);

  #elif OSX

    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

    if(KERN_SUCCESS != task_info(mach_task_self(), TASK_BASIC_INFO,
        (task_info_t)&t_info, &t_info_count)) {
      result = -1.0;
    } else {
      int temp_int = t_info.resident_size;
      result = static_cast<double>(temp_int);
      result /= 1024.0; // KB
      result /= 1024.0; // MB
      result /= 1024.0; // GB
    }
  #elif WIN32

  #endif

    return result;
  }
}
