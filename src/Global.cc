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
// File: Global.cc
//

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>

#include "Global.h"
#include "Parser.h"
#include "Demographics.h"
#include "Utils.h"

// global simulation variables
char Global::Model_file[FRED_STRING_SIZE];
char Global::Simulation_directory[FRED_STRING_SIZE];
char Global::Plot_directory[FRED_STRING_SIZE];
char Global::Visualization_directory[FRED_STRING_SIZE];
int Global::Simulation_run_number = 1;
unsigned long Global::Simulation_seed = 1;
std::chrono::high_resolution_clock::time_point Global::Simulation_start_time = std::chrono::high_resolution_clock::now();
int Global::Simulation_Day = 0;
int Global::Simulation_Days = 0;
int Global::Simulation_Hour = 0;
int Global::Simulation_Step = 0;

// global runtime properties
char Global::Output_directory[FRED_STRING_SIZE];
int Global::Quality_control = 0;
char Global::ErrorLogbase[FRED_STRING_SIZE];
bool Global::Track_age_distribution = false;
bool Global::Track_network_stats = false;

int Global::Verbose = 0;
int Global::Debug = 0;
int Global::Test = 0;
int Global::Reseed_day = -1;
int Global::Reseed_run = 1;
unsigned long Global::Seed = 0;
int Global::Use_FRED = 0;
int Global::Use_rules = 0;
int Global::Compile_FRED = 0;
int Global::Max_Loops = 0;
bool Global::Enable_Profiles = false;
bool Global::Enable_Records = false;
bool Global::Enable_Var_Records = false;
bool Global::Enable_List_Var_Records = false;
bool Global::Enable_Transmission_Bias = false;
bool Global::Enable_New_Transmission_Model = false;
bool Global::Enable_Hospitals = false;
bool Global::Enable_Group_Quarters = false;
bool Global::Enable_Visualization_Layer = false;
int Global::Visualization_Run = 1;
int Global::Health_Records_Run = 1;
bool Global::Enable_Population_Dynamics = false;
bool Global::Enable_Travel = false;
bool Global::Enable_Local_Workplace_Assignment = false;
bool Global::Enable_Fixed_Order_Condition_Updates = false;
bool Global::Enable_External_Updates = false;
bool Global::Enable_External_Variable_Initialization = false;
bool Global::Use_Mean_Latitude = false;
bool Global::Report_Serial_Interval = false;
bool Global::Report_County_Demographic_Information = false;
bool Global::Assign_Teachers = false;
bool Global::Report_Contacts = false;
bool Global::Error_found = false;

// grid layers
Neighborhood_Layer* Global::Neighborhoods = nullptr;
Regional_Layer* Global::Simulation_Region = nullptr;
Visualization_Layer* Global::Visualization = nullptr;

// global file pointers
FILE* Global::Statusfp = nullptr;
FILE* Global::Recordsfp = stdout;
FILE* Global::Birthfp = nullptr;
FILE* Global::Deathfp = nullptr;

spdlog::sink_ptr Global::StdoutSink = std::make_shared<spdlog::sinks::stdout_sink_st>();
spdlog::sink_ptr Global::ErrorFileSink = nullptr;
spdlog::sink_ptr Global::DebugFileSink = nullptr;
spdlog::sink_ptr Global::TraceFileSink = nullptr;

void Global::get_global_properties() {
  Parser::get_property("verbose", &Global::Verbose);
  Parser::get_property("debug", &Global::Debug);
  Parser::get_property("test", &Global::Test);
  Parser::get_property("quality_control", &Global::Quality_control);
  Parser::get_property("seed", &Global::Seed);
  Parser::get_property("reseed_day", &Global::Reseed_day);
  Parser::get_property("reseed_run", &Global::Reseed_run);
  Parser::get_property("outdir", Global::Output_directory);
  Parser::get_property("max_loops", &Global::Max_Loops);

  //Set global boolean flags
  Parser::get_property("track_age_distribution", &Global::Track_age_distribution);
  Parser::get_property("track_network_stats", &Global::Track_network_stats);
  Parser::get_property("enable_profiles", &Global::Enable_Profiles);
  Parser::get_property("enable_health_records", &Global::Enable_Records);
  Parser::get_property("enable_var_records", &Global::Enable_Var_Records);
  Parser::get_property("enable_list_var_records", &Global::Enable_List_Var_Records);
  Parser::get_property("enable_transmission_bias", &Global::Enable_Transmission_Bias);
  Parser::get_property("enable_new_transmission_model", &Global::Enable_New_Transmission_Model);
  Parser::get_property("enable_Hospitals", &Global::Enable_Hospitals);
  Parser::get_property("enable_group_quarters", &Global::Enable_Group_Quarters);
  Parser::get_property("enable_visualization_layer", &Global::Enable_Visualization_Layer);
  Parser::get_property("enable_population_dynamics", &Global::Enable_Population_Dynamics);
  Parser::get_property("enable_travel",&Global::Enable_Travel);
  Parser::get_property("enable_local_Workplace_assignment", &Global::Enable_Local_Workplace_Assignment);
  Parser::get_property("enable_fixed_order_condition_updates", &Global::Enable_Fixed_Order_Condition_Updates);
  Parser::get_property("enable_external_variable_initialization", &Global::Enable_External_Variable_Initialization);
  Parser::get_property("use_mean_latitude", &Global::Use_Mean_Latitude);
  Parser::get_property("assign_teachers", &Global::Assign_Teachers);
  Parser::get_property("report_serial_interval", &Global::Report_Serial_Interval);
  Parser::get_property("report_contacts", &Global::Report_Contacts);

  // set any properties that are dependent on other properties
  Parser::get_property("visualization_run", &Global::Visualization_Run);
  if(Global::Visualization_Run != -1 &&
      Global::Simulation_run_number != Global::Visualization_Run) {
    Global::Enable_Visualization_Layer = false;
  }

  Parser::get_property("health_records_run", &Global::Health_Records_Run);
  if(Global::Health_Records_Run != -1 &&
      Global::Simulation_run_number != Global::Health_Records_Run) {
    Global::Enable_Records = false;
  }

  if(Global::Compile_FRED) {
    Global::Debug = 0;
    Global::Verbose = 0;
    Global::Quality_control = 0;
  }
}

#ifndef _OPENMP
namespace fred {

  int omp_get_max_threads() {
    return 1;
  }

  int omp_get_num_threads() {
    return 1;
  }

  int omp_get_thread_num() {
    return 0;
  }
}
#endif
