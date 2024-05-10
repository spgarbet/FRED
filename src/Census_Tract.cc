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
// File: Census_Tract.cc
//

#include "Census_Tract.h"
#include "County.h"
#include "Parser.h"
#include "Person.h"
#include "Place.h"
#include "Random.h"

/**
 * Default destructor.
 */
Census_Tract::~Census_Tract() {
}

/**
 * Creates a Census_Tract with the specified admin code. Sets a County as the higher division 
 * to this census tract, and adds this census tract as a subdivison of the county.
 *
 * @param _admin_code the admin code
 */
Census_Tract::Census_Tract(long long int _admin_code) : Admin_Division(_admin_code) {

  this->workplaces_attended.clear();
  for(int i = 0; i < Global::GRADES; ++i) {
    this->schools_attended[i].clear();
  }

  // get the county associated with this tract, creating a new one if necessary
  int county_admin_code = this->get_admin_division_code() / 1000000;
  County* county = County::get_county_with_admin_code(county_admin_code);
  this->higher = county;
  county->add_subdivision(this);
}


/**
 * Sets up the school and workplace probabilities for a census tract.
 */
void Census_Tract::setup() {
  this->set_school_probabilities();
  this->set_workplace_probabilities();
  Census_Tract::census_tract_logger->info(
      "CENSUS_TRACT {:d} setup: population = {:d}  households = {:d}  workplaces attended = {:d}",
      this->get_admin_division_code(), this->get_population_size(), static_cast<int>(this->households.size()), 
      static_cast<int>(this->workplaces_attended.size()));
  for(int i = 0; i < Global::GRADES; ++i) {
    if(this->schools_attended[i].size() > 0) {
      Census_Tract::census_tract_logger->debug("CENSUS_TRACT {:d} setup: school attended for grade {:d} = {:d}",
          this->get_admin_division_code(), i, static_cast<int>(this->schools_attended[i].size()));
    }
  }
}

/**
 * _UNUSED_
 */
void Census_Tract::update(int day) {
}


// METHODS FOR SELECTING NEW SCHOOLS

/**
 * Rebuilds the school counts based on attendance distribution, then converts this data to probabilities.
 */
void Census_Tract::set_school_probabilities() {

  Census_Tract::census_tract_logger->info("set_school_probablities for admin_code {:d}", 
      this->get_admin_division_code());

  int total[Global::GRADES];
  this->school_id_lookup.clear();
  for(int g = 0; g < Global::GRADES; ++g) {
    this->schools_attended[g].clear();
    this->school_probabilities[g].clear();
    this->school_counts[g].clear();
    total[g] = 0;
  }

  // get number of people in this census_tract attending each school
  // at the start of the simulation
  int houses = this->households.size();
  for(int i = 0; i < houses; ++i) {
    Place* hh = this->households[i];
    int hh_size = hh->get_size();
    for(int j = 0; j < hh_size; ++j) {
      Person* person = hh->get_member(j);
      Place* school = person->get_school();
      int grade = person->get_age();
      if(school != nullptr && grade < Global::GRADES) {
        // add this person to the count for this school
        int school_id = school->get_id();
        if(this->school_counts[grade].find(school_id) == this->school_counts[grade].end()) {
          std::pair<int,int> new_school_count(school_id, 1);
          this->school_counts[grade].insert(new_school_count);
          if(this->school_id_lookup.find(school_id) == this->school_id_lookup.end()) {
            std::pair<int, Place*> new_school_id_map(school_id, school);
            this->school_id_lookup.insert(new_school_id_map);
          }
        } else {
          ++this->school_counts[grade][school_id];
        }
        ++total[grade];
      } // endif school != nullptr
    } // foreach Housemate
  } // foreach household

  // convert to probabilities
  for(int g = 0; g < Global::GRADES; ++g) {
    if(total[g] > 0) {
      for(attendance_map_itr_t itr = this->school_counts[g].begin(); itr != this->school_counts[g].end(); ++itr) {
        int school_id = itr->first;
        int count = itr->second;
        Place* school = this->school_id_lookup[school_id];
        this->schools_attended[g].push_back(school);
        double prob = (double)count / (double) total[g];
        this->school_probabilities[g].push_back(prob);
        Census_Tract::census_tract_logger->debug(
            "school {:s} admin_code {:d} grade {:d} attended by {:d} prob {:f}",
            school->get_label(), school->get_county_admin_code(), g, count, prob);
      }
    }
  }
}

/**
 * Selects a school at the specified grade with the largest vacancy rate.
 * If no vacancies are found, a school will be randomly selected based on the school probabilities.
 *
 * @param grade the grade
 * @return the selected school
 */
Place* Census_Tract::select_new_school(int grade) {

  // pick the school with largest vacancy rate in this grade
  Place* selected = nullptr;
  double max_vrate = 0.0;
  for(int i = 0; i < static_cast<int>(this->schools_attended[grade].size()); ++i) {
    Place* school = this->schools_attended[grade][i];
    double target = school->get_original_size_by_age(grade);
    double vrate = (target-school->get_size_by_age(grade))/target;
    if(vrate > max_vrate) {
      selected = school;
      max_vrate = vrate;
    }
  }
  if(selected != nullptr) {
    return selected;
  }

  Census_Tract::census_tract_logger->warn(
      "NO SCHOOL VACANCIES found on day {:d} in admin_code = {:d} grade = {:d} schools = {:d}",
      Global::Simulation_Day, this->get_admin_division_code(), grade, 
      static_cast<int>(this->schools_attended[grade].size()));

 // pick from the attendance distribution
  double r = Random::draw_random();
  double sum = 0.0;
  for(int i = 0; i < static_cast<int>(this->school_probabilities[grade].size()); ++i) {
    sum += this->school_probabilities[grade][i];
    if(r < sum) {
      return this->schools_attended[grade][i];
    }
  }
  Census_Tract::census_tract_logger->warn(
      "NO SCHOOL FOUND on day {:d} in admin_code = {:d} grade = {:d} schools = {:d} r = {:f} sum = {:f}",
      Global::Simulation_Day, this->get_admin_division_code(), grade,
      static_cast<int>(this->school_probabilities[grade].size()), r, sum);

  // fall back to selecting school from County
  return nullptr;
}


// METHODS FOR SELECTING NEW WORKPLACES

/**
 * Rebuilds the workplace counts based on attendance distribution, then converts this data to probabilities.
 */
void Census_Tract::set_workplace_probabilities() {

  // list of workplaces attended by people in this census_tract
  typedef std::unordered_map<int,int> attendance_map_t;
  typedef std::unordered_map<int,Place*> wid_map_t;
  typedef attendance_map_t::iterator attendance_map_itr_t;

  this->workplaces_attended.clear();
  this->workplace_probabilities.clear();

  // get number of people in this county attending each workplace
  // at the start of the simulation
  int houses = this->households.size();
  attendance_map_t workplace_counts;
  wid_map_t wid_to_workplace;
  workplace_counts.clear();
  wid_to_workplace.clear();
  int total = 0;
  for(int i = 0; i < houses; ++i) {
    Place* hh = this->households[i];
    int hh_size = hh->get_size();
    for(int j = 0; j < hh_size; ++j) {
      Person* person = hh->get_member(j);
      Place* workplace = person->get_workplace();
      if(workplace != nullptr) {
        int wid = workplace->get_id();
        if(workplace_counts.find(wid) == workplace_counts.end()) {
          std::pair<int, int> new_workplace_count(wid, 1);
          workplace_counts.insert(new_workplace_count);
          std::pair<int, Place*> new_wid_map(wid, workplace);
          wid_to_workplace.insert(new_wid_map);
        } else {
          ++workplace_counts[wid];
        }
        ++total;
      }
    }
  }
  if(total == 0) {
    return;
  }

  // convert to probabilities
  for(attendance_map_itr_t itr = workplace_counts.begin(); itr != workplace_counts.end(); ++itr) {
    int wid = itr->first;
    int count = itr->second;
    Place* workplace = wid_to_workplace[wid];
    this->workplaces_attended.push_back(workplace);
    double prob = static_cast<double>(count) / static_cast<double>(total);
    this->workplace_probabilities.push_back(prob);
    Census_Tract::census_tract_logger->debug("workplace {:s} admin_code {:d}  attended by {:d} prob {:f}", 
        workplace->get_label(), workplace->get_census_tract_admin_code(), count, prob);
  }
}

/**
 * Selects a workplace randomly based on the workplace probabilities.
 */
Place* Census_Tract::select_new_workplace() {
  double r = Random::draw_random();
  double sum = 0.0;
  for(int i = 0; i < static_cast<int>(this->workplace_probabilities.size()); ++i) {
    sum += this->workplace_probabilities[i];
    if(r < sum) {
      return this->workplaces_attended[i];
    }
  }
  return nullptr;
}

//////////////////////////////
//
// STATIC METHODS
//
//////////////////////////////


// static variables
std::vector<Census_Tract*> Census_Tract::census_tracts;
std::unordered_map<long long int,Census_Tract*> Census_Tract::lookup_map;

bool Census_Tract::is_log_initialized = false;
std::string Census_Tract::census_tract_log_level = "";
std::unique_ptr<spdlog::logger> Census_Tract::census_tract_logger = nullptr;

/**
 * Gets the census tract with the specified admin code. If no such census tract exists, one is created with the admin_code.
 *
 * @param census_tract_admin_code the census tract admin code
 * @return the census tract
 */
Census_Tract* Census_Tract::get_census_tract_with_admin_code(long long int census_tract_admin_code) {
  Census_Tract* census_tract = nullptr;
  std::unordered_map<long long int,Census_Tract*>::iterator itr;
  itr = Census_Tract::lookup_map.find(census_tract_admin_code);
  if(itr == Census_Tract::lookup_map.end()) {
    census_tract = new Census_Tract(census_tract_admin_code);
    Census_Tract::census_tracts.push_back(census_tract);
    Census_Tract::lookup_map[census_tract_admin_code] = census_tract;
  } else {
    census_tract = itr->second;
  }
  return census_tract;
}

/**
 * Calls the setup() function for each census tract in the census tracts vector.
 */
void Census_Tract::setup_census_tracts() {
  // set each census tract's school and workplace attendance probabilities
  for(int i = 0; i < Census_Tract::get_number_of_census_tracts(); ++i) {
    Census_Tract::census_tracts[i]->setup();
  }
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Census_Tract::setup_logging() {
  if(Census_Tract::is_log_initialized) {
    return;
  }

  if(Parser::does_property_exist("census_tract_log_level")) {
    Parser::get_property("census_tract_log_level",  &Census_Tract::census_tract_log_level);
  } else {
    Census_Tract::census_tract_log_level = "OFF";
  }

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Census_Tract::census_tract_logger = std::make_unique<spdlog::logger>("census_tract_logger", 
        sink_list.begin(), sink_list.end());
    Census_Tract::census_tract_logger->set_level(
        Utils::get_log_level_from_string(Census_Tract::census_tract_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Census_Tract::census_tract_logger->trace("<{:s}, {:d}>: Census_Tract logger initialized", 
      __FILE__, __LINE__  );
  Census_Tract::is_log_initialized = true;
}
