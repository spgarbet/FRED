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
// File: Group_Type.cc
//

#include <spdlog/spdlog.h>

#include "Condition.h"
#include "Date.h"
#include "Group.h"
#include "Group_Type.h"
#include "Parser.h"
#include "Person.h"
#include "Utils.h"

//////////////////////////
//
// STATIC VARIABLES
//
//////////////////////////

std::vector <Group_Type*> Group_Type::group_types;
std::vector<std::string> Group_Type::names;
std::unordered_map<std::string, int> Group_Type::group_name_map;
std::unordered_map<Person*, Group*> Group_Type::host_group_map;

bool Group_Type::is_log_initialized = false;
std::string Group_Type::group_type_log_level = "";
std::unique_ptr<spdlog::logger> Group_Type::group_type_logger = nullptr;

/**
 * Creates a Group_Type with the specified name. Initializes default variables.
 *
 * @param _name the name
 */
Group_Type::Group_Type(std::string _name) {
  this->name = _name;
  this->proximity_contact_rate = 0.0;
  this->proximity_same_age_bias = 0.0;
  this->can_transmit_cond = nullptr;
  this->contact_count_for_cond = nullptr;
  this->contact_rate_for_cond = nullptr;
  this->density_contact_prob = nullptr;
  this->deterministic_contacts_for_cond = nullptr;
  this->density_transmission_for_cond = nullptr;
  this->has_admin = false;
  Group_Type::group_name_map[this->name] = Group_Type::get_number_of_group_types();
  Group_Type::names.push_back(this->name);
}

/**
 * Default destructor.
 */
Group_Type::~Group_Type() {
}

/**
 * Gets properties of this group type.
 */
void Group_Type::get_properties() {
  Group_Type::group_type_logger->info("group_type {:s} get_properties entered", this->name.c_str());

  char property_name[FRED_STRING_SIZE];
  //char property_value[FRED_STRING_SIZE];

  // optional properties:
  Parser::disable_abort_on_failure();

  this->file_available = 0;
  snprintf(property_name, FRED_STRING_SIZE, "%s.file_available", this->name.c_str());
  Parser::get_property(property_name, &this->file_available);

  this->proximity_contact_rate = 0.0;
  snprintf(property_name, FRED_STRING_SIZE, "%s.contacts", this->name.c_str());
  Parser::get_property(property_name, &this->proximity_contact_rate);

  this->global_density_contact_prob = 1.0;
  snprintf(property_name, FRED_STRING_SIZE, "%s.density_contact_prob", this->name.c_str());
  Parser::get_property(property_name, &this->global_density_contact_prob);

  this->proximity_same_age_bias = 0.0;
  snprintf(property_name, FRED_STRING_SIZE, "%s.same_age_bias", this->name.c_str());
  Parser::get_property(property_name, &this->proximity_same_age_bias);

  printf("%s.contacts = %f\n", this->name.c_str(), this->proximity_contact_rate);

  int number_of_conditions = Condition::get_number_of_conditions();
  this->can_transmit_cond = new int [number_of_conditions];
  this->contact_count_for_cond = new int [number_of_conditions];
  this->contact_rate_for_cond = new double [number_of_conditions];
  this->density_contact_prob = new double [number_of_conditions];
  this->deterministic_contacts_for_cond = new bool [number_of_conditions];
  this->density_transmission_for_cond = new bool [number_of_conditions];

  for(int cond_id = 0; cond_id < number_of_conditions; ++cond_id) {
    std::string cond_name = Condition::get_name(cond_id);

    this->can_transmit_cond[cond_id] = 0;
    snprintf(property_name, FRED_STRING_SIZE, "%s.can_transmit_%s", this->name.c_str(), cond_name.c_str());
    Parser::get_property(property_name, &this->can_transmit_cond[cond_id]);
    Group_Type::group_type_logger->info("{:s} = {:d}", property_name, this->can_transmit_cond[cond_id]);

    this->contact_count_for_cond[cond_id] = 0;
    snprintf(property_name, FRED_STRING_SIZE, "%s.contact_count_for_%s", this->name.c_str(), cond_name.c_str());
    Parser::get_property(property_name, &this->contact_count_for_cond[cond_id]);
    Group_Type::group_type_logger->info("{:s} = {:d}", property_name, this->contact_count_for_cond[cond_id]);

    this->contact_rate_for_cond[cond_id] = 0.0;
    snprintf(property_name, FRED_STRING_SIZE, "%s.contact_rate_for_%s", this->name.c_str(), cond_name.c_str());
    Parser::get_property(property_name, &this->contact_rate_for_cond[cond_id]);
    Group_Type::group_type_logger->info("{:s} = {:f}", property_name, this->contact_rate_for_cond[cond_id]);

    this->density_contact_prob[cond_id] = this->global_density_contact_prob;
    snprintf(property_name, FRED_STRING_SIZE, "%s.density_contact_prob_for_%s", this->name.c_str(), cond_name.c_str());
    Parser::get_property(property_name, &this->density_contact_prob[cond_id]);
    Group_Type::group_type_logger->info("{:s} = {:f}", property_name, this->density_contact_prob[cond_id]);

    this->deterministic_contacts_for_cond[cond_id] = true;
    int n = 0;
    snprintf(property_name, FRED_STRING_SIZE, "%s.deterministic_contacts_for_%s", this->name.c_str(), cond_name.c_str());
    Parser::get_property(property_name, &n);
    this->deterministic_contacts_for_cond[cond_id] = n;
    Group_Type::group_type_logger->info("{:s} = {:d}", property_name, this->deterministic_contacts_for_cond[cond_id]);

    this->density_transmission_for_cond[cond_id] = false;
    n = 0;
    snprintf(property_name, FRED_STRING_SIZE, "%s.density_transmission_for_%s", this->name.c_str(), cond_name.c_str());
    Parser::get_property(property_name, &n);
    this->density_transmission_for_cond[cond_id] = n;
    Group_Type::group_type_logger->info("{:s} = {:d}", property_name, this->density_transmission_for_cond[cond_id]);
  }

  // group type schedule
  for(int day = 0; day < 7; ++day) {
    for(int hour = 0; hour < 24; ++hour) {
      this->open_at_hour[day][hour] = 0;
      this->starts_at_hour[day][hour] = 0;
      std::string dayname;
      switch(day) {
        case 0:
          dayname = "Sun";
          break;
        case 1:
          dayname = "Mon";
          break;
        case 2:
          dayname = "Tue";
          break;
        case 3:
          dayname = "Wed";
          break;
        case 4:
          dayname = "Thu";
          break;
        case 5:
          dayname = "Fri";
          break;
        case 6:
          dayname = "Sat";
          break;
      }
      snprintf(property_name, FRED_STRING_SIZE, "%s.starts_at_hour_%d_on_%s", this->name.c_str(), hour, dayname.c_str());
      if(Parser::does_property_exist(property_name)) {
        Parser::get_property(property_name, &this->starts_at_hour[day][hour]);
        Group_Type::group_type_logger->info("{:s} = {:d}", property_name, this->starts_at_hour[day][hour]);
      }
    }
  }

  // shortcuts
  for(int hour = 0; hour < 24; ++hour) {
    snprintf(property_name, FRED_STRING_SIZE, "%s.starts_at_hour_%d_on_weekdays", this->name.c_str(), hour);
    if(Parser::does_property_exist(property_name)) {
      Parser::get_property(property_name, &this->starts_at_hour[1][hour]);
      Parser::get_property(property_name, &this->starts_at_hour[2][hour]);
      Parser::get_property(property_name, &this->starts_at_hour[3][hour]);
      Parser::get_property(property_name, &this->starts_at_hour[4][hour]);
      Parser::get_property(property_name, &this->starts_at_hour[5][hour]);
    }
    snprintf(property_name, FRED_STRING_SIZE, "%s.starts_at_hour_%d_on_weekends", this->name.c_str(), hour);
    if(Parser::does_property_exist(property_name)) {
      Parser::get_property(property_name, &this->starts_at_hour[0][hour]);
      Parser::get_property(property_name, &this->starts_at_hour[6][hour]);
      Group_Type::group_type_logger->info("{:s} = {:d}", property_name, this->starts_at_hour[0][hour]);
    }
  }

  for(int day = 0; day < 7; ++day) {
    for(int hour = 0; hour < 24; ++hour) {
      int hr = hour;
      int d = day;
      for(int i = 0; i < this->starts_at_hour[day][hour]; ++i) {
        this->open_at_hour[d][hr] = 1;
        ++hr;
        if(hr == 24) {
          hr = 0;
          ++d;
          if(d == 7) {
            d = 0;
          }
        }
      }
    }
  }

  for(int day = 0; day < 7; ++day) {
    for(int hour = 0; hour < 24; ++hour) {
      if(this->starts_at_hour[day][hour]) {
        Group_Type::group_type_logger->info("{:s} hour {:d} day {:d} time_block {:d}", 
            this->name.c_str(), hour, day, this->starts_at_hour[day][hour]);
      }
    }
  }

  // does this group type have adminstrators?
  int n = 0;
  snprintf(property_name, FRED_STRING_SIZE, "%s.has_administrator", this->name.c_str());
  Parser::get_property(property_name, &n);
  this->has_admin = n;

  // if this group type has an admin, then create a global list variable for the type
  Group_Type::group_type_logger->info("ADMIN group_type {:s} has_admin = {:d}", this->name.c_str(), 
      this->has_admin);
  if(this->has_admin) {
    Person::include_global_list_variable(this->name + "List");
  }

  Parser::set_abort_on_failure();

  Group_Type::group_type_logger->info("group_type {:s} read_properties finished", this->name.c_str());
}

/**
 * Checks if this group type is open at the current time in the simulation.
 *
 * @return if this group type is open
 */
bool Group_Type::is_open() {
  return this->open_at_hour[Date::get_day_of_week(Global::Simulation_Day)][Global::Simulation_Hour];
}

/**
 * Gets the time block of this group type at the given day and hour.
 *
 * @param day the day
 * @param hour the hour
 * @return the time block
 */
int Group_Type::get_time_block(int day, int hour) {
  int weekday = Date::get_day_of_week(day);
  int value = this->starts_at_hour[weekday][hour];
  Group_Type::group_type_logger->info("get_time_block {:s} day {:d} day_of_week {:d} hour {:d} value {:d}", 
      this->name.c_str(), day, weekday, hour, value);
  return value;
}

//////////////////////////
//
// STATIC METHODS
//
//////////////////////////

/**
 * Gets the ID of the Group_Type with the specified name.
 *
 * @param type_name the group type name
 * @return the ID
 */
int Group_Type::get_type_id(std::string type_name) {
  std::unordered_map<std::string, int>::const_iterator found = Group_Type::group_name_map.find(type_name);
  if(found == Group_Type::group_name_map.end()) {
    Group_Type::group_type_logger->error("Help: GROUP_TYPE can't find a group type named {:s}", 
        type_name.c_str());
    return -1;
  } else {
    return found->second;
  }
}

/**
 * Gets the Group hosted by the specified Person from the host group map.
 *
 * @param person the person
 * @return the group
 */
Group* Group_Type::get_group_hosted_by(Person* person) {
  std::unordered_map<Person*, Group*>::const_iterator found = Group_Type::host_group_map.find(person);
  if(found != Group_Type::host_group_map.end()) {
    Group* group = found->second;
    return group;
  } else {
    return nullptr;
  }
}

/**
 * Adds the given Person : Group pair to the host group map.
 *
 * @param person the person
 * @param group the group
 */
void Group_Type::add_group_hosted_by(Person* person, Group* group) {
  Group_Type::host_group_map[person] = group;
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Group_Type::setup_logging() {
  if(Group_Type::is_log_initialized) {
    return;
  }

  if(Parser::does_property_exist("group_type_log_level")) {
    Parser::get_property("group_type_log_level", &Group_Type::group_type_log_level);
  } else {
    Group_Type::group_type_log_level = "OFF";
  }

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Group_Type::group_type_logger = std::make_unique<spdlog::logger>("group_type_logger", 
        sink_list.begin(), sink_list.end());
    Group_Type::group_type_logger->set_level(
        Utils::get_log_level_from_string(Group_Type::group_type_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Group_Type::group_type_logger->trace("<{:s}, {:d}>: Group_Type logger initialized", 
      __FILE__, __LINE__  );
  Group_Type::is_log_initialized = true;
}
