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
// File: Person.h
//
#ifndef _FRED_PERSON_H
#define _FRED_PERSON_H

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

#include <spdlog/spdlog.h>

#include "Date.h"
#include "Demographics.h"
#include "Global.h"
#include "Group_Type.h"
#include "Link.h"
#include "Network_Type.h"
#include "Place.h"
#include "Place_Type.h"
#include "Utils.h"

class Activities_Tracking_Data;
class Condition;
class Expression;
class Factor;
class Group;
class Hospital;
class Household;
class Infection;
class Natural_History;
class Network;
class Place;
class Population;
class Preference;

typedef struct {
  int person_index;
  int person_id;
  Person* person;
  Expression* expression;
  std::vector<int> change_day;
  std::vector<double> value_on_day;
} report_t;


typedef struct {
  int state;
  int last_transition_step;
  int next_transition_step;

  // transmission info
  double susceptibility;
  double transmissibility;
  int exposure_day;
  Person* source;
  Group* group;
  int number_of_hosts;

  // time each state was last entered
  int* entered;

  // status flags
  bool is_fatal;
  bool sus_set;
  bool trans_set;

} condition_t;

// household relationship codes (ver 2)
/**
 * The following enum defines symbolic names for an agent's relationship
 * to the householder.  The last element should always be
 * HOUSEHOLD_RELATIONSHIPS
 */
namespace Household_Relationship {
  enum e {
    HOUSEHOLDER = 0,
    SPOUSE = 1,
    CHILD = 2,
    SIBLING = 3,
    PARENT = 4,
    GRANDCHILD = 5,
    IN_LAW = 6,
    OTHER_RELATIVE = 7,
    BOARDER = 8,
    HOUSEMATE = 9,
    PARTNER = 10,
    FOSTER_CHILD = 11,
    OTHER_NON_RELATIVE = 12,
    INSTITUTIONALIZED_GROUP_QUARTERS_POP = 13,
    NONINSTITUTIONALIZED_GROUP_QUARTERS_POP = 14,
    HOUSEHOLD_RELATIONSHIPS
  };
};

/**
 * The following enum defines symbolic names for an agent's race.  to
 * the householder.  The last element should always be RACES
 */

namespace Race {
  enum e {
    UNKNOWN_RACE = -1,
    WHITE = 1,
    AFRICAN_AMERICAN,
    AMERICAN_INDIAN,
    ALASKA_NATIVE,
    TRIBAL,
    ASIAN,
    HAWAIIAN_NATIVE,
    OTHER_RACE,
    MULTIPLE_RACE,
    RACES
  };
};

#define MAX_MOBILITY_AGE 100

/**
 * The following enum defines symbolic names an agent's activity
 * profile.  The last element should always be ACTIVITY_PROFILE
 */
namespace Activity_Profile {
  enum e {
    INFANT,
    PRESCHOOL,
    STUDENT,
    TEACHER,
    WORKER,
    WEEKEND_WORKER,
    UNEMPLOYED,
    RETIRED,
    PRISONER,
    COLLEGE_STUDENT,
    MILITARY,
    NURSING_HOME_RESIDENT,
    UNDEFINED,
    ACTIVITY_PROFILE
  };
};

/**
 * This class represents an agent in the FRED simulation.
 *
 * The Person class attempts to simulate a real person going about their lives. As such, 
 * a wide variety of data is tracked by the class, such as their living situation, activity 
 * groups (which are Group objects of which the person is a member of), interactions and daily 
 * schedules, health status, etc.
 */
class Person {
public:

  Person();
  ~Person();
  void setup(std::string sp_id, int index, int id, int age, char sex, int race, int rel,
       Place* house, Place* school, Place* work, int day,
       bool today_is_birthday);
  void print(FILE* fp, int condition_id);
  bool is_present(int sim_day, Group* group);
  std::string to_string();

  /**
   * Gets the ID of the person.
   *
   * @return the ID
   */
  int get_id() const {
    return this->id;
  }

  /**
   * Gets the sp_id of the person. The synthetic population ID (sp_id) is
   * the value that is read in from the population files.
   *
   * @return the sp_id
   */
  std::string get_sp_id() const {
    return this->sp_id;
  }

  /**
   * Sets the population index of the person.
   *
   * @param idx the index
   */
  void set_pop_index(int idx) {
    this->index = idx;
  }

  /**
   * Gets the population index of the person.
   *
   * @return the population index
   */
  int get_pop_index() {
    return this->index;
  }

  void start_reporting(int condition_id, int state_id);
  void start_reporting(Rule *rule);
  double get_x();
  double get_y();

  /**
   * Checks if this person is alive.
   *
   * @return if this person is alive
   */
  bool is_alive() const {
    return this->alive;
  }

  void terminate(int day);


  // DEMOGRAPHICS

  /**
   * Gets the birthday of the person in simulation time.
   *
   * @return the birthday simulation day
   */
  int get_birthday_sim_day() {
    return this->birthday_sim_day;
  }

  /**
   * Gets the birth year of the person.
   *
   * @return the birth year
   */
  int get_birth_year() {
    return Date::get_year(this->birthday_sim_day);
  }

  double get_age_in_years() const;
  int get_age_in_days() const;
  int get_age_in_weeks() const;
  int get_age_in_months() const;
  double get_real_age() const;
  int get_age() const;

  /**
   * Gets the inital age of the person.
   *
   * @return the intial age
   */
  short int get_init_age() const {
    return this->init_age;
  }

  /**
   * Gets the sex of the person.
   *
   * @return the sex
   */
  const char get_sex() const {
    return this->sex;
  }

  /**
   * Gets the race of the person.
   *
   * @return the race
   */
  short int get_race() const {
    return this->race;
  }

  /**
   * Sets this person's number of children.
   *
   * @param n the number of children
   */
  void set_number_of_children(int n) {
    this->number_of_children = n;
  }

  /**
   * Gets this person's number of children.
   *
   * @return the number of children
   */
  int get_number_of_children() const {
    return this->number_of_children;
  }
  
  void update_birth_stats(int day, Person* self);

  /**
   * Checks if this person is an adult.
   *
   * @return if this person is an adult
   */
  bool is_adult() {
    return this->get_age() >= Global::ADULT_AGE;
  }

  /**
   * Checks if this person is a child.
   *
   * @return if this person is a child
   */  
  bool is_child() {
    return this->get_age() < Global::ADULT_AGE;
  }

  /**
   * Checks if this person is white.
   *
   * @return if this person is white
   */  
  bool is_white() {
    return this->get_race() == Race::WHITE;
  }

  /**
   * Checks if this person is non-white.
   *
   * @return if this person is non-white
   */  
  bool is_nonwhite() {
    return this->get_race() != Race::WHITE;
  }

  /**
   * Checks if this person is african american.
   *
   * @return if this person is african american
   */  
  bool is_african_american() {
    return this->get_race() == Race::AFRICAN_AMERICAN;
  }

  /**
   * Checks if this person is american indian.
   *
   * @return if this person is american indian
   */  
  bool is_american_indian() {
    return this->get_race() == Race::AMERICAN_INDIAN;
  }

  /**
   * Checks if this person is an alaskan native.
   *
   * @return if this person is an alaskan native
   */  
  bool is_alaska_native() {
    return this->get_race() == Race::ALASKA_NATIVE;
  }

  /**
   * Checks if this person is a hawaiian native.
   *
   * @return if this person is a hawaiian native
   */  
  bool is_hawaiian_native() {
    return this->get_race() == Race::HAWAIIAN_NATIVE;
  }

  /**
   * Checks if this person is tribal.
   *
   * @return if this person is tribal
   */  
  bool is_tribal() {
    return this->get_race() == Race::TRIBAL;
  }

  /**
   * Checks if this person is asian.
   *
   * @return if this person is asian
   */  
  bool is_asian() {
    return this->get_race() == Race::ASIAN;
  }

  /**
   * Checks if this person is an other race.
   *
   * @return if this person is an other race
   */  
  bool is_other_race() {
    return this->get_race() == Race::OTHER_RACE;
  }

  /**
   * Checks if this person is multiple races.
   *
   * @return if this person is multiple races
   */
  bool is_multiple_race() {
    return this->get_race() == Race::MULTIPLE_RACE;
  }

  int get_income(); 
  int get_adi_state_rank();
  int get_adi_national_rank();
  char* get_household_structure_label();

  /**
   * Checks if this person is deceased.
   *
   * @return if this person is deceased
   */
  const bool is_deceased() const {
    return this->deceased;
  }

  /**
   * Sets the person as deceased.
   */
  void set_deceased() {
    this->deceased = true;
  }

  /**
   * Sets the person's household relationship.
   *
   * @param rel the household relationship
   */
  void set_household_relationship(int rel) {
    this->household_relationship = rel;
  }

  /**
   * Gets the person's household relationship.
   *
   * @return the household relationship
   */
  const int get_household_relationship() const {
    return this->household_relationship;
  }

  /**
   * Checks if this person's household relationship is a householder.
   *
   * @return if this person is a householder
   */
  bool is_householder() {
    return this->household_relationship == Household_Relationship::HOUSEHOLDER;
  }

  /**
   * Makes this person's household relationship a householder.
   */
  void make_householder() {
    this->household_relationship = Household_Relationship::HOUSEHOLDER;
  }


  // PROFILE

  /**
   * Gets the profile of the person.
   *
   * @return the profile
   */
  int get_profile() {
    return this->profile;
  }

  /**
   * Sets the profile of the person.
   *
   * @param _profile the profile
   */
  void set_profile(int _profile) {
    this->profile = _profile;
  }

  void assign_initial_profile();
  void update_profile_after_changing_household();
  void update_profile_based_on_age();
  bool become_a_teacher(Place* school);

  /**
   * Checks if the profile of the person is a teacher.
   *
   * @return if the profile of the person is a teacher
   */
  bool is_teacher() {
    return this->profile == Activity_Profile::TEACHER;
  }

  /**
   * Checks if the profile of the person is a student.
   *
   * @return if the profile of the person is a student
   */  
  bool is_student() {
    return this->profile == Activity_Profile::STUDENT;
  }

  /**
   * Checks if the profile of the person is retired.
   *
   * @return if the profile of the person is retired
   */  
  bool is_retired() {
    return this->profile == Activity_Profile::RETIRED;
  }

  /**
   * Checks if the person is employed at a workplace.
   *
   * @return if the person is employed
   */
  bool is_employed() {
    return this->get_workplace() != nullptr;
  }

  /**
   * Checks if the profile of the person is a college student.
   *
   * @return if the profile of the person is a college student
   */  
  bool is_college_student() {
    return this->profile == Activity_Profile::COLLEGE_STUDENT;
  }

  /**
   * Checks if the profile of the person is a prisoner.
   *
   * @return if the profile of the person is a prisoner
   */  
  bool is_prisoner() {
    return this->profile == Activity_Profile::PRISONER;
  }

  /**
   * Checks if the profile of the person is a college student.
   *
   * @return if the profile of the person is a college student
   */  
  bool is_college_dorm_resident() {
    return this->profile == Activity_Profile::COLLEGE_STUDENT;
  }

  /**
   * Checks if the profile of the person is a military personnel.
   *
   * @return if the profile of the person is a military personnel
   */  
  bool is_military_base_resident() {
    return this->profile == Activity_Profile::MILITARY;
  }

  /**
   * Checks if the profile of the person is a nursing home resident.
   *
   * @return if the profile of the person is a nursing home resident
   */  
  bool is_nursing_home_resident() {
    return this->profile == Activity_Profile::NURSING_HOME_RESIDENT;
  }

  bool is_hospital_staff();
  bool is_prison_staff();
  bool is_college_dorm_staff();
  bool is_military_base_staff();
  bool is_nursing_home_staff();

  /**
   * Checks if this person lives in group quarters. This will be true if they live in either a 
   * college dorm, military base, prison, or nursing home.
   *
   * @return if this person lives in group quarters
   */
  bool lives_in_group_quarters() {
    return (this->is_college_dorm_resident() || this->is_military_base_resident()
      || this->is_prisoner() || this->is_nursing_home_resident());
  }

  /**
   * Checks if this person lives in their parent's home.
   *
   * @return if this person lives in parent's home
   */
  bool lives_in_parents_home() {
    return this->in_parents_home;
  }

  static std::string get_profile_name(int prof);
  static int get_profile_from_name(std::string profile_name);


  // ACTIVITIES
  void setup_activities(Place* house, Place* school, Place* work);
  void set_activity_group(int place_type_id, Group* group);
  void prepare_activities() {} // _UNUSED_
  void clear_activity_groups();

  /**
   * Sets the person as not living in their parent's home.
   */
  void unset_in_parents_home() {
    this->in_parents_home = false;
  }

  void schedule_activity(int day, int place_type_id);
  void cancel_activity(int day, int place_type_id);
  void begin_membership_in_activity_group(int i);
  void begin_membership_in_activity_groups();
  void end_membership_in_activity_group(int i);
  void end_membership_in_activity_groups();
  void store_activity_groups();
  void restore_activity_groups();
  int get_activity_group_id(int i);
  const char* get_activity_group_label(int i);
  void update_activities(int sim_day);
  void select_activity_of_type(int place_type_id);
  std::string activities_to_string();

  /**
   * Gets the Group that this person is a member of with the specified Group_Type.
   *
   * @param i the group type ID
   * @return the group
   */
  Group* get_activity_group(int i) {
    if(0 <= i && i < Group_Type::get_number_of_group_types()) {
      return this->link[i].get_group();
    } else {
      return nullptr;
    }
  }

  /**
   * Sets the Household of the person.
   *
   * @param p the household
   */
  void set_household(Place* p) {
    this->set_activity_group(Group_Type::get_type_id("Household"), p);
  }

  /**
   * Sets the neighborhood of the person.
   *
   * @param p the neighborhood
   */
  void set_neighborhood(Place* p) {
    this->set_activity_group(Group_Type::get_type_id("Neighborhood"), p);
  }

  /**
   * Sets the school of the person.
   *
   * @param p the school
   */
  void set_school(Place* p) {
    this->set_activity_group(Group_Type::get_type_id("School"), p);
    if(p != nullptr) {
      this->set_last_school(p);
    }
  }

  void set_last_school(Place* school);

  /**
   * Sets the classroom of the person.
   *
   * @param p the classroom
   */  
  void set_classroom(Place* p) {
    this->set_activity_group(Group_Type::get_type_id("Classroom"), p);
  }

  /**
   * Sets the workplace of the person.
   *
   * @param p the workplace
   */  
  void set_workplace(Place* p) {
    this->set_activity_group(Group_Type::get_type_id("Workplace"), p);
  }

  /**
   * Sets the workplace of the person.
   *
   * @param p the workplace
   */
  void set_office(Place* p) {
    this->set_activity_group(Group_Type::get_type_id("Office"), p);
  }

  /**
   * Sets the Hospital of the person.
   *
   * @param p the hospital
   */
  void set_hospital(Place* p) {
    this->set_activity_group(Group_Type::get_type_id("Hospital"), p);
  }

  void terminate_activities();

  void set_place_of_type(int type_id, Place* place);
  Place* get_place_of_type(int type_id);
  Group* get_group_of_type(int type_id);
  int get_place_size(int type_id);
  Network* get_network_of_type(int type_id);
  int get_network_size(int type_id);
  Household* get_stored_household();
  void assign_school();
  void assign_classroom();
  void assign_workplace();
  void assign_office();

  /**
   * Gets the neighborhood of the person.
   *
   * @return the neighborhood
   */
  Place* get_neighborhood() {
    return static_cast<Place*>(this->get_activity_group(Place_Type::get_type_id("Neighborhood")));
  }

  Household* get_household();

  /**
   * Gets the school of the person.
   *
   * @return the school
   */  
  Place* get_school() {
    return static_cast<Place*>(this->get_activity_group(Place_Type::get_type_id("School")));
  }

  /**
   * Gets the classroom of the person.
   *
   * @return the classroom
   */  
  Place* get_classroom() {
    return static_cast<Place*>(this->get_activity_group(Place_Type::get_type_id("Classroom")));
  }

  /**
   * Gets the workplace of the person.
   *
   * @return the workplace
   */  
  Place* get_workplace() {
    return static_cast<Place*>(this->get_activity_group(Place_Type::get_type_id("Workplace")));
  }

  /**
   * Gets the office of the person.
   *
   * @return the office
   */  
  Place* get_office() {
    return static_cast<Place*>(this->get_activity_group(Place_Type::get_type_id("Office")));
  }

  /**
   * Gets this person's Household's size.
   *
   * @return the household size
   */
  int get_household_size() {
    return this->get_place_size(Place_Type::get_type_id("Household"));
  }

  /**
   * Gets this person's neighborhood's size.
   *
   * @return the neighborhood size
   */  
  int get_neighborhood_size() {
    return this->get_place_size(Place_Type::get_type_id("Neighborhood"));
  }

  /**
   * Gets this person's school's size.
   *
   * @return the school size
   */  
  int get_school_size() {
    return this->get_place_size(Place_Type::get_type_id("School"));
  }

  /**
   * Gets this person's classroom's size.
   *
   * @return the classroom size
   */  
  int get_classroom_size() {
    return this->get_place_size(Place_Type::get_type_id("Classroom"));
  }

  /**
   * Gets this person's workplace's size.
   *
   * @return the workplace size
   */  
  int get_workplace_size() {
    return this->get_place_size(Place_Type::get_type_id("Workplace"));
  }

  /**
   * Gets this person's office's size.
   *
   * @return the office size
   */  
  int get_office_size() {
    return this->get_place_size(Place_Type::get_type_id("Office"));
  }

  /**
   * Gets this person's hospital's size.
   *
   * @return the hospital size
   */  
  int get_hospital_size() {
    return this->get_place_size(Place_Type::get_type_id("Hospital"));
  }

  int get_place_elevation(int type);
  int get_place_income(int type);
  void start_hosting(int place_type_id);
  void select_place_of_type(int place_type_id);
  void quit_place_of_type(int place_type_id);
  void join_place(Place* place);
  void join_network(int network_type_id);
  void quit_network(int network_type_id);
  void report_place_size(int place_type_id);
  Place* get_place_with_type_id(int place_type_id);
  person_vector_t get_placemates(int place_type_id, int maxn);
  void run_action_rules(int condition_id, int state, const rule_vector_t &rules);

  // SCHEDULE
  std::string schedule_to_string(int sim_day);

  // MIGRATION

  /**
   * Checks if this person is eligible to migrate.
   *
   * @return if this person is eligible to migrate
   */
  bool is_eligible_to_migrate() {
    return this->eligible_to_migrate;
  }

  /**
   * Sets this person as eligible to migrate.
   */
  void set_eligible_to_migrate() {
    this->eligible_to_migrate = true;
  }

  /**
   * Sets this person as ineligible to migrate.
   */
  void unset_eligible_to_migrate() {
    this->eligible_to_migrate = false;
  }

  void change_household(Place* house);
  void change_school(Place* place);
  void change_workplace(Place* place, int include_office = 1);

  /**
   * Sets this person as native.
   */
  void set_native() {
    this->native = true;
  }

  /**
   * Sets this person as non-native.
   */
  void unset_native() {
    this->native = false;
  }

  /**
   * Checks if this person is native.
   */
  bool is_native() {
    return this->native;
  }

  /**
   * Sets this person as original.
   */
  void set_original() {
    this->original = true;
  }

  /**
   * Sets this person as non-original.
   */
  void unset_original() {
    this->original = false;
  }

  /**
   * Checks if this person is original.
   */
  bool is_original() {
    return this->original;
  }

  // TRAVEL

  /**
   * Gets this person's Household.
   *
   * @return the household
   */
  Household* get_permanent_household() {
    return this->get_household();
  }

  void start_traveling(Person* visited);
  void stop_traveling();

  /**
   * Checks if this person is travelling.
   *
   * @return if this person is travelling
   */
  bool get_travel_status() {
    return this->is_traveling;
  }

  /**
   * Sets the simulation day that this person will return from travelling.
   *
   * @param sim_day the simulation day
   */
  void set_return_from_travel_sim_day(int sim_day) {
    this->return_from_travel_sim_day = sim_day;
  }

  /**
   * Gets the simulation day that this person will return from travelling.
   *
   * @return the simulation day
   */
  int get_return_from_travel_sim_day() {
    return this->return_from_travel_sim_day;
  }
  // list of activity locations, stored while traveling
  Group** stored_activity_groups;


  // GROUPS
  void update_member_index(Group* group, int pos);
  int get_number_of_people_in_group_in_state(int type_id, int condition_id, int state_id);
  int get_number_of_other_people_in_group_in_state(int type_id, int condition_id, int state_id);
  int get_group_size(int index);

  /**
   * Gets the size of the Group that this person is a member of with the specified Group_Type name.
   *
   * @return the size
   */
  int get_group_size(std::string group_name) {
    return this->get_group_size(Group_Type::get_type_id(group_name));
  }


  // MATERNITY
  Person* give_birth(int day);


  // HEALTHCARE
  void assign_hospital(Place* place);
  void assign_primary_healthcare_facility();

  /**
   * Gets this person's primary healthcare facility.
   *
   * @return the primary healthcare facility
   */
  Hospital* get_primary_healthcare_facility() {
    return this->primary_healthcare_facility;
  }

  void start_hospitalization(int sim_day, int length_of_stay);
  void end_hospitalization();
  Hospital* get_hospital();

  /**
   * Checks if this person is hospitalized.
   *
   * @return if this person is hospitalized
   */
  bool person_is_hospitalized() {
    return this->is_hospitalized;
  }

  int get_visiting_health_status(Place* place, int day, int condition_id);


  // NETWORKS
  void join_network(Network* network);
  void quit_network(Network* network);
  int get_degree();
  bool is_member_of_network(Network* network);
  void add_edge_to(Person* person, Network* network);
  void add_edge_from(Person* person, Network* network);
  void delete_edge_to(Person* person, Network* network);
  void delete_edge_from(Person* person, Network* network);
  bool is_connected_to(Person* person, Network* network);
  bool is_connected_from(Person* person, Network* network);
  int get_id_of_max_weight_inward_edge_in_network(Network* network);
  int get_id_of_max_weight_outward_edge_in_network(Network* network);
  int get_id_of_min_weight_inward_edge_in_network(Network* network);
  int get_id_of_min_weight_outward_edge_in_network(Network* network);
  int get_id_of_last_inward_edge_in_network(Network* network);
  int get_id_of_last_outward_edge_in_network(Network* network);
  void set_weight_to(Person* person, Network* network, double value);
  void set_weight_from(Person* person, Network* network, double value);
  double get_weight_to(Person* person, Network* network);
  double get_weight_from(Person* person, Network* network);
  double get_timestamp_to(Person* person, Network* network);
  double get_timestamp_from(Person* person, Network* network);
  int get_out_degree(Network* network);
  int get_in_degree(Network* network);
  int get_degree(Network* network);
  person_vector_t get_outward_edges(Network* network, int max_dist = 1);
  person_vector_t get_inward_edges(Network* network, int max_dist = 1);
  void clear_network(Network* network);
  Person* get_outward_edge(int n, Network* network);
  Person* get_inward_edge(int n, Network* network);

  // VARIABLES
  double get_var(int index);
  void set_var(int index, double value);
  double_vector_t get_list_var(int index);
  int get_list_size(int list_var_id);

  static void include_variable(std::string name);

  static void exclude_variable(std::string name);

  static void include_list_variable(std::string name);

  static void exclude_list_variable(std::string name);

  static void include_global_variable(std::string name);

  static void exclude_global_variable(std::string name);

  static void include_global_list_variable(std::string name);

  static void exclude_global_list_variable(std::string name);

  /**
   * Gets the number of global variables.
   *
   * @return the number
   */
  static int get_number_of_global_vars() {
    return Person::number_of_global_vars;
  }

  /**
   * Gets the number of global list variables.
   *
   * @return the number
   */
  static int get_number_of_global_list_vars() {
    return Person::number_of_global_list_vars;
  }

  static std::string get_global_var_name(int index);
  static std::string get_global_list_var_name(int index);
  static int get_global_var_id(std::string var_name);
  static int get_global_list_var_id(std::string var_name);
  static double get_global_var(int index);
  static double_vector_t get_global_list_var(int index);
  static int get_global_list_size(int list_var_id);
  static void push_back_global_list_var(int list_var_id, double value);
  static void initialize_personal_variables();
  static void external_initalize_personal_variables(bool is_group_quarters);
  void initialize_my_variables();

  // CONDITIONS
  void setup_conditions();
  Natural_History* get_natural_history (int condition_id) const;
  void initialize_conditions(int day);
  void update_condition(int day, int condition_id);
  void become_exposed(int condition_id, Person* source, Group* group, int day, int hour);
  void become_case_fatality(int condition_id, int day);
  void expose(Person* host, int source_condition_id, int condition_id, Group* group, int day, int hour);
  void update_group_counts(int day, int condition_id, Group* group);
  void terminate_conditions(int day);
  void set_state(int condition_id, int state, int step);

  /**
   * Gets this person's condition state of the specified Condition.
   *
   * @param condition_id the condition ID
   * @return the condition state
   */
  int get_state(int condition_id) const {
    return this->condition[condition_id].state;
  }

  /**
   * Gets the time this person entered the specified state of the specified Condition.
   *
   * @param condition_id the condition ID
   * @param state the condition state
   * @return the time
   */
  int get_time_entered(int condition_id, int state) const {
    return this->condition[condition_id].entered[state];
  }

  /**
   * Sets the last transition step for the specified Condition. This is the simulation step at which 
   * this person transitioned into their current condition state for the condition.
   *
   * @param condition_id the condition ID
   * @param step the step
   */
  void set_last_transition_step(int condition_id, int step) {
    this->condition[condition_id].last_transition_step = step;
  }

  /**
   * Gets the last transition step for the specified Condition. This is the simulation step at which 
   * this person transitioned into their current condition state for the condition.
   *
   * @param condition_id the condition ID
   * @return the last transition step
   */
  int get_last_transition_step(int condition_id) const {
    return this->condition[condition_id].last_transition_step;
  }

  /**
   * Sets the next transition step for the specified Condition. This is the simulation step at which 
   * this person will transition into the next condition state for the condition.
   *
   * @param condition_id the condition ID
   * @param step the step
   */
  void set_next_transition_step(int condition_id, int step) {
    this->condition[condition_id].next_transition_step = step;
  }

  /**
   * Gets the next transition step for the specified Condition. This is the simulation step at which 
   * this person will transition into the next condition state for the condition.
   *
   * @param condition_id the condition ID
   * @return the next transition step
   */
  int get_next_transition_step(int condition_id) const {
    return this->condition[condition_id].next_transition_step;
  }

  /**
   * Sets the exposure day for the specified Condition. This is the day this person was exposed 
   * to the condition.
   *
   * @param condition_id the condition ID
   * @param day the day
   */
  void set_exposure_day(int condition_id, int day) {
    this->condition[condition_id].exposure_day = day;
  }

  /**
   * Gets the exposure day for the specified Condition. This is the day this person was exposed 
   * to the condition.
   *
   * @param condition_id the condition ID
   * @return the day
   */  
  int get_exposure_day(int condition_id) const {
    return this->condition[condition_id].exposure_day;
  }

  double get_susceptibility(int condition_id) const;
  double get_transmissibility(int condition_id) const;
  int get_transmissions(int condition_id) const;

  /**
   * Checks if the specified Condition is fatal for this person.
   *
   * @param condition_id the condition ID
   * @return if the case is fatal
   */
  bool is_case_fatality(int condition_id) const {
    return this->condition[condition_id].is_fatal;
  }

  /**
   * Sets the specified Condition as fatal for this person.
   *
   * @param condition_id the condition ID
   */
  void set_case_fatality(int condition_id) {
    this->condition[condition_id].is_fatal = true;
  }

  /**
   * Sets the source of the specified Condition. This is the person from whom this person contracted 
   * the condition from.
   *
   * @param condition_id the condition ID
   * @param source the source person
   */
  void set_source(int condition_id, Person* source) {
    this->condition[condition_id].source = source;
  }

  /**
   * Gets the source of the specified Condition. This is the person from whom this person contracted 
   * the condition from.
   *
   * @param condition_id the condition ID
   * @return the source person
   */
  Person* get_source(int condition_id) const {
    return this->condition[condition_id].source;
  }

  /**
   * Sets the group for the specified Condition. This is the group that the condition is active in.
   *
   * @param condition_id the condition ID
   * @param group the group
   */
  void set_group(int condition_id, Group* group) {
    this->condition[condition_id].group = group;
  }

  /**
   * Gets the group for the specified Condition. This is the group that the condition is active in.
   *
   * @param condition_id the condition ID
   * @return the group
   */  
  Group* get_group(int condition_id) const {
    return this->condition[condition_id].group;
  }

  /**
   * Gets the ID of the group of which this person is a member in which the specified Condition is active.
   *
   * @param condition_id the condition ID
   * @return the ID
   */
  int get_exposure_group_id(int condition_id) const {
    return this->get_group_id(condition_id);
  }

  /**
   * Gets the label of the group of which this person is a member in which the specified Condition is active.
   *
   * @param condition_id the condition ID
   * @return the label
   */  
  char* get_exposure_group_label(int condition_id) const {
    return this->get_group_label(condition_id);
  }

  /**
   * Gets the ID of the group type of the group of which this person is a member in which 
   * the specified Condition is active.
   *
   * @param condition_id the condition ID
   * @return the ID
   */  
  int get_exposure_group_type_id(int condition_id) const {
    return this->get_group_type_id(condition_id);
  }

  /**
   * Gets the number of hosts of the specified Condition.
   *
   * @param condition_id the condition ID
   * @return the number of hosts
   */
  int get_hosts(int condition_id) const {
    return this->get_number_of_hosts(condition_id);
  }

  /**
   * Gets the exposure group for the specified Condition. This is the group of which this person is a member 
   * in which the condition is active.
   *
   * @param condition_id the condition ID
   * @return the group
   */    
  Group* get_exposure_group(int condition_id) const {
    return this->get_group(condition_id);
  }

  int get_group_id(int condition) const;
  char* get_group_label(int condition) const;
  int get_group_type_id(int condition) const;

  /**
   * Increments the number of hosts of the specified Condition.
   *
   * @param condition_id the condition ID
   */
  void increment_number_of_hosts(int condition_id) {
    this->condition[condition_id].number_of_hosts++;
  }

  /**
   * Gets the number of hosts of the specified Condition.
   *
   * @param condition_id the condition ID
   * @return the number of hosts
   */  
  int get_number_of_hosts(int condition_id) const {
    return this->condition[condition_id].number_of_hosts;
  }

  /**
   * Checks if this person is susceptible to the specified Condition.
   *
   * @param condition_id the condition ID
   * @return if this person is susceptible
   */
  bool is_susceptible(int condition_id) const {
    return this->get_susceptibility(condition_id) > 0.0;
  }

  /**
   * Checks if the specified Condition is transmissible by this person.
   *
   * @param condition_id the condition ID
   * @return if the condition is transmissible
   */
  bool is_transmissible(int condition_id) const {
    return this->get_transmissibility(condition_id) > 0.0;
  }

  /**
   * Checks if any conditions are transmissible by this person.
   *
   * @return if any condition is transmissible
   */
  bool is_transmissible() const {
    for(int i = 0; i < this->number_of_conditions; ++i) {
      if(this->is_transmissible(i)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Checks if this person was ever exposed to the specified Condition. This will be true if 
   * there is an exposure day set for the condition.
   *
   * @param condition_id the condition ID
   * @return if this person was ever exposed
   */
  bool was_ever_exposed(int condition_id) {
    return (0 <= this->get_exposure_day(condition_id));
  }

  /**
   * Checks if this person was exposed to the specified Condition externally. This will be true if 
   * the person has been exposed to the condition, but it is not active in any groups that the person 
   * is a member of.
   *
   * @return if this person was exposed externally
   */
  bool was_exposed_externally(int condition_id) {
    return this->was_ever_exposed(condition_id) && (this->get_group(condition_id)==nullptr);
  }

  /**
   * Gets the days in which this person has been in their current condition state of the specified Condition 
   * given a day. This will be the days since they transitioned into their current state.
   *
   * @param condition_id the condition ID
   * @param day the day
   * @return the days in health state
   */
  int get_days_in_health_state(int condition_id, int day) {
    return (day - 24 * this->get_last_transition_step(condition_id));
  }

  int get_new_health_state(int condition_id);
  void request_external_updates(FILE* fp, int day);
  void get_external_updates(FILE* fp, int day);

  /**
   * Checks if this person has ever been in the specified state of the specified condition.
   *
   * @param condition_id the condition ID
   * @param state the condition state
   * @return if this person was ever in state
   */
  bool was_ever_in_state(int condition_id, int state) {
    return this->condition[condition_id].entered[state] > -1;
  }

  // VACCINES
  
  /**
   * Sets the person to refuse vaccines.
   */
  void set_vaccine_refusal() {
    this->vaccine_refusal = true;
  }

  /**
   * Checks if this person refuses vaccines.
   *
   * @return if this person refuses vaccines
   */
  bool refuses_vaccines() {
    return this->vaccine_refusal;
  }

  /**
   * Sets this person as ineligible for vaccines.
   */
  void set_ineligible_for_vaccine() {
    this->ineligible_for_vaccine = true;
  }

  /**
   * Checks if this person is ineligible for vaccines.
   *
   * @return if this person is ineligible for vaccines
   */
  bool is_ineligible_for_vaccine() {
    return this->ineligible_for_vaccine;
  }

  /**
   * Sets this person to have received a vaccine.
   */
  void set_received_vaccine() {
    this->received_vaccine = true;
  }

  /**
   * Checks if this person has received a vaccine.
   *
   * @return if this person has received a vaccine
   */
  bool has_received_vaccine() {
    return this->received_vaccine;
  }


  // META AGENTS

  /**
   * Checks if this person is a meta agent.
   *
   * @return if this person is a meta agent
   */
  bool is_meta_agent() {
    return this->id < 0; 
  }

  /**
   * Checks if this person is an admin agent.
   *
   * @return if this person is an admin agent
   */
  bool is_admin_agent() {
    return this->id < -1; 
  }
  
  void set_admin_group(Group* group);
  Group* get_admin_group();
  bool has_closure();

  void get_record_string(char* result);

  //// STATIC METHODS

  static void get_population_properties();
  static void update(int sim_day);
  static void initialize_static_variables();
  static void initialize_static_activity_variables();
  static void setup();
  static void read_all_populations();
  static void read_population(const char* pop_dir, const char* pop_type);
  static void get_person_data(char* line, bool gq);
  static Person* add_person_to_population(std::string sp_id, int age, char sex, int race, int rel, Place* house,
            Place* school, Place* work, int day, bool today_is_birthday);
  
  /**
   * Gets the total number of persons.
   *
   * @return the population
   */
  static int get_population_size() {
    return Person::pop_size;
  }

  /**
   * Gets the person at the specified index in the static people vector.
   *
   * @param p the index
   * @return the person
   */
  static Person* get_person(int p) {
    if(p < Person::pop_size) {
      return Person::people[p];
    } else {
      return nullptr;
    }
  }

  /**
   * Gets the person with the specified id.
   *
   * @param id the ID
   * @return the person
   */
  static Person* get_person_with_id(int id) {
    if(0 <= id && id < static_cast<int>(Person::id_map.size())) {
      int pos = Person::id_map[id];
      if(pos < 0) {
        // person no longer in the population
        return nullptr;
      } else {
        return Person::people[pos];
      }
    } else if(id < 0) {
      // meta agent
      if(id == -1) {
        return Person::get_import_agent();
      } else {
        // admin agents have id -2 or less
        int pos = -id - 2;
        if(pos < static_cast<int>(Person::admin_agents.size())) {
          return Person::admin_agents[pos];
        } else {
          // invalid meta-agent
          return nullptr;
        }
      }
    } else {
      // person never existed
      return nullptr;
    }
  }

  static Person* select_random_person();
  static void prepare_to_die(int day, Person* person);
  static void remove_dead_from_population(int day);
  static void prepare_to_migrate(int day, Person* person);
  static void remove_migrants_from_population(int day);
  static void delete_person_from_population(int day, Person *person);
  static void assign_classrooms();
  static void assign_partitions();
  static void assign_primary_healthcare_facilities();
  static void report(int day);
  static void get_network_stats(char* directory);
  static void print_age_distribution(char* dir, char* date_string, int run);
  static void quality_control();
  static void finish();
  static void get_age_distribution(int* count_males_by_age, int* count_females_by_age);
  static void initialize_activities();

  /**
   * Checks if the input is loaded.
   *
   * @return if the input is loaded
   */
  static bool is_load_completed() {
    return Person::load_completed;
  }

  static void update_health_interventions(int day);
  static void update_population_demographics(int day);
  static void get_external_updates(int day);

  /**
   * Gets the static import agent.
   *
   * @return the import agent
   */
  static Person* get_import_agent() {
    return Person::Import_agent;
  }

  static Person* create_import_agent();
  static Person* create_admin_agent();

  /**
   * Gets the number of admin agents in the static admin agents vector.
   *
   * @return the number of admin agents
   */
  static int get_number_of_admin_agents() {
    return Person::admin_agents.size();
  }

  /**
   * Gets the admin agent at the specified index in the static admin agents vector.
   *
   * @param p the index
   * @return the admin agent
   */
  static Person* get_admin_agent(int p) {
    if(p < Person::get_number_of_admin_agents()) {
      return Person::admin_agents[p];
    } else {
      return nullptr;
    }
  }

  static std::string get_household_relationship_name(int rel);
  static int get_household_relationship_from_name(std::string rel_name);
  static std::string get_race_name(int race);
  static int get_race_from_name(std::string name);

  /**
   * Gets the number of personal variables.
   *
   * @return the number of variables
   */
  static int get_number_of_vars() {
    return Person::number_of_vars;
  }

  /**
   * Gets the number of personal list variables.
   *
   * @return the number of list variables
   */
  static int get_number_of_list_vars() {
    return Person::number_of_list_vars;
  }

  static std::string get_var_name(int index);
  static int get_var_id(std::string var_name);
  static std::string get_list_var_name(int index);
  static int get_list_var_id(std::string var_name);
  static void setup_logging();

private:

  std::string sp_id; // the synthetic population id (read in from the population files)
  int id; // Person's unique identifier (never reused)

  // index: Person's location in population container; once set, will be unique at any given time,
  // but can be reused over the course of the simulation for different people (after death/removal)
  int index;

  // demographics
  int birthday_sim_day;      // agent's birthday in simulation time
  short int init_age;           // Initial age of the agent
  short int number_of_children;      // number of births
  short int household_relationship; // relationship to the householder (see Global.h)
  short int race;        // see Global.h for race codes
  char sex;          // male or female?
  bool alive;
  bool deceased;        // is the agent deceased
  bool in_parents_home;             // still in parents home?
  Place* home_neighborhood;
  Place* last_school;

  // migration
  bool eligible_to_migrate;
  bool native;
  bool original;

  // vaccine
  bool vaccine_refusal;
  bool ineligible_for_vaccine;
  bool received_vaccine;

  // conditions
  int number_of_conditions;
  condition_t* condition;

  //Primary Care Location
  Hospital* primary_healthcare_facility;

  // previous infection serotype (for dengue)
  int previous_infection_serotype;

  // personal variables
  double* var;
  double_vector_t* list_var;

  // links to groups
  Link* link;

  // activity schedule:
  std::bitset<64> on_schedule; // true iff activity location is on schedule
  int schedule_updated;       // date of last schedule update
  bool is_traveling;        // true if traveling
  bool is_traveling_outside;   // true if traveling outside modeled area
  short int profile;                              // activities profile type
  bool is_hospitalized;
  int return_from_travel_sim_day;
  int sim_day_hospitalization_ends;

  // STATIC VARIABLES

  // population

  static person_vector_t people;
  static person_vector_t admin_agents;
  static person_vector_t death_list;    // list of agents to die today
  static person_vector_t migrant_list; // list of agents to out migrate today
  static int pop_size;
  static int next_id;
  static int next_meta_id;
  static std::vector<int> id_map;

  // personal variables
  static int number_of_vars;
  static std::vector<std::string> var_name;
  static Expression** var_expr;

  static int number_of_list_vars;
  static std::vector<std::string> list_var_name;
  static Expression** list_var_expr;

  // global variables
  static int number_of_global_vars;
  static std::vector<std::string> global_var_name;
  static double* global_var;

  static int number_of_global_list_vars;
  static std::vector<std::string> global_list_var_name;
  static double_vector_t* global_list_var;

  // meta_agents
  static Person* Import_agent;

  // used during input
  static bool is_initialized;
  static bool load_completed;
  static int enable_copy_files;
  static void parse_lines_from_stream(std::istream &stream, bool is_group_quarters_pop);
  
  // schedule
  static bool is_weekday;     // true if current day is Monday .. Friday
  static int day_of_week;     // day of week index, where Sun = 0, ... Sat = 6

  // output
  static int report_initial_population;
  static int output_population;
  static char pop_outfile[FRED_STRING_SIZE];
  static char output_population_date_match[FRED_STRING_SIZE];
  static void write_population_output_file(int day);
  static int Popsize_by_age [Demographics::MAX_AGE+1];
  static person_vector_t report_person;
  static std::vector<report_t*> report_vec;
  static int max_reporting_agents;

  // counters
  static double Sim_based_prob_stay_home_not_needed;
  static double Census_based_prob_stay_home_not_needed;
  static int entered_school;
  static int left_school;


  // admin lookup
  static std::unordered_map<Person*, Group*> admin_group_map;

  static bool record_location;

  // logging
  static bool is_log_initialized;
  static std::string person_log_level;
  static std::unique_ptr<spdlog::logger> person_logger;
};

#endif // _FRED_PERSON_H
