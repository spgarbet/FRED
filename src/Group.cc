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
// File: Group.cc
//

#include <list>

#include <spdlog/spdlog.h>

#include "Group.h"
#include "Condition.h"
#include "Parser.h"
#include "Person.h"
#include "Utils.h"

char Group::TYPE_UNSET = 'U';

char Group::SUBTYPE_NONE = 'X';
std::map<long long int, Group*> Group::sp_id_map;

bool Group::is_log_initialized = false;
std::string Group::group_log_level = "";
std::unique_ptr<spdlog::logger> Group::group_logger = nullptr;

/**
 * Creates a Group with the specified label and Group_Type. Initializes 
 * default variables.
 *
 * @param lab the label
 * @param _type_id the group type ID
 */
Group::Group(const char* lab, int _type_id) {
  this->sp_id = -1;
  this->index = -1;
  this->id = -1;
  this->type_id = _type_id;
  this->subtype = Group::SUBTYPE_NONE;

  strcpy(this->label, lab);

  this->N_orig = 0;             // orig number of members
  this->income = -1;

  // lists of people
  this->members.clear();

  int conditions = Condition::get_number_of_conditions();
  this->transmissible_people = new person_vector_t[conditions];

  // epidemic counters
  this->first_transmissible_count = new int[conditions];
  this->first_susceptible_count = new int[conditions];
  this->first_transmissible_day = new int[conditions];
  this->last_transmissible_day = new int[conditions];

  // zero out all condition-specific counts
  for(int d = 0; d < conditions; ++d) {
    this->transmissible_people[d].clear();
    this->first_transmissible_count[d] = 0;
    this->first_susceptible_count[d] = 0;
    this->first_transmissible_day[d] = -1;
    this->last_transmissible_day[d] = -2;
  }

  this->size_change_day.clear();
  this->size_on_day.clear();
  this->reporting_size = false;
  this->admin = nullptr;
  this->host = nullptr;
  this->contact_factor = 1.0;
}

/**
 * Deletes the transmissible people vector before destruction.
 */
Group::~Group() {
  if(this->transmissible_people != nullptr) {
    delete this->transmissible_people;
  } // redundant ???
  if(this->transmissible_people != nullptr) {
    delete this->transmissible_people;
  }
  if(this->transmissible_people != nullptr) {
    delete this->transmissible_people;
  }
}

/**
 * Adds the specified Person as a member of this group. Returns the index that the person was added at.
 *
 * @param per the person
 * @return the index
 */
int Group::begin_membership(Person* per) {
  if(static_cast<int>(this->get_size()) == static_cast<int>(this->members.capacity())) {
    // double capacity if needed (to reduce future reallocations)
    this->members.reserve(2 * this->get_size());
  }
  this->members.push_back(per);
  Group::group_logger->info("Enroll person {:d} age {:d} in group {:d} {:s}", per->get_id(), 
      per->get_age(), this->get_id(), this->get_label());
  return this->members.size() - 1;
}

/**
 * Removes the Person at the specified index as a member of this group.
 *
 * @param pos the index
 */
void Group::end_membership(int pos) {
  int size = this->members.size();
  if(!(0 <= pos && pos < size)) {
    Group::group_logger->debug("group {:d} {:s} pos = {:d} size = {:d}", this->get_id(), this->get_label(), 
        pos, size);
  }
  assert(0 <= pos && pos < size);
  Person* removed = this->members[pos];
  if(pos < size - 1) {
    Person* moved = this->members[size - 1];
    Group::group_logger->debug("UNENROLL group {:d} {:s} pos = {:d} size = {:d} removed {:d} moved {:d}",
       this->get_id(), this->get_label(), pos, size, removed->get_id(), moved->get_id());
    this->members[pos] = moved;
    moved->update_member_index(this, pos);
  } else {
    Group::group_logger->debug("UNENROLL group {:d} {:s} pos = {:d} size = {:d} removed {:d} moved NONE",
        this->get_id(), this->get_label(), pos, size, removed->get_id());
  }
  this->members.pop_back();
  Group::group_logger->info("UNENROLL group {:d} {:s} size = {:d}", this->get_id(), this->get_label(), 
      this->members.size());
}

/**
 * Prints the transmissible people of this group with the specified Condition.
 *
 * @param condition_id the condition ID
 */
void Group::print_transmissible(int condition_id) {
  Group::group_logger->info("INFECTIOUS in Group {:s} Condition {:d}:", this->get_label(), condition_id);
  int size = this->transmissible_people[condition_id].size();
  for(int i = 0; i < size; ++i) {
    Group::group_logger->info("{:d}", this->transmissible_people[condition_id][i]->get_id());
  }
}

/**
 * Gets the number of children in this group.
 *
 * @return the number of children
 */
int Group::get_children() {
  int children = 0;
  for(int i = 0; i < static_cast<int>(this->members.size()); ++i) {
    children += (this->members[i]->get_age() < Global::ADULT_AGE ? 1 : 0);
  }
  return children;
}

/**
 * Gets the number of adults in this group.
 *
 * @return the number of adults
 */
int Group::get_adults() {
  return (this->members.size() - this->get_children());
}

/**
 * Adds the specified Person as a transmissible person in this group with the specified Condition.
 *
 * @param condition_id the condition ID
 * @param person the person
 */
void Group::add_transmissible_person(int condition_id, Person* person) {
  Group::group_logger->info("ADD_INF: person {:d} mix_group {:s}", person->get_id(), this->label);
  this->transmissible_people[condition_id].push_back(person);
}

/**
 * Records the specified day as a transmissible day of the specified Condition. If this is the first 
 * day recorded as transmissible for the condition, the day will be set as the first transmissible day. 
 * Otherwise, the day will be set to the last transmissible day, which is subject to change.
 *
 * @param day the day
 * @param condition_id the condition ID
 */
void Group::record_transmissible_days(int day, int condition_id) {
  if(this->first_transmissible_day[condition_id] == -1) {
    this->first_transmissible_day[condition_id] = day;
    this->first_transmissible_count[condition_id] = get_number_of_transmissible_people(condition_id);
    this->first_susceptible_count[condition_id] = get_size() - get_number_of_transmissible_people(condition_id);
  }
  this->last_transmissible_day[condition_id] = day;
}

/**
 * Gets the sum of the personal variables with the specified variable index of all members of this group.
 *
 * @param var_id the variable index
 * @return the sum
 */
double Group::get_sum_of_var(int var_id) {
  double sum = 0.0;
  for(int i = 0; i < static_cast<int>(this->members.size()); ++i) {
    Person* person = this->members[i];
    sum += person->get_var(var_id);
  }
  return sum;
}

/**
 * Gets the median of the personal variables with the specified variable index of all members of this group.
 *
 * @param var_id the variable index
 * @return the median
 */
double Group::get_median_of_var(int var_id) {
  int size = get_size();
  double median = 0.0;
  if(size > 0) {
    std::vector<double> values;
    values.clear();
    for(int i = 0; i < static_cast<int>(this->members.size()); ++i) {
      Person* person = this->members[i];
      values.push_back(person->get_var(var_id));
    }
    std::sort(values.begin(), values.end());
    median = values[size / 2];
  }
  return median;
}

/**
 * Reports the size of this group for the specified day, if the reported size for the day is not already 
 * accurate. The size for the day will be reported as a size on day, and the day will be reported as a 
 * size change day.
 *
 * @param day the day
 */
void Group::report_size(int day) {
  int vec_size = this->size_on_day.size();
  if(vec_size == 0 || get_size() != this->size_on_day[vec_size - 1]) {
    this->size_change_day.push_back(day);
    this->size_on_day.push_back(get_size());
  }
}

/**
 * Gets the size of this group on a specified day.
 *
 * @param day the day
 * @return the size
 */
int Group::get_size_on_day(int day) {
  int size = 0;
  int vec_size = this->size_on_day.size();
  for(int i = 0; i < vec_size; ++i) {
    if(this->size_change_day[i] > day) {
      return size;
    } else {
      size = this->size_on_day[i];
    }
  }
  return size;
}

/**
 * Gets the proximity same age bias of this group's associated Group_Type.
 *
 * @return the proximity same age bias
 */
double Group::get_proximity_same_age_bias() {
  return Group_Type::get_group_type(this->type_id)->get_proximity_same_age_bias();
}

/**
 * Gets the density contact probability of the specified Condition for this group's associated Group_Type.
 *
 * @param condition_id the condition ID
 * @return the density contact probability
 */
double Group::get_density_contact_prob(int condition_id) {
  double result = Group_Type::get_group_type(this->type_id)->get_density_contact_prob(condition_id);
  if(this->contact_factor != 1.0) {
    result *= this->contact_factor;
  }
  return result;
}

/**
 * Gets the proximity contact rate of this group's associated Group_Type.
 *
 * @return the proximity contact rate
 */
double Group::get_proximity_contact_rate() {
  double result = Group_Type::get_group_type(this->type_id)->get_proximity_contact_rate();
  if (this->contact_factor != 1.0) {
    result *= this->contact_factor;
  }
  return result;
}

/**
 * Gets the contact rate of the specified Condtion for this group's associated Group_Type.
 *
 * @param condition_id the condition ID
 * @return the contact rate
 */
double Group::get_contact_rate(int condition_id) {
  return Group_Type::get_group_type(this->type_id)->get_contact_rate(condition_id);
}

/**
 * Checks if the use of deterministic contacts is enabled for the specified Condition in 
 * this group's associated Group_Type.
 *
 * @param condition_id the condition ID
 * @return if the use of deterministic contacts is enabled
 */
bool Group::use_deterministic_contacts(int condition_id) {
  return Group_Type::get_group_type(this->type_id)->use_deterministic_contacts(condition_id);
}

/**
 * Checks if the use of density transmission is enabled for the specified Condition in 
 * this group's associated Group_Type.
 *
 * @param condition_id the condition ID
 * @return if the use of density transmission is enabled
 */
bool Group::use_density_transmission(int condition_id) {
  return Group_Type::get_group_type(this->type_id)->use_density_transmission(condition_id);
}

/**
 * Checks if this group's associated Group_Type can transmit the specified Condition.
 *
 * @param condition_id the condition ID
 * @return if the group type can transmit
 */
bool Group::can_transmit(int condition_id) {
  return Group_Type::get_group_type(this->type_id)->can_transmit(condition_id);
}

/**
 * Gets this group's associated Group_Type's contact count for the specified Condition.
 *
 * @param condition_id the condition ID
 * @return the contact count
 */
int Group::get_contact_count(int condition_id) {
  return Group_Type::get_group_type(this->type_id)->get_contact_count(condition_id);
}

/**
 * Gets this group's associated Group_Type.
 *
 * @return the group type ID
 */
Group_Type* Group::get_group_type() {
  return Group_Type::get_group_type(this->type_id);
}

/**
 * Creates an administrator for the group.
 */
void Group::create_administrator() {

  if(Group::get_group_type()->has_administrator()) {
    
    Group::group_logger->info("CREATE_ADMIN group {:s} entered, admin = {:s}", this->get_label(), 
        this->admin==nullptr? "NULL":"NOT nullptr");
    if(this->admin != nullptr) {
      Group::group_logger->info("CREATE_ADMIN group {:s} ALREADY EXISTS: admin person {:d} admin_group {:s}", 
          this->get_label(), admin->get_id(), admin->get_admin_group()->get_label());
      return;
    }
    
    // generate a meta_agent
    this->admin = Person::create_admin_agent();
    
    this->admin->set_admin_group(this);
    
  }
  
  return;
}

/**
 * Checks if this group is open. This will be true if the group's Group_Type is open and the group does not have admin closure.
 *
 * @return if the group is open
 */
bool Group::is_open() {

  Group_Type* group_type = Group_Type::get_group_type(this->type_id);
  if(group_type->is_open() == false) {
    Group::group_logger->debug("group {:s} is closed at hour {:d} day {:d} because group_type is closed", Global::Simulation_Hour,
        Global::Simulation_Day, this->get_label());
    return false;
  }

  if(has_admin_closure()) {
    Group::group_logger->debug("group {:s} is closed due to admin closure", this->get_label());
    return false;
  }

  Group::group_logger->debug("group {:s} is open", this->get_label());
  return true;
}

/**
 * Checks if this group has admin closure.
 *
 * @return if the group has admin closure
 */
bool Group::has_admin_closure() {
  if(this->admin != nullptr) {
    if(this->admin->has_closure()) {
      return true;
    } else {
      return false;
    }
  }
  return false;
}

/**
 * Checks if this group is a Place by ensuring this group's Group_Type is a Place_Type.
 *
 * @return if this group is a place
 */
bool Group::is_a_place() {
  return (get_type_id() < Place_Type::get_number_of_place_types());
}

/**
 * Checks if this group is a Network by ensuring this group's Group_Type is a Network_Type.
 *
 * @return if this group is a network
 */
bool Group::is_a_network() {
  return (Place_Type::get_number_of_place_types() <= get_type_id());
}

/**
 * Checks if the specified Group_Type is a Place_Type.
 *
 * @param type_id the group type ID
 * @return if the group type is a place type
 */
bool Group::is_a_place(int type_id) {
  return (type_id < Place_Type::get_number_of_place_types());
}

/**
 * Checks if the specified Group_Type is a Network_Type.
 *
 * @param type_id the group type ID
 * @return if the group type is a network type
 */
bool Group::is_a_network(int type_id) {
  return (Place_Type::get_number_of_place_types() <= type_id);
}

/**
 * Sets the Synthetic Population ID of this group. 
 * There is a static map that is used to avoid duplications. If the value is a duplicate, a warning message will be created.
 *
 * @param value the SP ID
 */
void Group::set_sp_id(long long int value) {
  this->sp_id = value;
  if(Group::sp_id_map.insert(std::make_pair(value, this)).second == false) {
    // Note - we will probably have duplicates when we use multiple counties that border each other, since there are many people who work or go to school
    // across borders. If we use Utils::print_error(), then we set the  Global::Error_found flag to true, which will cause the simulation to abort.
    // To avoid this, we are simply writing a warning instead.
    char msg[FRED_STRING_SIZE];
    snprintf(msg, FRED_STRING_SIZE, "Place id %lld is duplicated for two places: %s and %s", value, get_label(), Group::sp_id_map[value]->get_label());
    Utils::print_warning(msg);
    Group::group_logger->warn("{:s}", msg);
  }
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Group::setup_logging() {
  if(Group::is_log_initialized) {
    return;
  }

  if(Parser::does_property_exist("group_log_level")) {
    Parser::get_property("group_log_level", &Group::group_log_level);
  } else {
    Group::group_log_level = "OFF";
  }

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Group::group_logger = std::make_unique<spdlog::logger>("group_logger", 
        sink_list.begin(), sink_list.end());
    Group::group_logger->set_level(
        Utils::get_log_level_from_string(Group::group_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Group::group_logger->trace("<{:s}, {:d}>: Group logger initialized", 
      __FILE__, __LINE__  );
  Group::is_log_initialized = true;
}
