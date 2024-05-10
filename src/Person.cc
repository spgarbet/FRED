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
// File: Person.cc
//

#include <array>
#include <cstdio>
#include <sstream>
#include <unordered_set>
#include <vector>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>

#include "Person.h"

#include "Census_Tract.h"
#include "Condition.h"
#include "County.h"
#include "Date.h"
#include "Demographics.h"
#include "Epidemic.h"
#include "Factor.h"
#include "Global.h"
#include "Expression.h"
#include "Geo.h"
#include "Group.h"
#include "Group_Type.h"
#include "Household.h"
#include "Link.h"
#include "Neighborhood_Layer.h"
#include "Neighborhood_Patch.h"
#include "Network.h"
#include "Network_Type.h"
#include "Parser.h"
#include "Place.h"
#include "Place_Type.h"
#include "Preference.h"
#include "Random.h"
#include "Rule.h"
#include "Travel.h"
#include "Utils.h"

// static variables
person_vector_t Person::people;
person_vector_t Person::admin_agents;
person_vector_t Person::death_list;
person_vector_t Person::migrant_list;
person_vector_t Person::report_person;
std::vector<int> Person::id_map;
std::vector<report_t*> Person::report_vec;
int Person::max_reporting_agents = 100;
int Person::pop_size = 0;
int Person::next_id = 0;
int Person::next_meta_id = -2;
Person* Person::Import_agent = nullptr;
std::unordered_map<Person*, Group*> Person::admin_group_map;
bool Person::record_location = false;
  
// personal variables
std::vector<std::string> Person::var_name;
int Person::number_of_vars = 0;
std::vector<std::string> Person::list_var_name;
int Person::number_of_list_vars = 0;
Expression** Person::var_expr;
Expression** Person::list_var_expr;

// global variables
std::vector<std::string> Person::global_var_name;
double* Person::global_var;
int Person::number_of_global_vars = 0;
std::vector<std::string> Person::global_list_var_name;
double_vector_t* Person::global_list_var;
int Person::number_of_global_list_vars = 0;

// used during input
bool Person::load_completed = false;
int Person::enable_copy_files = 0;
  
  // output data
int Person::report_initial_population = 0;
int Person::output_population = 0;
char Person::pop_outfile[FRED_STRING_SIZE];
char Person::output_population_date_match[FRED_STRING_SIZE];
int Person::Popsize_by_age [Demographics::MAX_AGE + 1];

// static variables
bool Person::is_initialized = false;

bool Person::is_log_initialized = false;
std::string Person::person_log_level = "";
std::unique_ptr<spdlog::logger> Person::person_logger = nullptr;

/**
 * Creates a Person with default variables.
 */
Person::Person() {
  this->sp_id = std::string("XXXXXXXXXX");
  this->id = -1;
  this->index = -1;
  this->eligible_to_migrate = true;
  this->native = true;
  this->original = false;
  this->vaccine_refusal = false;
  this->ineligible_for_vaccine = false;
  this->received_vaccine = false;
  this->init_age = -1;
  this->sex = 'n';
  this->birthday_sim_day = -1;
  this->deceased = false;
  this->household_relationship = -1;
  this->race = -1;
  this->number_of_children = -1;
  this->alive = true;
  this->previous_infection_serotype = 0;
  this->condition = nullptr;
  this->number_of_conditions = -1;
  this->var = nullptr;
  this->list_var = nullptr;
  this->home_neighborhood = nullptr;
  this->profile = Activity_Profile::UNDEFINED;
  this->schedule_updated = -1;
  this->stored_activity_groups = nullptr;
  this->primary_healthcare_facility = nullptr;
  this->is_traveling = false;
  this->is_traveling_outside = false;
  this->is_hospitalized = false;
  this->sim_day_hospitalization_ends = -1;
  this->last_school = nullptr;
  this->return_from_travel_sim_day = -1;
  this->in_parents_home = false;
  this->link = new Link[Group_Type::get_number_of_group_types()];
}

/**
 * Default destructor.
 */
Person::~Person() {
}

/**
 * Sets up this person for the simulation. Initializes properties passed through the parameters.
 *
 * @param _sp_id the synthetic population id
 * @param _index the popultation index
 * @param _id the ID
 * @param _age the age
 * @param _sex the sex
 * @param _race the race
 * @param rel the household relationship
 * @param house the household
 * @param school the school
 * @param work the workplace
 * @param day the day
 * @param today_is_birthday if today is the person's birthday; if not, birthday will be calculated from age
 */
void Person::setup(std::string _sp_id, int _index, int _id, int _age, char _sex,
   int _race, int rel, Place* house, Place* school, Place* work,
   int day, bool today_is_birthday) {
  Person::person_logger->info("Person::setup() sp_id {:s} id {:d} age {:d} house {:s} school {:s} work {:s}",
      _sp_id.c_str(), _id, _age, (house ? house->get_label() : "NULL"),
      (school ? school->get_label() : "NULL"), (work ? work->get_label() : "NULL"));
  this->index = _index;
  this->id = _id;
  this->sp_id = _sp_id;

  // adjust age for those over 89 (due to binning in the synthetic pop)
  if(0 <= this->id && _age > 89) {
    _age = 90;
    while(_age < Demographics::MAX_AGE && Random::draw_random() < 0.6) {
      ++_age;
    }
  }

  // set demographic variables
  this->init_age = _age;
  this->sex = _sex;
  this->race = _race;
  this->household_relationship = rel;
  this->deceased = false;
  this->number_of_children = 0;

  if(today_is_birthday) {
    this->birthday_sim_day = day;
  } else {
    // set the agent's birthday relative to simulation day
    this->birthday_sim_day = day - 365 * _age;
    // adjust for leap years:
    this->birthday_sim_day -= (_age / 4);
    // pick a random birthday in the previous year
    this->birthday_sim_day -= Random::draw_random_int(1,365);
  }
  this->setup_conditions();
  // meta_agents have no activities
  if(0 <= this->id) {
    this->setup_activities(house, school, work);
  }
}

/**
 * Prints details on the person to the specified file.
 *
 * @param fp the file
 * @param condition_id _UNUSED_
 */
void Person::print(FILE* fp, int condition_id) {
  if(fp == nullptr) {
    return;
  }
  fprintf(fp, "id %7d  age %3d  sex %c  race %d\n", id, this->get_age(), this->get_sex(), this->get_race());
  fflush(fp);
}

/**
 * Gets the number of people who share a Group of the specified Group_Type with this person that are in 
 * the specified state of the specified Condition.
 *
 * @param type_id the group type ID
 * @param condition_id the condition ID
 * @param state_id the condition state
 * @return the number of people
 */
int Person::get_number_of_people_in_group_in_state(int type_id, int condition_id, int state_id) {
  Group* group = this->get_activity_group(type_id);
  if(group != nullptr) {
    int count = 0;
    int size = group->get_size();
    for(int i = 0; i < size; ++i) {
      Person* person = group->get_member(i);
      if(person->get_state(condition_id) == state_id) {
        ++count;
      }
    }
    return count;
  }
  return 0;
}

/**
 * Gets the number of people who share a Group of the specified Group_Type with this person that are in 
 * the specified state of the specified Condition, excluding this person.
 *
 * @param type_id the group type ID
 * @param condition_id the condition ID
 * @param state_id the condition state
 * @return the number of other people
 */
int Person::get_number_of_other_people_in_group_in_state(int type_id, int condition_id, int state_id) {
  Group* group = this->get_activity_group(type_id);
  if(group != nullptr) {
    int count = 0;
    int size = group->get_size();
    for(int i = 0; i < size; ++i) {
      Person* person = group->get_member(i);
      if(person != this && person->get_state(condition_id) == state_id) {
        ++count;
      }
    }
    return count;
  }
  return 0;
}

/**
 * Creates a Person as a child of this person at the given day. Sex is chosen at random, 
 * and the baby shares the parent's race and Household. This person's number of children is incremented. 
 * The baby is added to the population and then returned.
 *
 * @param day the day
 * @return the baby
 */
Person* Person::give_birth(int day) {
  int age = 0;
  char sex = (Random::draw_random(0.0, 1.0) < 0.5 ? 'M' : 'F');
  int race = this->get_race();
  int rel = Household_Relationship::CHILD;
  Place* house = this->get_household();
  assert(house != nullptr);
  Place* school = nullptr;
  Place* work = nullptr;
  bool today_is_birthday = true;
  Person* baby = Person::add_person_to_population(Random::generate_GUID(), age, sex, race, rel, house, school, work, day, today_is_birthday);

  baby->initialize_conditions(day);

  this->number_of_children++;
  if(Global::Birthfp != nullptr) {
    // report births
    fprintf(Global::Birthfp, "day %d mother %d age %d\n", day, this->get_id(), get_age());
    fflush(Global::Birthfp);
  }

  Person::person_logger->debug("mother {:d} baby {:d}", this->get_id(), baby->get_id());
  
  if(Global::Enable_Records) {
    fprintf(Global::Recordsfp,
      "HEALTH RECORD: %s %s day %d person %d GIVES BIRTH {%s}\n",
      Date::get_date_string().c_str(),
      Date::get_12hr_clock().c_str(),
      Global::Simulation_Day,
      this->get_id(),
      this->to_string().c_str());
  }

  return baby;
}

/**
 * Returns a string representation of this person.
 *
 * @return the string representation
 */
std::string Person::to_string() {

  std::stringstream tmp_string_stream;
  tmp_string_stream << this->sp_id << " ";
  tmp_string_stream << this->id << " " << this->get_age() << " " <<  this->get_sex() << " ";
  tmp_string_stream << this->get_race() << " " ;
  tmp_string_stream << Place::get_place_label(this->get_household()) << " ";
  tmp_string_stream << Place::get_place_label(this->get_school()) << " ";
  tmp_string_stream << Place::get_place_label(this->get_classroom()) << " ";
  tmp_string_stream << Place::get_place_label(this->get_workplace()) << " ";
  tmp_string_stream << Place::get_place_label(this->get_office()) << " ";
  tmp_string_stream << Place::get_place_label(this->get_neighborhood()) << " ";
  tmp_string_stream << Place::get_place_label(this->get_hospital()) << " ";
  tmp_string_stream << this->get_household_relationship();

  return tmp_string_stream.str();
}

/**
 * Terminates this person at a given day in the simulation. Conditions and activities are also terminated.
 *
 * @param day the day
 */
void Person::terminate(int day) {
  Person::person_logger->debug("terminating person {:d}", id);
  this->terminate_conditions(day);
  this->terminate_activities();
  Demographics::terminate(this);
}

/**
 * Gets the global x value of this person's Household.
 *
 * @return the global x value
 */
double Person::get_x() {
  Place* hh = this->get_household();
  if(hh == nullptr) {
    return 0.0;
  } else {
    return hh->get_x();
  }
}

/**
 * Gets the global y value of this person's Household.
 *
 * @return the global y value
 */
double Person::get_y() {
  Place* hh = this->get_household();
  if(hh == nullptr) {
    return 0.0;
  } else {
    return hh->get_y();
  }
}

/**
 * Gets household structure label of this person's Household.
 *
 * @return the household structure label
 */
char* Person::get_household_structure_label() {
  return this->get_household()->get_household_structure_label();
}

/**
 * Gets the average income of this person's Household.
 *
 * @return the average income
 */
int Person::get_income() {
  return this->get_household()->get_income();
}

/**
 * Gets the elevation of the Place of the specified Place_Type that this person is a member of.
 *
 * @param type the place type
 * @return the elevation
 */
int Person::get_place_elevation(int type) {
  Place* place = this->get_place_of_type(type);
  if(place != nullptr) {
    return place->get_elevation();
  } else {
    return 0;
  }
}

/**
 * Gets the average income of the Place of the specified Place_Type that this person is a member of.
 *
 * @param type the place type
 * @return the average income
 */
int Person::get_place_income(int type) {
  Place* place = this->get_place_of_type(type);
  if(place != nullptr) {
    return place->get_income();
  } else {
    return 0;
  }
}

/**
 * Quits the Place of the specified Place_Type that this person is a member of. That place's group state counts 
 * are decremented.
 *
 * @param place_type_id the place type ID
 */
void Person::quit_place_of_type(int place_type_id) {

  Place* place = this->get_place_of_type(place_type_id);
  int size = place == nullptr ? -1 : place->get_size();
  Person::person_logger->info("person {:d} QUIT PLACE {:s} size {:d}", this->id, 
      (place ? place->get_label() : "NULL"), size);

  // decrement the group_state_counts associated with this place
  if(place != nullptr) {
    for(int cond_id = 0; cond_id < Condition::get_number_of_conditions(); ++cond_id) {
      int state = this->get_state(cond_id);
      Condition::get_condition(cond_id)->decrement_group_state_count(place_type_id, place, state);
    }
  }

  this->set_activity_group(place_type_id, nullptr);
  Person::person_logger->debug(
      "HEALTH RECORD: {:s} {:s} day {:d} person {:d} QUITS PLACE type {:s} label {:s} new size = {:d}",
      Date::get_date_string().c_str(), Date::get_12hr_clock().c_str(),
      Global::Simulation_Day, this->get_id(),
      Place_Type::get_place_type_name(place_type_id).c_str(),
      (place == nullptr ? "NULL" : place->get_label()), size);
  if(Global::Enable_Records) {
    fprintf(Global::Recordsfp,
      "HEALTH RECORD: %s %s day %d person %d QUITS PLACE type %s label %s new size = %d\n",
      Date::get_date_string().c_str(),
      Date::get_12hr_clock().c_str(),
      Global::Simulation_Day,
      this->get_id(),
      Place_Type::get_place_type_name(place_type_id).c_str(),
      (place == nullptr ? "NULL" : place->get_label()),
      size);
  }

  size = (place == nullptr ? -1 : place->get_size());
  Person::person_logger->info("AFTER person {:d} QUIT PLACE {:s} size {:d}", this->id, 
      (place ? place->get_label() : "NULL"), size);
}

/**
 * Joins the specified Place. If this person was already a member of a place with the same Place_Type as the new 
 * place, the old place is quit. The new place's group state counts are incremented.
 *
 * @param place the place
 */
void Person::join_place(Place* place) {

  Person::person_logger->info("JOIN_PLACE entered person {:d} place {:s}",
      this->id, place ? place->get_label() : "NULL");

  if(place == nullptr) {
    return;
  }

  if(place->get_max_size() <= place->get_size()) {
    return;
  }

  int place_type_id = place->get_place_type_id();

  Place* old_place = this->get_place_of_type(place_type_id);
  if(old_place == place) {
    return;
  }

  this->quit_place_of_type(place_type_id);
  
  int size = place==nullptr ? -1 : place->get_size();
  Person::person_logger->debug("person {:d} JOIN PLACE {:s} size {:d}", this->id, 
      place ? place->get_label() : "NULL", size);

  this->set_activity_group(place_type_id, place);

  // increment the group_state_counts associated with this place
  if(place != nullptr) {
    for(int cond_id = 0; cond_id < Condition::get_number_of_conditions(); ++cond_id) {
      int state = this->get_state(cond_id);
      Condition::get_condition(cond_id)->increment_group_state_count(place_type_id, place, state);
    }
  }
  size = (place == nullptr ? -1 : place->get_size());

  Person::person_logger->debug(
      "HEALTH RECORD: {:s} {:s} day {:d} person {:d} JOINS PLACE type {:s} label {:s} new size = {:d}",
      Date::get_date_string().c_str(),
      Date::get_12hr_clock().c_str(),
      Global::Simulation_Day,
      this->get_id(),
      Place_Type::get_place_type_name(place_type_id).c_str(),
      (place == nullptr ? "NULL" : place->get_label()),
      size);

  if(Global::Enable_Records) {
    fprintf(Global::Recordsfp,
      "HEALTH RECORD: %s %s day %d person %d JOINS PLACE type %s label %s new size = %d\n",
      Date::get_date_string().c_str(),
      Date::get_12hr_clock().c_str(),
      Global::Simulation_Day,
      this->get_id(),
      Place_Type::get_place_type_name(place_type_id).c_str(),
      (place == nullptr ? "NULL" : place->get_label()),
      size);
  }
}

/**
 * Selects a random Place of the specified Place_Type and joins that place.
 *
 * @param place_type_id the place type ID
 */
void Person::select_place_of_type(int place_type_id) {
  Place* place = Place_Type::select_place_of_type(place_type_id, this);
  this->join_place(place);
}

/**
 * Joins the specified Network_Type's associated Network.
 *
 * @param network_type_id the network type ID
 */
void Person::join_network(int network_type_id) {
  Network* network = Network_Type::get_network_type(network_type_id)->get_network();
  this->join_network(network);
}

/**
 * Removes this person from the Network with the specified Network_Type.
 *
 * @param network_type_id the network type ID
 */
void Person::quit_network(int network_type_id) {
  Person::person_logger->info("quit_network type {:d}", network_type_id);
  this->link[network_type_id].remove_from_network(this);
}

/**
 * Enables the specified Place_Type to report it's size, as well as the Place of that type that 
 * this person is linked to.
 *
 * @param place_type_id the place type ID
 */
void Person::report_place_size(int place_type_id) {
  Place* place = this->get_place_of_type(place_type_id);
  if(place != nullptr) {
    place->start_reporting_size();
    Place_Type::report_place_size(place_type_id);
  }
}

/**
 * Sets the specified Place as this person's activity group of the specified Place_Type.
 *
 * @param i the place type ID
 * @param place the place
 */
void Person::set_place_of_type(int type, Place* place) {
	this->set_activity_group(type, place);
}

/**
 * Gets the Place of the specified Place_Type that this person is linked to. If the place type is a hosted group, 
 * it will get the place hosted by this person. If not, it will get the place associated with this person's 
 * Link to the specified Group_Type.
 *
 * @param type the place type ID
 * @return the place
 */
Place* Person::get_place_of_type(int type) {
  if(type == Group_Type::HOSTED_GROUP) {
    return Place_Type::get_place_hosted_by(this);
  }
  if(type < Place_Type::get_number_of_place_types()) {
    return this->link[type].get_place();
  }
  return nullptr;
}

/**
 * Gets the Group of the specified Group_Type that this person is a member of. It will do this by getting the group 
 * associated with this person's Link to the specified Group_Type.
 *
 * @param type the group type ID
 * @return the group
 */
Group* Person::get_group_of_type(int type) {
  if(type < 0) {
    return nullptr;
  }
  if(type < Group_Type::get_number_of_group_types()) {
    return this->link[type].get_group();
  }
  return nullptr;
}

/**
 * Gets the network of the specified type that this person is a member of. It will do this by getting the network 
 * associated with this person's Link to the specified Group_Type.
 *
 * @param type the network type ID
 * @return the network
 */
Network* Person::get_network_of_type(int type) {
  if(type < Place_Type::get_number_of_place_types() + Network_Type::get_number_of_network_types()) {
    return this->link[type].get_network();
  }
  return nullptr;
}

/**
 * Gets the ADI state rank of the block group in which this person's Household is located.
 *
 * @return the ADI state rank
 */
int Person::get_adi_state_rank() {
  return this->get_household()->get_adi_state_rank();
}

/**
 * Gets the ADI national rank of the block group in which this person's Household is located.
 *
 * @return the ADI national rank
 */
int Person::get_adi_national_rank() {
  return this->get_household()->get_adi_national_rank();
}


/**
 * Gets the placemates of this person in a Place of the specified Place_Type that this person is a member 
 * of. If there are more placemates than the specified maximum, then placemates will be selected at random 
 * from the place's members until the maximum number are selected.
 *
 * @param place_type_id the place type ID
 * @param maxn the maximum number
 * @return the placemates
 */
person_vector_t Person::get_placemates(int place_type_id, int maxn) {
  person_vector_t result;
  result.clear();
  Place* place = this->get_place_of_type(place_type_id);
  if(place != nullptr) {
    int size = place->get_size();
    if(size <= maxn) {
      for(int i = 0; i < size; ++i) {
        Person* per2 = place->get_member(i);
        if(per2 != this) {
          result.push_back(per2);
        }
      }
    } else {
      // randomize the order of processing the members
      std::vector<int> shuffle_index;
      shuffle_index.clear();
      shuffle_index.reserve(size);
      for(int i = 0; i < size; ++i) {
        shuffle_index.push_back(i);
      }
      FYShuffle<int>(shuffle_index);

      // return the first maxn
      for(int i = 0; i < maxn; ++i) {
        result.push_back(place->get_member(shuffle_index[i]));
      }
    }
  }
  return result;
}

/**
 * Gets the size of the Place with the specified Place_Type that this person is a member of.
 *
 * @param type_id the place type ID
 * @return the size
 */
int Person::get_place_size(int type_id) {
  return get_group_size(type_id);
}

/**
 * Gets the size of the network with the specified Network_Type that this person is a member of.
 *
 * @param type_id the network type ID
 * @return the size
 */
int Person::get_network_size(int type_id) {
  return get_group_size(type_id);
}

/**
 * Gets the age of this person in years.
 *
 * @return the age in years
 */
double Person::get_age_in_years() const {
  return double(Global::Simulation_Day - this->birthday_sim_day) / 365.25;
}

/**
 * Gets the age of this person in days.
 *
 * @return the age in days
 */
int Person::get_age_in_days() const {
  return Global::Simulation_Day - this->birthday_sim_day;
}

/**
 * Gets the age of this person in weeks.
 *
 * @return the age in weeks
 */
int Person::get_age_in_weeks() const {
  return get_age_in_days() / 7;
}

/**
 * Gets the age of this person in months.
 *
 * @return the age in months
 */
int Person::get_age_in_months() const {
  return static_cast<int>(this->get_age_in_years() / 12.0);
}

/**
 * Gets the real age of this person. This will be the age in years.
 *
 * @return the real age
 */
double Person::get_real_age() const {
  return static_cast<double>(Global::Simulation_Day - this->birthday_sim_day) / 365.25;
}

/**
 * Gets the age of this person. This will be the integer value of the real age.
 *
 * @return the age
 */
int Person::get_age() const {
  return static_cast<int>(this->get_real_age());
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

///// STATIC METHODS

/**
 * Gets properties of the population.
 */
void Person::get_population_properties() {
  Person::person_logger->info("get_population_properties entered");
  
  // optional properties:
  Parser::disable_abort_on_failure();

  Parser::get_property("report_initial_population", &Person::report_initial_population);
  Parser::get_property("output_population", &Person::output_population);
  Parser::get_property("pop_outfile", Person::pop_outfile);
  Parser::get_property("output_population_date_match", Person::output_population_date_match);
  Parser::get_property("max_reporting_agents", &Person::max_reporting_agents);

  // restore requiring properties
  Parser::set_abort_on_failure();

  Person::person_logger->info("get_population_properties finish");
}

/**
 * Initializes the static variables of the Person class, if they have not already been initialized.
 */
void Person::initialize_static_variables() {

  Person::person_logger->info("initialize_static_variables entered");

  //Setup the static variables if they aren't already initialized
  if(!Person::is_initialized) {

    // read optional properties
    Parser::disable_abort_on_failure();
  
    Person::record_location = false;
    int tmp;
    Parser::get_property("record_location", &tmp);
    Person::record_location = tmp;

    // restore requiring properties
    Parser::set_abort_on_failure();

    Person::is_initialized = true;
  }

  Person::person_logger->info("initialize_static_variables finished");
}


/**
 * Adds a person to the population with the given characteristics. A new person object will be created, 
 * and setup will be called to initialize its properties. They will then be added to the population and returned.
 * All persons in the population must be created using add_person_to_population.
 *
 * @param sp_id the synthetic population id
 * @param age the age
 * @param sex the sex
 * @param race the race
 * @param rel the household relationship
 * @param house the household
 * @param school the school
 * @param work the workplace
 * @param day the day
 * @param today_is_birthday if the given day is this person's birthday
 * @return the person
 */
Person* Person::add_person_to_population(std::string sp_id, int age, char sex, int race, int rel,
    Place* house, Place* school, Place* work,
    int day, bool today_is_birthday) {

  Person* person = new Person;
  int id = Person::next_id++;
  int idx = Person::people.size();
  person->setup(sp_id, idx, id, age, sex, race, rel, house, school, work, day, today_is_birthday);
  Person::people.push_back(person);
  Person::pop_size = Person::people.size();
  Person::id_map.push_back(idx);
  return person;
}

/**
 * Creates and returns an admin agent. This person is initialized with default admin properties, and added 
 * to the static admin agents vector.
 *
 * @return the admin agent
 */
Person* Person::create_admin_agent() {
  Person* agent = new Person;
  int id = Person::next_meta_id--;
  agent->setup(Random::generate_GUID(), id, id, 999, 'M', -1, 0, nullptr, nullptr, nullptr, 0, true);
  Person::admin_agents.push_back(agent);
  return agent;
}

/**
 * Creates and returns an import agent. This person is initialized with default import properties.
 *
 * @return the import agent
 */
Person* Person::create_import_agent() {
  Person* agent = new Person;
  int id = -1;
  agent->setup(Random::generate_GUID(), id, id, 999, 'M', -1, 0, nullptr, nullptr, nullptr, 0, true);
  return agent;
}

/**
 * Prepares the specified Person for death on the given day. They will be added to the death list and marked 
 * as deceased.
 *
 * @param day the day
 * @param person the person
 */
void Person::prepare_to_die(int day, Person* person) {
  if(person->is_meta_agent()) {
    return;
  }
  if(person->is_deceased() == false) {
    // add person to daily death_list
    Person::death_list.push_back(person);
    Person::person_logger->debug("prepare_to_die PERSON: {:d}", person->get_id());
    person->set_deceased();
  }
}

/**
 * Prepares the specified Person for migration on the given day. If they are eligible to migrate, they will be added to 
 * the migrant list and marked as no longer eligible to migrate. Because this type of migration occurs when a destination 
 * is not specified, the person will be moved out of the simulation. Since they will no longer be part of the simulation, 
 * the migrant is also marked as deceased, and will be removed from the population.
 *
 * @param day the day
 * @param person the person
 */
void Person::prepare_to_migrate(int day, Person* person) {
  if(person->is_meta_agent()) {
    return;
  }
  if(person->is_eligible_to_migrate() && person->is_deceased() == false) {
    // add person to daily migrant_list
    Person::migrant_list.push_back(person);
    Person::person_logger->debug("prepare_to_migrate PERSON: {:d}", person->get_id());
    person->unset_eligible_to_migrate();
    person->set_deceased();
  }
}

/**
 * Sets up the Person class for the simulation. Initializes global variables and list variables. 
 * Creates the import agent, and records initial statistics on the demographics of the population.
 */
void Person::setup() {
  Person::person_logger->info("setup population entered");

  Person::people.clear();
  Person::pop_size = 0;
  Person::death_list.clear();
  Person::migrant_list.clear();

  // read optional properties
  Parser::disable_abort_on_failure();
  
  int number_of_list_vars = Person::number_of_global_list_vars;
  Person::person_logger->info("GLOBAL_LIST_VAR setup {:d}", number_of_list_vars);
  if(number_of_list_vars > 0) {
    Person::global_list_var = new double_vector_t [number_of_list_vars];
    for (int i = 0; i < number_of_list_vars; i++) {
      Person::person_logger->info("GLOBAL_LIST_VAR setup {:s}", Person::global_list_var_name[i].c_str());
      Person::global_list_var[i].clear();
      if(Parser::does_property_exist(Person::global_list_var_name[i])) {
        std::string value;
        Parser::get_property(Person::global_list_var_name[i], &value);
        Expression *expr = new Expression(value);
        if(expr->parse()==false) {
          Person::person_logger->error("HELP: BAD EXPRESSION for global list_var {:s} = |{:s}|",
              Person::global_list_var_name[i].c_str(), value.c_str());
          Utils::print_error("Global list var " + Person::global_list_var_name[i] + " expression " +  value + " not recognized.");
        } else {
          Person::global_list_var[i] = expr->get_list_value(nullptr);
          for(int j = 0; j < static_cast<int>(Person::global_list_var[i].size()); ++j) {
            Person::person_logger->trace("<{:s}, {:d}>: INIT LIST VAR {:s}[{:d}] = {:f}",
                __FILE__, __LINE__, Person::get_global_list_var_name(i).c_str(), j, Person::global_list_var[i][j]);
          }
        }
      }
    }
  }

  if(Person::number_of_global_vars > 0) {
    Person::person_logger->info("GLOBAL_VAR setup {:d}", Person::number_of_global_vars);
    Person::global_var = new double [Person::number_of_global_vars];
    for(int i = 0; i < Person::number_of_global_vars; ++i) {
      Person::global_var[i] = 0.0;
      if(Parser::does_property_exist(Person::global_var_name[i])) {
        std::string value;
        Parser::get_property(Person::global_var_name[i], &value);
        Expression *expr = new Expression(value);
        if(expr->parse() == false) {
          Person::person_logger->error("HELP: BAD EXPRESSION for global var {:s} = |{:s}|", 
              Person::global_var_name[i].c_str(), value.c_str());
          Utils::print_error("Global var " + Person::global_var_name[i] + " expression " +  value + " not recognized.");
        } else {
          Person::global_var[i] = expr->get_value(nullptr,nullptr);
          Person::person_logger->info("GLOBAL VAR {:s} = {:f}", Person::global_var_name[i].c_str(), global_var[i]);
        }
      }
    }
  }

  if(Person::number_of_vars > 0) {
    Person::var_expr = new Expression* [Person::number_of_vars];
    for(int i = 0; i < Person::number_of_vars; ++i) {
      if(Parser::does_property_exist(Person::var_name[i])) {
        std::string value;
        Parser::get_property(Person::var_name[i], &value);
        Person::var_expr[i] = new Expression(value);
        if(Person::var_expr[i]->parse() == false) {
          Person::person_logger->error("HELP: BAD EXPRESSION for var {:s} = |{:s}|", 
              Person::var_name[i].c_str(), value.c_str());
          Utils::print_error("Variable " + Person::var_name[i] + " expression " +  value + " not recognized.");
        }
      } else {
        Person::var_expr[i] = nullptr;
      }
    }
  }

  Person::person_logger->info("Reading {:d} List Var expressions", Person::number_of_list_vars);
  if(Person::number_of_list_vars > 0) {
    Person::list_var_expr = new Expression* [Person::number_of_list_vars];
    for(int i = 0; i < Person::number_of_list_vars; ++i) {
      if(Parser::does_property_exist(Person::list_var_name[i])) {
        std::string value;
        Parser::get_property(Person::list_var_name[i], &value);
        Person::list_var_expr[i] = new Expression(value);
        if(Person::list_var_expr[i]->parse() == false) {
          Person::person_logger->error("HELP: BAD EXPRESSION for list var {:s} = |{:s}|",
              Person::list_var_name[i].c_str(), value.c_str());
          Utils::print_error("List Variable " + Person::list_var_name[i] + " expression " +  value + " not recognized.");
        } else {
          Person::person_logger->debug("List Var {:s} = |{:s}|", Person::list_var_name[i].c_str(), 
              Person::list_var_expr[i]->get_name().c_str());
        }
      } else {
        Person::person_logger->info("List Var {:s} NOT FOUND", Person::list_var_name[i].c_str());
        Person::list_var_expr[i] = nullptr;
      }
    }
  }

  // restore requiring properties
  Parser::set_abort_on_failure();

  // define the Import_agent
  Person::Import_agent = Person::create_import_agent();

  Person::read_all_populations();

  Person::load_completed = true;
  
  Person::initialize_activities();

  // record age-specific popsize
  for(int age = 0; age <= Demographics::MAX_AGE; ++age) {
    Person::Popsize_by_age[age] = 0;
  }
  for(int p = 0; p < Person::get_population_size(); ++p) {
    Person* person = get_person(p);
    int age = person->get_age();
    if(age > Demographics::MAX_AGE) {
      age = Demographics::MAX_AGE;
    }
    ++Person::Popsize_by_age[age];
  }

  // print initial demographics if requested
  if(Person::report_initial_population) {
    char pfilename [FRED_STRING_SIZE];
    snprintf(pfilename, FRED_STRING_SIZE, "%s/population.txt", Global::Simulation_directory);
    FILE* pfile = fopen(pfilename, "w");
    for(int p = 0; p < Person::get_population_size(); ++p) {
      Person* person = get_person(p);
      fprintf(pfile, "%d,%d,%c,%d,%f,%f\n",
        person->get_id(), person->get_age(),
        person->get_sex(), person->get_race(),
        person->get_household()->get_latitude(),
        person->get_household()->get_longitude()
      );
    }
    fclose(pfile);
  }

  Person::person_logger->info("population setup finished");
}

/**
 * Reads person data from a file at the specified line. This data will then be passed to add a Person with 
 * the gathered data to the population. The gq parameter specifies if the person is a resident of a group 
 * quarters, which will change what data is gathered. For example, a person's group quarters will be 
 * both their house and workplace.
 *
 * @param line the line
 * @param gq if group quarters
 */
void Person::get_person_data(char* line, bool gq) {

  int day = Global::Simulation_Day;

  // person data
  char label[FRED_STRING_SIZE];
  Place* house = nullptr;
  Place* work = nullptr;
  Place* school = nullptr;
  int age = -1;
  int race = -1;
  int household_relationship = -1;
  char sex = 'X';
  bool today_is_birthday = false;

  // place labels
  char house_label[FRED_STRING_SIZE] = "";
  char school_label[FRED_STRING_SIZE] = "";
  char work_label[FRED_STRING_SIZE];
  char gq_label[FRED_STRING_SIZE] = "";
  char tmp_house_label[FRED_STRING_SIZE] = "";
  char tmp_school_label[FRED_STRING_SIZE] = "";
  char tmp_work_label[FRED_STRING_SIZE] = "";

  strcpy(label, "X");
  strcpy(house_label, "X");
  strcpy(school_label, "X");
  strcpy(work_label, "X");

  if(gq) {
    sscanf(line, "%s %s %d %c", label, gq_label, &age, &sex);
    snprintf(house_label, FRED_STRING_SIZE, "GH-%s", gq_label);
    snprintf(work_label, FRED_STRING_SIZE, "GW-%s", gq_label);
  } else {
    sscanf(line, "%s %s %d %c %d %d %s %s", label, tmp_house_label, &age, &sex, &race, &household_relationship, tmp_school_label, tmp_work_label);
    snprintf(house_label, FRED_STRING_SIZE, "H-%s", tmp_house_label);
    snprintf(work_label, FRED_STRING_SIZE, "W-%s", tmp_work_label);
    snprintf(school_label,FRED_STRING_SIZE, "S-%s", tmp_school_label);
  }

  if(strcmp(tmp_school_label, "X") != 0 && Global::GRADES <= age) {
    // person is too old for schools in FRED!
    Person::person_logger->warn("WARNING: person {:s} age {:d} is too old to attend school {:s}",
        label, age, school_label);
    // reset school _label
    strcpy(school_label, "X");
  }

  // set pointer to primary places in init data object
  house = Place::get_household_from_label(house_label);
  work =  Place::get_workplace_from_label(work_label);
  school = Place::get_school_from_label(school_label);

  if(house == nullptr) {
    // we need at least a household (homeless people not yet supported), so
    // skip this person
    Person::person_logger->warn("WARNING: skipping person {:s} -- {:s} {:s}", label, 
        "no household found for label =", house_label);
    return;
  }

  // warn if we can't find workplace
  if(strcmp(tmp_work_label, "X") != 0 && work == nullptr) {
    Person::person_logger->warn("WARNING: person {:s} -- no workplace found for label = {:s}", label, work_label);
    if(Global::Enable_Local_Workplace_Assignment) {
      work = Place::get_random_workplace();
      if(work != nullptr) {
        Person::person_logger->warn("WARNING: person {:s} assigned to workplace {:s}",
            label, work->get_label());
      } else {
        Person::person_logger->warn("WARNING: no workplace available for person {:s}", label);
      }
    }
  }

  // warn if we can't find school.
  if(strcmp(tmp_school_label, "X") != 0 && school == nullptr) {
    Person::person_logger->warn("WARNING: person {:s} -- no school found for label = {:s}", label, school_label);
  }

  Person::add_person_to_population(std::string(label), age, sex, race, household_relationship, house, school, work, day, today_is_birthday);
}

/**
 * Reads the population for every location ID. After the persons are intialized from reading the population, 
 * they are marked as original.
 */
void Person::read_all_populations() {

  // process each specified location
  int locs = Place::get_number_of_location_ids();
  for(int i = 0; i < locs; ++i) {
    char pop_dir[FRED_STRING_SIZE];
    Place::get_population_directory(pop_dir, i);
    Person::read_population(pop_dir, "people");
    if(Global::Enable_Group_Quarters) {
      Person::read_population(pop_dir, "gq_people");
    }
  }

  // mark all original people as original
  for(int i = 0; i < static_cast<int>(Person::people.size()); ++i) {
    Person::people[i]->set_original();
  }

  // report on time take to read populations
  Utils::fred_print_lap_time("reading populations");

}

/**
 * Reads the population of the specified type from the specified directory. For each line, 
 * get the person data.
 *
 * @param pop_dir the population directory
 * @param pop_type the population type
 */
void Person::read_population(const char* pop_dir, const char* pop_type) {

  Person::person_logger->info("read population entered");

  char population_file[FRED_STRING_SIZE];
  bool is_group_quarters_pop = (strcmp(pop_type, "gq_people") == 0);
  FILE* fp = nullptr;

  snprintf(population_file, FRED_STRING_SIZE, "%s/%s.txt", pop_dir, pop_type);

  // verify that the file exists
  fp = Utils::fred_open_file(population_file);
  if(fp == nullptr) {
    Utils::fred_abort("population_file %s not found\n", population_file);
  }
  fclose(fp);

  if(Global::Compile_FRED) {
    return;
  }

  std::ifstream stream(population_file, std::ifstream::in);

  char line[FRED_STRING_SIZE];
  // discard header line
  stream.getline(line, FRED_STRING_SIZE);
  while(stream.good()) {
    stream.getline(line, FRED_STRING_SIZE);
    // skip empty lines and header lines ...
    if((line[0] == '\0') || strncmp(line, "sp_id", 6) == 0 || strncmp(line, "per_id", 7) == 0) {
      continue;
    }
    Person::get_person_data(line, is_group_quarters_pop);
  }

  Person::person_logger->info("finished reading population, pop_size = {:d}", Person::pop_size);
}

/**
 * For each Person currently in the death list, deletes them from the population at the given day. The 
 * death list is then cleared.
 *
 * @param day the day
 */
void Person::remove_dead_from_population(int day) {
  Person::person_logger->info("remove_dead_from_population");
  size_t deaths = Person::death_list.size();
  for(size_t i = 0; i < deaths; ++i) {
    Person* person = Person::death_list[i];
    Person::delete_person_from_population(day, person);
  }
  // clear the death list
  Person::death_list.clear();
  Person::person_logger->info("remove_dead_from_population finished");
}

/**
 * For each migrant currently in the migrant list, deletes them from the population at the given day. The 
 * migrant list is then cleared. These are migrants who are moving out of the simulation, and will no longer 
 * be a part of the simulated population.
 *
 * @param day the day
 */
void Person::remove_migrants_from_population(int day) {
  Person::person_logger->info("remove_migrant_from_population");
  size_t migrants = Person::migrant_list.size();
  for(size_t i = 0; i < migrants; ++i) {
    Person* person = Person::migrant_list[i];
    Person::delete_person_from_population(day, person);
  }
  // clear the migrant list
  Person::migrant_list.clear();
  Person::person_logger->info("remove_migrant_from_population finished");
}

/**
 * Deletes the specified Person from the population at the given day. They are terminated, 
 * removed from population data structures which are then restructured, and deleted from memory.
 *
 * @param day the day
 * @param person the person
 */
void Person::delete_person_from_population(int day, Person* person) {
  Person::person_logger->info("DELETING PERSON: {:d}", person->get_id());

  person->terminate(day);

  // delete from population data structure
  int id = person->get_id();
  int idx = person->get_pop_index();
  Person::id_map[id] = -1;

  if(Person::pop_size > 1) {
    // move last person in vector to this person's position
    Person::people[idx] = Person::people[Person::pop_size - 1];

    // inform the move person of its new index
    Person::people[idx]->set_pop_index(idx);

    // update the id_map
    Person::id_map[Person::people[idx]->get_id()] = idx;
  }

  // remove last element in vector
  Person::people.pop_back();

  // record new population_size
  Person::pop_size = Person::people.size();

  // call Person's destructor directly!!!
  person->~Person();
}

/**
 * Generates a report on the population, which is outputted to the file. This report will only be generated 
 * on the first day of the simulation, on days matching the date pattern in the program file, and on 
 * the last day of the simulation.
 *
 * @param day the day
 */ 
void Person::report(int day) {

  // Write out the population if the output_population property is set.
  // Will write only on the first day of the simulation, on days
  // matching the date pattern in the program file, and the on
  // the last day of the simulation
  if(Person::output_population > 0) {
    int month;
    int day_of_month;
    sscanf(Person::output_population_date_match,"%d-%d", &month, &day_of_month);
    if(day == 0 || (month == Date::get_month() && day_of_month == Date::get_day_of_month())) {
      Person::write_population_output_file(day);
    }
  }


  if(Global::Enable_Population_Dynamics) {
    int year = Date::get_year();
    if(2010 <= year && Date::get_month() == 6 && Date::get_day_of_month()==30) {
      int males[18];
      int females[18];
      int male_count = 0;
      int female_count = 0;
      int natives = 0;
      int originals = 0;
      std::vector<double>ages;
      ages.clear();
      ages.reserve(Person::get_population_size());

      for(int i = 0; i < 18; ++i) {
        males[i] = 0;
        females[i] = 0;
      }
      for(int p = 0; p < Person::get_population_size(); ++p) {
        Person* person = get_person(p);
        int age = person->get_age();
        ages.push_back(person->get_real_age());
        int age_group = age / 5;
        if(age_group > 17) {
          age_group = 17;
        }
        if(person->get_sex()=='M') {
          ++males[age_group];
          ++male_count;
        } else {
          ++females[age_group];
          ++female_count;
        }
        if(person->is_native()) {
          ++natives;
        }
        if(person->is_original()) {
          ++originals;
        }
      }
      std::sort(ages.begin(), ages.end());
      double median = ages[Person::get_population_size()/2];

      char filename[FRED_STRING_SIZE];
      snprintf(filename, FRED_STRING_SIZE, "%s/pop-%d.txt",
      Global::Simulation_directory,
      Global::Simulation_run_number);
      FILE *fp = nullptr;
      if(year == 2010) {
        fp = fopen(filename,"w");
      } else {
        fp = fopen(filename,"a");
      }
      assert(fp != nullptr);
      fprintf(fp, "%d total %d males %d females %d natives %d %f orig %d %f median_age %0.2f\n",
      Date::get_year(),
      Person::get_population_size(),
      male_count, female_count,
      natives, static_cast<double>(natives) / static_cast<double>(Person::get_population_size()),
      originals, static_cast<double>(originals) / static_cast<double>(Person::get_population_size()),
      median);
      fclose(fp);

      if(year % 5 == 0) {
        snprintf(filename, FRED_STRING_SIZE, "%s/pop-ages-%d-%d.txt", Global::Simulation_directory,
          year, Global::Simulation_run_number);
        fp = fopen(filename,"w");
        assert(fp != nullptr);
        for(int i = 0; i < 18; ++i) {
        int lower = 5 * i;
        char label[16];
        if(lower < 85) {
          snprintf(label, 16, "%d-%d",lower,lower+4);
        } else {
          snprintf(label,16, "85+");
        }
        fprintf(fp, "%d %s %d %d %d %d\n", Date::get_year(), label, lower,
          males[i], females[i], males[i]+females[i]);
      }
      fclose(fp);
      }
    }
  }

  int report_size = Person::report_vec.size();
  for(int i = 0; i < report_size; ++i) {
    report_t* report = Person::report_vec[i];
    double value = report->expression->get_value(report->person);
    int vec_size = report->value_on_day.size();
    if(vec_size == 0 || value != report->value_on_day[vec_size - 1]) {
      report->change_day.push_back(day);
      report->value_on_day.push_back(value);
    }
  }
}

/**
 * Outputs final data on the population for the simulation.
 */
void Person::finish() {
  // Write the population to the output file if the property is set
  // Will write only on the first day of the simulation, days matching
  // the date pattern in the program file, and the last day of the
  // simulation
  if(Person::output_population > 0) {
    Person::write_population_output_file(Global::Simulation_Days);
  }

  // write final reports
  if(Person::report_vec.size() > 0) {

    char dir[FRED_STRING_SIZE];
    char outfile[FRED_STRING_SIZE];
    FILE* fp;

    snprintf(dir, FRED_STRING_SIZE, "%s/RUN%d/DAILY", Global::Simulation_directory, Global::Simulation_run_number);
    Utils::fred_make_directory(dir);

    for(int i = 0; i < static_cast<int>(Person::report_vec.size()); ++i) {
      int person_index = Person::report_vec[i]->person_index;
      std::string expression_str = Person::report_vec[i]->expression->get_name();

      snprintf(outfile, FRED_STRING_SIZE, "%s/PERSON.Person%d_%s.txt", dir, person_index, expression_str.c_str());
      fp = fopen(outfile, "w");
      if(fp == nullptr) {
        Utils::fred_abort("Fred: can't open file %s\n", outfile);
      }
      for(int day = 0; day < Global::Simulation_Days; ++day) {
        double value = 0.0;
        int vec_size = Person::report_vec[i]->value_on_day.size();
        for(int j = 0; j < vec_size; ++j) {
          if(Person::report_vec[i]->change_day[j] > day) {
            break;
          } else {
            value = Person::report_vec[i]->value_on_day[j];
          }
        }
        fprintf(fp, "%d %f\n", day, value);
      }
      fclose(fp);
    }

    // create a csv file for PERSON

    // this joins two files with same value in column 1, from
    // https://stackoverflow.com/questions/14984340/using-awk-to-process-input-from-multiple-files
    char awkcommand[FRED_STRING_SIZE];
    snprintf(awkcommand,FRED_STRING_SIZE, "awk 'FNR==NR{a[$1]=$2 FS $3;next}{print $0, a[$1]}' ");

    char command[FRED_STRING_SIZE];
    char dailyfile[FRED_STRING_SIZE];

    snprintf(outfile, FRED_STRING_SIZE, "%s/RUN%d/%s.csv", Global::Simulation_directory, Global::Simulation_run_number, "PERSON");

    for(int i = 0; i < static_cast<int>(Person::report_vec.size()); ++i) {
      int person_index = Person::report_vec[i]->person_index;
      std::string expression_str = Person::report_vec[i]->expression->get_name();
      snprintf(dailyfile, FRED_STRING_SIZE, "%s/PERSON.Person%d_%s.txt", dir, person_index, expression_str.c_str());
      if(i == 0) {
        snprintf(command, FRED_STRING_SIZE, "cp %s %s", dailyfile, outfile);
      } else {
        snprintf(command, FRED_STRING_SIZE, "%s %s %s > %s.tmp; mv %s.tmp %s", awkcommand, dailyfile, outfile, outfile, outfile, outfile);
      }
      system(command);
    }  

    // create a header line for the csv file
    char headerfile[FRED_STRING_SIZE];
    snprintf(headerfile, FRED_STRING_SIZE, "%s/RUN%d/%s.header", Global::Simulation_directory, Global::Simulation_run_number, "PERSON");
    fp = fopen(headerfile, "w");
    fprintf(fp, "Day ");
    for(int i = 0; i < static_cast<int>(Person::report_vec.size()); ++i) {
      int person_index = Person::report_vec[i]->person_index;
      std::string expression_str = Person::report_vec[i]->expression->get_name();
      fprintf(fp, "PERSON.Person%d_%s ", person_index, expression_str.c_str()); 
    }
    fprintf(fp, "\n");
    fclose(fp);

    // concatenate header line
    snprintf(command, FRED_STRING_SIZE, "cat %s %s > %s.tmp; mv %s.tmp %s; unlink %s",
    headerfile, outfile, outfile, outfile, outfile, headerfile);
    system(command);

    // replace spaces with commas
    snprintf(command, FRED_STRING_SIZE, "sed -E 's/ +/,/g' %s | sed -E 's/,$//' | sed -E 's/,/ /' > %s.tmp; mv %s.tmp %s",
    outfile, outfile, outfile, outfile);
    system(command);

  }
}

/**
 * Performs quality control on the population. Checks that all persons have a Household, and outputs data on 
 * the age distribution.
 */
void Person::quality_control() {
  Person::person_logger->info("population quality control check");

  // check population
  for(int p = 0; p < Person::get_population_size(); ++p) {
    Person* person = get_person(p);
    if(person->get_household() == nullptr) {
      Person::person_logger->error("HELP: Person {:d} has no home.", person->get_id());
    }
  }

  int n0, n5, n18, n50, n65;
  int count[20];
  int total = 0;
  n0 = n5 = n18 = n50 = n65 = 0;
  // age distribution
  for(int c = 0; c < 20; ++c) {
    count[c] = 0;
  }
  for(int p = 0; p < Person::get_population_size(); ++p) {
    Person* person = get_person(p);
    int a = person->get_age();

    if(a < 5) {
      ++n0;
    } else if(a < 18) {
      ++n5;
    } else if(a < 50) {
      ++n18;
    } else if(a < 65) {
      ++n50;
    }else {
      ++n65;
    }
    int n = a / 5;
    if(n < 20) {
      ++count[n];
    } else {
      ++count[19];
    }
    ++total;
  }
  Person::person_logger->debug("Age distribution: {:d} people", total);
  for(int c = 0; c < 20; ++c) {
    Person::person_logger->debug("age {:2d} to {:d}: {:6d} ({:.2f})", 5 * c, 5 * (c + 1) - 1, count[c], 
        (100.0 * count[c]) / total);
  }
  Person::person_logger->debug("AGE 0-4: {:d} {:.2f}", n0, (100.0 * n0) / total);
  Person::person_logger->debug("AGE 5-17: {:d} {:.2f}", n5, (100.0 * n5) / total);
  Person::person_logger->debug("AGE 18-49: {:d} {:.2f}", n18, (100.0 * n18) / total);
  Person::person_logger->debug("AGE 50-64: {:d} {:.2f}", n50, (100.0 * n50) / total);
  Person::person_logger->debug("AGE 65-100: {:d} {:.2f}", n65, (100.0 * n65) / total);

  Person::person_logger->info("population quality control finished");
}

/**
 * Assigns a primary healthcare facility for each Person in the population.
 */
void Person::assign_primary_healthcare_facilities() {
  assert(Place::is_load_completed());
  assert(Person::is_load_completed());
  Person::person_logger->info("assign primary healthcare entered");
  for(int p = 0; p < Person::get_population_size(); ++p) {
    Person* person = get_person(p);
    person->assign_primary_healthcare_facility();

  }
  Person::person_logger->info("assign primary healthcare finished");
}

/**
 * Outputs the properties of each Person in the population to a csv file in the given directory.
 *
 * @param directory the directory
 */
void Person::get_network_stats(char* directory) {
  Person::person_logger->info("get_network_stats entered");
  char filename[FRED_STRING_SIZE];
  snprintf(filename,FRED_STRING_SIZE, "%s/degree.csv", directory);
  FILE* fp = fopen(filename, "w");
  fprintf(fp, "id,age,deg,h,n,s,c,w,o\n");
  for(int p = 0; p < Person::get_population_size(); ++p) {
    Person* person = get_person(p);
    fprintf(fp, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n", person->get_id(), person->get_age(), person->get_degree(),
    person->get_household_size(), person->get_neighborhood_size(), person->get_school_size(),
    person->get_classroom_size(), person->get_workplace_size(), person->get_office_size());
  }
  fclose(fp);
  Person::person_logger->info("get_network_stats finished");
}

/**
 * Outputs data on the age distribution of the population to the specified directory, given a 
 * date string and a run number.
 *
 * @param dir the directory
 * @param date_string the date string
 * @param run the run
 */
void Person::print_age_distribution(char* dir, char* date_string, int run) {
  FILE* fp;
  int count[Demographics::MAX_AGE + 1];
  double pct[Demographics::MAX_AGE + 1];
  char filename[FRED_STRING_SIZE];
  snprintf(filename, FRED_STRING_SIZE, "%s/age_dist_%s.%02d", dir, date_string, run);
  Person::person_logger->info("print_age_dist entered, filename = {:s}", filename);
  fflush(stdout);
  for(int i = 0; i < 21; ++i) {
    count[i] = 0;
  }
  for(int p = 0; p < Person::get_population_size(); ++p) {
    Person* person = get_person(p);
    int age = person->get_age();
    assert(age >= 0);
    if(age > Demographics::MAX_AGE) {
      age = Demographics::MAX_AGE;
    }
    count[age]++;
  }
  fp = fopen(filename, "w");
  for(int i = 0; i < 21; ++i) {
    pct[i] = 100.0 * count[i] / Person::pop_size;
    fprintf(fp, "%d  %d %f\n", i * 5, count[i], pct[i]);
  }
  fclose(fp);
}

/**
 * Selects a random Person from the population.
 *
 * @return the random person
 */
Person* Person::select_random_person() {
  int i = Random::draw_random_int(0, Person::get_population_size() - 1);
  return get_person(i);
}

/**
 * Outputs the string representation of each Person in the population to a file.
 *
 * @param day _UNUSED_
 */
void Person::write_population_output_file(int day) {

  //Loop over the whole population and write the output of each Person's to_string to the file
  char population_output_file[FRED_STRING_SIZE];
  snprintf(population_output_file, FRED_STRING_SIZE, "%s/%s_%s.txt", Global::Output_directory, Person::pop_outfile,
  Date::get_date_string().c_str());
  FILE* fp = fopen(population_output_file, "w");
  if(fp == nullptr) {
    Utils::fred_abort("HELP: population_output_file %s not found\n", population_output_file);
  }

  for(int p = 0; p < Person::get_population_size(); ++p) {
    Person* person = get_person(p);
    fprintf(fp, "%s\n", person->to_string().c_str());
  }
  fflush(fp);
  fclose(fp);
}

/**
 * Gets an age distribution of the population for each sex. Uses the parameters as arrays of integers that 
 * track the number of people of the corresponding sex who are at an age, with the age as the index.
 *
 * @param count_males_by_age the male age distribution
 * @param count_females_by_age the female age distribution
 */
void Person::get_age_distribution(int* count_males_by_age, int* count_females_by_age) {
  for(int i = 0; i <= Demographics::MAX_AGE; ++i) {
    count_males_by_age[i] = 0;
    count_females_by_age[i] = 0;
  }
  for(int p = 0; p < Person::get_population_size(); ++p) {
    Person* person = get_person(p);
    int age = person->get_age();
    if(age > Demographics::MAX_AGE) {
      age = Demographics::MAX_AGE;
    }
    if(person->get_sex() == 'F') {
      ++count_females_by_age[age];
    } else {
      ++count_males_by_age[age];
    }
  }
}

/**
 * Prepares activities for each Person in the population.
 */
void Person::initialize_activities() {
  for(int p = 0; p < Person::get_population_size(); ++p) {
    Person* person = get_person(p);
    person->prepare_activities();
  }
}

/**
 * Updates the population demographics for the given day. On July 31, all students in the population 
 * are removed as members of the school for the summer. On Aug 1, all persons in the population 
 * have their profile updated based on age. This will restore school-aged persons as students.
 *
 * @param day the day
 */
void Person::update_population_demographics(int day) {
  if(!Global::Enable_Population_Dynamics) {
    return;
  }
  Demographics::update(day);

  // end_membership all student on July 31 each year
  if(Date::get_month() == 7 && Date::get_day_of_month() == 31) {
    for(int p = 0; p < Person::get_population_size(); ++p) {
      Person* person = get_person(p);
      if(person->is_student()) {
        person->change_school(nullptr);
      }
    }
  }

  // update everyone's demographic profile based on age on Aug 1 each year.
  // this re-begin_memberships all school-age student in a school.
  if(Date::get_month() == 8 && Date::get_day_of_month() == 1) {
    for(int p = 0; p < Person::get_population_size(); ++p) {
      Person* person = get_person(p);
      person->update_profile_based_on_age();
    }
  }

}

/**
 * Requests external updates for each person of the population who are in a state of a condition in which 
 * external updates are enabled. Outputs data on the requests to files.
 */
void Person::get_external_updates(int day) {

  FILE* fp = nullptr;
  char filename [FRED_STRING_SIZE];

  // create the API directory, if necessary
  char dirname [FRED_STRING_SIZE];
  snprintf(dirname, FRED_STRING_SIZE, "%s/RUN%d/API", Global::Simulation_directory, Global::Simulation_run_number);
  Utils::fred_make_directory(dirname);

  person_vector_t updates;
  updates.clear();

  char requests_file[FRED_STRING_SIZE];
  snprintf(requests_file, FRED_STRING_SIZE, "%s/requests", dirname);
  FILE* reqfp = fopen(requests_file, "w");

  int requests = 0;
  for(int p = 0; p < Person::get_population_size(); ++p) {
    Person* person = get_person(p);
    bool update = false;
    int number_of_conditions = Condition::get_number_of_conditions();
    for(int condition_id = 0; update==false && condition_id < number_of_conditions; ++condition_id) {
      Condition* condition = Condition::get_condition(condition_id);
      if(condition->is_external_update_enabled()) {
        int state = person->get_state(condition_id);
        update = condition->state_gets_external_updates(state);
      }
    }
    if(update) {
      updates.push_back(person);
      snprintf(filename, FRED_STRING_SIZE, "%s/request.%d", dirname, person->get_id());
      fp = fopen(filename, "w");
      person->request_external_updates(fp, day);
      fclose(fp);

      // add filename to the list of requests
      fprintf(reqfp, "request.%d\n", person->get_id());
      ++requests;
    }
  }
  fclose(reqfp);

  if(requests > 0) {
    char command[FRED_STRING_SIZE];
    snprintf(command, FRED_STRING_SIZE, "%s/bin/FRED_API %s", getenv("FRED_HOME"), dirname);
    system(command);

    snprintf(filename,FRED_STRING_SIZE, "%s/results_ready", dirname);
    int tries = 0;
    while(tries < 1000 && nullptr == (fp = fopen(filename, "r"))) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      ++tries;
    }
    if(fp != nullptr) {
      fclose(fp);
      unlink(filename);
      
      for(int p = 0; p < static_cast<int>(updates.size()); ++p) {
        Person* person = updates[p];
        snprintf(filename, FRED_STRING_SIZE, "%s/results.%d", dirname, person->get_id());
        fp = fopen(filename, "r");
        person->get_external_updates(fp, day);
        fclose(fp);
      }
    }
  }

  snprintf(filename, FRED_STRING_SIZE, "%s/log", dirname);
  fp = nullptr;
  if(day == 0) {
    fp = fopen(filename, "w");
  } else {
    fp = fopen(filename, "a");
  }
  fprintf(fp, "day %d requests %d\n", day, requests);
  fclose(fp);
}

/**
 * Sets an admin group for this person if they are a meta agent. The person and specified Group 
 * will be added to the admin group map, and this group will be set as an activity group of this person.
 *
 * @param group the group
 */
void Person::set_admin_group(Group* group) {
  if(this->is_meta_agent() && group != nullptr) {
    Person::admin_group_map[this] = group;
    this->set_activity_group(group->get_type_id(), group);
  }
}

/**
 * Gets the admin group for this person in the admin group map, if one exists.
 *
 * @return the admin group
 */
Group* Person::get_admin_group() {
  if(this->is_meta_agent()) {
    std::unordered_map<Person*,Group*>::const_iterator found = Person::admin_group_map.find(this);
    if(found != admin_group_map.end()) {
      return found->second;
    } else {
      return nullptr;
    }
  } else {
    return nullptr;
  }
}

/**
 * Checks if this person has closure to a condition state. This will be true if the person is a meta agent 
 * with an admin group that's group type is closed to a condition state which the person is in.
 *
 * @return if this person has closure
 */
bool Person::has_closure() {
  if(this->is_meta_agent()) {
    Group* group = this->get_admin_group();
    int group_type_id = group->get_type_id();
    for(int cond_id = 0; cond_id < this->number_of_conditions; ++cond_id) {
      int state = this->get_state(cond_id);
      if(Condition::get_condition(cond_id)->is_closed(state,group_type_id)) {
        Person::person_logger->debug("meta person {:d} admin CLOSES group {:s} in state {:s}.{:s}",
            this->id, group->get_label(), Condition::get_name(cond_id).c_str(),
            Condition::get_condition(cond_id)->get_state_name(state).c_str());
        return true;
      } else {
        Person::person_logger->debug("meta person {:d} admin does not close group {:s} in state {:s}.{:s}",
            this->id, group->get_label(), Condition::get_name(cond_id).c_str(),
            Condition::get_condition(cond_id)->get_state_name(state).c_str());
      }
    }
    Person::person_logger->debug("meta person {:d} admin does not close group {:s}", this->id, group->get_label());
  }
  return false;
}

/**
 * Sets this person as the host of a Place of the specified Place_Type if that person is not already a member of 
 * a place of that place type. This will generate a new place for the person to host, set the place 
 * as the activity Group of the Group_Type for the person, and add the host and place to the host group map.
 *
 * @param place_type_id the place type ID
 */
void Person::start_hosting(int place_type_id) {
  Person::person_logger->info("START_HOSTING person {:d} place_type {:s}", get_id(), 
      Place_Type::get_place_type_name(place_type_id).c_str());
  Place* place = get_place_of_type(place_type_id);
  if(place == nullptr) {
    place = Place_Type::generate_new_place(place_type_id, this);
    this->set_place_of_type(place_type_id, place);
    Group_Type::add_group_hosted_by(this, place);
    Person::person_logger->info("START_HOSTING finished person {:d} place_type {:s} place {:s}",
        this->get_id(), Place_Type::get_place_type_name(place_type_id).c_str(), place ? place->get_label() : "NULL");
  } else {
    Person::person_logger->info("START_HOSTING person {:d} place_type {:s} -- current place not nullptr",
        this->get_id(), Place_Type::get_place_type_name(place_type_id).c_str());
  }
}

/**
 * Gets the name of the specified household relationship index.
 *
 * @param rel the household relationship index
 * @return the household relationship string
 */
std::string Person::get_household_relationship_name(int rel) {
  switch(rel) {

  case Household_Relationship::HOUSEHOLDER:
    return "householder";
    break;

  case Household_Relationship::SPOUSE:
    return "spouse";
    break;

  case Household_Relationship::CHILD:
    return "child";
    break;

  case Household_Relationship::SIBLING:
    return "sibling";
    break;

  case Household_Relationship::PARENT:
    return "parent";
    break;

  case Household_Relationship::GRANDCHILD:
    return "grandchild";
    break;

  case Household_Relationship::IN_LAW:
    return "in_law";
    break;

  case Household_Relationship::OTHER_RELATIVE:
    return "other_relative";
    break;

  case Household_Relationship::BOARDER:
    return "boarder";
    break;

  case Household_Relationship::HOUSEMATE:
    return "housemate";
    break;

  case Household_Relationship::PARTNER:
    return "partner";
    break;

  case Household_Relationship::FOSTER_CHILD:
    return "foster_child";
    break;

  case Household_Relationship::OTHER_NON_RELATIVE:
    return "other_non_relative";
    break;

  case Household_Relationship::INSTITUTIONALIZED_GROUP_QUARTERS_POP:
    return "institutionalized_group_quarters_pop";
    break;

  case Household_Relationship::NONINSTITUTIONALIZED_GROUP_QUARTERS_POP:
    return "noninstitutionalized_group_quarters_pop";
    break;

  default:
    return "unknown";
  }
}

/**
 * Gets the household relationship index of the specified name.
 *
 * @param name the household relationship name
 * @return the household relationship index
 */
int Person::get_household_relationship_from_name(std::string name) {
  for(int i = 0; i < Household_Relationship::HOUSEHOLD_RELATIONSHIPS; ++i) {
    if(name == Person::get_household_relationship_name(i)) {
      return i;
    }
  }
  return -1;
}

/**
 * Gets the name of the specified race index.
 *
 * @param n the race index
 * @return the race name
 */
std::string Person::get_race_name(int n) {
  switch(n) {

  case Race::UNKNOWN_RACE:
    return "unknown_race";
    break;

  case Race::WHITE :
    return "white";
    break;

  case Race::AFRICAN_AMERICAN:
    return "african_american";
    break;

  case Race::AMERICAN_INDIAN:
    return "american_indian";
    break;

  case Race::ALASKA_NATIVE:
    return "alaska_native";
    break;

  case Race::TRIBAL:
    return "tribal";
    break;

  case Race::ASIAN:
    return "asian";
    break;

  case Race::HAWAIIAN_NATIVE:
    return "hawaiian_native";
    break;

  case Race::OTHER_RACE:
    return "other_race";
    break;

  case Race::MULTIPLE_RACE:
    return "multiple_race";
    break;

  default:
    return "unknown";
  }
}

/**
 * Gets the race index of the specified name.
 *
 * @param name the race name
 * @return the race index
 */
int Person::get_race_from_name(std::string name) {
  for(int i = -1; i < Race::RACES; ++i) {
    if(name == Person::get_race_name(i)) {
      return i;
    }
  }
  return -1;
}

/**
 * Gets the name of the personal variable at the specified index.
 *
 * @param id the index, or ID
 * @return the variable name
 */
std::string Person::get_var_name(int id) {
  if(Person::number_of_vars <= id) {
    return "";
  }
  return Person::var_name[id];
}

/**
 * Gets the index of the personal variable with the specified name.
 *
 * @param vname the variable name
 * @return the index, or ID
 */
int Person::get_var_id(std::string vname) {
  for(int i = 0; i < Person::number_of_vars; ++i) {
    if(vname == Person::var_name[i]) {
      return i;
    }
  }
  return -1;
}

/**
 * Gets the name of the personal list variable at the specified index.
 *
 * @param id the index, or ID
 * @return the list variable name
 */
std::string Person::get_list_var_name(int id) {
  if(Person::number_of_list_vars <= id) {
    return "";
  }
  return Person::list_var_name[id];
}

/**
 * Gets the index of the personal list variable with the specified name.
 *
 * @param vname the list variable name
 * @return the index, or ID
 */
int Person::get_list_var_id(std::string vname) {
  for(int i = 0; i < Person::number_of_list_vars; ++i) {
    if(vname == Person::list_var_name[i]) {
      return i;
    }
  }
  return -1;
}

/**
 * Gets the name of the global variable at the specified index.
 *
 * @param id the index, or ID
 * @return the list variable name
 */
std::string Person::get_global_var_name(int id) {
  if(Person::number_of_global_vars <= id) {
    return "";
  }
  return Person::global_var_name[id];
}

/**
 * Gets the index of the global variable with the specified name.
 *
 * @param vname the variable name
 * @return the index, or ID
 */
int Person::get_global_var_id(std::string vname) {
  for(int i = 0; i < Person::number_of_global_vars; ++i) {
    if(vname == Person::global_var_name[i]) {
      return i;
    }
  }
  return -1;
}

/**
 * Gets the name of the global list variable at the specified index.
 *
 * @param id the index, or ID
 * @return the list variable name
 */
std::string Person::get_global_list_var_name(int id) {
  if(Person::get_number_of_global_list_vars() <= id) {
    return "";
  }
  return Person::global_list_var_name[id];
}

/**
 * Gets the index of the global list variable with the specified name.
 *
 * @param vname the list variable name
 * @return the index, or ID
 */
int Person::get_global_list_var_id(std::string vname) {
  for(int i = 0; i < Person::get_number_of_global_list_vars(); ++i) {
    if(vname == Person::global_list_var_name[i]) {
      return i;
    }
  }
  return -1;
}

/**
 * Starts reporting for this person if not already done so given a rule.
 *
 * @param rule the rule
 */
void Person::start_reporting(Rule *rule) {

  Expression* expression = rule->get_expression();
  for(int i = 0; i < static_cast<int>(Person::report_vec.size()); ++i) {
    report_t* report = Person::report_vec[i];
    if(report->person==this && report->expression->get_name() == expression->get_name()) {
      return;
    }
  }
  
  // find index for this person in the reporting list
  int index = -1;
  int report_person_size = Person::report_person.size();
  for(int i = 0; i < report_person_size; ++i) {
    if(Person::report_person[i] == this) {
      index = i;
      break;
    }
  }

  if(index < 0) {
    if(Person::max_reporting_agents < static_cast<int>(Person::report_vec.size())) {
      Person::report_person.push_back(this);
      index = report_person_size;
    } else {
      // no room for more reporters
      return;
    }
  }

  // insert new report
  Person::report_vec.push_back(new report_t);
  int n = Person::report_vec.size() - 1;
  Person::report_vec[n]->person_index = index;
  Person::report_vec[n]->person_id = this->id;
  Person::report_vec[n]->person = this;
  Person::report_vec[n]->expression = expression;
}


//////////////////////////////////////////////

// FROM HEALTH

#include "Date.h"
#include "Condition.h"
#include "Household.h"
#include "Natural_History.h"
#include "Group.h"
#include "Parser.h"
#include "Person.h"
#include "Place.h"
#include "Random.h"
#include "Utils.h"

/**
 * Sets up the conditions for this person. Each property of the conditions and their corresponding states will 
 * be initialized to their default values.
 */
void Person::setup_conditions() {
  Person::person_logger->info("Person::setup for person {:d}", get_id());
  this->alive = true;
  
  this->number_of_conditions = Condition::get_number_of_conditions();

  this->condition = new condition_t[this->number_of_conditions];

  for(int condition_id = 0; condition_id < this->number_of_conditions; ++condition_id) {
    this->condition[condition_id].state = 0;
    this->condition[condition_id].susceptibility = 0;
    this->condition[condition_id].transmissibility = 0;
    this->condition[condition_id].last_transition_step = -1;
    this->condition[condition_id].next_transition_step = -1;
    this->condition[condition_id].exposure_day = -1;
    this->condition[condition_id].is_fatal = false;
    this->condition[condition_id].source = nullptr;
    this->condition[condition_id].group = nullptr;
    this->condition[condition_id].number_of_hosts = 0;
    int states = this->get_natural_history(condition_id)->get_number_of_states();
    this->condition[condition_id].entered = new int [states];
    for (int i = 0; i < states; ++i) {
      this->condition[condition_id].entered[i] = -1;
    }
  }
  this->previous_infection_serotype = -1;

}

/**
 * Initializes this person's variables. Then, for each Condition of this person, initializes this person 
 * in that condition's associated epidemic at the given day.
 *
 * @param day the day
 */
void Person::initialize_conditions(int day) {
  this->initialize_my_variables();
  for(int condition_id = 0; condition_id < this->number_of_conditions; ++condition_id) {
    Condition::get_condition(condition_id)->initialize_person(this, day);
  }
}

/**
 * Exposes this person to the specified Condition. This condition was transmitted from the specified source Person 
 * in the specified Group, at the given day and hour. A transmission Network will be initialized if one does not 
 * already exist for the condition, and an edge will be added between this person and the source. The Place_Type 
 * that the condition transmits in will also be noted, and the Place of that type that the source is linked to will 
 * be set as active for this person. That place's group state counts will be incremented based on this person's 
 * states of each condition.
 *
 * @param condition_id the condition ID
 * @param source the source person
 * @param group the group
 * @param day the day
 * @param hour the hour
 */
void Person::become_exposed(int condition_id, Person* source, Group* group, int day, int hour) {
  
  Person::person_logger->info("HEALTH: become_exposed: person {:d} is exposed to condition {:d} day {:d} hour {:d}",
      this->get_id(), condition_id, day, hour);
  
  if(Global::Enable_Records) {
    fprintf(Global::Recordsfp,
      "HEALTH RECORD: %s %s day %d person %d age %d is %s to %s%s%s",
      Date::get_date_string().c_str(), Date::get_12hr_clock(hour).c_str(),
      day, get_id(), get_age(),
      (source == Person::get_import_agent() ? "an IMPORTED EXPOSURE" : "EXPOSED"),
      Condition::get_name(condition_id).c_str(),
      group == nullptr ? "" : " at ",
      group == nullptr ? "" : group->get_label());
    if(source == Person::get_import_agent()) {
      fprintf(Global::Recordsfp, "\n");
    } else {
      fprintf(Global::Recordsfp, " from person %d age %d\n", source->get_id(), source->get_age());
    }
  }

  this->set_source(condition_id, source);
  this->set_group(condition_id, group);
  this->set_exposure_day(condition_id, day);

  if(Condition::get_condition(condition_id)->get_transmission_network()!=nullptr) {
    Network* network = Condition::get_condition(condition_id)->get_transmission_network();
    if(network) {
      this->join_network(network);
      source->join_network(network);
      source->add_edge_to(this, network);
      this->add_edge_from(source, network);
    }
  }

  int place_type_id = Condition::get_condition(condition_id)->get_place_type_to_transmit();
  if(0 <= place_type_id) {
    Place* place = nullptr;
    if(source == Person::get_import_agent()) {
      Person::person_logger->debug("PLACE_TRANSMISSION generate new place");
      place = Place_Type::generate_new_place(place_type_id, this);
    } else {
      place = source->get_place_of_type(place_type_id);
      Person::person_logger->debug("PLACE TRANSMISSION inherit place {:s} from source {:d}", 
          (place ? place->get_label() : "NULL"), source->get_id());
    }
    if(place->get_size() < place->get_max_size()) {
      this->set_place_of_type(place_type_id, place);

      // increment the group_state_counts for all conditions for this place
      for(int cond_id = 0; cond_id < Condition::get_number_of_conditions(); ++cond_id) {
        int state = get_state(cond_id);
        Condition::get_condition(cond_id)->increment_group_state_count(place_type_id, place, state);
      }
      
      if(Global::Enable_Records) {
        fprintf(Global::Recordsfp,
          "HEALTH RECORD: %s %s day %d person %d GETS TRANSMITTED PLACE type %s label %s from person %d size = %d\n",
          Date::get_date_string().c_str(), Date::get_12hr_clock(hour).c_str(),
          day, this->get_id(),
          Place_Type::get_place_type_name(place_type_id).c_str(),
          (place == nullptr ? "NULL" : place->get_label()),
          source->get_id(),
          (place == nullptr ? 0 : place->get_size()));
      }
    }
  }

  Person::person_logger->info(
      "HEALTH: become_exposed FINISHED: person {:d} is exposed to condition {:d} day {:d} hour {:d}",
      get_id(), condition_id, day, hour);
  
}

/**
 * Sets the specified Condition as fatal for this person on the given day. This person will be queued for death 
 * and removal from the population.
 *
 * @param condition_id the condition ID
 * @param day the day
 */
void Person::become_case_fatality(int condition_id, int day) {
  if(this->is_meta_agent()) {
    return;
  }
  Person::person_logger->info("CONDITION {:s} STATE {:s} is FATAL: day {:d} person {:d}",
      Condition::get_name(condition_id).c_str(),
      Condition::get_condition(condition_id)->get_state_name(this->get_state(condition_id)).c_str(),
      day, this->get_id());

  if(Global::Enable_Records) {
    fprintf(Global::Recordsfp,
      "HEALTH RECORD: %s %s day %d person %d age %d sex %c race %d income %d is CASE_FATALITY for %s.%s\n",
      Date::get_date_string().c_str(), Date::get_12hr_clock(Global::Simulation_Hour).c_str(),
      Global::Simulation_Day,
      this->get_id(), this->get_age(), this->get_sex(), this->get_race(), this->get_income(),
      Condition::get_name(condition_id).c_str(),
      Condition::get_condition(condition_id)->get_state_name(this->get_state(condition_id)).c_str());
  }

  // set status flags
  this->set_case_fatality(condition_id);

  // queue removal from population
  Person::prepare_to_die(day, this);
  Person::person_logger->info("become_case_fatality finished for person {:d}", this->get_id());
}

/**
 * _UNUSED
 */
void Person::update_condition(int day, int condition_id) {
} // end Person::update_condition //

/**
 * Gets this person's susceptibility to the specified Condition.
 *
 * @param condition_id the condition ID
 * @return the susceptibility
 */
double Person::get_susceptibility(int condition_id) const {
  return this->condition[condition_id].susceptibility;
}

/**
 * Gets the transmissiblility of the specified Condition by this person.
 *
 * @param condition_id the condition ID
 * @return the susceptibility
 */
double Person::get_transmissibility(int condition_id) const {
  return this->condition[condition_id].transmissibility;
}

/**
 * Gets the number of transmissions this person has been the source of for the specified Condition.
 *
 * @param condition_id the condition ID
 * @return the number of transmissions
 */
int Person::get_transmissions(int condition_id) const {
  return this->condition[condition_id].number_of_hosts;
}

/**
 * Exposes the specified Person, who is now a host. This transmission will be tracked under the number of transmissions 
 * by this person for the source Condition, and the exposed person will be transmitted the condition to transmit dependent 
 * on this person's state in the source condition. This exposure took place in the specified Group 
 * at the given day and hour. The source condition's cohort host count will be incremented for the day at which this 
 * person was exposed to the source condition.
 *
 * @param host the exposed host
 * @param source_condition_id this person's condition
 * @param condition_id the condition to transmit
 * @param group the group
 * @param day the day
 * @param hour the hour
 */
void Person::expose(Person* host, int source_condition_id, int condition_id, Group* group, int day, int hour) {

  host->become_exposed(condition_id, this, group, day, hour);

  this->condition[source_condition_id].number_of_hosts++;
  
  int exp_day = this->get_exposure_day(source_condition_id);

  if(0 <= exp_day) {
    Condition* condition = Condition::get_condition(source_condition_id);
    condition->increment_cohort_host_count(exp_day);
  }

}

/**
 * Terminates this person in each Condition at the given day. This person is set to not alive.
 *
 * @param day the day
 */
void Person::terminate_conditions(int day) {
  Person::person_logger->info("TERMINATE CONDITIONS for person {:d} day {:d}", get_id(), day);

  for(int condition_id = 0; condition_id < Condition::get_number_of_conditions(); ++condition_id) {
    Condition::get_condition(condition_id)->terminate_person(this, day);
  }
  this->alive = false;
}

/**
 * Sets the specified Condition and state for the person. The current time step is tracked as the step 
 * this condition state was entered, as well as the last transition step in the condition for the person.
 *
 * @param condition_id the condition ID
 * @param state the state
 * @param day _UNUSED_
 */
void Person::set_state(int condition_id, int state, int day) {
  this->condition[condition_id].state = state;
  int current_time = 24 * Global::Simulation_Day + Global::Simulation_Hour;
  this->condition[condition_id].entered[state] = current_time;
  this->set_last_transition_step(condition_id, current_time);
  Person::person_logger->info("set_state person {:d} cond {:d} state {:d}", this->get_id(), condition_id, state);
}

/**
 * Gets the ID of the Group of which this person is a member in which the specified Condition is active.
 *
 * @param condition_id the condition ID
 * @return the group ID
 */
int Person::get_group_id(int condtion_id) const {
  return (this->get_group(condtion_id) ? this->get_group(condtion_id)->get_id() : -1);
}

/**
 * Gets the label of the Group of which this person is a member in which the specified Condition is active.
 *
 * @param condition_id the condition ID
 * @return the group label
 */
char* Person::get_group_label(int condtion_id) const {
  return (this->get_group(condtion_id) ? this->get_group(condtion_id)->get_label() : (char*)"X");
}

/**
 * Gets the Group_Type of the Group of which this person is a member in which the specified Condition is active.
 *
 * @param condition_id the condition ID
 * @return the group type ID
 */
int Person::get_group_type_id(int condition_id) const {
  return (this->get_group(condition_id) ? this->get_group(condition_id)->get_type_id() : -1);
}

/**
 * Gets the value this person's personal variable at the specified index. If this person's variables are 
 * not yet initialized, they are done so first.
 *
 * @param index the index
 * @return the value
 */
double Person::get_var(int index) {
  int number_of_vars = Person::get_number_of_vars();
  Person::person_logger->info("get_var person {:d} index {:d} number of vars {:d}", this->id, index, number_of_vars);

  if(this->var == nullptr) {
    this->initialize_my_variables();
  }

  if(index < number_of_vars) {
    assert(this->var != nullptr);
    return this->var[index];
  } else {
    Person::person_logger->error("ERR: Can't find variable person {:d} index = {:d} vars = {:d}", this->id, 
        index, number_of_vars);
    // exit(0);
    return 0.0;
  }
}

/**
 * Sets the value this person's personal variable at the specified index. If this person's variables are 
 * not yet initialized, they are done so first.
 *
 * @param index the index
 * @param value the value
 */
void Person::set_var(int index, double value) {
  int number_of_vars = Person::get_number_of_vars();
  Person::person_logger->info("set_var person {:d} index {:d} number of vars {:d}", this->id, index, number_of_vars);
  if(this->var == nullptr) {
    this->initialize_my_variables();
  }

  if(index < number_of_vars) {
    this->var[index] = value;
  }
}

/**
 * Gets the value of the global variable at the specified index.
 *
 * @param index the index
 * @return the value
 */
double Person::get_global_var(int index) {
  int number_of_vars = Person::get_number_of_global_vars();
  if(index < number_of_vars) {
    return Person::global_var[index];
  } else {
    Person::person_logger->error("ERR: Can't find global var index = {:d} vars = {:d}", 
        index, number_of_vars);
    return 0.0;
  }
}

/**
 * Gets the size of this person's personal list variable at the specified index. If this person's variables are 
 * not yet initialized, they are done so first.
 *
 * @param list_var_id the index
 * @return the size
 */
int Person::get_list_size(int list_var_id) {
  if(this->list_var == nullptr) {
    this->initialize_my_variables();
  }

  if(0 <= list_var_id && list_var_id < Person::get_number_of_list_vars()) {
    return this->list_var[list_var_id].size();
  } else {
    return -1;
  }
}

/**
 * Gets the size of the global list variable at the specified index. 
 *
 * @param list_var_id the index
 * @return the size
 */
int Person::get_global_list_size(int list_var_id) {
  if(0 <= list_var_id && list_var_id < Person::get_number_of_global_list_vars()) {
    return Person::global_list_var[list_var_id].size();
  } else {
    return -1;
  }
}

/**
 * Gets the value this person's personal list variable at the specified index. If this person's variables are 
 * not yet initialized, they are done so first.
 *
 * @param index the index
 * @return the value
 */
double_vector_t Person::get_list_var(int index) {
  int number_of_list_vars = Person::get_number_of_list_vars();
  if(index < number_of_list_vars) {
    int size = this->list_var[index].size();
    for(int i = 0; i < size; ++i) {
      Person::person_logger->trace("<{:s}, {:d}>: person {:d} [{:d}] = {:f}", 
          __FILE__, __LINE__, this->id, i, this->list_var[index][i]);
    }
    
    if(this->list_var == nullptr) {
      this->initialize_my_variables();
    }

    return this->list_var[index];
  } else {
    Person::person_logger->critical("ERR: index = {:d} vars = {:d}", index, number_of_vars);
    assert(0);
    double_vector_t empty;
    return empty;
  }
}

/**
 * Gets the value of the global list variable at the specified index.
 *
 * @param index the index
 * @return the value
 */
double_vector_t Person::get_global_list_var(int index) {
  int number_of_list_vars = Person::get_number_of_global_list_vars();
  if(index < number_of_list_vars) {
    return Person::global_list_var[index];
  } else {
    Person::person_logger->critical("ERR: index = {:d} vars = {:d}", index, number_of_vars);
    assert(0);
    double_vector_t empty;
    return empty;
  }
}

/**
 * Sets the value of the global list variable at the specified index.
 *
 * @param list_var_id the index
 * @param value the value
 */
void Person::push_back_global_list_var(int list_var_id, double value) {
  Person::global_list_var[list_var_id].push_back(value);
}

/**
 * Outputs data on properties of this person, their conditions, and their personal variables to the 
 * specified file on the given day.
 *
 * @param fp the file
 * @param day the day
 */
void Person::request_external_updates(FILE* fp, int day) {
  fprintf(fp, "day = %d\n", day);
  fprintf(fp, "person = %d\n", this->get_id());
  fprintf(fp, "age = %d\n", this->get_age());
  fprintf(fp, "race = %d\n", this->get_race());
  fprintf(fp, "sex = %c\n", this->get_sex());
  for(int condition_id = 0; condition_id < this->number_of_conditions; ++condition_id) {
    Condition* condition = Condition::get_condition(condition_id);
    int state = this->get_state(condition_id);
    fprintf(fp, "%s = %s\n", condition->get_name(), condition->get_state_name(state).c_str());
  }
  int number_of_vars = Person::get_number_of_vars();
  for(int i = 0; i < number_of_vars; ++i) {
    fprintf(fp, "%s = %f\n", Person::get_var_name(i).c_str(), this->var[i]);
  }  
  fflush(fp);
}

/**
 * Updates this person's personal variables on the given day by scanning the specified update file.
 *
 * @param fp the file
 * @param day the day
 */
void Person::get_external_updates(FILE* fp, int day) {
  int ival;
  char cval;
  double fval;
  char vstr[FRED_STRING_SIZE];
  char vstr2[FRED_STRING_SIZE];

  fscanf(fp, "day = %d ", &ival);
  if(ival != day) {
    Person::person_logger->critical("Error: day out of sync {:d} {:d}", day, ival);
    assert(ival == day);
  }

  fscanf(fp, "person = %d ", &ival);
  if(ival != get_id()) {
    Person::person_logger->critical("Error: id {:d} {:d}", this->get_id(), ival);
    assert(ival == this->get_id());
  }

  fscanf(fp, "age = %d ", &ival);
  if(ival != get_age()) {
    Person::person_logger->critical("Error: age {:d} {:d}", this->get_age(), ival);
    assert(ival == this->get_age());
  }

  fscanf(fp, "race = %d ", &ival);
  if(ival != get_race()) {
    Person::person_logger->critical("Error: race {:d} {:d}", this->get_race(), ival);
    assert(ival == this->get_race());
  }

  fscanf(fp, "sex = %c ", &cval);
  if(cval != get_sex()) {
    Person::person_logger->critical("Error: sex {:c} {:c}", this->get_sex(), cval);
    assert(cval == this->get_sex());
  }

  for(int condition_id = 0; condition_id < this->number_of_conditions; ++condition_id) {
    Condition* condition = Condition::get_condition(condition_id);
    int state = this->get_state(condition_id);

    fscanf(fp, "%s = %s ", vstr, vstr2);
    assert(strcmp(vstr, condition->get_name()) == 0);
    assert(strcmp(vstr2, condition->get_state_name(state).c_str()) == 0);
  }
  int number_of_vars = Person::get_number_of_vars();
  for(int i = 0; i < number_of_vars; ++i) {
    fscanf(fp, "%s = %lf ", vstr, &fval);
    snprintf(vstr2, FRED_STRING_SIZE, "%s", Person::get_var_name(i).c_str());
    assert(strcmp(vstr, vstr2)==0);
    this->var[i] = fval;
  }  
}

/**
 * Gets the Natural_History of the specified Condition.
 *
 * @param condition_id the condition ID
 * @return the natural history
 */
Natural_History* Person::get_natural_history(int condition_id) const {
  return Condition::get_condition(condition_id)->get_natural_history();
}


///////////////////////////////////////////

//// ACTIVITIES

bool Person::is_weekday = false;
int Person::day_of_week = 0;

/**
 * Gets the label of the specified Place.
 *
 * @param place the place
 * @return the label
 */
const char* get_label_for_place(Place* place) {
  return (place != nullptr ? place->get_label() : "NULL");
}

/**
 * Sets this person's activities, such as household, neighborhood, school, and workplace. Calls to assign their 
 * initial profile.
 *
 * @param house the household
 * @param school the school
 * @param work the workplace
 */
void Person::setup_activities(Place* house, Place* school, Place* work) {
  Person::person_logger->info("ACTIVITIES_SETUP: person {:d} age {:d} household {:s}", this->get_id(),
      this->get_age(), house->get_label());

  assert(house != nullptr);
  this->clear_activity_groups();
  this->set_household(house);
  this->set_school(school);
  this->set_workplace(work);

  // get the neighborhood from the household
  this->set_neighborhood(this->get_household()->get_patch()->get_neighborhood());
  if(this->get_neighborhood() != nullptr) {
    Person::person_logger->info("ACTIVITIES_SETUP: person {:d} neighborhood {:d} {:s}", this->get_id(),
        this->get_neighborhood()->get_id(), this->get_neighborhood()->get_label());
  } else {
    Person::person_logger->error("HELP: NO NEIGHBORHOOD for person {:d} house {:d}", this->get_id(),
        this->get_household()->get_id());
  }
  this->home_neighborhood = this->get_neighborhood();

  // assign profile
  this->assign_initial_profile();

  // need to set the daily schedule
  this->schedule_updated = -1;
  this->is_traveling = false;
  this->is_traveling_outside = false;

  std::stringstream ss;
  ss << fmt::format("ACTIVITY::SETUP finished for person {:d} ", get_id());
  for(int n = 0; n < Place_Type::get_number_of_place_types(); n++) {
    ss << fmt::format("{:s} {:s} ", Place_Type::get_place_type_name(n).c_str(), 
        this->get_place_of_type(n) ? this->get_place_of_type(n)->get_label() : "NULL");
  }
  Person::person_logger->trace("<{:s}, {:d}>: {:s}", __FILE__, __LINE__, ss.str());
}

/**
 * Assigns this person an activity profile dependent on factors such as their age and household relationship.
 */
void Person::assign_initial_profile() {
  int age = this->get_age();
  if(age == 0) {
    this->profile = Activity_Profile::PRESCHOOL;
    this->in_parents_home = true;
  } else if(this->get_school() != nullptr) {
    this->profile = Activity_Profile::STUDENT;
    this->in_parents_home = true;
  } else if(age < Global::SCHOOL_AGE) {
    this->profile = Activity_Profile::PRESCHOOL;// child at home
    this->in_parents_home = true;
  } else if(age < Global::ADULT_AGE) {
    this->profile = Activity_Profile::STUDENT;// school age
    this->in_parents_home = true;
  } else if(get_workplace() != nullptr) {
    this->profile = Activity_Profile::WORKER;// worker
  } else if(age < Global::RETIREMENT_AGE) {
    this->profile = Activity_Profile::WORKER;// worker
  } else if(Global::RETIREMENT_AGE <= age) {
    if(Random::draw_random()< 0.5 ) {
      this->profile = Activity_Profile::RETIRED;// retired
    } else {
      this->profile = Activity_Profile::WORKER;// older worker
    }
  } else {
    this->profile = Activity_Profile::UNEMPLOYED;
  }

  if(this->profile == Activity_Profile::WORKER || this->profile == Activity_Profile::UNEMPLOYED) {
    // determine if I am living in parents house
    int rel = this->get_household_relationship();
    if(rel == Household_Relationship::CHILD || rel == Household_Relationship::GRANDCHILD || rel == Household_Relationship::FOSTER_CHILD) {
      this->in_parents_home = true;
    }
  }  

  // weekend worker
  if((this->profile == Activity_Profile::WORKER && Random::draw_random() < 0.2)) {  // 20% weekend worker
    this->profile = Activity_Profile::WEEKEND_WORKER;
  }

  // profiles for group quarters residents
  if(this->get_household()->is_college()) {
    this->profile = Activity_Profile::COLLEGE_STUDENT;
    this->update_profile_after_changing_household();
    this->in_parents_home = false;
    return;
  }
  if(this->get_household()->is_military_base()) {
    this->profile = Activity_Profile::MILITARY;
    this->update_profile_after_changing_household();
    this->in_parents_home = false;
    return;
  }
  if(this->get_household()->is_prison()) {
    this->profile = Activity_Profile::PRISONER;
    Person::person_logger->trace(
        "<{:s}, {:d}>: INITIAL PROFILE AS PRISONER ID {:d} AGE {:d} SEX {:c} HOUSEHOLD {:s}", 
        __FILE__, __LINE__, get_id(), age, get_sex(), get_household()->get_label());
   this->update_profile_after_changing_household();
    this->in_parents_home = false;
    return;
  }
  if(this->get_household()->is_nursing_home()) {
    this->profile = Activity_Profile::NURSING_HOME_RESIDENT;
    this->update_profile_after_changing_household();
    this->in_parents_home = false;
    return;
  }
}

/**
 * Updates the Person variable is_weekday.
 *
 * @param sim_day _UNUSED_
 */
void Person::update(int sim_day) {
  Person::person_logger->info("Activities update entered");
  // decide if this is a weekday:
  Person::is_weekday = Date::is_weekday();
  Person::person_logger->info("Activities update completed");
}

/**
 * Updates this person's activities for the given simulation day. These activities will depend on person 
 * properties.
 *
 * @param sim_day the numbered simulation day in the range of [0, Global::Simulation_Days)
 */
void Person::update_activities(int sim_day) {

  Person::person_logger->info("update_activities for person {:d} day {:d}", this->get_id(), sim_day);

  // update activity schedule only once per day
  if(sim_day <= this->schedule_updated) {
    return;
  }

  this->schedule_updated = sim_day;

  // clear the schedule
  this->on_schedule.reset();

  // reset neighborhood
  if(this->get_household()->get_patch() != nullptr) {
    this->set_neighborhood(this->get_household()->get_patch()->get_neighborhood());
  } else {
    this->set_neighborhood(nullptr);
  }

  // normally participate in household activities
  this->on_schedule[Place_Type::get_type_id("Household")] = true;

  // non-built-in activities
  for(int i = Place_Type::get_type_id("Hospital") + 1; i < Group_Type::get_number_of_group_types(); ++i) {
    if(this->link[i].is_member()) {
      this->on_schedule[i] = 1;
    } else {
      this->on_schedule[i] = 1;
    }
  }

  if(this->profile == Activity_Profile::PRISONER || this->profile == Activity_Profile::NURSING_HOME_RESIDENT) {
    // prisoners and nursing home residents stay indoors
    this->on_schedule[Place_Type::get_type_id("Workplace")] = true;
    this->on_schedule[Place_Type::get_type_id("Office")] = true;
    return;
  }

  // normally visit the neighborhood
  this->on_schedule[Place_Type::get_type_id("Neighborhood")] = true;

  // decide which neighborhood to visit today
  if(is_transmissible()) {
    if(this->home_neighborhood != nullptr) {
      Place* destination_neighborhood = Global::Neighborhoods->select_destination_neighborhood(this->home_neighborhood);
      this->set_neighborhood(destination_neighborhood);
    } else {
      this->set_neighborhood(nullptr);
    }
  } else {
    if(this->get_household()->get_patch() != nullptr) {
      this->set_neighborhood(this->get_household()->get_patch()->get_neighborhood());
    } else {
      this->set_neighborhood(nullptr);
    }
  }

  // attend school only on weekdays
  if(Person::is_weekday) {
    if(this->get_school() != nullptr) {
      this->on_schedule[Place_Type::get_type_id("School")] = true;
      if(this->get_classroom() != nullptr) {
        this->on_schedule[Place_Type::get_type_id("Classroom")] = true;
      }
    }
  }

  // normal worker work only on weekdays
  if(Person::is_weekday) {
    if(this->get_workplace() != nullptr) {
      this->on_schedule[Place_Type::get_type_id("Workplace")] = true;
      if(this->get_office() != nullptr) {
        this->on_schedule[Place_Type::get_type_id("Office")] = true;
      }
    }
  } else {
    // students with jobs and weekend worker work on weekends
    if(this->profile == Activity_Profile::WEEKEND_WORKER || this->profile == Activity_Profile::STUDENT) {
      if(this->get_workplace() != nullptr) {
        this->on_schedule[Place_Type::get_type_id("Workplace")] = true;
        if(this->get_office() != nullptr) {
          this->on_schedule[Place_Type::get_type_id("Office")] = true;
        }
      }
    }
  }

  Person::person_logger->info("update_activities on day {:d}\n{:s}", sim_day, schedule_to_string(sim_day).c_str());
}

/**
 * _UNUSED_
 */
void Person::start_hospitalization(int sim_day, int length_of_stay) {
}

/**
 * _UNUSED_
 */
void Person::end_hospitalization() {
}

/**
 * Assigns this person randomly to a school. First, will attempt to select a school from the census tract, then the county, 
 * and lastly will randomly assign one.
 */
void Person::assign_school() {
  int day = Global::Simulation_Day;
//  Place* old_school = this->last_school;
  int grade = this->get_age();
  Person::person_logger->info("assign_school entered for person {:d} age {:d} grade {:d}", get_id(), get_age(), grade);

  Household* hh = this->get_household();
  assert(hh != nullptr);

  Place* school = nullptr;
  school = Census_Tract::get_census_tract_with_admin_code(hh->get_census_tract_admin_code())->select_new_school(grade);
  if(school == nullptr) {
    school = County::get_county_with_admin_code(hh->get_county_admin_code())->select_new_school(grade);
    Person::person_logger->debug("DAY {:d} ASSIGN_SCHOOL FROM COUNTY {:s} selected for person {:d} age {:d}",
        day, school->get_label(), this->get_id(), this->get_age());
  } else {
    Person::person_logger->debug("DAY {:d} ASSIGN_SCHOOL FROM CENSUS_TRACT {:s} selected for person {:d} age {:d}",
        day, school->get_label(), this->get_id(), this->get_age());
  }
  if(school == nullptr) {
    school = Place::get_random_school(grade);
  }
  assert(school != nullptr);
  this->set_school(school);
  this->set_classroom(nullptr);
  this->assign_classroom();
  Person::person_logger->info("assign_school finished for person {:d} age {:d}: school {:s} classroom {:s}",
      this->get_id(), this->get_age(), this->get_school()->get_label(), this->get_classroom()->get_label());
}

/**
 * Assigns this person a classroom by selecting a partition of this person's school.
 */
void Person::assign_classroom() {
  assert(get_school() != nullptr && this->get_classroom() == nullptr);
  Person::person_logger->info("assign classroom entered");

  Place* school = this->get_school();
  Place* place = school->select_partition(this);
  if(place == nullptr) {
    Person::person_logger->warn("CLASSROOM_WARNING: assign classroom returns nullptr: person {:d} age {:d} school {:s}",
        this->get_id(), this->get_age(), school->get_label());
  }
  this->set_classroom(place);
  Person::person_logger->info("assign classroom finished");
}

/**
 * Assigns this person randomly to a workplace. First, will attempt to select a workplace from the census tract, then 
 * the county.
 */
void Person::assign_workplace() {
  Household* hh;
  hh = this->get_household();
  Place* p = nullptr;
  p = Census_Tract::get_census_tract_with_admin_code(hh->get_census_tract_admin_code())->select_new_workplace();
  if(p == nullptr) {
    p = County::get_county_with_admin_code(hh->get_county_admin_code())->select_new_workplace();
    Person::person_logger->debug("ASSIGN_WORKPLACE FROM COUNTY {:s} selected for person {:d} age {:d}",
        p->get_label(), this->get_id(), this->get_age());
  } else {
    Person::person_logger->debug("ASSIGN_WORKPLACE FROM CENSUS_TRACT {:s} selected for person {:d} age {:d}",
        p->get_label(), this->get_id(), this->get_age());
  }
  this->change_workplace(p);
}

/**
 * Assigns this person an office by selecting a partition of this person's workplace.
 */
void Person::assign_office() {
  if(get_workplace() != nullptr && this->get_office() == nullptr && this->get_workplace()->is_workplace()
     && Place_Type::get_place_type("Workplace")->get_partition_capacity() > 0) {
    Place* place = get_workplace()->select_partition(this);
    if(place == nullptr) {
      Person::person_logger->warn("OFFICE WARNING: No office assigned for person {:d} workplace {:d}", 
          this->get_id(), this->get_workplace()->get_id());
    }
    this->set_office(place);
  }
}

/**
 * _UNUSED_
 */
void Person::assign_primary_healthcare_facility() {
}

/**
 * _UNUSED_
 */
void Person::assign_hospital(Place* place) {
}

int Person::get_degree() {
  int degree;
  int n;
  degree = 0;
  n = this->get_group_size(Place_Type::get_type_id("Neighborhood"));
  if(n > 0) {
    degree += (n - 1);
  }
  n = this->get_group_size(Place_Type::get_type_id("School"));
  if(n > 0) {
    degree += (n - 1);
  }
  n = this->get_group_size(Place_Type::get_type_id("Workplace"));
  if(n > 0) {
    degree += (n - 1);
  }
  n = this->get_group_size(Place_Type::get_type_id("Hospital"));
  if(n > 0) {
    degree += (n - 1);
  }
  return degree;
}


/**
 * Gets the size of the Group with the specified Group_Type that this person is a member of.
 *
 * @param index the group type ID
 * @return the size
 */
int Person::get_group_size(int index) {
  int size = 0;
  if(this->get_activity_group(index) != nullptr) {
    size = this->get_activity_group(index)->get_size();
  }
  return size;
}

/**
 * _UNUSED_
 */
bool Person::is_hospital_staff() {
  bool ret_val = false;
  return ret_val;
}

/**
 * Checks if this person is prison staff.
 *
 * @return if this person is prison staff
 */
bool Person::is_prison_staff() {
  bool ret_val = false;

  if(this->profile == Activity_Profile::WORKER || this->profile == Activity_Profile::WEEKEND_WORKER) {
    if(this->get_workplace() != nullptr && this->get_household() != nullptr) {
      if(this->get_workplace()->is_prison() && !this->get_household()->is_prison()) {
        ret_val = true;
      }
    }
  }

  return ret_val;
}

/**
 * Checks if this person is college dorm staff.
 *
 * @return if this person is college dorm staff
 */
bool Person::is_college_dorm_staff() {
  bool ret_val = false;

  if(this->profile == Activity_Profile::WORKER || this->profile == Activity_Profile::WEEKEND_WORKER) {
    if(this->get_workplace() != nullptr && this->get_household() != nullptr) {
      if(this->get_workplace()->is_college() && !this->get_household()->is_college()) {
        ret_val = true;
      }
    }
  }

  return ret_val;
}

/**
 * Checks if this person is military base staff.
 *
 * @return if this person is military base staff
 */
bool Person::is_military_base_staff() {
  bool ret_val = false;

  if(this->profile == Activity_Profile::WORKER || this->profile == Activity_Profile::WEEKEND_WORKER) {
    if(this->get_workplace() != nullptr && this->get_household() != nullptr) {
      if(this->get_workplace()->is_military_base() &&
         !this->get_household()->is_military_base()) {
        ret_val = true;
      }
    }
  }

  return ret_val;
}

/**
 * Checks if this person is nursing home staff.
 *
 * @return if this person is nursing home staff
 */
bool Person::is_nursing_home_staff() {
  bool ret_val = false;

  if(this->profile == Activity_Profile::WORKER || this->profile == Activity_Profile::WEEKEND_WORKER) {
    if(this->get_workplace() != nullptr && this->get_household() != nullptr) {
      if(this->get_workplace()->is_nursing_home() && !this->get_household()->is_nursing_home()) {
        ret_val = true;
      }
    }
  }

  return ret_val;
}

/**
 * Updates this person's profile following a change of household. Will check for out of sync activity profiles 
 * and correct them.
 */
void Person::update_profile_after_changing_household() {
  int age = this->get_age();
  int day = Global::Simulation_Day;

  // profiles for new group quarters residents
  if(get_household()->is_college()) {
    if(this->profile != Activity_Profile::COLLEGE_STUDENT) {
      this->profile = Activity_Profile::COLLEGE_STUDENT;
      this->change_school(nullptr);
      this->change_workplace(this->get_household()->get_group_quarters_workplace());
      Person::person_logger->debug("AFTER_MOVE CHANGING PROFILE TO COLLEGE_STUDENT: person {:d} age {:d} DORM {:s}",
          this->get_id(), age, this->get_household()->get_label());
    }
    this->in_parents_home = false;
    return;
  }

  if(this->get_household()->is_military_base()) {
    if(this->profile != Activity_Profile::MILITARY) {
      this->profile = Activity_Profile::MILITARY;
      this->change_school(nullptr);
      this->change_workplace(this->get_household()->get_group_quarters_workplace());
      Person::person_logger->debug("AFTER_MOVE CHANGING PROFILE TO MILITARY: person {:d} age {:d} BARRACKS {:s}",
          this->get_id(), age, this->get_household()->get_label());
    }
    this->in_parents_home = false;
    return;
  }

  if(this->get_household()->is_prison()) {
    if(this->profile != Activity_Profile::PRISONER) {
      this->profile = Activity_Profile::PRISONER;
      this->change_school(nullptr);
      this->change_workplace(this->get_household()->get_group_quarters_workplace());
      Person::person_logger->debug("AFTER_MOVE CHANGING PROFILE TO PRISONER: person {:d} age {:d} PRISON {:s}",
          this->get_id(), age, this->get_household()->get_label());
    }
    this->in_parents_home = false;
    return;
  }

  if(this->get_household()->is_nursing_home()) {
    if(this->profile != Activity_Profile::NURSING_HOME_RESIDENT) {
      this->profile = Activity_Profile::NURSING_HOME_RESIDENT;
      this->change_school(nullptr);
      this->change_workplace(get_household()->get_group_quarters_workplace());
      Person::person_logger->debug("AFTER_MOVE CHANGING PROFILE TO NURSING HOME: person {:d} age {:d} NURSING_HOME {:s}",
          this->get_id(), age, this->get_household()->get_label());
    }
    this->in_parents_home = false;
    return;
  }

  // former military
  if(this->profile == Activity_Profile::MILITARY && this->get_household()->is_military_base() == false) {
    this->change_school(nullptr);
    this->change_workplace(nullptr);
    this->profile = Activity_Profile::WORKER;
    this->assign_workplace();
    Person::person_logger->debug(
        "AFTER_MOVE CHANGING PROFILE FROM MILITRAY TO WORKER: person {:d} age {:d} sex {:c} WORKPLACE {:s} OFFICE {:s}",
        this->get_id(), age, this->get_sex(), this->get_workplace()->get_label(), this->get_office()->get_label());
    return;
  }

  // former prisoner
  if(this->profile == Activity_Profile::PRISONER && this->get_household()->is_prison() == false) {
    this->change_school(nullptr);
    this->change_workplace(nullptr);
    this->profile = Activity_Profile::WORKER;
    this->assign_workplace();
    Person::person_logger->debug(
        "AFTER_MOVE CHANGING PROFILE FROM PRISONER TO WORKER: person {:d} age {:d} sex {:c} WORKPLACE {:s} OFFICE {:s}",
        this->get_id(), age, this->get_sex(), this->get_workplace()->get_label(), this->get_office()->get_label());
    return;
  }

  // former college student
  if(this->profile == Activity_Profile::COLLEGE_STUDENT && this->get_household()->is_college() == false) {
    if(Random::draw_random() < 0.25) {
      // time to leave college for good
      this->change_school(nullptr);
      this->change_workplace(nullptr);
      // get a job
      this->profile = Activity_Profile::WORKER;
      this->assign_workplace();
      Person::person_logger->debug(
          "AFTER_MOVE CHANGING PROFILE FROM COLLEGE STUDENT TO WORKER: id {:d} age {:d} sex {:c} HOUSE {:s} WORKPLACE {:s} OFFICE {:s}",
          this->get_id(), age, this->get_sex(), this->get_household()->get_label(), this->get_workplace()->get_label(), get_office()->get_label());
    }
    return;
  }

  // school age => select a new school
  if(this->profile == Activity_Profile::STUDENT && age < Global::ADULT_AGE) {
    Place* school = this->get_school();
    Place* old_school = this->last_school;
    int grade = this->get_age();
    Household* hh = this->get_household();
    assert(hh != nullptr);

    // stay in current school if this grade is available and is attended by student in this census tract
    Census_Tract* ct = Census_Tract::get_census_tract_with_admin_code(hh->get_census_tract_admin_code());
    if(school != nullptr && ct->is_school_attended(school->get_id(), grade)) {
      this->set_classroom(nullptr);
      this->assign_classroom();
      assert(this->get_school() && this->get_classroom());
      Person::person_logger->debug(
          "DAY {:d} AFTER_MOVE STAY IN CURRENT SCHOOL: person {:d} age {:d} LAST_SCHOOL {:s} SCHOOL {:s} SIZE {:d} ORIG {:d} CLASSROOM {:s}",
          day, this->get_id(), age, (old_school == nullptr ? "NULL" : old_school->get_label()),
          this->get_school()->get_label(), this->get_school()->get_size(),
          this->get_school()->get_original_size(), this->get_classroom()->get_label());
    } else {
      // select a new school
      this->change_school(nullptr);
      this->change_workplace(nullptr);
      this->assign_school();
      assert(this->get_school() && this->get_classroom());
      Person::person_logger->debug(
          "DAY {:d} AFTER_MOVE SELECT NEW SCHOOL: person {:d} age {:d} LAST_SCHOOL {:s} SCHOOL {:s} SIZE {:d} ORIG {:d} CLASSROOM {:s}",
          day, this->get_id(), age,
          (old_school == nullptr ? "NULL" : old_school->get_label()),
           this->get_school()->get_label(), this->get_school()->get_size(),
           this->get_school()->get_original_size(), this->get_classroom()->get_label());
    }
    return;
  }


  // worker => select a new workplace
  if(this->profile == Activity_Profile::WORKER || this->profile == Activity_Profile::WEEKEND_WORKER) {
    this->change_school(nullptr);
    Place* old_workplace = this->get_workplace();
    this->change_workplace(nullptr);
    this->assign_workplace();
    Person::person_logger->debug(
        "AFTER_MOVE SELECT NEW WORKPLACE: person {:d} age {:d} sex {:c} OLD WORKPLACE {:s} NEW WORKPLACE {:s} OFFICE {:s}",
        this->get_id(), age, this->get_sex(),
        (old_workplace == nullptr ? "NULL" : old_workplace->get_label()),
        this->get_workplace()->get_label(), this->get_office()->get_label());
    return;
  }
}

/**
 * Updates this person's profile based on their age. Will check for outdated activity profiles and correct them.
 */
void Person::update_profile_based_on_age() {
  int age = this->get_age();
  int day = Global::Simulation_Day;

  // pre-school children entering school
  if(this->profile == Activity_Profile::PRESCHOOL && Global::SCHOOL_AGE <= age && age < Global::ADULT_AGE) {
    this->profile = Activity_Profile::STUDENT;
    this->change_school(nullptr);
    this->change_workplace(nullptr);
    this->assign_school();
    assert(this->get_school() && this->get_classroom());
    Person::person_logger->debug(
        "AGE_UP CHANGING PROFILE FROM PRESCHOOL TO STUDENT: person {:d} age {:d} SCHOOL {:s} SIZE {:d} ORIG {:d} CLASSROOM {:s}", 
        this->get_id(), age, this->get_school()->get_label(), this->get_school()->get_size(), this->get_school()->get_original_size(),
        this->get_classroom()->get_label());
    return;
  }

  // school age
  if(this->profile == Activity_Profile::STUDENT && age < Global::ADULT_AGE) {
    Place* old_school = this->last_school;
    Place* school = this->get_school();
    int grade = this->get_age();
    // stay in current school if this grade is available
    if(school != nullptr && school->get_number_of_partitions_by_age(grade) > 0) {
      Person::person_logger->debug("DAY {:d} AGE_UP checking school status, age = {:d} classroms {:d}", 
          day, age,this-> get_school()->get_number_of_partitions_by_age(grade));
      this->set_classroom(nullptr);
      this->assign_classroom();
      assert(this->get_school() && this->get_classroom());
      Person::person_logger->debug(
          "DAY {:d} AGE_UP STAY IN SCHHOL: person {:d} age {:d} LAST_SCHOOL {:s} SCHOOL {:s} SIZE {:d} ORIG {:d} CLASSROOM {:s}",
          day, this->get_id(), age,
          (old_school == nullptr ? "NULL" : old_school->get_label()),
          this->get_school()->get_label(), get_school()->get_size(),
          this->get_school()->get_original_size(), this->get_classroom()->get_label());
    } else {
      this->change_school(nullptr);
      this->change_workplace(nullptr);
      this->assign_school();
      assert(this->get_school() && this->get_classroom());
      Person::person_logger->debug(
          "DAY {:d} AGE_UP KEEPING STUDENT PROFILE: person {:d} age {:d} LAST_SCHOOL {:s} SCHOOL {:s} SIZE {:d} ORIG {:d} CLASSROOM {:s}",
          day,  this->get_id(), age,
          (old_school == nullptr ? "NULL" : old_school->get_label()),
          this->get_school()->get_label(), this->get_school()->get_size(),
         this->get_school()->get_original_size(), this->get_classroom()->get_label());
    }
    return;
  }


  // graduating from school
  if(this->profile == Activity_Profile::STUDENT && Global::ADULT_AGE <= age) {
    Place* old_school = this->last_school;
    this->change_school(nullptr);
    this->change_workplace(nullptr);
    this->profile = Activity_Profile::WORKER;
    this->assign_workplace();
    Person::person_logger->debug(
        "DAY {:d} AGE_UP CHANGING PROFILE FROM STUDENT TO WORKER: person {:d} age {:d} LAST_SCHOOL {:s} sex {:c} WORKPLACE {:s} OFFICE {:s}",
        day, this->get_id(), age, (old_school == nullptr ? "NULL" : old_school->get_label()),
        this->get_sex(), this->get_workplace()->get_label(), this->get_office()->get_label());
    return;
  }


  // unemployed worker re-entering workplace
  if(this->profile == Activity_Profile::WORKER || this->profile == Activity_Profile::WEEKEND_WORKER) {
    if(this->get_workplace() == nullptr) {
      this->assign_workplace();
      Person::person_logger->debug(
          "AGE_UP CHANGING PROFILE FROM UNEMPLOYED TO WORKER: person {:d} age {:d} sex {:c} WORKPLACE {:s} OFFICE {:s}",
          this->get_id(), age, this->get_sex(), get_workplace()->get_label(), this->get_office()->get_label());
    }
    // NOTE: no return;
  }

  // possibly entering retirement
  if(this->profile != Activity_Profile::RETIRED && Global::RETIREMENT_AGE <= age && get_household()->is_group_quarters()==false) {
    if(Random::draw_random()< 0.5 ) {
      // quit working
      if(is_teacher()) {
        this->change_school(nullptr);
      }
      this->change_workplace(nullptr);
      this->profile = Activity_Profile::RETIRED;
      Person::person_logger->debug("AGE_UP CHANGING PROFILE TO RETIRED: person {:d} age {:d} sex", 
          this->get_id(), age, this->get_sex());
    }
    return;
  }

}

/**
 * Sets this person as travelling. They will be staying with the specified Person, meaning they will temporarily share 
 * a Household, neighborhood, and, if this person is a worker, a workplace and office. If no visited person is specified, 
 * they will be set as visiting outside the modeled area. Otherwise, their activity groups will be temporarily disabled.
 *
 * @param visited the visited person
 */
void Person::start_traveling(Person* visited) {

  if(visited == nullptr) {
    this->is_traveling_outside = true;
  } else {
    this->store_activity_groups();
    this->clear_activity_groups();
    assert(visited->get_household() != nullptr);
    this->set_household(visited->get_household());
    this->set_neighborhood(visited->get_neighborhood());
    if(this->profile == Activity_Profile::WORKER) {
      this->set_workplace(visited->get_workplace());
      this->set_office(visited->get_office());
    }
  }
  this->is_traveling = true;
  Person::person_logger->info("start traveling: id = {:d}", get_id());
}

/**
 * Stops a person from travelling. If they were not traveling outside the modeled area, their acticity groups will 
 * be restored.
 */
void Person::stop_traveling() {
  if(!this->is_traveling_outside) {
    this->restore_activity_groups();
  }
  this->is_traveling = false;
  this->is_traveling_outside = false;
  this->return_from_travel_sim_day = -1;
  Person::person_logger->info("stop traveling: id = {:d}", get_id());
}

/**
 * Checks if this person is elegible to be a teacher. This will be true if they are not currently in school. 
 * If successful, their school will be set to the specified school and they will be withdrawn from their current 
 * workplace.
 *
 * @param school the school
 * @return if the person is now a teacher
 */
bool Person::become_a_teacher(Place* school) {
  bool success = false;
  Person::person_logger->info("become_a_teacher: person {:d} age {:d}", this->get_id(), this->get_age());
  // print(this);
  if(get_school() != nullptr) {
    Person::person_logger->warn("become_a_teacher: person {:d} age {:d} ineligible -- already goes to school {:d} {:s}",
        this->get_id(), this->get_age(), this->get_school()->get_id(), this->get_school()->get_label());
    this->profile = Activity_Profile::STUDENT;
  } else {
    // set profile
    this->profile = Activity_Profile::TEACHER;
    // join the school
    Person::person_logger->debug("set school to {:s}", school->get_label());
    this->set_school(school);
    this->set_classroom(nullptr);
    success = true;
  }
  
  // withdraw from this workplace and any associated office
  Place* workplace = this->get_workplace();
  Person::person_logger->debug("leaving workplace {:d} {:s}", workplace->get_id(), workplace->get_label());
  this->change_workplace(nullptr);
  if(success) {
    Person::person_logger->info("become_a_teacher finished for person {:d} age {:d}  school {:s}", 
        this->get_id(), this->get_age(), school->get_label());
    // print(this);
  }
  return success;
}

/**
 * Sets this person's school to the specified Place. Assigns them a classroom.
 *
 * @param place the place
 */
void Person::change_school(Place* place) {
  Person::person_logger->info("person {:d} set school {:s}", this->get_id(), place ? place->get_label() : "NULL");
  this->set_school(place);
  Person::person_logger->info("set classroom to nullptr");
  this->set_classroom(nullptr);
  if(place != nullptr) {
    Person::person_logger->info("assign classroom");
    this->assign_classroom();
  }
}

/**
 * Sets this person's workplace to the specified Place. If include_office equates to true, an office 
 * will also be assigned.
 *
 * @param place the workplace
 * @param include_office whether or not to assign an office
 */
void Person::change_workplace(Place* place, int include_office) {
  Person::person_logger->info("person {:d} set workplace {:s}", this->get_id(), place ? place->get_label() : "NULL");
  this->set_workplace(place);
  this->set_office(nullptr);
  if(place != nullptr) {
    if(include_office) {
      this->assign_office();
    }
  }
}

/**
 * Gets this person's schedule as a string for the given day.
 *
 * @param day the day
 * @return the schedule as a string
 */
std::string Person::schedule_to_string(int day) {
  std::stringstream ss;
  ss << "day " << day << " schedule for person " << this->get_id() << "  ";
  for(int p = 0; p < Place_Type::get_number_of_place_types(); ++p) {
    if(this->get_activity_group(p)) {
      ss << Place_Type::get_place_type_name(p) << ": ";
      ss << (this->on_schedule[p] ? "+" : "-");
      ss << this->get_activity_group_label(p) << " ";
    }
  }
  return ss.str();
}

/**
 * Gets this person's activities as a string.
 *
 * @return the activities as a string
 */
std::string Person::activities_to_string() {
  std::stringstream ss;
  ss << "Activities for person " << get_id() << ": ";
  for(int p = 0; p < Place_Type::get_number_of_place_types(); ++p) {
    if(this->get_activity_group(p)) {
      ss << Place_Type::get_place_type_name(p) << ": ";
      ss << this->get_activity_group_label(p) << " ";
    }
  }
  return ss.str();
}

/**
 * Changes this person's Household to the specified house. Their household and neighborhood will be 
 * updated, along with their profile if needed.
 *
 * @param house the house
 */
void Person::change_household(Place* house) {

  // household can not be nullptr
  assert(house != nullptr);

  Person::person_logger->info("move_to_new_house start person {:d} house {:s} subtype {:c}",
      this->get_id(), house->get_label(), house->get_subtype());

  // change household
  this->set_household(house);

  // set neighborhood
  this->set_neighborhood(house->get_patch()->get_neighborhood());

  // possibly update the profile depending on new household
  this->update_profile_after_changing_household();

  Person::person_logger->info("move_to_new_house finished person {:d} house {:s} subtype {:c} profile {:d}",
      this->get_id(), this->get_household()->get_label(),
      this->get_household()->get_subtype(), this->profile);
}

/**
 * Terminates this person's activities. If they are travelling, their travel will also be terminated. If they are 
 * travelling outside the modeled area, their activity groups will first be restored. Their membership in all activity 
 * groups will then be ended.
 */
void Person::terminate_activities() {
  if(this->get_travel_status()) {
    if(this->is_traveling && !this->is_traveling_outside) {
      this->restore_activity_groups();
    }
    Travel::terminate_person(this);
  }

  // withdraw from society
  this->end_membership_in_activity_groups();
}

/**
 * Gets the visiting health status given a place, day, and Condition. The status is initialized to 0. If this person 
 * is travelling outside the modeled area, this will be returned. If the given place is on the person's schedule, the 
 * following conditions will be checked: if the person is susceptible to the condition, the status is set to 1; if the 
 * condition is transmissible by the person, the status is set to 2; otherwise, the status is set to 3.
 *
 * @param place the palce
 * @param day the day
 * @param condition_id the condition ID
 * @return the status
 */
int Person::get_visiting_health_status(Place* place, int day, int condition_id) {

  // assume we are not visiting this place today
  int status = 0;

  // traveling abroad?
  if(this->is_traveling_outside) {
    return status;
  }

  if(day > this->schedule_updated) {
    // get list of places to visit today
    this->update_activities(day);
  }

  // see if the given place is on my schedule today
  int place_type_id = place->get_type_id();
  if(this->on_schedule[place_type_id] && this->get_activity_group(place_type_id) == place) {
    if(this->is_susceptible(condition_id)) {
      status = 1;
    } else if(this->is_transmissible(condition_id)) {
      status = 2;
    } else {
      status = 3;
    }
  }

  return status;
}

/**
 * Updates the member index in this person's Link to the specified Group.
 *
 * @param group the group
 * @param new_index the new index
 */
void Person::update_member_index(Group* group, int new_index) {
  int type = group->get_type_id();
  if(group == this->get_activity_group(type)) {
    Person::person_logger->info("update_member_index for person {:d} type {:d} new_index {:d}", 
        this->get_id(), type, new_index);
    this->link[type].update_member_index(new_index);
    return;
  } else {
    std::stringstream ss;
    ss << fmt::format("update_member_index: person %d group %s not found at pos %d in daily activity locations: ",
        this->get_id(), group->get_label(), type);
    for(int i = 0; i < Group_Type::get_number_of_group_types(); ++i) {
      ss << fmt::format("%s ", this->link[i].get_group() == nullptr ? "NULL" : this->link[i].get_group()->get_label());
    }
    Person::person_logger->critical(ss.str());
    assert(0);
  }
}

///////////////////////////////////

/**
 * Ends this person's membership in every Group they are linked to.
 */
void Person::clear_activity_groups() {
  Person::person_logger->info("clear_activity_groups entered group_types = {:d}", Group_Type::get_number_of_group_types());
  for(int i = 0; i < Group_Type::get_number_of_group_types(); ++i) {
    if(this->link[i].is_member()) {
      this->link[i].end_membership(this);
    }
    assert(this->link[i].get_place() == nullptr);
  }
  Person::person_logger->info("clear_activity_groups finished group_types = {:d}", Group_Type::get_number_of_group_types());
}
  
/**
 * Begins a membership in a Group of the specified Group_Type that this person is linked to.
 *
 * @param i the group type ID
 */
void Person::begin_membership_in_activity_group(int i) {
  Group* group = this->get_activity_group(i);
  if(group != nullptr) {
    this->link[i].begin_membership(this, group);
  }
}

/**
 * Begins a membership in a Group for each Group_Type.
 */
void Person::begin_membership_in_activity_groups() {
  for(int i = 0; i < Group_Type::get_number_of_group_types(); ++i) {
    this->begin_membership_in_activity_group(i);
  }
}

/**
 * Ends the membership in the Group of the specified Group_Type that this person is linked to.
 *
 * @param i the group type ID
 */
void Person::end_membership_in_activity_group(int i) {
  Group* group = this->get_activity_group(i);
  if(group != nullptr) {
    this->link[i].end_membership(this);
  }
}

/**
 * Ends the membership in the Group in each Group_Type.
 */
void Person::end_membership_in_activity_groups() {
  for(int i = 0; i < Group_Type::get_number_of_group_types(); ++i) {
    this->end_membership_in_activity_group(i);
  }
  this->clear_activity_groups(); // this seems redundant
}

/**
 * Stores the activity groups of each Group_Type.
 */
void Person::store_activity_groups() {
  int group_types = Group_Type::get_number_of_group_types();
  this->stored_activity_groups = new Group*[group_types];
  for(int i = 0; i < group_types; ++i) {
    this->stored_activity_groups[i] = this->get_activity_group(i);
  }
}

/**
 * Reinstates the stored activity groups.
 */
void Person::restore_activity_groups() {
  int group_types = Group_Type::get_number_of_group_types();
  for(int i = 0; i < group_types; ++i) {
    this->set_activity_group(i, this->stored_activity_groups[i]);
  }
  delete[] this->stored_activity_groups;
}

/**
 * Gets the ID of this person's activity Group of the specified Group_Type.
 *
 * @param p the group type ID
 * @return the ID
 */
int Person::get_activity_group_id(int p) {
  return (this->get_activity_group(p) == nullptr ? -1 : this->get_activity_group(p)->get_id());
}

/**
 * Gets the label of this person's activity Group of the specified Group_Type.
 *
 * @param p the group type ID
 * @return the label
 */
const char* Person::get_activity_group_label(int p) {
  return (this->get_activity_group(p) == nullptr ? "NULL" : this->get_activity_group(p)->get_label());
}

/**
 * Sets this persons activity group of the specified Group_Type to the specified Group. Their membership to the old group 
 * of this type will first be ended. If this person is a meta agent, they will simply be linked to the group, and unlinked 
 * from the old.
 *
 * @param i the group type ID
 * @param group the group
 */
void Person::set_activity_group(int i, Group* group) {

  Group* old_group = this->get_activity_group(i);

  Person::person_logger->info("person {:d} SET ACTIVITY GROUP {:d} group {:d} {:s} size {:d}",
      this->id, i, (group ? group->get_id() : -1), (group ? group->get_label() : "NULL"),
      (group ? group->get_size() : -1));

  // update link if necessary
  if(group != old_group) {
    if(old_group != nullptr) {
      // remove old link
      if(this->is_meta_agent()) {
        this->link[i].unlink(this);
      } else {
        this->link[i].end_membership(this);
      }
    }
    if(group != nullptr) {
      if(this->is_meta_agent()) {
        this->link[i].link(this, group);
      } else {
        this->link[i].begin_membership(this, group);
      }
    }
  }
  Person::person_logger->info("person {:d} SET ACTIVITY done GROUP {:d} group {:d} {:s} size {:d}",
      this->id, i, (group ? group->get_id() : -1), (group ? group->get_label(): "NULL"),
      (group ? group->get_size() : -1));

}

/**
 * Checks if this person is present in the specified Group on the given day. This will be true if 
 * none of this person's condition states are absent from the Group_Type of the group.
 *
 * @param sim_day the numbered simulation day in the range of [0, Global::Simulation_Days)
 * @param group the group
 * @return if this person is present
 */
bool Person::is_present(int sim_day, Group* group) {

  if(group == nullptr) {
    return false;
  }
  if(this->is_meta_agent()) {
    return false;
  }

  int type_id = group->get_type_id();

  // not here if traveling abroad
  if(this->is_traveling_outside) {
    return false;
  }

  // update list of groups to visit today if not already done
  if(sim_day > this->schedule_updated) {
    this->update_activities(sim_day);
  }

  // see if this group is on the list
  if(this->on_schedule[group->get_type_id()]) {
    for(int cond_id = 0; cond_id < Condition::get_number_of_conditions(); ++cond_id) {
      int state = this->get_state(cond_id);
      if(Condition::get_condition(cond_id)->is_absent(state, type_id)) {
        return false;
      }
    }
    return true;
  } else {
    return false;
  }

}

/**
 * Begins a membership in the specified Network through this person's Link for the Group_Type of the network.
 *
 * @param network the network
 */
void Person::join_network(Network* network) {
  int network_type_id = network->get_type_id();
  if(this->link[network_type_id].is_member()) {
    return;
  }
  this->link[network_type_id].begin_membership(this, network);
  Person::person_logger->info("JOINED NETWORK: person {:d} network {:s} type_id {:d} size {:d}",
      this->get_id(), network->get_label(), network_type_id, network->get_size());
}

/**
 * Removes this person from the specified Network through this person's Link for the Group_Type of the network.
 *
 * @param network the network
 */
void Person::quit_network(Network* network) {
  Person::person_logger->info("UNENROLL NETWORK: id = {:d}", get_id());
  int network_type_id = network->get_type_id();
  this->link[network_type_id].remove_from_network(this);
}

/**
 * Joins the specified Network and and adds an edge to the specified Person through this person's Link for the Group_Type 
 * of the network.
 *
 * @param other the other person
 * @param network the network
 */
void Person::add_edge_to(Person* other, Network* network) {
  if(other == nullptr) {
    return;
  }
  int n = network->get_type_id();
  if(0 <= n) {
    this->join_network(network);
    this->link[n].add_edge_to(other);
  }
}

/**
 * Joins the specified Network and and adds an edge from the specified Person through this person's Link for the Group_Type 
 * of the network.
 *
 * @param other the other person
 * @param network the network
 */
void Person::add_edge_from(Person* other, Network* network) {
  if(other == nullptr) {
    return;
  }
  int n = network->get_type_id();
  if(0 <= n) {
    this->join_network(network);
    this->link[n].add_edge_from(other);
  }
}

/**
 * Deletes the edge to the specified Person in the specified Network through this person's Link for the Group_Type of the 
 * network.
 *
 * @param person the person
 * @param network the network
 */
void Person::delete_edge_to(Person* person, Network* network) {
  if(person == nullptr) {
    return;
  }
  int n = network->get_type_id();
  if(0 <= n) {
    this->link[n].delete_edge_to(person);
  }
}

/**
 * Deletes the edge from the specified Person in the specified Network through this person's Link for the Group_Type of the 
 * network.
 *
 * @param person the person
 * @param network the network
 */
void Person::delete_edge_from(Person* person, Network* network) {
  if(person == nullptr) {
    return;
  }
  int n = network->get_type_id();
  if(0 <= n) {
    this->link[n].delete_edge_from(person);
  }
}

/**
 * Checks if this person is a member of the specified Network by ensuring there is a network associated with this person's 
 * Link for the Group_Type of the network.
 *
 * @param network the network
 * @return if this person is a member
 */
bool Person::is_member_of_network(Network* network) {
  int n = network->get_type_id();
  return this->link[n].get_network() != nullptr;
}

/**
 * Checks if this person is connected to the specified Person in the specified Network through this person's Link for 
 * the Group_Type of the network.
 *
 * @param person the person
 * @param network the network
 * @return if this person is connected to
 */
bool Person::is_connected_to(Person* person, Network* network) {
  int n = network->get_type_id();
  if(0 <= n) {
    return this->link[n].is_connected_to(person);
  }
  return false;
}

/**
 * Checks if this person is connected from the specified Person in the specified Network through this person's Link for 
 * the Group_Type of the network.
 *
 * @param person the person
 * @param network the network
 * @return if this person is connected from
 */
bool Person::is_connected_from(Person* person, Network* network) {
  int n = network->get_type_id();
  if(0 <= n) {
    return this->link[n].is_connected_from(person);
  }
  return false;
}

/**
 * Gets the ID of the person that the inward edge with the largest weight points from in the specified Network through 
 * this person's Link for the Group_Type of the network.
 *
 * @param network the network
 * @return the ID
 */
int Person::get_id_of_max_weight_inward_edge_in_network(Network* network) {
  int n = network->get_type_id();
  if(0 <= n) {
    return this->link[n].get_id_of_max_weight_inward_edge();
  } else {
    return -99999999;
  }
}

/**
 * Gets the ID of the person that the outward edge with the largest weight points to in the specified Network through 
 * this person's Link for the Group_Type of the network.
 *
 * @param network the network
 * @return the ID
 */
int Person::get_id_of_max_weight_outward_edge_in_network(Network* network) {
  int n = network->get_type_id();
  if(0 <= n) {
    return this->link[n].get_id_of_max_weight_outward_edge();
  } else {
    return -99999999;
  }
}

/**
 * Gets the ID of the person that the inward edge with the smallest weight points from in the specified Network through 
 * this person's Link for the Group_Type of the network.
 *
 * @param network the network
 * @return the ID
 */
int Person::get_id_of_min_weight_inward_edge_in_network(Network* network) {
  int n = network->get_type_id();
  if(0 <= n) {
    return this->link[n].get_id_of_min_weight_inward_edge();
  } else {
    return -99999999;
  }
}

/**
 * Gets the ID of the person that the outward edge with the smallest weight points from in the specified Network through 
 * this person's Link for the Group_Type of the network.
 *
 * @param network the network
 * @return the ID
 */
int Person::get_id_of_min_weight_outward_edge_in_network(Network* network) {
  int n = network->get_type_id();
  if(0 <= n) {
    return this->link[n].get_id_of_min_weight_outward_edge();
  } else {
    return -99999999;
  }
}

/**
 * Gets the ID of the last person in the inward edge person vector in the specified Network through this person's Link 
 * for the Group_Type of the network.
 *
 * @param network the network
 * @return the ID
 */
int Person::get_id_of_last_inward_edge_in_network(Network* network) {
  int n = network->get_type_id();
  if(0 <= n) {
    return this->link[n].get_id_of_last_inward_edge();
  } else {
    return -99999999;
  }
}

/**
 * Gets the ID of the last person in the outward edge person vector in the specified Network through this person's Link 
 * for the Group_Type of the network.
 *
 * @param network the network
 * @return the ID
 */
int Person::get_id_of_last_outward_edge_in_network(Network* network) {
  int n = network->get_type_id();
  if(0 <= n) {
    return this->link[n].get_id_of_last_outward_edge();
  } else {
    return -99999999;
  }
}

/**
 * Gets the weight of the outward edge to the given person in the specified Network through this person's Link for 
 * the Group_Type of the network.
 *
 * @param person the person
 * @param network the network
 * @return the weight
 */
double Person::get_weight_to(Person* person, Network* network) {
  int n = network->get_type_id();
  if(0 <= n) {
    return this->link[n].get_weight_to(person);
  }
  return 0.0;
}

/**
 * Sets the weight of the outward edge to the given person in the specified Network through this person's Link for 
 * the Group_Type of the network.
 *
 * @param person the person
 * @param network the network
 * @param value the weight
 */
void Person::set_weight_to(Person* person, Network* network, double value) {
  int n = network->get_type_id();
  if(0 <= n) {
    this->link[n].set_weight_to(person, value);
  }
}

/**
 * Sets the weight of the inward edge from the given person in the specified Network through this person's Link for 
 * the Group_Type of the network.
 *
 * @param person the person
 * @param network the network
 * @param value the weight
 */
void Person::set_weight_from(Person* person, Network* network, double value) {
  int n = network->get_type_id();
  if(0 <= n) {
    this->link[n].set_weight_from(person, value);
  }
}

/**
 * Gets the weight of the inward edge from the given person in the specified Network through this person's Link for 
 * the Group_Type of the network.
 *
 * @param person the person
 * @param network the network
 * @return the weight
 */
double Person::get_weight_from(Person* person, Network* network) {
  int n = network->get_type_id();
  if(0 <= n) {
    return this->link[n].get_weight_from(person);
  }
  return 0.0;
}

/**
 * Gets the timestamp of the outward edge to the given person in the specified Network through this person's Link for 
 * the Group_Type of the network.
 *
 * @param person the person
 * @param network the network
 * @return the timestamp
 */
double Person::get_timestamp_to(Person* person, Network* network) {
  int n = network->get_type_id();
  if(0 <= n) {
    return this->link[n].get_timestamp_to(person);
  }
  return 0.0;
}

/**
 * Gets the timestamp of the inward edge from the given person in the specified Network through this person's Link for 
 * the Group_Type of the network.
 *
 * @param person the person
 * @param network the network
 * @return the timestamp
 */
double Person::get_timestamp_from(Person* person, Network* network) {
  int n = network->get_type_id();
  if(0 <= n) {
    return this->link[n].get_timestamp_from(person);
  }
  return 0.0;
}

/**
 * Gets the out-degree of this person in the specified Network through this person's Link for the Group_Type of the 
 * network.
 *
 * @param network the network
 * @return the out-degree
 */
int Person::get_out_degree(Network* network) {
  int n = network->get_type_id();
  if(0 <= n) {
    return this->link[n].get_out_degree();
  } else {
    return 0;
  }
}

/**
 * Gets the in-degree of this person in the specified Network through this person's Link for the Group_Type of the 
 * network.
 *
 * @param network the network
 * @return the in-degree
 */
int Person::get_in_degree(Network* network) {
  int n = network->get_type_id();
  if(0 <= n) {
    return this->link[n].get_in_degree();
  } else {
    return 0;
  }
}

/**
 * Gets the degree of this person in the specified Network through this person's Link for the Group_Type of the network.
 *
 * @param network the network
 * @return the degree
 */
int Person::get_degree(Network* network) {
  int n = network->get_type_id();
  if(0 <= n) {
    if(network->is_undirected()) {
      return this->link[n].get_in_degree();
    } else {
      return this->link[n].get_in_degree() + this->link[n].get_out_degree();
    }
  } else {
    return 0;
  }
}

/**
 * Clears this person's Link for the Group_Type of the specified Network.
 *
 * @param network the network
 */
void Person::clear_network(Network* network) {
  int n = network->get_type_id();
  if(0 <= n) {
    this->link[n].clear();
  }
}

/**
 * Gets the outward edge person vector in the specified Network through this person's Link for the Group_Type 
 * of the network. This method is then called recursively to get the outward edges of each outward edge. 
 * The max distance specifies the depth of the recursive search, and is decremented each time the function is 
 * recursively called. The results are returned as a person vector sorted by their IDs.
 *
 * @param network the network
 * @param max_dist the distance of the search
 */
person_vector_t Person::get_outward_edges(Network* network, int max_dist) {
  Person::person_logger->info("get_outward_edges person d network s max_dist d",
      this->get_id(), (network ? network->get_label() : "NULL"), max_dist);
  std::unordered_set<int> found = {};
  person_vector_t results;
  results.clear();
  assert(network != nullptr);
  if(network == nullptr) {
    return results;
  }
  int n = network->get_type_id();
  if(1 <= max_dist) {
    person_vector_t tmp = this->link[n].get_outward_edges();
    for(int k = 0; k < static_cast<int>(tmp.size()); ++k) {
      if(tmp[k] != this && found.insert(tmp[k]->get_id()).second) {
        results.push_back(tmp[k]);
        Person::person_logger->debug("add direct link to person {:d} result = {:d}", tmp[k]->get_id(), results.size());
      }
    }
    if(max_dist > 1) {
      int size = results.size();
      for(int i = 0; i < size; ++i) {
        Person* other = results[i];
        tmp = other->get_outward_edges(network, max_dist - 1);
        for(int k = 0; k < static_cast<int>(tmp.size()); ++k) {
          if(tmp[k] != this && found.insert(tmp[k]->get_id()).second) {
            results.push_back(tmp[k]);
            Person::person_logger->debug("add indirect link thru person {:d} to person {:d} result = {:d}", 
                other->get_id(), tmp[k]->get_id(), results.size());
          }
        }
      }
    }
  }
  // sort the results by id
  std::sort(results.begin(), results.end(), Utils::compare_id);

  Person::person_logger->info("get_outward_edges finished person {:d} network {:s} max_dist {:d}",
      this->get_id(), (network ? network->get_label() : "NULL"), max_dist);

  return results;
}

/**
 * Gets the inward edge person vector in the specified Network through this person's Link for the Group_Type 
 * of the network. This method is then called recursively to get the inward edges of each inward edge. 
 * The max distance specifies the depth of the recursive search, and is decremented each time the function is 
 * recursively called. The results are returned as a person vector sorted by their IDs.
 *
 * @param network the network
 * @param max_dist the distance of the search
 */
person_vector_t Person::get_inward_edges(Network* network, int max_dist) {
  Person::person_logger->info("get_linked_people person {:d} network {:s} max_dist {:d}",
      this->get_id(), (network ? network->get_label() : "NULL"), max_dist);
  std::unordered_set<int> found = {};
  person_vector_t results;
  results.clear();
  assert(network != nullptr);
  if(network == nullptr) {
    return results;
  }
  int n = network->get_type_id();
  if(1 <= max_dist) {
    person_vector_t tmp = this->link[n].get_inward_edges();
    for(int k = 0; k < static_cast<int>(tmp.size()); ++k) {
      if(tmp[k] != this && found.insert(tmp[k]->get_id()).second) {
        results.push_back(tmp[k]);
        Person::person_logger->debug("add direct link to person {:d} result = {:d}", tmp[k]->get_id(), results.size());
      }
    }
    if(max_dist > 1) {
      int size = results.size();
      for(int i = 0; i < size; ++i) {
        Person* other = results[i];
        tmp = other->get_inward_edges(network, max_dist-  1);
        for(int k = 0; k < static_cast<int>(tmp.size()); ++k) {
          if(tmp[k] != this && found.insert(tmp[k]->get_id()).second) {
            results.push_back(tmp[k]);
            Person::person_logger->debug("add indirect link thru person {:d} to person {:d} result = {:d}", 
                other->get_id(), tmp[k]->get_id(), results.size());
          }
        }
      }
    }
  }
  // sort the results by id
  std::sort(results.begin(), results.end(), Utils::compare_id);

  Person::person_logger->info("get_inward_edges finished person {:d} network {:s} max_dist {:d}", 
      this->get_id(), (network ? network->get_label() : "NULL"), max_dist);

  return results;
}

/**
 * Gets the outward edge at the specified index in the outward edge person vector of the specified Network through 
 * this person's Link for the Group_Type of the network.
 *
 * @param k the index
 * @param network the network
 * @return the outward edge
 */
Person* Person::get_outward_edge(int k, Network* network) {
  int n = network->get_type_id();
  if(0 <= n) {
    return this->link[n].get_outward_edge(k);
  } else {
    return nullptr;
  }
}

/**
 * Gets the inward edge at the specified index in the inward edge person vector of the specified Network through 
 * this person's Link for the Group_Type of the network.
 *
 * @param k the index
 * @param network the network
 * @return the inward edge
 */
Person* Person::get_inward_edge(int k, Network* network) {
  int n = network->get_type_id();
  if(0 <= n) {
    return this->link[n].get_inward_edge(k);
  } else {
    return nullptr;
  }
}

/**
 * Gets this person's Household through this person's Link for the Household Group_Type.
 *
 * @return the household
 */
Household* Person::get_household() {
  int i = Place_Type::get_type_id("Household");
  Group* group = this->link[i].get_group();
  group = this->get_activity_group(i); // redundant
  Household* hh = static_cast<Household*>(group);
  return hh;
}

/**
 * Gets this person's Household from their stored activity groups.
 *
 * @return the household
 */
Household* Person::get_stored_household() {
  return static_cast<Household*>(this->stored_activity_groups[Place_Type::get_type_id("Household")]);
}

/**
 * Gets this person's Hospital.
 *
 * @return the hospital
 */
Hospital* Person::get_hospital() {
  return static_cast<Hospital*>( get_activity_group(Place_Type::get_type_id("Hospital")));
}

/**
 * Sets this person's last school.
 *
 * @param school the school
 */
void Person::set_last_school(Place* school) {
  this->last_school = school;
}

/**
 * Selects a random Place of the specified Place_Type and sets it as an activity group of this person.
 *
 * @param place_type_id the place type ID
 */
void Person::select_activity_of_type(int place_type_id) {
  Place_Type* place_type = Place_Type::get_place_type(place_type_id);
  if(place_type != nullptr) {
    Place *place = Place_Type::select_place_of_type(place_type_id, this);
    this->set_activity_group(place_type_id, place);
  }
}

/**
 * Adds this person's activity group of the specified Group_Type to the schedule for the given day.
 *
 * @param day the day
 * @param group_type_id the group type ID
 */
void Person::schedule_activity(int day, int group_type_id) {
  Group* group = this->get_activity_group(group_type_id);
  this->on_schedule[group_type_id] = true;
  if(group != nullptr && this->is_present(day, group) == false) {
    this->on_schedule[group_type_id] = true; // redundant
  }
}

/**
 * Removes this person's activity group of the specified Group_Type from the schedule for the given day.
 *
 * @param day the day
 * @param group_type_id the group type ID
 */
void Person::cancel_activity(int day, int group_type_id) {
  Group* group = this->get_activity_group(group_type_id);
  Person::person_logger->info("CANCEL group {:s}", group == nullptr ? "NULL" : group->get_label());
  if(group != nullptr && this->is_present(day, group)) {
    Person::person_logger->info("CANCEL_ACTIVITY person {:d} day {:d} group_type {:s}", this->get_id(), day,
        Group_Type::get_group_type(group_type_id)->get_name());
    this->on_schedule[group_type_id] = false;
  }
}

/**
 * Gets the name of the activity profile represented by the specified activity profile index.
 *
 * @param prof the activity profile index
 * @return the activity profile name
 */
std::string Person::get_profile_name(int prof) {
  switch(prof) {
  case Activity_Profile::INFANT:
    return "infant";
    break;

  case Activity_Profile::PRESCHOOL:
    return "preschool";
    break;

  case Activity_Profile::STUDENT:
    return "student";
    break;

  case Activity_Profile::TEACHER:
    return "teacher";
    break;

  case Activity_Profile::WORKER:
    return "worker";
    break;

  case Activity_Profile::WEEKEND_WORKER:
    return "weekend_worker";
    break;

  case Activity_Profile::UNEMPLOYED:
    return "unemployed";
    break;

  case Activity_Profile::RETIRED:
    return "retired";
    break;

  case Activity_Profile::PRISONER:
    return "prisoner";
    break;

  case Activity_Profile::COLLEGE_STUDENT:
    return "college_student";
    break;

  case Activity_Profile::MILITARY:
    return "military";
    break;

  case Activity_Profile::NURSING_HOME_RESIDENT:
    return "nursing_home_resident";
    break;

  case Activity_Profile::UNDEFINED:
    return "undefined";
    break;

  default:
    return "unknown";
  }
}

/**
 * Gets the activity profile index of the specified activity profile name.
 *
 * @param name the activity profile name
 * @return the activity profile index
 */
int Person::get_profile_from_name(std::string name) {
  for(int i = 0; i < Activity_Profile::ACTIVITY_PROFILE; ++i) {
    if(name == Person::get_profile_name(i)) {
      return i;
    }
  }
  return -1;
}

/**
 * Runs the given action rules given a Condition and condition state.
 *
 * @param condition_id the condition
 * @param state the condition state
 * @param rules the action rules
 */
void Person::run_action_rules(int condition_id, int state, const rule_vector_t &rules) {
  int day = Global::Simulation_Day;
  for(int i = 0; i <static_cast<int>(rules.size()); ++i) {
    Rule* rule = rules[i];
    Person::person_logger->trace("<{:s}, {:d}>: {:s}", __FILE__, __LINE__, rule->to_string());
    if(rule->applies(this) == false) {
      continue;
    }

    int cond_id = rule->get_cond_id();

    std::string action_str = rule->get_action();
    int action = rule->get_action_id();
    std::string group_name = rule->get_group();
    int group_type_id = rule->get_group_type_id();

    std::string network_name = rule->get_network();
    Network* network = Network::get_network(network_name);
    int network_type_id = network ? network->get_type_id() : -1;

    Expression* expr = rule->get_expression();
    Expression* expr2 = rule->get_expression2();
    double value = 0;
    int var_id = -1;
    switch (action) {

    case Rule_Action::GIVE_BIRTH:
      this->give_birth(day);
      break;

    case Rule_Action::DIE:
    case Rule_Action::DIE_OLD:
      this->condition[cond_id].susceptibility = 0;
      break;

    case Rule_Action::SUS:
      this->condition[cond_id].susceptibility = expr->get_value(this);
      break;

    case Rule_Action::SET_SUS:
      this->condition[rule->get_source_cond_id()].susceptibility = expr2->get_value(this);
      break;

    case Rule_Action::SET_TRANS:
      if(0 <= this->id) {
        this->condition[rule->get_source_cond_id()].transmissibility = expr2->get_value(this);
      } else if(this->id == -1) {
        int source_cond_id = rule->get_source_cond_id();
        double old_value = Condition::get_condition(source_cond_id)->get_transmissibility();
        double value = expr2->get_value(this);
        Condition::get_condition(rule->get_source_cond_id())->set_transmissibility(value);
        if(Global::Enable_Records && Global::Enable_Var_Records && old_value != value) {
          char tmp[FRED_STRING_SIZE];
          this->get_record_string(tmp);
          fprintf(Global::Recordsfp, "%s state %s.%s changes %s.transmissibility from %f to %f\n", tmp,
              this->get_natural_history(condition_id)->get_name(),
              this->get_natural_history(condition_id)->get_state_name(state).c_str(),
              Condition::get_condition(rule->get_source_cond_id())->get_name(), old_value, value);
        }
      }
      break;

    case Rule_Action::TRANS:
      this->condition[cond_id].transmissibility = expr->get_value(this);
      break;

    case Rule_Action::JOIN:
      {
        if(Group::is_a_place(group_type_id)) {
          if(expr2) {
            long long int sp_id = expr2->get_value(this);
            Place* place = static_cast<Place*>(Group::get_group_from_sp_id(sp_id));
            this->join_place(place);
          } else {
            this->select_place_of_type(group_type_id);
          }
        } else {
          if(Group::is_a_network(group_type_id)) {
            this->join_network(network_type_id);
          }
        }
      }
      break;

    case Rule_Action::QUIT:
      if(Group::is_a_place(group_type_id)) {
        this->quit_place_of_type(group_type_id);
      } else {
        this->quit_network(network_type_id);
      }
      break;

    case Rule_Action::ADD_EDGE_FROM:
      {
        double_vector_t id_vec;
        if(expr->is_list_expression()) {
          id_vec = expr->get_list_value(this);
        } else {
          id_vec.push_back(expr->get_value(this));
        }
        int size = id_vec.size();
        for(int i = 0; i < size; ++i) {
          int other_id = id_vec[i];
          Person* other = Person::get_person_with_id(other_id);
          this->add_edge_from(other, network);
          other->add_edge_to(this, network);
          if(network->is_undirected()) {
            this->add_edge_to(other, network);
            other->add_edge_from(this, network);
          }
        }
      }
      break;

    case Rule_Action::ADD_EDGE_TO:
      {
        double_vector_t id_vec;
        if(expr->is_list_expression()) {
          id_vec = expr->get_list_value(this);
        } else {
           id_vec.push_back(expr->get_value(this));
        }
        int size = id_vec.size();
        for(int i = 0; i < size; ++i) {
          int other_id = id_vec[i];
          Person* other = Person::get_person_with_id(other_id);
          this->add_edge_to(other, network);
          other->add_edge_from(this, network);
          if(network->is_undirected()) {
            this->add_edge_from(other, network);
            other->add_edge_to(this, network);
          }
        }
      }
      break;

    case Rule_Action::DELETE_EDGE_FROM:
      {
        double_vector_t id_vec;
        if(expr->is_list_expression()) {
          id_vec = expr->get_list_value(this);
        } else {
          id_vec.push_back(expr->get_value(this));
        }
        int size = id_vec.size();
        for(int i = 0; i < size; ++i) {
          int other_id = id_vec[i];
          Person* other = Person::get_person_with_id(other_id);
          this->delete_edge_from(other, network);
          other->delete_edge_to(this, network);
          if(network->is_undirected()) {
            this->delete_edge_to(other, network);
            other->delete_edge_from(this, network);
          }
        }
      }
      break;

    case Rule_Action::DELETE_EDGE_TO:
      {
        double_vector_t id_vec;
        if(expr->is_list_expression()) {
          id_vec = expr->get_list_value(this);
        } else {
          id_vec.push_back(expr->get_value(this));
        }
        int size = id_vec.size();
        for(int i = 0; i < size; ++i) {
          int other_id = id_vec[i];
          Person* other = Person::get_person_with_id(other_id);
          this->delete_edge_to(other, network);
          other->delete_edge_from(this, network);
          if(network->is_undirected()) {
            this->delete_edge_from(other, network);
            other->delete_edge_to(this, network);
          }
        }
      }
      break;

    case Rule_Action::SET:
      {
        var_id = rule->get_var_id();
        bool global = rule->is_global();
        Person::person_logger->debug("var_id = {:d} global = {:d}", var_id, global);
        Person* other = nullptr;
        Expression* other_expr = rule->get_expression2();
        if(other_expr) {
          int person_id = other_expr->get_value(this);
          other = Person::get_person_with_id(person_id);
        }
        value = rule->get_value(this, other);
        Person::person_logger->debug("var_id = {:d} global = {:d} value = {:f}", var_id, global, value);
        if(global) {
          if(Global::Enable_Records && Global::Enable_Var_Records && Person::global_var[var_id] != value) {
            char tmp[FRED_STRING_SIZE];
            this->get_record_string(tmp);
            fprintf(Global::Recordsfp, "%s state %s.%s changes %s from %f to %f\n", tmp,
                this->get_natural_history(condition_id)->get_name(),
                this->get_natural_history(condition_id)->get_state_name(state).c_str(),
                Person::get_global_var_name(var_id).c_str(),
                Person::global_var[var_id], value);
          }
          Person::global_var[var_id] = value;
        } else {
          Person::person_logger->debug("var_id = {:d} global = {:d} value = {:f} other==nullptr {:d}", 
              var_id, global, value, other==nullptr);
          if(other == nullptr) {
            Person::person_logger->debug("state {:s}.{:s} changes {:s} from {:f} to {:f}", 
                this->get_natural_history(condition_id)->get_name(),
                this->get_natural_history(condition_id)->get_state_name(state).c_str(),
                Person::get_var_name(var_id).c_str(), this->var[var_id], value);
            if(Global::Enable_Records && Global::Enable_Var_Records && this->var[var_id] != value) {
              char tmp[FRED_STRING_SIZE];
              this->get_record_string(tmp);
              fprintf(Global::Recordsfp, "%s state %s.%s changes %s from %f to %f\n", tmp,
                  this->get_natural_history(condition_id)->get_name(),
                  this->get_natural_history(condition_id)->get_state_name(state).c_str(),
                  Person::get_var_name(var_id).c_str(), this->var[var_id], value);
              }
            this->var[var_id] = value;
          } else {
            if(Global::Enable_Records && Global::Enable_Var_Records && other->get_var(var_id) != value) {
              char tmp[FRED_STRING_SIZE];
              this->get_record_string(tmp);
              fprintf(Global::Recordsfp, "%s state %s.%s changes other %d age %d var %s from %f to %f\n", tmp,
                  this->get_natural_history(condition_id)->get_name(),
                  this->get_natural_history(condition_id)->get_state_name(state).c_str(),
                  other->get_id(), other->get_age(),
                  Person::get_var_name(var_id).c_str(),
                  other->get_var(var_id), value);
            }
            other->set_var(var_id, value);
          }
          Person::person_logger->debug("finished setting var_id = {:d} global = {:d} value = {:f}", var_id, global, value);
        }
      }
      break;

    case Rule_Action::SET_LIST:
      {
        Person::person_logger->debug("run SET_LIST person {:d} cond {:d} state {:d} rule: {:s}",
            this->id, condition_id, state, rule->get_name().c_str());

        var_id = rule->get_list_var_id();
        bool global = rule->is_global();
        if(global) {
          Person::person_logger->debug("global_list_var {:d} {:s}", var_id, Person::get_global_list_var_name(var_id).c_str());
          double_vector_t list_value = rule->get_expression()->get_list_value(this);
          Person::person_logger->debug("AFTER SET_LIST list_var {:d} size {:d} => size {:d}",
              var_id, Person::global_list_var[var_id].size(), list_value.size());
          Person::global_list_var[var_id] = list_value;
          for(int i = 0; i < static_cast<int>(Person::global_list_var[var_id].size()); ++i) {
            Person::person_logger->trace("<{:s}, {:d}>: SET_LIST person {:d} day {:d} hour {:d} var {:s}[{:d}] {:f}",
                __FILE__, __LINE__, this->id, Global::Simulation_Day, Global::Simulation_Hour,
                Person::get_global_list_var_name(var_id).c_str(), i, Person::global_list_var[var_id][i]);
          }
        } else {
          Person::person_logger->debug("list_var {:d} {:s}", var_id, Person::get_list_var_name(var_id).c_str());
          double_vector_t list_value = rule->get_expression()->get_list_value(this);
          Person::person_logger->debug("AFTER SET_LIST list_var {:d} size {:d} => size {:d}",
              var_id, this->list_var[var_id].size(), list_value.size());
          this->list_var[var_id] = list_value;
          if(Global::Enable_Records && Global::Enable_List_Var_Records) {
            int day = Global::Simulation_Day;
            int hour = Global::Simulation_Hour;
            for(int i = 0; i < static_cast<int>(this->list_var[var_id].size()); ++i) {
              fprintf(Global::Recordsfp, "HEALTH RECORD: %s %s day %d person %d SET LIST VAR %s[%d] %f\n",
                  Date::get_date_string().c_str(), Date::get_12hr_clock(hour).c_str(),
                  day, this->id, Person::get_list_var_name(var_id).c_str(), i, this->list_var[var_id][i]);
            }
          }
        }
      }
      break;

    case Rule_Action::SET_STATE:
      {
        int source_cond_id = rule->get_source_cond_id();
        int source_state_id = rule->get_source_state_id();
        int dest_state_id = rule->get_dest_state_id();
        Person::person_logger->debug("person {:d} source_cond_id {:d} source_state_id {:d} dest_state_id {:d}", 
            this->get_id(), source_cond_id, source_state_id, dest_state_id);
        if(source_state_id < 0 || get_state(source_cond_id)==source_state_id) {
          int day = Global::Simulation_Day;
          int hour = Global::Simulation_Hour;
          if(Global::Enable_Records) {
            fprintf(Global::Recordsfp, "HEALTH RECORD: %s %s day %d person %d ENTERING state %s.%s MODIFIES state %s.%s to %s.%s\n",
                Date::get_date_string().c_str(), Date::get_12hr_clock(hour).c_str(), day,
                this->get_id(), rule->get_cond().c_str(), rule->get_state().c_str(), rule->get_source_cond().c_str(),
                rule->get_source_state().c_str(), rule->get_source_cond().c_str(), rule->get_dest_state().c_str());
            fflush(Global::Recordsfp);
          }
          Person::person_logger->debug("get_condition({:d})->get_epidemic()->update_state(this, day, hour, {:d}, 0)", 
              source_cond_id, dest_state_id);
          Condition::get_condition(source_cond_id)->get_epidemic()->update_state(this, day, hour, dest_state_id, 0);
        }
      }
      break;

    case Rule_Action::SET_WEIGHT:
      {
        if(is_member_of_network(network)) {
          double_vector_t id_vec;
          if(expr->is_list_expression()) {
            id_vec = expr->get_list_value(this);
          } else {
            id_vec.push_back(expr->get_value(this));
          }
          int size = id_vec.size();
          for(int i = 0; i < size; ++i) {
            int person_id = id_vec[i];
            Person* other = Person::get_person_with_id(person_id);
            if(other != nullptr) {
              double value = expr2->get_value(this,other);
              this->set_weight_to(other,network,value);
              other->set_weight_from(this, network,value);
              if(network->is_undirected()) {
                this->set_weight_from(other,network,value);
                other->set_weight_to(this, network,value);
              }
            }
          }
        }
      }
      break;

    case Rule_Action::REPORT:
      this->start_reporting(rule);
      break;

    case Rule_Action::RANDOMIZE_NETWORK:
      if(this->is_meta_agent()) {
        Group* group = this->get_admin_group();
        if(group && group->get_type_id() == network_type_id) {
          double mean_degree = rule->get_expression()->get_value(this, nullptr);
          double max_degree = rule->get_expression2()->get_value(this, nullptr);
          network->randomize(mean_degree, max_degree);
        }
      }
      break;

    case Rule_Action::ABSENT:
    case Rule_Action::PRESENT:
    case Rule_Action::CLOSE:
      // all schedule-related action rules are handled by Natural_History for efficiency
      break;

    case Rule_Action::SET_CONTACTS:
      if(this->is_admin_agent()) {
        Group* group = this->get_admin_group();
        if(group != nullptr) {
          double value = rule->get_expression()->get_value(this,nullptr);
          group->set_contact_factor(value);
          Person::person_logger->info("SET_CONTACTS of group {:s} to {:f}", group->get_label(), group->get_contact_factor());
        }
      }
      break;

    case Rule_Action::IMPORT_COUNT:
    case Rule_Action::IMPORT_PER_CAPITA:
    case Rule_Action::IMPORT_LOCATION:
    case Rule_Action::IMPORT_ADMIN_CODE:
    case Rule_Action::IMPORT_AGES:
    case Rule_Action::COUNT_ALL_IMPORT_ATTEMPTS:
    case Rule_Action::IMPORT_LIST:
      // all import-related action rules are handled by Natural_History for efficiency
      break;

    default:
      Person::person_logger->error("unknown action {:d} {:s}", action, rule->get_action().c_str());
      // exit(0);
      break;

    }
    Person::person_logger->trace("<{:s}, {:d}>: finished: {:s}", __FILE__, __LINE__, rule->to_string());
  }
}

/**
 * Stores recorded data on this person and the time as a C string in the buffer pointed by result.
 *
 * @param result the buffer pointer
 */
void Person::get_record_string(char* result) {

  if(Person::record_location) {
    snprintf(result, FRED_STRING_SIZE, "HEALTH RECORD: %s %s day %d person %d age %d sex %c race %d latitude %f longitude %f income %d",
        Date::get_date_string().c_str(),
        Date::get_12hr_clock(Global::Simulation_Hour).c_str(),
        Global::Simulation_Day,
        this->get_id(),
        this->get_age(),
        this->get_sex(),
        this->get_race(),
        this->get_household() ? this->get_household()->get_latitude() : 0.0,
        this->get_household() ? this->get_household()->get_longitude() : 0.0,
        this->get_household() ? this->get_income() : 0 );
  } else {
    snprintf(result, FRED_STRING_SIZE, "HEALTH RECORD: %s %s day %d person %d age %d sex %c race %d household %s school %s income %d",
        Date::get_date_string().c_str(),
        Date::get_12hr_clock(Global::Simulation_Hour).c_str(),
        Global::Simulation_Day,
        this->get_id(),
        this->get_age(),
        this->get_sex(),
        this->get_race(),
        this->get_household() ? this->get_household()->get_label() : "NONE",
        this->get_school() ? this->get_school()->get_label() : "NONE",
        this->get_household() ? this->get_income() : 0 );
  }
}

/**
 * Initializes personal variables for all persons and admin agents.
 *
 */
void Person::initialize_personal_variables() {

  // Initialize the personal variables for each agent
  for(int p = 0; p < Person::get_population_size(); ++p) {
    Person* person = Person::get_person(p);
    person->initialize_my_variables();
  }
  // Initialize the personal variables for each admin agent
  for(int p = 0; p < static_cast<int>(Person::admin_agents.size()); ++p) {
    Person* person = Person::admin_agents[p];
    person->initialize_my_variables();
  }

  // If we are reading initialization data from a file and we are on the first day of the simulation
  // go ahead and set the values from a file
  if(Global::Enable_External_Variable_Initialization && Global::Simulation_Day == 0) {
    Person::external_initalize_personal_variables(false);

    if(Global::Enable_Group_Quarters) {
      Person::external_initalize_personal_variables(true);
    }
  }
}

/**
 * If external variable initialization is enabled \code{.cpp} Global::Enable_External_Variable_Initialization) \endcode,
 * look in the each population directory for a file named "variables.csv" or "gq_variables.csv" (for group quarters).
 *
 * For each column header in the csv file, there should be a matching variable and an intialization value for that variable
 * E.g.
 * sp_id,var_1,var_2,var_3
 * abcd,1.2,0.9,12.8
 * efgh,2.4,1.8,25.6
 *
 * Note: the variables should also be defined in the .fred file
 * \code
 * my var_1
 * my var_2
 * my var_3
 * enable_external_variable_initialization = 1
 * \endcode
 *
 * @param is_group_quarters whether or not we are initalizing from a group quarters file
 */
void Person::external_initalize_personal_variables(bool is_group_quarters) {

  // If this was called to look at group quarters, but GQ is not enabled, return
  if(is_group_quarters && !Global::Enable_Group_Quarters) {
    return;
  }

  // If we are reading initialization data from a file and we are on the first day of the simulation
  // go ahead and set the values from a file
  if(Global::Enable_External_Variable_Initialization && Global::Simulation_Day == 0) {
    // Look in each population directory for variables.csv and gq_variables.csv
    int locs = Place::get_number_of_location_ids();
    for(int i = 0; i < locs; ++i) {
      char pop_dir[FRED_STRING_SIZE];
      Place::get_population_directory(pop_dir, i);

      char var_init_file[FRED_STRING_SIZE];
      char line[FRED_STRING_SIZE];

      FILE* fp = nullptr;

      // File should be either gq_variables.csv or variables.csv depending on whether this is a group quarters population file
      snprintf(var_init_file, FRED_STRING_SIZE, "%s/%s.csv", pop_dir, (is_group_quarters ? "gq_variables" : "variables"));

      // verify that the file exists
      fp = Utils::fred_open_file(var_init_file);
      if(fp == nullptr) {
        continue;
      }
      fclose(fp);

      int row_num = 0;
      bool is_header_var_name_found = false;
      std::map<std::string, std::vector<double>> sp_id_to_value_vec_map;
      //std::map<std::string, std::array<double>>::iterator it;

      string_vector_t var_name_vec;
      var_name_vec.clear();

      std::ifstream var_stream(var_init_file, std::ifstream::in);

      char var_line[FRED_STRING_SIZE];

      while(var_stream.good()) {
        var_stream.getline(line, FRED_STRING_SIZE);
        ++row_num;

        // skip empty lines
        if(line[0] == '\0') {
          continue;
        }

        // Must have variable names from first non-empty line (header row) in order to parse further
        // If the header row is after some value rows, then all values before it is found will be ignored
        if(!is_header_var_name_found) {
          std::string sp_id_str = std::string("sp_id");
          std::string per_id_str = std::string("per_id");
          // header line is used for variable names
          if(strncmp(line, "sp_id", 5) == 0 || strncmp(line, "per_id", 6) == 0) {
            var_name_vec = Utils::get_string_vector(std::string(line), ',');
            is_header_var_name_found = true;
            for(auto itr_name_vec = var_name_vec.begin(); itr_name_vec != var_name_vec.end(); ++itr_name_vec) {
              bool is_found = false;
              // If it is the id field, skip it
              if((*itr_name_vec) == sp_id_str || (*itr_name_vec) == per_id_str) {
                continue;
              }
              // Make sure that each of the variables is actually one that has been defined in the parameter file
              int number_of_vars = Person::get_number_of_vars();
              for(int i = 0; i < number_of_vars; ++i) {
                if((*itr_name_vec) == Person::var_name[i]) {
                  is_found = true;
                  break;
                }
              }
              if(!is_found) {
                Utils::fred_abort("Variable initialization file, %s, has error: field [%s] has no matching variable declaration in the input file\n",
                    var_init_file, itr_name_vec->c_str());
              }
            }
          }
        } else {
          string_vector_t fields_vec = Utils::get_string_vector(std::string(line), ',');
          if(fields_vec.size() != var_name_vec.size()) {
            Utils::fred_abort("Variable initialization file, %s, has a mismatch: from header expect %d fields, but row %d has %d fields\n", var_init_file,
                static_cast<int>(var_name_vec.size()), static_cast<int>(fields_vec.size()), row_num);
          }

          std::vector<double> tmp_vec;
          for(std::size_t i = 0; i < fields_vec.size(); ++i) {
            if(i > 0) {
              if(!Utils::is_number(fields_vec[i])) {
                Utils::fred_abort("Variable initialization file, %s, has type error: row %d has non-numeric field value [%s]\n", var_init_file,
                    row_num, fields_vec[i].c_str());
              }
              tmp_vec.push_back(std::stod(fields_vec[i]));
            }
          }
          sp_id_to_value_vec_map[fields_vec[0]] = tmp_vec;
        }
      }

      // Now I have a mapping, so loop through the population and set all of the variables
      // Initialize the personal variables for each agent
      for(int p = 0; p < Person::get_population_size(); ++p) {
        Person* person = get_person(p);
        //person->initialize_my_variables();
        std::map<std::string, std::vector<double>>::iterator itr_map = sp_id_to_value_vec_map.find(person->get_sp_id());
        if(itr_map != sp_id_to_value_vec_map.end()) {
          // sp_id found
          std::vector<double> tmp_vec = itr_map->second;

          // Find the appropriate index for the each variable and set the value
          for(std::size_t name_vec_idx = 0; name_vec_idx < var_name_vec.size(); ++name_vec_idx) {
            int var_idx = -1;
            int number_of_vars = Person::get_number_of_vars();
            for(int i = 0; i < number_of_vars; ++i) {
              if(var_name_vec[name_vec_idx] == Person::var_name[i]) {
                var_idx = i;
                break;
              }
            }
            if(var_idx >= 0) {
              person->var[var_idx] = tmp_vec[name_vec_idx - 1]; // Need to subtract 1 because the name vector has sp_id whereas the value vector does not
              // Print a record if we var records are enabled
              if(Global::Enable_Records && Global::Enable_Var_Records) {
                char tmp[FRED_STRING_SIZE];
                person->get_record_string(tmp);
                fprintf(Global::Recordsfp, "%s initialization sets %s to %f\n", tmp,
                    Person::get_var_name(var_idx).c_str(), tmp_vec[name_vec_idx - 1]);
              }
            }
          }
        }
      }
    }
  }
}

/**
 * Initializes the values of all personal variables and list variables for this person.
 */
void Person::initialize_my_variables() {

  int number_of_vars = Person::get_number_of_vars();
  if(number_of_vars > 0) {
    this->var = new double[number_of_vars];
    for(int i = 0; i < number_of_vars; ++i) {
      if(Person::var_expr[i] != nullptr) {
        this->var[i] = Person::var_expr[i]->get_value(this, nullptr);
      } else {
        this->var[i] = 0.0;
      }
    }
  }

  if(this->id == -1) {
    if(Person::is_log_initialized) {
      Person::person_logger->info("initialize_my_variables: person {:d} number_of_vars {:d}", this->id, number_of_vars);
    }
  }

  int number_of_list_vars = Person::get_number_of_list_vars();
  if(number_of_list_vars > 0) {
    this->list_var = new double_vector_t [number_of_list_vars];
    for(int i = 0; i < number_of_list_vars; ++i) {
      this->list_var[i].clear();
      if(Person::list_var_expr[i] != nullptr) {
        if(Person::is_log_initialized) {
          Person::person_logger->debug("evaluating list_var_expr {:s} for person {:d}",
              Person::list_var_expr[i]->get_name().c_str(), this->id);
        }
        this->list_var[i] = Person::list_var_expr[i]->get_list_value(this,nullptr);
      } else {
        this->list_var[i].clear();
      }
    }
  }
}

/**
 * Includes the specified variable names as personal variables.
 *
 * @param name_list a list of names
 */
void Person::include_variable(std::string name_list) {
  string_vector_t names = Utils::get_string_vector(name_list, ' ');
  for(int j = 0; j < static_cast<int>(names.size()); ++j) {
    std::string name = names[j];
    int found = 0;
    int size = Person::var_name.size();
    for(int i = 0; found == 0 && i < size; ++i) {
      if(name == Person::var_name[i]) {
        found = 1;
      }
    }
    if(found == 0) {
      Person::var_name.push_back(name);
      ++Person::number_of_vars;
      if(Person::is_log_initialized) {
        Person::person_logger->info("ADDING PERSONAL VAR {:s} num = {:d}", name.c_str(), Person::number_of_vars);
      }
    }
  }
}

/**
 * Includes the specified list variable names as personal list variables.
 *
 * @param name_list a list of names
 */
void Person::include_list_variable(std::string name_list) {
  string_vector_t names = Utils::get_string_vector(name_list, ' ');
  for(int j = 0; j < static_cast<int>(names.size()); ++j) {
    std::string name = names[j];
    int found = 0;
    int size = Person::list_var_name.size();
    for(int i = 0; found == 0 && i < size; ++i) {
      if(name == Person::list_var_name[i]) {
        found = 1;
      }
    }
    if(found == 0) {
      Person::list_var_name.push_back(name);
      ++Person::number_of_list_vars;
      if(Person::is_log_initialized) {
        Person::person_logger->info("ADDING PERSONAL LIST_VAR {:s} num = {:d}", name.c_str(), Person::number_of_list_vars);
      }
    }
  }
}

/**
 * Includes the specified variable names as global variables.
 *
 * @param name_list a list of names
 */
void Person::include_global_variable(std::string name_list) {
  string_vector_t names = Utils::get_string_vector(name_list, ' ');
  for(int j = 0; j < static_cast<int>(names.size()); ++j) {
    std::string name = names[j];
    int found = 0;
    int size = Person::global_var_name.size();
    for(int i = 0; found==0 && i < size; ++i) {
      if(name == Person::global_var_name[i]) {
        found = 1;
      }
    }
    if(found == 0) {
      Person::global_var_name.push_back(name);
      ++Person::number_of_global_vars;
      if(Person::is_log_initialized) {
        Person::person_logger->info("ADDING GLOBAL VAR {:s} num = {:d}", name.c_str(), Person::number_of_global_vars);
      }
    }
  }
}

/**
 * Includes the specified list variable names as global list variables.
 *
 * @param name_list a list of names
 */
void Person::include_global_list_variable(std::string name_list) {
  string_vector_t names = Utils::get_string_vector(name_list, ' ');
  for(int j = 0; j < static_cast<int>(names.size()); ++j) {
    std::string name = names[j];
    int found = 0;
    int size = Person::global_list_var_name.size();
    for(int i = 0; found == 0 && i < size; ++i) {
      if(name == Person::global_list_var_name[i]) {
        found = 1;
      }
    }
    if(found == 0) {
      Person::global_list_var_name.push_back(name);
      ++Person::number_of_global_list_vars;
      if(Person::is_log_initialized) {
        Person::person_logger->info("ADDING GLOBAL LIST_VAR {:s} num = {:d}", name.c_str(),
            Person::number_of_global_list_vars);
      }
    }
  }
}

/**
 * Excludes the personal variable with the specified name.
 *
 * @param name the name
 */
void Person::exclude_variable(std::string name) {
  int size = Person::var_name.size();
  for(int i = 0; i < size; ++i) {
    if(name == Person::var_name[i]) {
      for(int j = i + 1; j < size; ++j) {
        Person::var_name[j - 1] = Person::var_name[j];
      }
      Person::var_name.pop_back();
      --Person::number_of_vars;
    }
  }
}

/**
 * Excludes the personal list variable with the specified name.
 *
 * @param name the name
 */
void Person::exclude_list_variable(std::string name) {
  int size = Person::list_var_name.size();
  for(int i = 0; i < size; ++i) {
    if(name == Person::list_var_name[i]) {
      for(int j = i + 1; j < size; ++j) {
        Person::list_var_name[j - 1] = Person::list_var_name[j];
      }
      Person::list_var_name.pop_back();
      --Person::number_of_list_vars;
    }
  }
}

/**
 * Excludes the global variable with the specified name.
 *
 * @param name the name
 */
void Person::exclude_global_variable(std::string name) {
  int size = Person::global_var_name.size();
  for(int i = 0; i < size; ++i) {
    if(name == Person::global_var_name[i]) {
      for(int j = i + 1; j < size; ++j) {
        Person::global_var_name[j - 1] = Person::global_var_name[j];
      }
      Person::global_var_name.pop_back();
      --Person::number_of_global_vars;
    }
  }
}

/**
 * Excludes the global list variable with the specified name.
 *
 * @param name the name
 */
void Person::exclude_global_list_variable(std::string name) {
  int size = Person::global_list_var_name.size();
  for(int i = 0; i < size; ++i) {
    if(name == Person::global_list_var_name[i]) {
      for(int j = i + 1; j < size; ++j) {
        Person::global_list_var_name[j - 1] = Person::global_list_var_name[j];
      }
      Person::global_list_var_name.pop_back();
      --Person::number_of_global_list_vars;
    }
  }
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Person::setup_logging() {
  if(Person::is_log_initialized) {
    return; 
  }

  if(Parser::does_property_exist("person_log_level")) {
    Parser::get_property("person_log_level", &Person::person_log_level);
  } else {
    Person::person_log_level = "OFF";
  }
  
  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Person::person_logger = std::make_unique<spdlog::logger>("person_logger", 
        sink_list.begin(), sink_list.end());
    Person::person_logger->set_level(
        Utils::get_log_level_from_string(Person::person_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Person::person_logger->trace("<{:s}, {:d}>: Person logger initialized", 
      __FILE__, __LINE__  );
  Person::is_log_initialized = true;
}
