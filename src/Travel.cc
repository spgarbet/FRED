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
// File: Travel.cc
//

#include <sstream>
#include <vector>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>

#include "Age_Map.h"
#include "Events.h"
#include "Geo.h"
#include "Global.h"
#include "Household.h"
#include "Parser.h"
#include "Person.h"
#include "Random.h"
#include "Travel.h"
#include "Utils.h"

Events* Travel::return_queue = new Events;

// static variables
char trips_per_day_file[FRED_STRING_SIZE];
char hub_file[FRED_STRING_SIZE];
double mean_trip_duration = 0;			// mean days per trip
double* Travel_Duration_Cdf = nullptr;		// cdf for trip duration
int max_Travel_Duration = 0;			// number of days in cdf
Age_Map* travel_age_prob = nullptr;
std::vector<hub_t> hubs;
int num_hubs = 0;
// a matrix containing the number of trips per day
// between hubs, read from an external file
int** trips_per_day;

bool Travel::is_log_initialized = false;
std::string Travel::travel_log_level = "";
std::unique_ptr<spdlog::logger> Travel::travel_logger = nullptr;

/**
 * Gets the properites of travel from the travel hub file and the trips per day file.
 */
void Travel::get_properties() {
  Parser::get_property("travel_hub_file", hub_file);
  Parser::get_property("trips_per_day_file", trips_per_day_file);
}

/**
 * Sets up travel by reading the relevant files. Creates an age map with a prefix of travel_age_prob. This will 
 * map the probability of travel to ages.
 *
 * @param directory _UNUSED_
 */
void Travel::setup(char* directory) {
  assert(Global::Enable_Travel);
  read_hub_file();
  read_trips_per_day_file();
  setup_travelers_per_hub();
  travel_age_prob = new Age_Map();
  travel_age_prob->read_properties("travel_age_prob");
}

/**
 * Reads in values from the hub file.
 */
void Travel::read_hub_file() {
  FILE* fp = Utils::fred_open_file(hub_file);
  if(fp == nullptr) {
    Utils::fred_abort("Help! Can't open travel_hub_file %s\n", hub_file);
  }
  Travel::travel_logger->info("reading travel_hub_file {:s}", hub_file);
  hubs.clear();
  hub_t hub;
  hub.users.clear();
  while(fscanf(fp, "%d %lf %lf %d", &hub.id, &hub.lat, &hub.lon, &hub.pop) == 4) {
    hubs.push_back(hub);
  }
  fclose(fp);
  num_hubs = (int) hubs.size();
  Travel::travel_logger->info("num_hubs = {:d}", num_hubs);
  trips_per_day = new int*[num_hubs];
  for(int i = 0; i < num_hubs; ++i) {
    trips_per_day[i] = new int [num_hubs];
  }
}

/**
 * Reads in values from the trips per day file.
 */
void Travel::read_trips_per_day_file() {
  Parser::get_property("trips_per_day_file", trips_per_day_file);
  FILE*fp = Utils::fred_open_file(trips_per_day_file);
  if(fp == nullptr) {
    Utils::fred_abort("Help! Can't open trips_per_day_file %s\n", trips_per_day_file);
  }
  Travel::travel_logger->info("reading trips_per_day_file {:s}", trips_per_day_file);
  for(int i = 0; i < num_hubs; ++i) {
    for(int j = 0; j < num_hubs; ++j) {
      int n;
      if(fscanf(fp, "%d ", &n) != 1) {
	      Utils::fred_abort("ERROR: read failed on file %s", trips_per_day_file);
      }
      trips_per_day[i][j] = n;
    }
  }
  fclose(fp);

  for(int i = 0; i < num_hubs; ++i) {
    std::stringstream ss;
    ss << fmt::format("trips_per_day[{:d}]: ", i);
    for(int j = 0; j < num_hubs; ++j) {
      ss << fmt::format("{:d} ", trips_per_day[i][j]);
    }
    Travel::travel_logger->trace("<{:s}, {:d}>: {:s}", __FILE__, __LINE__, ss.str());
  }
  Travel::travel_logger->info("finished reading trips_per_day_file {:s}", trips_per_day_file);
}

/**
 * Sets up travel hubs for all households based on distance to the nearest hub. If a hub exists within the household's 
 * county, that will be set as the households hub.
 */
void Travel::setup_travelers_per_hub() {
  int households = Place::get_number_of_households();
  Travel::travel_logger->info("Preparing to set households: {:d}", households);
  for(int i = 0; i < households; ++i) {
    Household* h = Place::get_household(i);
    double h_lat = h->get_latitude();
    double h_lon = h->get_longitude();
    long long int h_id = h->get_census_tract_admin_code();
    int h_county = h->get_county_admin_code();
    Travel::travel_logger->trace("<{:s}, {:d}>: h_id: {:d} h_county: {:d}", __FILE__, __LINE__, h_id, h_county);
    // find the travel hub closest to this household
    double max_distance = 166.0;		// travel at most 100 miles to nearest airport
    double min_dist = 100000000.0;
    int closest = -1;
    for(int j = 0; j < num_hubs; ++j) {
      double dist = Geo::xy_distance(h_lat, h_lon, hubs[j].lat, hubs[j].lon);
      if(dist < max_distance && dist < min_dist) {
        closest = j;
        min_dist = dist;
      }
      //Assign travelers to hub based on 'county' rather than distance
      if(hubs[j].id == h_county){
        closest = j;
        min_dist = dist;
      }
    }
    if(closest > -1) {
      Travel::travel_logger->debug(
          "h_id: {:d} from county: {:d}  assigned to the airport: {:d}, distance:  {:f}",
          h_id, h_county, hubs[closest].id, min_dist);
      // add everyone in the household to the user list for this hub
      int Housemates = h->get_size();
      for(int k = 0; k < Housemates; ++k) {
        Person* person = h->get_member(k);
        hubs[closest].users.push_back(person);
      }
    }
  }

  // adjustment for partial user base
  for(int i = 0; i < num_hubs; ++i) {
    hubs[i].pct = 0.5 + (100.0 * hubs[i].users.size()) / hubs[i].pop;
  }
  // print hubs
  for(int i = 0; i < num_hubs; ++i) {
    Travel::travel_logger->info("Hub {:d}: lat = {:f} lon = {:f} users = {:d} pop = {:d} pct = {:d}",
        hubs[i].id, hubs[i].lat, hubs[i].lon, static_cast<int>(hubs[i].users.size()),
        hubs[i].pop, hubs[i].pct);
  }
}

/**
 * Updates travel for the given day. For every pair of hubs, travel is attempted from the first hub to the second. 
 * Random people from the first hub's users will be selected, and will travel based on the travel probability for their age. 
 * A random host will be selected from the second hub's users to host the traveller. The travel duration is randomly 
 * selected from a distribution, and their return even is created for the traveller.
 *
 * @param day the day
 */
void Travel::update_travel(int day) {

  if(!Global::Enable_Travel) {
    return;
  }

  Travel::travel_logger->info("update_travel entered day {:d}",day);

  // initiate new trips
  for(int i = 0; i < num_hubs; ++i) {
    if(hubs[i].users.size() == 0) {
      continue;
    }
    for(int j = 0; j < num_hubs; ++j) {
      if(hubs[j].users.size() == 0) {
        continue;
      }
      int successful_trips = 0;
      int count = (trips_per_day[i][j] * hubs[i].pct + 0.5) / 100;
      Travel::travel_logger->debug("TRIPCOUNT day {:d} i {:d} j {:d} count {:d}", day, i, j, count);
      for(int t = 0; t < count; ++t) {
        // select a potential traveler determined by travel_age_prob property
        Person* traveler = nullptr;
        Person* host = nullptr;
        int attempts = 0;
        while(traveler == nullptr && attempts < 100) {
          // select a random member of the source hub's user group
          int v = Random::draw_random_int(0, static_cast<int>(hubs[i].users.size()) - 1);
          traveler = hubs[i].users[v];
          double prob_travel_by_age = travel_age_prob->find_value(traveler->get_real_age());
          if(prob_travel_by_age < Random::draw_random()) {
            traveler = nullptr;
          }
          attempts++;
        }
        if(traveler != nullptr) {
          // select a potential travel host determined by travel_age_prob property
          attempts = 0;
          while(host == nullptr && attempts < 100) { // redundant
            // select a random member of the destination hub's user group
            int v = Random::draw_random_int(0, static_cast<int>(hubs[j].users.size()) - 1);
            host = hubs[j].users[v];

            //	          double prob_travel_by_age = travel_age_prob->find_value(host->get_real_age());
            //	          if(prob_travel_by_age < Random::draw_random()) {
            //	            host = nullptr;
            //	          }

            attempts++;
          }
        }
        // travel occurs only if both traveler and host are not already traveling
        if(traveler != nullptr && (!traveler->get_travel_status()) &&
          host != nullptr && (!host->get_travel_status())) {
          // put traveler in travel status
          traveler->start_traveling(host);
          if(traveler->get_travel_status()) {
            // put traveler on list for given number of days to travel
            int duration = Random::draw_from_distribution(max_Travel_Duration, Travel_Duration_Cdf);
            int return_sim_day = day + duration;
            Travel::add_return_event(return_sim_day, traveler);
            traveler->set_return_from_travel_sim_day(return_sim_day);
            Travel::travel_logger->debug(
                "RETURN_FROM_TRAVEL EVENT ADDED today {:d} duration {:d} returns {:d} id {:d} age {:d}",
                day, duration, return_sim_day, traveler->get_id(),traveler->get_age());
            successful_trips++;
          }
        }
      }
      Travel::travel_logger->debug("DAY {:d} SRC = {:d} DEST = {:d} TRIPS = {:d}", 
          day, hubs[i].id, hubs[j].id, successful_trips);
    }
  }

  // process travelers who are returning home
  Travel::find_returning_travelers(day);

  Travel::travel_logger->info("update_travel finished");

  return;
}

/**
 * Stops traveling for each Person in the return queue for the given day.
 *
 * @param day the day
 */
void Travel::find_returning_travelers(int day) {
  int size = return_queue->get_size(24*day);
  for (int i = 0; i < size; i++) {
    Person * person = return_queue->get_event(24*day, i);
    Travel::travel_logger->debug("RETURNING FROM TRAVEL today {:d} id {:d} age {:d}",
		    day, person->get_id(), person->get_age());
    person->stop_traveling();
  }
  return_queue->clear_events(24*day);
}

/**
 * Deletes the return event for the specified Person on the day that the person returns from travel.
 *
 * @param person the person
 */
void Travel::terminate_person(Person* person) {
  if(!person->get_travel_status()) {
    return;
  }
  int return_day = person->get_return_from_travel_sim_day();
  assert(Global::Simulation_Day <= return_day);
  Travel::delete_return_event(return_day, person);
}

/**
 * _UNUSED_
 */
void Travel::quality_control(char* directory) {
}

/**
 * Adds a return event to the return queue for the specified Person on the given day.
 *
 * @param day the day
 * @param person the person
 */
void Travel::add_return_event(int day, Person* person) {
  Travel::return_queue->add_event(24*day, person);
}

/**
 * Deletes the return event from the return queue for the specified Person on the given day.
 *
 * @param day the day
 * @param person the person
 */
void Travel::delete_return_event(int day, Person* person) {
  Travel::return_queue->delete_event(24*day, person);
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Travel::setup_logging() {
  if(Travel::is_log_initialized) {
    return;
  }

  if(Parser::does_property_exist("travel_log_level")) {
    Parser::get_property("travel_log_level", &Travel::travel_log_level);
  } else {
    Travel::travel_log_level = "OFF";
  }

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Travel::travel_logger = std::make_unique<spdlog::logger>("travel_logger", 
        sink_list.begin(), sink_list.end());
    Travel::travel_logger->set_level(
        Utils::get_log_level_from_string(Travel::travel_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Travel::travel_logger->trace("<{:s}, {:d}>: Travel logger initialized", 
      __FILE__, __LINE__  );
  Travel::is_log_initialized = true;
}
