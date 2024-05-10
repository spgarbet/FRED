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
// File: Place.cc
//
#include <algorithm>
#include <climits>
#include <sstream>
#include <iostream>
#include <limits>
#include <math.h>
#include <new>
#include <set>
#include <string>
#include <typeinfo>
#include <unistd.h>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>

#include "Block_Group.h"
#include "Census_Tract.h"
#include "Condition.h"
#include "County.h"
#include "Date.h"
#include "Geo.h"
#include "Global.h"
#include "Hospital.h"
#include "Household.h"
#include "Neighborhood_Layer.h"
#include "Neighborhood_Patch.h"
#include "Parser.h"
#include "Person.h"
#include "Place.h"
#include "Place_Type.h"
#include "Random.h"
#include "Regional_Layer.h"
#include "Regional_Patch.h"
#include "State.h"
#include "Utils.h"
#include "Visualization_Layer.h"

#define PI 3.14159265359

// static place subtype codes
char Place::SUBTYPE_NONE = 'X';
char Place::SUBTYPE_COLLEGE = 'C';
char Place::SUBTYPE_PRISON = 'P';
char Place::SUBTYPE_MILITARY_BASE = 'M';
char Place::SUBTYPE_NURSING_HOME = 'N';
char Place::SUBTYPE_HEALTHCARE_CLINIC = 'I';
char Place::SUBTYPE_MOBILE_HEALTHCARE_CLINIC = 'Z';

bool Place::load_completed = false;
bool Place::is_primary_care_assignment_initialized = false;

int Place::next_place_id = 0;

// geo info
fred::geo Place::min_lat = 0;
fred::geo Place::max_lat = 0;
fred::geo Place::min_lon = 0;
fred::geo Place::max_lon = 0;
bool Place::country_is_usa = false;
bool Place::country_is_colombia = false;
bool Place::country_is_india = false;
std::vector<int> Place::state_admin_code;
std::map<std::string, std::string>  Place::hh_label_hosp_label_map;
std::map<std::string, int>  Place::hosp_label_hosp_id_map;

// map of place type names
LabelMapT* Place::household_label_map;
LabelMapT* Place::school_label_map;
LabelMapT* Place::workplace_label_map;

// lists of places by type
place_vector_t Place::place_list;
place_vector_t Place::schools_by_grade[Global::GRADES];
place_vector_t Place::gq;

bool Place::Update_elevation = false;

char Place::Population_directory[FRED_STRING_SIZE];
char Place::Country[FRED_STRING_SIZE];
char Place::Population_version[FRED_STRING_SIZE];

// mean size of "household" associated with group quarters
double Place::College_dorm_mean_size = 3.5;
double Place::Military_barracks_mean_size = 12;
double Place::Prison_cell_mean_size = 1.5;
double Place::Nursing_home_room_mean_size = 1.5;

// non-resident staff for group quarters
int Place::College_fixed_staff = 0;
double Place::College_resident_to_staff_ratio = 0;
int Place::Prison_fixed_staff = 0;
double Place::Prison_resident_to_staff_ratio = 0;
int Place::Nursing_home_fixed_staff = 0;
double Place::Nursing_home_resident_to_staff_ratio = 0;
int Place::Military_fixed_staff = 0;
double Place::Military_resident_to_staff_ratio = 0;
int Place::School_fixed_staff = 0;
double Place::School_student_teacher_ratio = 0;
bool Place::Household_hospital_map_file_exists = false;
int Place::Hospital_fixed_staff = 1.0;
double Place::Hospital_worker_to_bed_ratio = 1.0;
double Place::Hospitalization_radius = 0.0;
int Place::Hospital_overall_panel_size = 0;
std::vector<std::string> Place::location_id_vec;
HospitalIDCountMapT Place::Hospital_ID_total_assigned_size_map;
HospitalIDCountMapT Place::Hospital_ID_current_assigned_size_map;

bool Place::is_log_initialized = false;
std::string Place::place_log_level = "";
std::unique_ptr<spdlog::logger> Place::place_logger = nullptr;

/**
 * Creates a Place with the specified properties. The label and type ID are passed into the Group constructor, 
 * and the given geological coordinates are set as this place's location. Default variables are initialized.
 *
 * @param lab the label
 * @param _type_id the place type ID
 * @param lon the longitude
 * @param lat the latitude
 */
Place::Place(const char* lab, int _type_id, fred::geo lon, fred::geo lat) : Group(lab, _type_id) {

  this->set_id(-1);      // actual id assigned in Place::add_place
  this->set_subtype(Place::SUBTYPE_NONE);
  this->staff_size = 0;
  this->members.reserve(8); // initial slots for 8 people -- this is expanded in begin_membership()
  this->members.clear();
  this->patch = nullptr;
  this->container = nullptr;
  this->longitude = lon;
  this->latitude = lat;
  this->admin_code = 0;  // assigned elsewhere

  this->original_size_by_age = nullptr;
  this->partitions_by_age = nullptr;

  int conditions = Condition::get_number_of_conditions();
  this->transmissible_people = new person_vector_t[conditions];

  // zero out all condition-specific counts
  for(int d = 0; d < conditions; ++d) {
    this->transmissible_people[d].clear();
  }

  this->elevation = 0.0;
  this->income = 0;
  this->partitions.clear();
  this->next_partition = 0;
  this->vaccination_rate = -1.0;
}

/**
 * Prepares this place. Creates an administrator for this group, sets the median income, and sets the elevation of all 
 * partitions to this place's elevation. If this place is a school, prepares vaccination rates.
 */
void Place::prepare() {
  Place::place_logger->debug("Prepare place {:d} {:s}", this->id, this->label);

  Group::create_administrator();

  this->N_orig = this->members.size();

  // find median income
  int size = this->get_size();
  std::vector<int> income_list;
  income_list.reserve(size);
  for(int i = 0; i < size; ++i) {
    income_list.push_back(this->members[i]->get_income());
  }
  std::sort(income_list.begin(), income_list.end());
  if(size > 0) {
    int median = income_list[size / 2];
    this->set_income(median);
  } else {
    this->set_income(0);
  }

  // set elevation of partitions
  int rooms = this->partitions.size();
  for(int i = 0; i < rooms; ++i) {
    this->partitions[i]->set_partition_elevation(this->get_elevation());
  }

  if(this->is_school()) {
    this->prepare_vaccination_rates();
  }

  Place::place_logger->debug("Prepare place {:d} {:s} finished", this->id, this->label);
}

/**
 * Outputs this place's ID and label to status.
 *
 * @param condition_id _UNUSED_
 */
void Place::print(int condition_id) {
  Place::place_logger->info("place {:d} {:s}", this->id, this->label);
}

/**
 * Turns members of this place to teachers in the specified school, if eligible. The staff size 
 * of the school will be incremented according to the amount of successful new teachers.
 *
 * @param school the school
 */
void Place::turn_workers_into_teachers(Place* school) {
  std::vector<Person*> workers;
  workers.reserve(static_cast<int>(this->members.size()));
  workers.clear();
  for(int i = 0; i < static_cast<int>(this->members.size()); ++i) {
    workers.push_back(this->members[i]);
  }
  Place::place_logger->debug("turn_workers_into_teachers: place {:d} {:s} has {:d} workers", this->get_id(), 
      this->get_label(), this->members.size());
  int new_teachers = 0;
  for(int i = 0; i < static_cast<int>(workers.size()); ++i) {
    Person* person = workers[i];
    assert(person != nullptr);
    Place::place_logger->debug("Potential teacher {:d} age {:d}", person->get_id(), person->get_age());
    if(person->become_a_teacher(school)) {
      ++new_teachers;
      Place::place_logger->debug("new teacher {:d} age {:d} moved from workplace {:d} {:s} to school {:d} {:s}",
          person->get_id(), person->get_age(), this->get_id(), this->get_label(), school->get_id(), school->get_label());
    }
  }
  school->set_staff_size(school->get_staff_size() + new_teachers);
  Place::place_logger->info("{:d} new teachers reassigned from workplace {:s} to school {:s}", new_teachers,
	    this->get_label(), school->get_label());
}

/**
 * Reassigns workers of this place to be workers of the specified Place. That place's staff size will be incremented 
 * according to the number of workers reassigned.
 *
 * @param new_place the new workplace
 */
void Place::reassign_workers(Place* new_place) {
  person_vector_t workers;
  workers.reserve((int)this->members.size());
  workers.clear();
  for(int i = 0; i < static_cast<int>(this->members.size()); ++i) {
    workers.push_back(this->members[i]);
  }
  int reassigned_workers = 0;
  for(int i = 0; i < static_cast<int>(workers.size()); ++i) {
    workers[i]->change_workplace(new_place, 0);
    ++reassigned_workers;
  }
  new_place->set_staff_size(new_place->get_staff_size() + reassigned_workers);
  Place::place_logger->info("{:d} workers reassigned from workplace {:s} to place {:s}", reassigned_workers,
	    this->get_label(), new_place->get_label());
}

//////////////////////////////////////////////////////////
//
// PLACE SPECIFIC DATA
//
//////////////////////////////////////////////////////////

/**
 * Gets the label of the specified Place.
 *
 * @param p the place
 * @return the label
 */
char* Place::get_place_label(Place* p) {
  return ((p == nullptr) ? (char*) "-1" : p->get_label());
}

/**
 * Gets the admin code of the Block_Group in which this place is located.
 *
 * @return the block group admin code
 */
long long int Place::get_block_group_admin_code() {
  return this->admin_code;
}

/**
 * Gets the Block_Group in which this place is located.
 *
 * @return the block group
 */
Block_Group* Place::get_block_group() {
  return Block_Group::get_block_group_with_admin_code(this->admin_code);
}

/**
 * Gets the admin code of the Census_Tract in which this place is located.
 *
 * @return the census tract admin code
 */
long long int Place::get_census_tract_admin_code() {
  return this->get_block_group_admin_code() / 10;
}

/**
 * Gets the admin code of the County in which this place is located.
 *
 * @return the county admin code
 */
int Place::get_county_admin_code() {
  if(Place::is_country_usa()) {
    return static_cast<int>(this->get_census_tract_admin_code() / 1000000);
  }
  if(Place::is_country_india()) {
    return static_cast<int>((this->admin_code / 1000000) % 1000);
  } else {
    return static_cast<int>(this->get_census_tract_admin_code() / 1000000);
  }
}

/**
 * Gets the admin code of the State in which this place is located.
 *
 * @return the state admin code
 */
int Place::get_state_admin_code() {
  if(Place::is_country_usa()) {
    return this->get_county_admin_code() / 1000;
  }
  if(Place::is_country_india()) {
    return static_cast<int>(this->admin_code / 1000000000);
  } else {
    return this->get_county_admin_code() / 1000;
  }
}

/**
 * Sets the elevation of this place.
 *
 * @param elev the elevation
 */
void Place::set_elevation(double elev) {
  this->elevation = elev;
}

/**
 * Gets the ADI state rank of the Block_Group in which this place is located.
 *
 * @return the ADI state rank
 */
int Place::get_adi_state_rank() {
  return Block_Group::get_block_group_with_admin_code(get_block_group_admin_code())->get_adi_state_rank();
}

/**
 * Gets the ADI national rank of the Block_Group in which this place is located.
 *
 * @return the ADI national rank
 */
int Place::get_adi_national_rank() {
  return Block_Group::get_block_group_with_admin_code(get_block_group_admin_code())->get_adi_national_rank();
}

//// STATIC METHODS

/**
 * Gets the Household at the specified index in the household Place_Type's places vector.
 *
 * @param i the index
 * @return the household
 */
Household* Place::get_household(int i) {
  if(0 <= i && i < Place::get_number_of_households()) {
    return static_cast<Household*>(Place_Type::get_household_place_type()->get_place(i));
  } else {
    return nullptr;
  }
}

/**
 * Gets the neighborhood at the specified index in the neighborhood Place_Type's places vector.
 *
 * @param i the index
 * @return the neighborhood
 */
Place* Place::get_neighborhood(int i) {
  if(0 <= i && i < Place::get_number_of_neighborhoods()) {
    return Place_Type::get_neighborhood_place_type()->get_place(i);
  } else {
    return nullptr;
  }
}

/**
 * Gets the school at the specified index in the school Place_Type's places vector.
 *
 * @param i the index
 * @return the school
 */
Place* Place::get_school(int i) {
  if(0 <= i && i < Place::get_number_of_schools()) {
    return Place_Type::get_school_place_type()->get_place(i);
  } else {
    return nullptr;
  }
}

/**
 * Gets the workplace at the specified index in the workplace Place_Type's places vector.
 *
 * @param i the index
 * @return the workplace
 */
Place* Place::get_workplace(int i) {
  if(0 <= i && i < Place::get_number_of_workplaces()) {
    return Place_Type::get_workplace_place_type()->get_place(i);
  } else {
    return nullptr;
  }
}

/**
 * Gets the Hospital at the specified index in the hospital Place_Type's places vector.
 *
 * @param i the index
 * @return the hospital
 */
Hospital* Place::get_hospital(int i) {
  if(0 <= i && i < Place::get_number_of_hospitals()) {
    return static_cast<Hospital*>(Place_Type::get_hospital_place_type()->get_place(i));
  } else {
    return nullptr;
  }
}

/**
 * Gets the xy distance between two specified places.
 *
 * @param p1 the first place
 * @param p2 the second place
 * @return the xy distance
 */
double Place::distance_between_places(Place* p1, Place* p2) {
  return Geo::xy_distance(p1->get_latitude(), p1->get_longitude(), p2->get_latitude(), p2->get_longitude());
}

/**
 * Gets properties for self and derived subclasses.
 */
void Place::get_place_properties() {

  char property_name[FRED_STRING_SIZE];

  // read optional properties
  Parser::disable_abort_on_failure();
  
  Place::household_label_map = new LabelMapT();
  Place::school_label_map = new LabelMapT();
  Place::workplace_label_map = new LabelMapT();
  Place::gq.clear();
  for(int grade = 0; grade < Global::GRADES; ++grade) {
    Place::schools_by_grade[grade].clear();
  }

  // get properties for derived subclasses
  Household::get_properties();
  Hospital::get_properties();
  Block_Group::read_adi_file();

  // population properties
  strcpy(Place::Population_directory, "$FRED_HOME/data/country");
  Parser::get_property("population_directory", Place::Population_directory);

  strcpy(Place::Population_version, "RTI_2010_ver1");
  Parser::get_property("population_version", Place::Population_version);

  strcpy(Place::Country, "usa");
  Parser::get_property("country", Place::Country);
  for(int i = 0; i < static_cast<int>(strlen(Place::Country)); ++i) {
    Place::Country[i] = tolower(Place::Country[i]);
  }

  Place::country_is_usa = strcmp(Place::Country, "usa") == 0;
  Place::country_is_colombia = strcmp(Place::Country, "colombia") == 0;
  Place::country_is_india = strcmp(Place::Country, "india") == 0;

  Place::location_id_vec.clear();

  strcpy(property_name, "locations_file");
  if(Parser::does_property_exist(property_name)) {
    char locations_filename[FRED_STRING_SIZE];
    strcpy(locations_filename, "$FRED_HOME/data/locations.txt");
    Parser::get_property(property_name, locations_filename);
    FILE* loc_fp = Utils::fred_open_file(locations_filename);
    if(loc_fp != nullptr) {
      char loc_id[FRED_STRING_SIZE];
      // int n = 0;
      while(fscanf(loc_fp, "%s", loc_id) == 1) {
        Place::location_id_vec.push_back(std::string(loc_id));
      }
      fclose(loc_fp);
      // exit(0);
    } else {
      char msg[FRED_STRING_SIZE];
      snprintf(msg, FRED_STRING_SIZE, "Can't find locations_file |%s|", locations_filename);
      Utils::print_error(msg);
    }
  } else {
    std::vector<std::string> location_names;
    location_names.clear();
    char loc_value[FRED_STRING_SIZE];
    strcpy(loc_value,"");
    Parser::get_property("locations", loc_value);
  
    // split the property value into separate strings
    string_vector_t parts = Utils::get_string_vector(loc_value, ' ');
    for(int i = 0; i < static_cast<int>(parts.size()); ++i) {
      location_names.push_back(parts[i]);
    }

    if(location_names.size()>0) {
      char locations_filename[FRED_STRING_SIZE];
      snprintf(locations_filename, FRED_STRING_SIZE, "$FRED_HOME/data/country/%s/locations.txt", Place::Country);
      FILE* loc_fp = Utils::fred_open_file(locations_filename);
      if(loc_fp != nullptr) {
        char* line = nullptr;
        size_t linecap = 0;
        size_t linelen;
        std::string linestr;
        while((linelen = getline(&line, &linecap, loc_fp)) > 0) {
          if(static_cast<int>(linelen) == -1) { // Note: size_t is an unsigned int and thus CAN'T be -1
            break;
          }
          // remove newline char
          if(line[strlen(line) - 1] == '\n') {
            line[strlen(line) - 1] = '\0';
          }
          std::string linestr = std::string(line);
          for(int i = 0; i < static_cast<int>(location_names.size()); ++i) {
            std::string loc = location_names[i] + " ";
            if(linestr.find(loc) == 0) {
              // get remainder of the line
              std::string fips_codes = linestr.substr(loc.length());
              // split fips_codes into separate strings
              char fips[FRED_STRING_SIZE];
              strcpy(fips, fips_codes.c_str());

              // split the fip_codes into separate strings
              string_vector_t parts = Utils::get_string_vector(fips_codes, ' ');
              for(int j = 0; j < static_cast<int>(parts.size()); ++j) {
                Place::location_id_vec.push_back(parts[j]);
              }
            }
          }
        }
        fclose(loc_fp);
      } else {
        char msg[FRED_STRING_SIZE];
        snprintf(msg, FRED_STRING_SIZE, "Can't find locations_file |%s|", locations_filename);
        Utils::print_error(msg);
      }
    }
  }
  
  // remove any duplicate location ids
  int size = Place::location_id_vec.size();
  if(size == 0) {
    char error_file[FRED_STRING_SIZE];
    snprintf(error_file, FRED_STRING_SIZE, "%s/errors.txt", Global::Simulation_directory);
    FILE* fp = fopen(error_file, "a");
    fprintf(fp, "FRED Error (file %s) No locations specified\n", Global::Model_file);
    fclose(fp);
    exit(0);
  }

  for(int j = size - 1; j > 0; --j) {
    bool duplicate = false;
    for(int i = 0; duplicate==false && i < j; ++i) {
      if(Place::location_id_vec[i] == Place::location_id_vec[j]) {
        duplicate = true;
      }
    }
    if(duplicate) {
      for(int k = j; k < static_cast<int>(Place::location_id_vec.size()) - 1; ++k) {
        Place::location_id_vec[k] = Place::location_id_vec[k + 1];
      }
      Place::location_id_vec.pop_back();
    }
  }
  for(int i = 0; i < static_cast<int>(Place::location_id_vec.size()); ++i) {
    Place::place_logger->info("location_id_vec[{:d}] = {:s}", i, get_location_id(i));
    Place::verify_pop_directory(get_location_id(i));
  }

  Parser::get_property("update_elevation", &Place::Update_elevation);

  // school staff size
  Parser::get_property("School_fixed_staff", &Place::School_fixed_staff);
  Parser::get_property("School_student_teacher_ratio", &Place::School_student_teacher_ratio);

  // group quarter properties
  Parser::get_property("college_dorm_mean_size", &Place::College_dorm_mean_size);
  Parser::get_property("military_barracks_mean_size", &Place::Military_barracks_mean_size);
  Parser::get_property("prison_cell_mean_size", &Place::Prison_cell_mean_size);
  Parser::get_property("nursing_home_room_mean_size", &Place::Nursing_home_room_mean_size);

  Parser::get_property("college_fixed_staff", &Place::College_fixed_staff);
  Parser::get_property("college_resident_to_staff_ratio", &Place::College_resident_to_staff_ratio);
  Parser::get_property("prison_fixed_staff", &Place::Prison_fixed_staff);
  Parser::get_property("prison_resident_to_staff_ratio", &Place::Prison_resident_to_staff_ratio);
  Parser::get_property("nursing_home_fixed_staff", &Place::Nursing_home_fixed_staff);
  Parser::get_property("nursing_home_resident_to_staff_ratio", &Place::Nursing_home_resident_to_staff_ratio);
  Parser::get_property("military_fixed_staff", &Place::Military_fixed_staff);
  Parser::get_property("military_resident_to_staff_ratio", &Place::Military_resident_to_staff_ratio);

  // hospitalization properties
  Parser::get_property("Hospital_worker_to_bed_ratio", &Place::Hospital_worker_to_bed_ratio);
  Place::Hospital_worker_to_bed_ratio = (Place::Hospital_worker_to_bed_ratio == 0.0 ? 1.0 : Place::Hospital_worker_to_bed_ratio);
  Parser::get_property("hospitalization_radius", &Place::Hospitalization_radius);
  Parser::get_property("Hospital_fixed_staff", &Place::Hospital_fixed_staff);

  char hosp_file_dir[FRED_STRING_SIZE];
  char hh_hosp_map_file_name[FRED_STRING_SIZE];

  Parser::get_property("Household_Hospital_map_file_directory", hosp_file_dir);
  Parser::get_property("Household_Hospital_map_file", hh_hosp_map_file_name);

  if(strcmp(hh_hosp_map_file_name, "none") == 0) {
    Place::Household_hospital_map_file_exists = false;
  } else {
    //If there is a file mapping Households to Hospitals, open it
    FILE* hospital_household_map_fp = nullptr;

    char filename[FRED_STRING_SIZE];
    snprintf(filename, FRED_STRING_SIZE, "%s%s", hosp_file_dir, hh_hosp_map_file_name);
    hospital_household_map_fp = Utils::fred_open_file(filename);

    if(hospital_household_map_fp != nullptr) {
      Place::Household_hospital_map_file_exists = true;

      char hh_label[FRED_STRING_SIZE];
      char hosp_label[FRED_STRING_SIZE];
      while(fscanf(hospital_household_map_fp, "%s,%s", hh_label, hosp_label) == 2) {
        if(strcmp(hh_label, "hh_id") == 0) {
          continue;
        }
        if(strcmp(hh_label, "sp_id") == 0) {
          continue;
        }
        std::string hh_label_str(hh_label);
        std::string hosp_label_str(hosp_label);
        Place::hh_label_hosp_label_map.insert(std::pair<std::string, std::string>(hh_label_str, hosp_label_str));
      }
      fclose(hospital_household_map_fp);
    }
  }

  // the following are included here to make them visible to check_properties.
  // they are conditionally read in elsewhere.

  char elevation_data_dir[FRED_STRING_SIZE];
  snprintf(elevation_data_dir, FRED_STRING_SIZE, "none");
  Parser::get_property("elevation_data_directory", elevation_data_dir);

  char map_file_dir[FRED_STRING_SIZE];
  char map_file_name[FRED_STRING_SIZE];
  Parser::get_property("Household_Hospital_map_file_directory", map_file_dir);
  Parser::get_property("Household_Hospital_map_file", map_file_name);

  // restore requiring properties
  Parser::set_abort_on_failure();

}

/**
 * Reads all places into the simulation and creates the Neighborhood_Layers and Regional_Layer.
 */
void Place::read_all_places() {

  // clear the vectors and maps
  Place::state_admin_code.clear();
  Place::hosp_label_hosp_id_map.clear();
  Place::hh_label_hosp_label_map.clear();

  // to compute the region's bounding box
  Place::min_lat = Place::min_lon = 999;
  Place::max_lat = Place::max_lon = -999;

  // process each specified location
  int locs = Place::get_number_of_location_ids();
  for(int i = 0; i < locs; ++i) {
    Place::read_places(Place::get_location_id(i));
  }

  // temporarily compute income levels to use for group quarters
  Place_Type::get_household_place_type()->prepare();

  // read group quarters separately so that we can assign household incomes
  for(int i = 0; i < locs; ++i) {
    Place::read_gq_places(get_location_id(i));
  }

  int total = 0;
  for(int i = 0; i < Place_Type::get_number_of_place_types(); ++i) {
    total += Place_Type::get_place_type(i)->get_number_of_places();
  }
  Place::place_logger->info("total count = {:d}", total);
  Place::place_logger->info("finished total places = {:d}", next_place_id);
  assert(total == next_place_id);

  if(Global::Use_Mean_Latitude) {
    // Make projection based on the location file.
    fred::geo mean_lat = (min_lat + max_lat) / 2.0;
    Geo::set_km_per_degree(mean_lat);
    Place::place_logger->info("min_lat: {:f}  max_lat: {:f}  mean_lat: {:f}", min_lat, max_lat, mean_lat);
  } else {
    // DEFAULT: Use mean US latitude (see Geo.cc)
    Place::place_logger->info("min_lat: {:f}  max_lat: {:f}", min_lat, max_lat);
  }

  // create geographical grids
  Global::Simulation_Region = new Regional_Layer(min_lon, min_lat, max_lon, max_lat);

  // layer containing neighborhoods
  Global::Neighborhoods = new Neighborhood_Layer();

  // add all places to the Neighborhood Layer
  Place_Type::add_places_to_neighborhood_layer();
  // Neighborhood_Layer::setup call Neighborhood_Patch::make_neighborhood
  Global::Neighborhoods->setup();
  Place::place_logger->info("Created {:d} neighborhoods", Place::get_number_of_neighborhoods());

  // add workplaces to Regional grid (for worker reassignment)
  int number_places = Place::get_number_of_workplaces();
  for(int p = 0; p < number_places; ++p) {
    Global::Simulation_Region->add_workplace(Place::get_workplace(p));
  }

  // add hospitals to Regional grid (for household hospital assignment)
  number_places = get_number_of_hospitals();
  for(int p = 0; p < number_places; ++p) {
    Global::Simulation_Region->add_hospital(Place::get_hospital(p));
  }

  Place::load_completed = true;
  number_places = Place::get_number_of_households() + Place::get_number_of_neighborhoods()
    + Place::get_number_of_schools() + Place::get_number_of_workplaces() + Place::get_number_of_hospitals();

  Place::place_logger->info("read_all_places finished: households = {:d}", Place::get_number_of_households());
  Place::place_logger->info("read_all_places finished: neighborhoods = {:d}", Place::get_number_of_neighborhoods());
  Place::place_logger->info("read_all_places finished: schools = {:d}", Place::get_number_of_schools());
  Place::place_logger->info("read_all_places finished: workplaces = {:d}", Place::get_number_of_workplaces());
  Place::place_logger->info("read_all_places finished: hospitals = {:d}", Place::get_number_of_hospitals());
  Place::place_logger->info("read_all_places finished: Places = {:d}", number_places);

}

/**
 * Verifies that a population directory containing the given location ID exists.
 *
 * @param loc_id the location ID
 */
void Place::verify_pop_directory(const char* loc_id) {
  char pop_dir[FRED_STRING_SIZE];
  snprintf(pop_dir, FRED_STRING_SIZE, "%s/%s/%s/%s", Place::Population_directory, Place::Country, Place::Population_version, loc_id);

  if(Utils::does_path_exist(pop_dir) == false) {
    char msg[FRED_STRING_SIZE];
    snprintf(msg, FRED_STRING_SIZE, "Can't find population directory |%s|", pop_dir);
    Utils::print_error(msg);
  }
}

/**
 * Reads in places in the population directory containing the given location ID.
 *
 * @param loc_id the location ID
 */
void Place::read_places(const char* loc_id) {

  Place::place_logger->info("read places {:s} entered", loc_id);

  Place::verify_pop_directory(loc_id);

  char pop_dir[FRED_STRING_SIZE];
  snprintf(pop_dir, FRED_STRING_SIZE, "%s/%s/%s/%s", Place::Population_directory, Place::Country, Place::Population_version, loc_id);
  
  // Record the actual synthetic population in the log file
  // Need to write this part to the LOG file for fred_job
  Utils::fred_log("POPULATION_FILE: %s\n", pop_dir);
  Place::place_logger->info("POPULATION_FILE: {:s}", pop_dir);

  if(Global::Compile_FRED && Place_Type::get_number_of_place_types() <= 7) {
    return;
  }

  // read household locations
  char location_file[FRED_STRING_SIZE];
  snprintf(location_file, FRED_STRING_SIZE, "%s/households.txt", pop_dir);
  Place::read_household_file(location_file);
  Utils::fred_print_lap_time("Places.read_household_file");

  // read school locations
  snprintf(location_file, FRED_STRING_SIZE, "%s/schools.txt", pop_dir);
  Place::read_school_file(location_file);

  // read workplace locations
  snprintf(location_file, FRED_STRING_SIZE, "%s/workplaces.txt", pop_dir);
  Place::read_workplace_file(location_file);

  // read hospital locations
  snprintf(location_file, FRED_STRING_SIZE, "%s/hospitals.txt", pop_dir);
  Place::read_hospital_file(location_file);

  // read in user-defined place types
  Place_Type::read_places(pop_dir);

  Place::place_logger->info("read places {:s} finished", loc_id);
}

/**
 * Locates and reads the group quarters file in the population directory containing the given location ID.
 *
 * @param loc_id the location ID
 */
void Place::read_gq_places(const char* loc_id) {

  Place::place_logger->info("read gq_places entered");
  
  if(Global::Compile_FRED && Place_Type::get_number_of_place_types() <= 7) {
    return;
  }

  if(Global::Enable_Group_Quarters || Place::Update_elevation) {

    char pop_dir[FRED_STRING_SIZE];
    snprintf(pop_dir, FRED_STRING_SIZE, "%s/%s/%s/%s", Place::Population_directory, Place::Country, Place::Population_version, loc_id);

    // read group quarters locations (a new workplace and household is created 
    // for each group quarters)
    char location_file[FRED_STRING_SIZE];
    snprintf(location_file, FRED_STRING_SIZE, "%s/gq.txt", pop_dir);
    Place::read_group_quarters_file(location_file);
    Utils::fred_print_lap_time("Places.read_group_quarters_file");

  }
  Place::place_logger->info("read gq_places finished");
}

/**
 * Reads in households to the simulation from the household file at the given location file path. Assigns 
 * a Synthetic Population ID.
 *
 * @param location_file the file path
 */
void Place::read_household_file(char* location_file) {

  char line[FRED_STRING_SIZE];

  // data to fill in from input file
  int type_id = Place_Type::HOUSEHOLD;
  char place_subtype = Place::SUBTYPE_NONE;
  char new_label[FRED_STRING_SIZE];
  char label[FRED_STRING_SIZE];
  long long int admin_code = 0;
  long long int sp_id = 0;
  double lat = 0;
  double lon = 0;
  double elevation = 0;
  int race;
  int income;
  int n = 0;

  FILE* fp = Utils::fred_open_file(location_file);

  // skip header line
  fgets(label, FRED_STRING_SIZE, fp);

  // read first data line
  strcpy(line, "");
  fgets(line, FRED_STRING_SIZE, fp);
  int items = sscanf(line, "%s %lld %d %d %lf %lf %lf", label, &admin_code, &race, &income, &lat, &lon, &elevation);
  
  while(6 <= items) {
    sscanf(label, "%lld", &sp_id);
    if(!Group::sp_id_exists(sp_id + 100000000)) {
      // negative income disallowed
      if(income < 0) {
        income = 0;
      }

      snprintf(new_label, FRED_STRING_SIZE, "H-%s", label);

      Household* place = static_cast<Household*>(Place::add_place(new_label, type_id, place_subtype, lon, lat, elevation, admin_code));
      place->set_sp_id(sp_id + 100000000);

      // household race and income
      place->set_household_race(race);
      place->set_income(income);

      ++n;

      Place::update_geo_boundaries(lat, lon);
    }

    // get next line
    strcpy(line, "");
    fgets(line, FRED_STRING_SIZE, fp);
    items = sscanf(line, "%s %lld %d %d %lf %lf %lf", label, &admin_code, &race, &income, &lat, &lon, &elevation);

  }
  fclose(fp);
  Place::place_logger->info("finished reading in {:d} households", n);
  return;
}

/**
 * Reads in workplaces to the simulation from the household file at the given location file path. Assigns 
 * a Synthetic Population ID.
 *
 * @param location_file the file path
 */
void Place::read_workplace_file(char* location_file) {
  char line[FRED_STRING_SIZE];

  // data to fill in from input file
  int type_id = Place_Type::WORKPLACE;
  char place_subtype = Place::SUBTYPE_NONE;
  char label[FRED_STRING_SIZE];
  char new_label[FRED_STRING_SIZE];
  double lat;
  double lon;
  double elevation = 0;
  long long int admin_code = 0;
  long long int sp_id = 0;

  FILE* fp = Utils::fred_open_file(location_file);
  if(fp == nullptr) {
    return;
  }

  // skip header line
  fgets(label, FRED_STRING_SIZE, fp);

  // read first data line
  strcpy(line, "");
  fgets(line, FRED_STRING_SIZE, fp);
  int items = sscanf(line, "%s %lf %lf %lf", label, &lat, &lon, &elevation);

  while(3 <= items) {
    snprintf(new_label, FRED_STRING_SIZE, "W-%s", label);
    sscanf(label, "%lld", &sp_id);

    if(!Group::sp_id_exists(sp_id)) {
      Place* place = Place::add_place(new_label, type_id, place_subtype, lon, lat, elevation, admin_code);
      place->set_sp_id(sp_id);
    }

    // read next data line
    strcpy(line, "");
    fgets(line, FRED_STRING_SIZE, fp);
    items = sscanf(line, "%s %lf %lf %lf", label, &lat, &lon, &elevation);
  }
  fclose(fp);
  return;
}

/**
 * Reads in hospitals to the simulation from the household file at the given location file path. Assigns 
 * a Synthetic Population ID.
 *
 * @param location_file the file path
 */
void Place::read_hospital_file(char* location_file) {
  char line[FRED_STRING_SIZE];

  // data to fill in from input file
  int type_id = Place_Type::HOSPITAL;
  char place_subtype = Place::SUBTYPE_NONE;
  char label[FRED_STRING_SIZE];
  char new_label[FRED_STRING_SIZE];
  double lat;
  double lon;
  double elevation = 0;
  int workers;
  int physicians;
  int beds;
  long long int admin_code = 0;
  long long int sp_id = 0;

  FILE* fp = Utils::fred_open_file(location_file);
  if(fp == nullptr) {
    return;
  }

  // skip header line
  fgets(label, FRED_STRING_SIZE, fp);

  if(fp != nullptr) {
    // read first data line
    strcpy(line, "");
    fgets(line, FRED_STRING_SIZE, fp);
    int items = sscanf(line, "%s %d %d %d %lf %lf %lf", label, &workers, &physicians, &beds, &lat, &lon, &elevation);
    while(6 <= items) {
      snprintf(new_label, FRED_STRING_SIZE, "M-%s", label);
      sscanf(label, "%lld", &sp_id);
      if(!Group::sp_id_exists(sp_id + 600000000)) {
        Hospital* place = static_cast<Hospital*>(Place::add_place(new_label, type_id, place_subtype, lon, lat, elevation, admin_code));

        place->set_sp_id(sp_id + 600000000);

        place->set_employee_count(workers);
        place->set_physician_count(physicians);
        place->set_bed_count(beds);

        std::string hosp_label_str(label);
        int hosp_id = Place::get_number_of_hospitals() - 1;
        Place::hosp_label_hosp_id_map.insert(std::pair<std::string, int>(hosp_label_str, hosp_id));
      }
      
      // read next data line
      strcpy(line, "");
      fgets(line, FRED_STRING_SIZE, fp);
      items = sscanf(line, "%s %d %d %d %lf %lf %lf", label, &workers, &physicians, &beds, &lat, &lon, &elevation);
    }
    fclose(fp);
  }
  Place::place_logger->info("read_hospital_file: found {:d} hospitals", get_number_of_hospitals());
  return;
}

/**
 * Reads in schools to the simulation from the household file at the given location file path. Assigns 
 * a Synthetic Population ID.
 *
 * @param location_file the file path
 */
void Place::read_school_file(char* location_file) {
  char line[FRED_STRING_SIZE];

  // data to fill in from input file
  int type_id = Place_Type::SCHOOL;
  char place_subtype = Place::SUBTYPE_NONE;
  char new_label[FRED_STRING_SIZE];
  char label[FRED_STRING_SIZE];
  long long int admin_code = 0;
  double lat;
  double lon;
  double elevation = 0;
  long long int sp_id = 0;

  FILE* fp = Utils::fred_open_file(location_file);
  if(fp == nullptr) {
    return;
  }

  // skip header line
  fgets(label, FRED_STRING_SIZE, fp);

  // read first data line
  strcpy(line, "");
  fgets(line, FRED_STRING_SIZE, fp);
  int items = sscanf(line, "%s %lld %lf %lf %lf", label, &admin_code, &lat, &lon, &elevation);
  
  while(4 <= items) {

    // if(Place::country_is_usa) {
    // convert county admin code to block group code
    admin_code *= 10000000;
    // }

    sscanf(label, "%lld", &sp_id);
    if(!Group::sp_id_exists(sp_id)) {
      snprintf(new_label, FRED_STRING_SIZE, "S-%s", label);
      Place* place = Place::add_place(new_label, type_id, place_subtype, lon, lat, elevation, admin_code);

      place->set_sp_id(sp_id);
    }

    // read next data line
    strcpy(line, "");
    fgets(line, FRED_STRING_SIZE, fp);
    items = sscanf(line, "%s %lld %lf %lf %lf", label, &admin_code, &lat, &lon, &elevation);
  }
  fclose(fp);
  return;
}

/**
 * Reads in group quarters to the simulation from the household file at the given location file path. Assigns 
 * a Synthetic Population ID.
 *
 * @param location_file the file path
 */
void Place::read_group_quarters_file(char* location_file) {
  char line[FRED_STRING_SIZE];

  // data to fill in from input file
  char place_subtype = Place::SUBTYPE_NONE;
  char gq_type;
  char id[FRED_STRING_SIZE];
  char label[FRED_STRING_SIZE];
  long long int admin_code = 0;
  double lat;
  double lon;
  double elevation = 0;
  int capacity;
  int income;
  long long int sp_id = 0;

  FILE* fp = Utils::fred_open_file(location_file);
  if(fp == nullptr) {
    return;
  }

  // skip header line
  fgets(label, FRED_STRING_SIZE, fp);

  // read first data line
  strcpy(line, "");
  fgets(line, FRED_STRING_SIZE, fp);
  int items = sscanf(line, "%s %c %lld %d %lf %lf %lf", id, &gq_type, &admin_code, &capacity, &lat, &lon, &elevation);
  
  while(6 <= items) {
    update_geo_boundaries(lat, lon);

    // set number of units and subtype for this group quarters
    int number_of_units = 0;
    if(gq_type == 'C') {
      number_of_units = capacity / Place::College_dorm_mean_size;
      place_subtype = Place::SUBTYPE_COLLEGE;
      income = Place_Type::get_household_place_type()->get_income_second_quartile();
    }
    if(gq_type == 'M') {
      number_of_units = capacity / Place::Military_barracks_mean_size;
      place_subtype = Place::SUBTYPE_MILITARY_BASE;
      income = Place_Type::get_household_place_type()->get_income_second_quartile();
    }
    if(gq_type == 'P') {
      number_of_units = capacity / Place::Prison_cell_mean_size;
      place_subtype = Place::SUBTYPE_PRISON;
      income = Place_Type::get_household_place_type()->get_income_first_quartile();
    }
    if(gq_type == 'N') {
      number_of_units = capacity / Place::Nursing_home_room_mean_size;
      place_subtype = Place::SUBTYPE_NURSING_HOME;
      income = Place_Type::get_household_place_type()->get_income_first_quartile();
    }
    if(number_of_units == 0) {
      number_of_units = 1;
    }

    // add a workplace for this group quarters
    snprintf(label, FRED_STRING_SIZE, "GW-%s", id);
    Place::place_logger->debug("Adding GQ Workplace {:s} subtype {:c}", label, place_subtype);
    Place* workplace = Place::add_place(label, Place_Type::WORKPLACE, place_subtype, lon, lat, elevation, admin_code);
    sscanf(id, "%lld", &sp_id);
    sp_id *= 10000;
    workplace->set_sp_id(sp_id);
    
    // add as household
    snprintf(label, FRED_STRING_SIZE, "GH-%s",id);
    Place::place_logger->debug("Adding GQ Household {:s} subtype {:c}", label, place_subtype);
    Household *place = static_cast<Household *>(Place::add_place(label, Place_Type::HOUSEHOLD, place_subtype, lon, lat,
        elevation, admin_code));
    place->set_group_quarters_units(number_of_units);
    place->set_group_quarters_workplace(workplace);
    place->set_income(income);
    sp_id += 1;
    place->set_sp_id(sp_id);

    // add this to the list of externally defined gq's
    Place::gq.push_back(place);

    // generate additional household units associated with this group quarters
    for(int i = 1; i < number_of_units; ++i) {
      snprintf(label, FRED_STRING_SIZE, "GH-%s-%03d", id, i+1);
      Household *place = static_cast<Household *>(Place::add_place(label, Place_Type::HOUSEHOLD, place_subtype, lon, lat,
          elevation, admin_code));
      Place::place_logger->debug("Adding GQ Household {:s} subtype {:c} out of {:d} units", label, place_subtype, 
          number_of_units);
      place->set_income(income);
      sp_id += 1;
      place->set_sp_id(sp_id);
    }

    // read next data line
    strcpy(line, "");
    fgets(line, FRED_STRING_SIZE, fp);
    items = sscanf(line, "%s %c %lld %d %lf %lf %lf", id, &gq_type, &admin_code, &capacity, &lat, &lon, &elevation);
  }
  fclose(fp);
  return;
}

/**
 * Reads in places to the simulation as the specified Place_Type from the location file at the given location file path. 
 * Assigns a Synthetic Population ID.
 *
 * @param location_file the file path
 */
void Place::read_place_file(char* location_file, int type_id) {
  char line[FRED_STRING_SIZE];

  // data to fill in from input file
  char place_subtype = Place::SUBTYPE_NONE;
  char label[FRED_STRING_SIZE];
  double lat;
  double lon;
  long long int admin_code = 0;
  double elevation = 0;
  long long int sp_id = 0;

  FILE* fp = Utils::fred_open_file(location_file);
  if(fp == nullptr) {
    return;
  }

  // skip header line
  fgets(label, FRED_STRING_SIZE, fp);

  // read first data line
  strcpy(line, "");
  fgets(line, FRED_STRING_SIZE, fp);
  int items = sscanf(line, "%lld %lf %lf %lf", &sp_id, &lat, &lon, &elevation);
  if(sp_id == 0) {
    sp_id = Place_Type::get_place_type(type_id)->get_next_sp_id();
  }
  
  while(4 <= items) {
    snprintf(label, FRED_STRING_SIZE, "%s-%lld", Place_Type::get_place_type_name(type_id).c_str(), sp_id);
    Place::place_logger->info("{:s} {:d} {:f} {:f} {:f}", label, sp_id, lat, lon, elevation);
    if(!Group::sp_id_exists(sp_id)) {
      Place* place = Place::add_place(label, type_id, place_subtype, lon, lat, elevation, admin_code);
      place->set_sp_id(sp_id);
    }
    // read next data line
    strcpy(line, "");
    fgets(line, FRED_STRING_SIZE, fp);
    items = sscanf(line, "%lld %lf %lf %lf", &sp_id, &lat, &lon, &elevation);
    if(sp_id == 0) {
      sp_id = Place_Type::get_place_type(type_id)->get_next_sp_id();
    }
  }
  fclose(fp);
  return;
}

/**
 * Adds all Household objects to their corresponding Block_Group.
 */
void Place::setup_block_groups() {

  Place::place_logger->info("setup_block_groups BLOCK");

  int size = Place::get_number_of_households();
  for(int p = 0; p < size; ++p) {
    Place* place = Place_Type::get_household_place_type()->get_place(p);
    long long int admin_code = place->get_admin_code();

    // get the block group associated with this code, creating a new one if necessary
    Block_Group* block_group = Block_Group::get_block_group_with_admin_code(admin_code);
    block_group->add_household(place);

  }

  Place::place_logger->info("setup_block_groups finished BLOCK");
}

/**
 * Prepares varying places for the simulation.
 */
void Place::prepare_places() {

  Place::place_logger->info("prepare_places entered");

  // this is needed when reading in place files
  // delete_place_label_map();

  for(int i = 0; i < Place_Type::get_number_of_place_types(); ++i) {
    int n = Place_Type::get_place_type(i)->get_number_of_places();
    for(int p = 0; p < n; ++p) {
      Place* place = Place_Type::get_place_type(i)->get_place(p);
      place->prepare();
    }
  }
  
  Global::Neighborhoods->prepare();

  // create lists of school by grade
  int number_of_schools = Place::get_number_of_schools();
  for(int p = 0; p < number_of_schools; ++p) {
    Place* school = Place::get_school(p);
    for(int grade = 0; grade < Global::GRADES; ++grade) {
      if(school->get_original_size_by_age(grade) > 0) {
        Place::schools_by_grade[grade].push_back(school);
      }
    }
  }

  for(int grade = 0; grade < Global::GRADES; ++grade) {
    int size = Place::schools_by_grade[grade].size();
    std::stringstream ss;
    ss << fmt::format("GRADE = {:d} SCHOOLS = {:d}: ", grade, size);
    for(int i = 0; i < size; ++i) {
      ss << fmt::format("{:s} ", Place::schools_by_grade[grade][i]->get_label());
    }
    Place::place_logger->trace("<{:s}, {:d}>: {:s}", __FILE__, __LINE__, ss.str());
  }
  
  print_status_of_schools(0);

  if(Global::Enable_Visualization_Layer) {
    // add list of counties to visualization data directory
    char filename[256];
    snprintf(filename, 256, "%s/COUNTIES", Global::Visualization_directory);
    FILE* fp = fopen(filename, "w");
    for(int i = 0; i < County::get_number_of_counties(); ++i) {
      if(Place::country_is_usa) {
        fprintf(fp, "%05d\n", static_cast<int>(County::get_county_with_index(i)->get_admin_division_code()));
      } else {
        fprintf(fp, "%05d\n", static_cast<int>(County::get_county_with_index(i)->get_admin_division_code()));
      }
    }
    fclose(fp);
    
    // add list of census_tracts to visualization data directory
    snprintf(filename, 256, "%s/CENSUS_TRACTS", Global::Visualization_directory);
    fp = fopen(filename, "w");
    for(int i = 0; i < Census_Tract::get_number_of_census_tracts(); ++i) {
      long long int admin_code = Census_Tract::get_census_tract_with_index(i)->get_admin_division_code();
      fprintf(fp, "%011lld\n", admin_code);
    }
    fclose(fp);
    
    // add geographical bounding box to visualization data directory
    snprintf(filename, 256, "%s/BBOX", Global::Visualization_directory);
    fp = fopen(filename, "w");
    fprintf(fp, "ymin = %0.6f\n", Place::min_lat);
    fprintf(fp, "xmin = %0.6f\n", Place::min_lon);
    fprintf(fp, "ymax = %0.6f\n", Place::max_lat);
    fprintf(fp, "xmax = %0.6f\n", Place::max_lon);
    fclose(fp);
  }

  if(Place_Type::get_school_place_type()->is_vaccination_rate_enabled()) {
    for(int p = 0; p < Place::get_number_of_households(); ++p) {
      static_cast<Household*>(Place::get_household(p))->set_household_vaccination();
    }
  }

  // log state info
  for(int i = 0; i < State::get_number_of_states(); ++i) {
    int admin_code = static_cast<int>(State::get_state_with_index(i)->get_admin_division_code());
    int hh = State::get_state_with_index(i)->get_number_of_households();
    int pop = State::get_state_with_index(i)->get_population_size();
    if(Place::country_is_usa) {
      Place::place_logger->info("STATE[{:d}] = {:0}2d  hh = {:d} pop = {:d}", i, admin_code, hh, pop);
    } else {
      Place::place_logger->info("STATE[{:d}] = {:d}  hh = {:d}  pop = {:d}", i, admin_code, hh, pop);
    }
  }

  // log county info
  for(int i = 0; i < County::get_number_of_counties(); ++i) {
    int admin_code = (int) County::get_county_with_index(i)->get_admin_division_code();
    int hh = County::get_county_with_index(i)->get_number_of_households();
    int pop = County::get_county_with_index(i)->get_population_size();
    if(Place::country_is_usa) {
      Place::place_logger->info("COUNTIES[{:d}] = {:0}5d  hh = {:d} pop = {:d}", i, admin_code, hh, pop);
    } else {
      Place::place_logger->info("COUNTIES[{:d}] = {:0}5d  hh = {:d}  pop = {:d}", i, admin_code, hh, pop);
    }
  }

  // log census tract info
  for(int i = 0; i < Census_Tract::get_number_of_census_tracts(); ++i) {
    long long int admin_code = Census_Tract::get_census_tract_with_index(i)->get_admin_division_code();
    int hh = Census_Tract::get_census_tract_with_index(i)->get_number_of_households();
    int pop = Census_Tract::get_census_tract_with_index(i)->get_population_size();
    Place::place_logger->info("CENSUS_TRACTS[{:d}] = {:0}11ld  households = {:d}  pop = {:d}", i, 
        admin_code, hh, pop);
  }
}
/**
 * Prints status of schools in the simulation for the given day, including the label, original size by age, 
 * current size by age, and the difference between current and original sizes.
 *
 * @param day the day
 */
void Place::print_status_of_schools(int day) {
  int students_per_grade[Global::GRADES];
  for(int i = 0; i < Global::GRADES; ++i) {
    students_per_grade[i] = 0;
  }

  int number_of_schools = get_number_of_schools();
  for(int p = 0; p < number_of_schools; ++p) {
    Place *school = get_school(p);
    for(int grade = 0; grade < Global::GRADES; ++grade) {
      int total = school->get_original_size();
      int orig = school->get_original_size_by_age(grade);
      int now = school->get_size_by_age(grade);
      students_per_grade[grade] += now;
      if(total > 1500 && orig > 0) {
        Place::place_logger->debug("{:s} GRADE {:d} ORIG {:d} NOW {:d} DIFF {:d}", 
            school->get_label(), grade, school->get_original_size_by_age(grade), 
            school->get_size_by_age(grade), 
            school->get_size_by_age(grade) - school->get_original_size_by_age(grade));
      }
    }
  }

  int year = day / 365;
  int total_students = 0;
  for(int i = 0; i < Global::GRADES; ++i) {
    Place::place_logger->debug("YEAR {:d} GRADE {:d} STUDENTS {:d}", year, i, students_per_grade[i]);
    total_students += students_per_grade[i];
  }
  Place::place_logger->debug("YEAR {:d} TOTAL_STUDENTS {:d}", year, total_students);
}

/**
 * Resets the current daily patient count for all hospitals, if
 * hospitals are enabled.
 *
 * @param day _UNUSED_
 */
void Place::update(int day) {
  Place::place_logger->info("update places entered");
  
  if(Global::Enable_Hospitals) {
    for(int p = 0; p < get_number_of_hospitals(); ++p) {
      Place::get_hospital(p)->reset_current_daily_patient_count();
    }
  }

  Place::place_logger->info("update places finished");
}

/**
 * Gets the Household with the specified label from the static household label map.
 *
 * @param s the label
 * @return the household
 */
Place* Place::get_household_from_label(const char* s) {
  assert(Place::household_label_map != nullptr);
  if(s[0] == '\0' || strcmp(s, "X") == 0) {
    return nullptr;
  }
  LabelMapT::iterator itr;
  std::string str(s);
  itr = Place::household_label_map->find(str);
  if(itr != Place::household_label_map->end()) {
    return get_household(itr->second);
  } else {
    Place::place_logger->error("Help!  can't find household with label = {:s}", str.c_str());
    return nullptr;
  }
}

/**
 * Gets the school with the specified label from the static school label map.
 *
 * @param s the label
 * @return the school
 */
Place* Place::get_school_from_label(const char* s) {
  assert(Place::school_label_map != nullptr);
  if(s[0] == '\0' || strcmp(s, "X") == 0 || strcmp(s, "S-X") == 0) {
    return nullptr;
  }
  LabelMapT::iterator itr;
  std::string str(s);
  itr = Place::school_label_map->find(str);
  if(itr != Place::school_label_map->end()) {
    return get_school(itr->second);
  } else {
    Place::place_logger->error("Help!  can't find school with label = {:s}", str.c_str());
    return nullptr;
  }
}

/**
 * Gets the workplace with the specified label from the static workplace label map.
 *
 * @param s the label
 * @return the workplace
 */
Place* Place::get_workplace_from_label(const char* s) {
  assert(Place::workplace_label_map != nullptr);
  if(s[0] == '\0' || strcmp(s, "X") == 0 || strcmp(s, "W-X") == 0) {
    return nullptr;
  }
  LabelMapT::iterator itr;
  std::string str(s);
  itr = Place::workplace_label_map->find(str);
  if(itr != Place::workplace_label_map->end()) {
    return Place::get_workplace(itr->second);
  } else {
    Place::place_logger->error("Help!  can't find workplace with label = {:s}", str.c_str());
    return nullptr;
  }
}

/**
 * Creates a Place with the specified properties and adds it to its corresponding label map.
 *
 * @param label the label
 * @param type_id the place type ID
 * @param subtype the subtype
 * @param lon the longitude
 * @param lat the latitude
 * @param elevation the elevation
 * @param admin_code the admin code
 * @return the created place
 */
Place* Place::add_place(char* label, int type_id, char subtype, fred::geo lon, fred::geo lat, double elevation, long long int admin_code) {

  Place::place_logger->info("add_place {:s} type {:d} = {:s} subtype {:c}", label, type_id, 
      Place_Type::get_place_type_name(type_id).c_str(), subtype);

  std::string label_str;
  label_str.assign(label);

  if(Place::country_is_usa == false) {
    if(type_id == Place_Type::HOUSEHOLD) {
      if(Place::household_label_map->find(label_str) != Place::household_label_map->end()) {
        Place::place_logger->warn("duplicate household label found: {:s}", label);
        return get_household_from_label(label);
      }
    }
    if(type_id == Place_Type::SCHOOL) {
      if(Place::school_label_map->find(label_str) != Place::school_label_map->end()) {
        Place::place_logger->warn("duplicate school label found: {:s}", label);
        return get_school_from_label(label);
      }
    }
    if(type_id == Place_Type::WORKPLACE) {
      if(Place::workplace_label_map->find(label_str) != Place::workplace_label_map->end()) {
        Place::place_logger->warn("duplicate workplace label found: {:s}", label);
        return Place::get_workplace_from_label(label);
      }
    }
  }

  Place* place = nullptr;
  if(type_id == Place_Type::HOUSEHOLD) {
    place = new Household(label, subtype, lon, lat);
    Place_Type::get_place_type(type_id)->add_place(place);
    Place::household_label_map->insert(std::make_pair(label_str, Place::get_number_of_households() - 1));
  } else if(type_id == Place_Type::WORKPLACE) {
    place = new Place(label, type_id, lon, lat);
    Place_Type::get_place_type(type_id)->add_place(place);
    Place::workplace_label_map->insert(std::make_pair(label_str, Place::get_number_of_workplaces() - 1));
  } else if(type_id == Place_Type::SCHOOL) {
    place = new Place(label, type_id, lon, lat);
    Place_Type::get_place_type(type_id)->add_place(place);
    Place::school_label_map->insert(std::make_pair(label_str, Place::get_number_of_schools() - 1));
  } else if(type_id == Place_Type::HOSPITAL) {
    place = new Hospital(label, subtype, lon, lat);
    Place_Type::get_place_type(type_id)->add_place(place);
  } else {
    place = new Place(label, type_id, lon, lat);
    Place_Type::get_place_type(type_id)->add_place(place);
  }

  int id = Place::get_new_place_id();
  place->set_id(id);
  place->set_subtype(subtype);
  place->set_admin_code(admin_code);
  place->set_elevation(elevation);
  Place::save_place(place);
    
  Place::place_logger->info(
      "add_place finished id {:d} lab {:s} type {:d} = {:s} subtype {:c} lat {:f} lon {:f} admin {:d} elev {:f}",
      place->get_id(), place->get_label(), place->get_type_id(),
      Place_Type::get_place_type_name(type_id).c_str(), place->get_subtype(),
      place->get_latitude(), place->get_longitude(), place->get_admin_code(), place->get_elevation());

  return place;
}

/**
 * Sets up group quarters by assigning residents into individual units.
 */
void Place::setup_group_quarters() {

  Place::place_logger->info("setup group quarters entered");

  int p = 0;
  int units = 0;
  int num_households = get_number_of_households();
  while(p < num_households) {
    Household* house = Place::get_household(p++);
    Household* new_house;
    if(house->is_group_quarters()) {
      int gq_size = house->get_size();
      int gq_units = house->get_group_quarters_units();
      Place::place_logger->info(
          "GQ_setup: house {:d} label {:s} subtype {:c} initial size {:d} units {:d}", 
          p, house->get_label(), house->get_subtype(), gq_size, gq_units);
      int units_filled = 1;
      if(gq_units > 1) {
        person_vector_t Housemates;
        Housemates.clear();
        for(int i = 0; i < gq_size; ++i) {
          Person* person = house->get_member(i);
          Housemates.push_back(person);
        }
        int min_per_unit = gq_size / gq_units;
        int larger_units = gq_size - min_per_unit * gq_units;
        int smaller_units = gq_units - larger_units;
        Place::place_logger->info(
            "GQ min_per_unit {:d} smaller = {:d}  larger = {:d} total = {:d}  orig = {:d}", 
            min_per_unit, smaller_units, larger_units, 
            smaller_units*min_per_unit + larger_units*(min_per_unit+1), gq_size);
        int next_person = min_per_unit;
        for(int i = 1; i < smaller_units; ++i) {
          // assert(units_filled < gq_units);
          new_house = Place::get_household(p++);
          Place::place_logger->info("GQ smaller new_house {:s} unit {:d} subtype {:c} size {:d}",
              new_house->get_label(), i, new_house->get_subtype(), new_house->get_size());
          for(int j = 0; j < min_per_unit; ++j) {
            Person* person = Housemates[next_person++];
            person->change_household(new_house);
          }
          Place::place_logger->info("GQ smaller new_house {:s} subtype {:c} size {:d}",
              new_house->get_label(), new_house->get_subtype(), new_house->get_size());
          units_filled++;
          Place::place_logger->info("GQ size of smaller unit {:s} = {:d} remaining in main house {:d}",
              new_house->get_label(), new_house->get_size(), house->get_size());
        }
        for(int i = 0; i < larger_units; ++i) {
          new_house = Place::get_household(p++);
          for(int j = 0; j < min_per_unit + 1; ++j) {
            Person* person = Housemates[next_person++];
            person->change_household(new_house);
          }
          units_filled++;
        }
      }
      units += units_filled;
    }
  }
  Place::place_logger->info("setup group quarters finished, units = {:d}", units);
}

/**
 * Sets up households by locating the head of the household and setting household structure.
 */
void Place::setup_households() {

  Place::place_logger->info("setup households entered");

  int num_households = Place::get_number_of_households();
  for(int p = 0; p < num_households; ++p) {
    Household* house = Place::get_household(p);
    if(house->get_size() == 0) {
      Place::place_logger->warn("Warning: house {:d} label {:s} has zero size.", 
          house->get_id(), house->get_label());
      continue;
    }

    // ensure that each household has an identified householder
    Person* person_with_max_age = nullptr;
    Person* head_of_household = nullptr;
    int max_age = -99;
    for(int j = 0; j < house->get_size() && head_of_household == nullptr; ++j) {
      Person* person = house->get_member(j);
      assert(person != nullptr);
      if(person->is_householder()) {
        head_of_household = person;
        continue;
      } else {
        int age = person->get_age();
        if(age > max_age) {
          max_age = age;
          person_with_max_age = person;
        }
      }
    }

    if(head_of_household == nullptr) {
      assert(person_with_max_age != nullptr);
      person_with_max_age->make_householder();
      head_of_household = person_with_max_age;
    }
    assert(head_of_household != nullptr);

    // make sure everyone know who's the head
    for(int j = 0; j < house->get_size(); ++j) {
      Person* person = house->get_member(j);
      if(person != head_of_household && person->is_householder()) {
        person->set_household_relationship(Household_Relationship::HOUSEMATE);
      }
    }
    assert(head_of_household != nullptr);
    Place::place_logger->debug(
        "HOLDER: house {:d} label {:s} is_group_quarters {:d} householder {:d} age {:d}", 
        house->get_id(), house->get_label(), house->is_group_quarters() ? 1 : 0, head_of_household->get_id(),
        head_of_household->get_age());

    // setup household structure type
    house->set_household_structure();
    house->set_orig_household_structure();
  }

  Place::place_logger->info("setup households finished");
}

/**
 * Gets the max size of this place's Place_Type.
 *
 * @return the max size
 */
int Place::get_max_size() {
  return Place_Type::get_place_type(type_id)->get_max_size();
}

/**
 * Sets up partitions for all place types.
 */
void Place::setup_partitions() {
  Place::place_logger->info("setup partitions entered");
  int number_of_place_types = Place_Type::get_number_of_place_types();
  for(int i = 0; i < number_of_place_types; ++i) {
    Place_Type::get_place_type(i)->setup_partitions();
  }
  Place::place_logger->info("setup partitions finished");
}

/**
 * Reassigns workers to schools, hospitals, and group quarters, depending on what is enabled.
 */
void Place::reassign_workers() {

  if(Global::Assign_Teachers) {
    Place::reassign_workers_to_schools();
  }

  if(Global::Enable_Hospitals) {
    Place::reassign_workers_to_hospitals();
  }

  if(Global::Enable_Group_Quarters) {
    Place::reassign_workers_to_group_quarters(Place::SUBTYPE_COLLEGE, Place::College_fixed_staff,
        Place::College_resident_to_staff_ratio);
    Place::reassign_workers_to_group_quarters(Place::SUBTYPE_PRISON, Place::Prison_fixed_staff,
        Place::Prison_resident_to_staff_ratio);
    Place::reassign_workers_to_group_quarters(Place::SUBTYPE_MILITARY_BASE, Place::Military_fixed_staff,
        Place::Military_resident_to_staff_ratio);
    Place::reassign_workers_to_group_quarters(Place::SUBTYPE_NURSING_HOME, Place::Nursing_home_fixed_staff,
        Place::Nursing_home_resident_to_staff_ratio);
  }

  Utils::fred_print_lap_time("reassign workers");
}

/**
 * For each school, reassign workers from nearby workplaces to teachers at the school in accordance with staff data retrieved 
 * from http://www.statemaster.com/graph/edu_ele_sec_pup_rat-elementary-secondary-pupil-teacher-ratio".
 */
void Place::reassign_workers_to_schools() {

  // staff data from:
  // http://www.statemaster.com/graph/edu_ele_sec_pup_rat-elementary-secondary-pupil-teacher-ratio
  int fixed_staff = Place::School_fixed_staff;
  double staff_ratio = Place::School_student_teacher_ratio;

  int number_of_schools = Place::get_number_of_schools();

  Place::place_logger->info(
      "reassign workers to schools entered. schools = {:d} fixed_staff = {:d} staff_ratio = {:f}",
      number_of_schools, fixed_staff, staff_ratio);

  for(int p = 0; p < number_of_schools; ++p) {
    Place* school = Place::get_school(p);
    fred::geo lat = school->get_latitude();
    fred::geo lon = school->get_longitude();
    double x = Geo::get_x(lon);
    double y = Geo::get_y(lat);
    Place::place_logger->debug("Reassign teachers to school {:s} in county {:d} at ({:f},{:f})",
        school->get_label(), school->get_county_admin_code(), x, y);
    
    // ignore school if it is outside the region
    Regional_Patch* regional_patch = Global::Simulation_Region->get_patch(lat, lon);
    if(regional_patch == nullptr) {
      Place::place_logger->info("school {:s} OUTSIDE_REGION lat {:f} lon {:f}",
          school->get_label(), lat, lon);
      continue;
    }

    // target staff size
    int n = school->get_original_size();
    int staff = fixed_staff;
    if(staff_ratio > 0.0) {
      staff += (0.5 + static_cast<double>(n) / staff_ratio);
    }
    Place::place_logger->debug("school {:s} students {:d} fixed_staff = {:d} tot_staff = {:d}",
        school->get_label(), n, fixed_staff, staff);

    Place* nearby_workplace = regional_patch->get_nearby_workplace(school, staff);
    if(nearby_workplace != nullptr) {
      // make all the workers in selected workplace teachers at the nearby school
      nearby_workplace->turn_workers_into_teachers(school);
    } else {
      Place::place_logger->info(
          "NO NEARBY_WORKPLACE FOUND FOR SCHOOL {:s} in county {:d} at lat {:f} lon {:f}",
          school->get_label(), school->get_county_admin_code(), lat, lon);
    }
  }
}

/**
 * For each hospital, reassign workers from nearby workplaces to workers at the hospital in accordance with staff data.
 */
void Place::reassign_workers_to_hospitals() {

  int number_places = get_number_of_hospitals();
  Place::place_logger->info("reassign workers to hospitals entered. places = {:d}", number_places);

  int fixed_staff = Place::Hospital_fixed_staff;
  double staff_ratio = (1.0 / Place::Hospital_worker_to_bed_ratio);

  for(int p = 0; p < number_places; ++p) {
    Hospital* hosp = Place::get_hospital(p);
    fred::geo lat = hosp->get_latitude();
    fred::geo lon = hosp->get_longitude();
    double x = Geo::get_x(lon);
    double y = Geo::get_y(lat);
    Place::place_logger->info("Reassign workers to hospital {:s} in county {:d} at ({:f},{:f})", 
        hosp->get_label(), hosp->get_county_admin_code(), x, y);

    // ignore hospital if it is outside the region
    Regional_Patch* regional_patch = Global::Simulation_Region->get_patch(lat, lon);
    if(regional_patch == nullptr) {
      Place::place_logger->info("hospital OUTSIDE_REGION lat {:f} lon {:f}", lat, lon);
      continue;
    }

    // target staff size
    int n = hosp->get_employee_count(); // From the input file
    Place::place_logger->debug("Size {:d}", n);

    int staff = fixed_staff;
    if(staff_ratio > 0.0) {
      staff += (0.5 + static_cast<double>(n) / staff_ratio);
    }
    
    Place* nearby_workplace = regional_patch->get_nearby_workplace(hosp, staff);
    if(nearby_workplace != nullptr) {
      // make all the workers in selected workplace as workers in the target place
      nearby_workplace->reassign_workers(hosp);
    } else {
      Place::place_logger->info(
          "NO NEARBY_WORKPLACE FOUND for hospital {:s} in county {:d} at lat {:f} lon {:f}",
          hosp->get_label(), hosp->get_county_admin_code(), lat, lon);
    }
  }
}

/**
 * For each workplace of the specified subtype, reassign workers from nearby workplaces to workers at that workplace 
 * in accordance with staff data given by the fixed staff and resident to staff ratio data.
 *
 * @param subtype the subtype
 * @param fixed_staff the fixed staff value
 * @param resident_to_staff_ratio the resident to staff ratio value
 */
void Place::reassign_workers_to_group_quarters(char subtype, int fixed_staff, double resident_to_staff_ratio) {
  int number_places = Place::get_number_of_workplaces();
  Place::place_logger->info(
      "reassign workers to group quarters subtype {:c} entered. total workplaces = {:d}", 
      subtype, number_places);
  for(int p = 0; p < number_places; ++p) {
    Place* place = Place::get_workplace(p);
    if(place->get_subtype() == subtype) {
      fred::geo lat = place->get_latitude();
      fred::geo lon = place->get_longitude();
      // target staff size
      Place::place_logger->debug("Size {:d} ", place->get_size());
      int staff = fixed_staff;
      if(resident_to_staff_ratio > 0.0) {
        staff += 0.5 + static_cast<double>(place->get_size()) / resident_to_staff_ratio;
      }

      Place::place_logger->info(
          "REASSIGN WORKERS to GQ {:s} subtype {:c} target staff {:d} at ({:f},{:f})", 
          place->get_label(), subtype, staff, lat, lon);

      // ignore place if it is outside the region
      Regional_Patch* regional_patch = Global::Simulation_Region->get_patch(lat, lon);
      if(regional_patch == nullptr) {
        Place::place_logger->info(
            "REASSIGN WORKERS to place GQ {:s} subtype {:c} FAILED -- OUTSIDE_REGION lat {:f} lon {:f}", 
            place->get_label(), subtype, lat, lon);
        continue;
      }

      Place* nearby_workplace = regional_patch->get_nearby_workplace(place, staff);
      if(nearby_workplace != nullptr) {
        // make all the workers in selected workplace as workers in the target place
        Place::place_logger->info(
            "REASSIGN WORKERS: NEARBY_WORKPLACE FOUND {:s} for GQ {:s} subtype {:c} at lat {:f} lon {:f}",
            nearby_workplace->get_label(), place->get_label(), subtype, lat, lon);
        nearby_workplace->reassign_workers(place);
      } else {
        Place::place_logger->info(
            "REASSIGN WORKERS: NO NEARBY_WORKPLACE FOUND for GQ {:s} subtype {:c} at lat {:f} lon {:f}", 
            place->get_label(), subtype, lat, lon);
      }
    }
  }
}

/**
 * Gets a random Household.
 *
 * @return the household
 */
Place* Place::get_random_household() {
  int size = Place::get_number_of_households();
  if(size > 0) {
    return Place::get_household(Random::draw_random_int(0, size - 1));
  } else {
    return nullptr;
  }
}

/**
 * Gets a random workplace.
 *
 * @return the workplace
 */
Place* Place::get_random_workplace() {
  int size = Place::get_number_of_workplaces();
  if(size > 0) {
    return Place::get_workplace(Random::draw_random_int(0, size - 1));
  } else {
    return nullptr;
  }
}

/**
 * Gets a random school at the specified grade.
 *
 * @param grade the grade
 * @return the school
 */
Place* Place::get_random_school(int grade) {
  int size = static_cast<int>(Place::schools_by_grade[grade].size());
  if(size > 0) {
    return Place::schools_by_grade[grade][Random::draw_random_int(0, size - 1)];
  } else {
    return nullptr;
  }
}

/**
 * Prints data on the household size distribution to the specified directory with the given date string and run number.
 *
 * @param dir the directory
 * @param date_string the date string
 * @param run the run number
 */
void Place::print_household_size_distribution(char* dir, char* date_string, int run) {
  FILE* fp;
  int count[11];
  double pct[11];
  char filename[FRED_STRING_SIZE];
  snprintf(filename, FRED_STRING_SIZE, "%s/household_size_dist_%s.%02d", dir, date_string, run);
  Place::place_logger->info("print_household_size_dist entered, filename = {:s}", filename);
  for(int i = 0; i < 11; ++i) {
    count[i] = 0;
  }
  int total = 0;
  int number_households = Place::get_number_of_households();
  for(int p = 0; p < number_households; ++p) {
    int n = get_household(p)->get_size();
    if(n < 11) {
      ++count[n];
    } else {
      ++count[10];
    }
    ++total;
  }
  fp = fopen(filename, "w");
  for(int i = 0; i < 11; ++i) {
    pct[i] = (100.0 * count[i]) / number_households;
    fprintf(fp, "size %d count %d pct %f\n", i * 5, count[i], pct[i]);
  }
  fclose(fp);
}

/**
 * Deletes the household, school, and/or workplace label maps if they are being used.
 */
void Place::delete_place_label_map() {
  if(Place::household_label_map) {
    delete Place::household_label_map;
    Place::household_label_map = nullptr;
  }
  if(Place::school_label_map) {
    delete Place::school_label_map;
    Place::school_label_map = nullptr;
  }
  if(Place::workplace_label_map) {
    delete Place::workplace_label_map;
    Place::workplace_label_map = nullptr;
  }
}

/**
 * _UNUSED_
 */
void Place::finish() {
  return;
}

/**
 * For each Household, set the current size and target size in the parameters for the index of the household. The target size will 
 * be the household's original size.
 *
 * @param target_size the target size integer array
 * @param current_size the current size integer array
 * @return the number of households
 */
int Place::get_housing_data(int* target_size, int* current_size) {
  int num_households = Place::get_number_of_households();
  for(int i = 0; i < num_households; ++i) {
    Household* h = Place::get_household(i);
    current_size[i] = h->get_size();
    target_size[i] = h->get_original_size();
  }
  return num_households;
}

/**
 * Swaps residents of the Household at the first index with residents of the Household at the second index.
 *
 * @param house_index1 the first house index
 * @param house_index2 the second house index
 */
void Place::swap_houses(int house_index1, int house_index2) {

  Household* h1 = Place::get_household(house_index1);
  Household* h2 = Place::get_household(house_index2);
  if(h1 == nullptr || h2 == nullptr) {
    return;
  }

  Place::place_logger->info(
      "HOUSING: swapping house {:s} with {:d} beds and {:d} occupants with {:s} with {:d} beds and {:d} occupants",
      h1->get_label(), h1->get_original_size(), h1->get_size(), h2->get_label(), h2->get_original_size(), h2->get_size());

  // get pointers to residents of house h1
  person_vector_t temp1;
  temp1.clear();
  person_vector_t Housemates1 = h1->get_inhabitants();
  for(person_vector_t::iterator itr = Housemates1.begin(); itr != Housemates1.end(); ++itr) {
    temp1.push_back(*itr);
  }

  // get pointers to residents of house h2
  person_vector_t temp2;
  temp2.clear();
  person_vector_t Housemates2 = h2->get_inhabitants();
  for(person_vector_t::iterator itr = Housemates2.begin(); itr != Housemates2.end(); ++itr) {
    temp2.push_back(*itr);
  }

  // move first group into house h2
  for(person_vector_t::iterator itr = temp1.begin(); itr != temp1.end(); ++itr) {
    (*itr)->change_household(h2);
  }

  // move second group into house h1
  for(person_vector_t::iterator itr = temp2.begin(); itr != temp2.end(); ++itr) {
    (*itr)->change_household(h1);
  }

  Place::place_logger->info(
      "HOUSING: swapped house {:s} with {:d} beds and {:d} occupants with {:s} with {:d} beds and {:d} occupants",
      h1->get_label(), h1->get_original_size(), h1->get_size(), h2->get_label(), h2->get_original_size(), h2->get_size());
}

/**
 * Swaps residents of the first Household with residents of the second Household.
 *
 * @param house_index1 the first house index
 * @param house_index2 the second house index
 */
void Place::swap_houses(Household* h1, Household* h2) {

  if(h1 == nullptr || h2 == nullptr)
    return;

  Place::place_logger->info(
      "HOUSING: swapping house {:s} with {:d} beds and {:d} occupants with {:s} with {:d} beds and {:d} occupants",
      h1->get_label(), h1->get_original_size(), h1->get_size(), h2->get_label(), h2->get_original_size(), h2->get_size());

  // get pointers to residents of house h1
  person_vector_t temp1;
  temp1.clear();
  person_vector_t Housemates1 = h1->get_inhabitants();
  for(person_vector_t::iterator itr = Housemates1.begin(); itr != Housemates1.end(); ++itr) {
    temp1.push_back(*itr);
  }

  // get pointers to residents of house h2
  person_vector_t temp2;
  temp2.clear();
  person_vector_t Housemates2 = h2->get_inhabitants();
  for(person_vector_t::iterator itr = Housemates2.begin(); itr != Housemates2.end(); ++itr) {
    temp2.push_back(*itr);
  }

  // move first group into house h2
  for(person_vector_t::iterator itr = temp1.begin(); itr != temp1.end(); ++itr) {
    (*itr)->change_household(h2);
  }

  // move second group into house h1
  for(person_vector_t::iterator itr = temp2.begin(); itr != temp2.end(); ++itr) {
    (*itr)->change_household(h1);
  }

  Place::place_logger->info(
      "HOUSING: swapped house {:s} with {:d} beds and {:d} occupants with {:s} with {:d} beds and {:d} occupants",
      h1->get_label(), h1->get_original_size(), h1->get_size(), h2->get_label(), h2->get_original_size(), h2->get_size());
}

/**
 * Moves residents of the Household at the second index into the Household at the first index.
 *
 * @param house_index1 the first house index
 * @param house_index2 the second house index
 */
void Place::combine_households(int house_index1, int house_index2) {

  Household* h1 = Place::get_household(house_index1);
  Household* h2 = Place::get_household(house_index2);
  if(h1 == nullptr || h2 == nullptr) {
    return;
  }

  Place::place_logger->info(
      "HOUSING: combining house {:s} with {:d} beds and {:d} occupants with {:s} with {:d} beds and {:d} occupants",
      h1->get_label(), h1->get_original_size(), h1->get_size(), h2->get_label(), h2->get_original_size(), h2->get_size());

  // get pointers to residents of house h2
  person_vector_t temp2;
  temp2.clear();
  person_vector_t Housemates2 = h2->get_inhabitants();
  for(person_vector_t::iterator itr = Housemates2.begin(); itr != Housemates2.end(); ++itr) {
    temp2.push_back(*itr);
  }

  // move into house h1
  for(person_vector_t::iterator itr = temp2.begin(); itr != temp2.end(); ++itr) {
    (*itr)->change_household(h1);
  }

  Place::place_logger->info(
      "HOUSING: combined house {:s} with {:d} beds and {:d} occupants with {:s} with {:d} beds and {:d} occupants",
      h1->get_label(), h1->get_original_size(), h1->get_size(), h2->get_label(), h2->get_original_size(), h2->get_size());

}

/**
 * Gets the Hospital assigned to the specified Household in the household label hospital label map.
 *
 * @param hh the household
 */
Hospital* Place::get_hospital_assigned_to_household(Household* hh) {
  assert(Place::is_load_completed());
  if(Place::hh_label_hosp_label_map.find(std::string(hh->get_label())) != Place::hh_label_hosp_label_map.end()) {
    std::string hosp_label = Place::hh_label_hosp_label_map.find(std::string(hh->get_label()))->second;
    if(Place::hosp_label_hosp_id_map.find(hosp_label) != Place::hosp_label_hosp_id_map.end()) {
      int hosp_id = Place::hosp_label_hosp_id_map.find(hosp_label)->second;
      return static_cast<Hospital*>(Place::get_hospital(hosp_id));
    } else {
      return nullptr;
    }
  }

  return nullptr;
}

/**
 * Updates all counties for the given day if population dynamics are enabled.
 *
 * @param day the day
 */
void Place::update_population_dynamics(int day) {

  if(!Global::Enable_Population_Dynamics) {
    return;
  }

  int number_counties = County::get_number_of_counties();
  for(int i = 0; i < number_counties; ++i) {
    County::get_county_with_index(i)->update(day);
  }

}

/**
 * Updates the maximum and/or minimum geo coordinates. The specified latitude or longitude exceeds either of the two bounds, 
 * that bound is set to the corresponding latitude or longitude.
 *
 * @param lat the latitude
 * @param lon the longitude
 */
void Place::update_geo_boundaries(fred::geo lat, fred::geo lon) {
  // update max and min geo coords
  if(lat != 0.0) {
    if(lat < Place::min_lat) {
      Place::min_lat = lat;
    }
    if(Place::max_lat < lat) {
      Place::max_lat = lat;
    }
  }
  if(lon != 0.0) {
    if(lon < Place::min_lon) {
      Place::min_lon = lon;
    }
    if(Place::max_lon < lon) {
      Place::max_lon = lon;
    }
  }
  return;
}

/**
 * Outputs elevation data to files in the elevation data directory.
 */
void Place::get_elevation_data() {

  Utils::fred_print_lap_time("Places.get_elevation_started");

  int miny = static_cast<int>(Global::Simulation_Region->get_min_lat());
  int maxy = 1 + static_cast<int>(Global::Simulation_Region->get_max_lat());

  int maxw = -(static_cast<int>(Global::Simulation_Region->get_min_lon()) - 1);
  int minw = -(static_cast<int>(Global::Simulation_Region->get_max_lon()));

  Place::place_logger->info("miny {:d} maxy {:d} minw {:d} maxw {:d}", miny, maxy, minw, maxw);

  // read optional properties
  Parser::disable_abort_on_failure();
  
  char elevation_data_dir[FRED_STRING_SIZE];
  snprintf(elevation_data_dir, FRED_STRING_SIZE, "none");
  Parser::get_property("elevation_data_directory", elevation_data_dir);

  // restore requiring properties
  Parser::set_abort_on_failure();

  if(strcmp(elevation_data_dir, "none")==0) {
    return;
  }

  char outdir[FRED_STRING_SIZE];
  snprintf(outdir, FRED_STRING_SIZE, "%s/ELEV", Global::Simulation_directory);
  Utils::fred_make_directory(outdir);
  char cmd[FRED_STRING_SIZE];
  char key[FRED_STRING_SIZE];
  char zip_file[FRED_STRING_SIZE];
  char elevation_file[FRED_STRING_SIZE];

  for(int y = miny; y <= maxy; ++y) {
    for(int x = minw; x <= maxw; ++x) {
      snprintf(key, FRED_STRING_SIZE, "n%dw%03d", y, x);
      snprintf(zip_file, FRED_STRING_SIZE, "%s/%s.zip", elevation_data_dir, key);
      Place::place_logger->info("looking for {:s}", zip_file);
      FILE* ftest = fopen(zip_file, "r");
      if(ftest != nullptr) {
        fclose(ftest);
        Place::place_logger->info("process zip file {:s}", zip_file);
        snprintf(elevation_file, FRED_STRING_SIZE, "%s/%s.txt", outdir, key);
        Place::place_logger->info("elevation_file = |{:s}|", elevation_file);
        snprintf(cmd, FRED_STRING_SIZE, "rm -f %s", elevation_file);
        system(cmd);
        snprintf(cmd, FRED_STRING_SIZE, "unzip %s/%s -d %s", elevation_data_dir, key, outdir);
        system(cmd);
        FILE* fp = Utils::fred_open_file(elevation_file);
        if(fp != nullptr) {
          fred::geo lat, lon;
          double elev;
          double x, y;
          while(fscanf(fp, "%lf %lf %lf ", &x, &y, &elev) == 3) {
            lat = y;
            lon = x;
            Neighborhood_Patch* patch = Global::Neighborhoods->get_patch(lat, lon);
            if(patch != nullptr) {
              int row = patch->get_row();
              int col = patch->get_col();
              for(int r = row - 1; r <= row + 1; ++r) {
                for(int c = col -1 ; c <= col + 1; ++c) {
                  Neighborhood_Patch* p = Global::Neighborhoods->get_patch(r,c);
                  if(p != nullptr) {
                    p->add_elevation_site(lat, lon, elev);
                  }
                }
              }
            } else {
            }
          }
          fclose(fp);
          Utils::fred_print_lap_time("Places.get_elevation process elevation file");
          unlink(elevation_file);
        } else {
          Place::place_logger->critical("file {:s} could not be opened", elevation_file);
          exit(0);
        }
      }
    }
  }
  Utils::fred_print_lap_time("Places.get_elevation");
}

/**
 * Updates household data in the specified location file.
 *
 * @param location_file the location file
 */
void Place::update_household_file(char* location_file) {

  char line[FRED_STRING_SIZE];

  // data to fill in from input file
  char label[FRED_STRING_SIZE];
  char admin_code_str[FRED_STRING_SIZE];
  char new_file[FRED_STRING_SIZE];
  double lat;
  double lon;
  double elevation = 0;
  int race;
  int income;

  FILE* fp = Utils::fred_open_file(location_file);
  if(fp == nullptr) {
    return;
  }

  snprintf(new_file, FRED_STRING_SIZE, "%s-elev", location_file);
  FILE* newfp = fopen(new_file, "w");

  // update header line
  fgets(label, FRED_STRING_SIZE, fp);
  snprintf(label, FRED_STRING_SIZE, "sp_id\tstcotrbg\trace\thh_income\tlatitude\tlongitude\televation\n");
  fputs(label, newfp);

  // read first data line
  strcpy(line, "");
  fgets(line, FRED_STRING_SIZE, fp);
  int items = sscanf(line, "%s %s %d %d %lf %lf %lf", label, admin_code_str, &race, &income, &lat, &lon, &elevation);
  
  int n = 0;
  while(6 <= items) {
    elevation = Place::get_household(n)->get_elevation();
    fprintf(newfp, "%s\t%s\t%d\t%d\t%0.7lf\t%0.7lf\t%lf\n", label, admin_code_str, race, income, lat, lon, elevation);

    // get next line
    strcpy(line, "");
    fgets(line, FRED_STRING_SIZE, fp);
    items = sscanf(line, "%s %s %d %d %lf %lf %lf", label, admin_code_str, &race, &income, &lat, &lon, &elevation);
    ++n;
  }
  fclose(fp);
  fclose(newfp);

  // cleanup
  char cmd[FRED_STRING_SIZE];
  snprintf(cmd, FRED_STRING_SIZE, "mv %s %s", new_file, location_file);
  system(cmd);

  Place::place_logger->info("finished updating {:d} households", n);
  return;
}

/**
 * Updates school data in the specified location file.
 *
 * @param location_file the location file
 */
void Place::update_school_file(char* location_file) {

  char line[FRED_STRING_SIZE];

  // data to fill in from input file
  char label[FRED_STRING_SIZE];
  char admin_code_str[FRED_STRING_SIZE];
  char new_file[FRED_STRING_SIZE];
  double lat;
  double lon;
  double elevation = 0;

  FILE* fp = Utils::fred_open_file(location_file);
  if(fp == nullptr) {
    return;
  }

  snprintf(new_file, FRED_STRING_SIZE, "%s-elev", location_file);
  FILE* newfp = fopen(new_file, "w");

  // update eader line
  fgets(label, FRED_STRING_SIZE, fp);
  snprintf(label, FRED_STRING_SIZE, "sp_id\tstco\tlatitude\tlongitude\televation\n");
  fputs(label, newfp);

  // read first data line
  strcpy(line, "");
  fgets(line, FRED_STRING_SIZE, fp);
  int items = sscanf(line, "%s %s %lf %lf %lf", label, admin_code_str, &lat, &lon, &elevation);
  
  int n = 0;
  while(4 <= items) {
    elevation = get_school(n)->get_elevation();
    fprintf(newfp, "%s\t%s\t%0.7lf\t%0.7lf\t%lf\n", label, admin_code_str, lat, lon, elevation);

    // get next line
    strcpy(line, "");
    fgets(line, FRED_STRING_SIZE, fp);
    items = sscanf(line, "%s %s %lf %lf %lf", label, admin_code_str, &lat, &lon, &elevation);
    ++n;
  }
  fclose(fp);
  fclose(newfp);

  // cleanup
  char cmd[FRED_STRING_SIZE];
  snprintf(cmd, FRED_STRING_SIZE, "mv %s %s", new_file, location_file);
  system(cmd);

  Place::place_logger->info("finished updating {:d} schools", n);
  return;
}

/**
 * Updates workplace data in the specified location file.
 *
 * @param location_file the location file
 */
void Place::update_workplace_file(char* location_file) {

  char line[FRED_STRING_SIZE];

  // data to fill in from input file
  char label[FRED_STRING_SIZE];
  char new_file[FRED_STRING_SIZE];
  double lat;
  double lon;
  double elevation = 0;

  FILE* fp = Utils::fred_open_file(location_file);
  if(fp == nullptr) {
    return;
  }

  snprintf(new_file, FRED_STRING_SIZE, "%s-elev", location_file);
  FILE* newfp = fopen(new_file, "w");

  // update header line
  fgets(label, FRED_STRING_SIZE, fp);
  snprintf(label, FRED_STRING_SIZE, "sp_id\tlatitude\tlongitude\televation\n");
  fputs(label, newfp);

  // read first data line
  strcpy(line, "");
  fgets(line, FRED_STRING_SIZE, fp);
  int items = sscanf(line, "%s %lf %lf %lf", label, &lat, &lon, &elevation);
  
  int n = 0;
  while(3 <= items) {
    elevation = get_workplace(n)->get_elevation();
    fprintf(newfp, "%s\t%0.7lf\t%0.7lf\t%lf\n", label, lat, lon, elevation);

    // get next line
    strcpy(line, "");
    fgets(line, FRED_STRING_SIZE, fp);
    items = sscanf(line, "%s %lf %lf %lf", label, &lat, &lon, &elevation);
    ++n;
  }
  fclose(fp);
  fclose(newfp);

  // cleanup
  char cmd[FRED_STRING_SIZE];
  snprintf(cmd, FRED_STRING_SIZE, "mv %s %s", new_file, location_file);
  system(cmd);

  Place::place_logger->info("finished updating {:d} workplaces", n);
  return;
}

/**
 * Updates hospital data in the specified location file.
 *
 * @param location_file the location file
 */
void Place::update_hospital_file(char* location_file) {

  char line[FRED_STRING_SIZE];

  // data to fill in from input file
  char label[FRED_STRING_SIZE];
  char new_file[FRED_STRING_SIZE];
  double lat;
  double lon;
  double elevation = 0;
  int workers, physicians, beds;

  FILE* fp = Utils::fred_open_file(location_file);
  if(fp == nullptr) {
    return;
  }

  snprintf(new_file, FRED_STRING_SIZE, "%s-elev", location_file);
  FILE* newfp = fopen(new_file, "w");

  // update header line
  fgets(label, FRED_STRING_SIZE, fp);
  snprintf(label, FRED_STRING_SIZE, "hosp_id\tworkers\tphysicians\tbeds\tlatitude\tlongitude\televation\n");
  fputs(label, newfp);

  // read first data line
  strcpy(line, "");
  fgets(line, FRED_STRING_SIZE, fp);
  int items = sscanf(line, "%s %d %d %d %lf %lf %lf", label, &workers, &physicians, &beds, &lat, &lon, &elevation);
  
  int n = 0;
  while(6 <= items) {
    elevation = get_hospital(n)->get_elevation();
    fprintf(newfp, "%s\t%d\t%d\t%d\t%0.7lf\t%0.7lf\t%lf\n", label, workers, physicians, beds, lat, lon, elevation);

    // get next line
    strcpy(line, "");
    fgets(line, FRED_STRING_SIZE, fp);
    items = sscanf(line, "%s %d %d %d %lf %lf %lf", label, &workers, &physicians, &beds, &lat, &lon, &elevation);
    ++n;
  }
  fclose(fp);
  fclose(newfp);

  // cleanup
  char cmd[FRED_STRING_SIZE];
  snprintf(cmd, FRED_STRING_SIZE, "mv %s %s", new_file, location_file);
  system(cmd);

  Place::place_logger->info("finished updating {:d} hospitals", n);
  return;
}

/**
 * Updates group quarters data in the specified location file.
 *
 * @param location_file the location file
 */
void Place::update_gq_file(char* location_file) {

  char line[FRED_STRING_SIZE];

  // data to fill in from input file
  char label[FRED_STRING_SIZE];
  char type_str[FRED_STRING_SIZE];
  char admin_code[FRED_STRING_SIZE];
  char new_file[FRED_STRING_SIZE];
  int persons;
  double lat;
  double lon;
  double elevation = 0;

  FILE* fp = Utils::fred_open_file(location_file);
  if(fp == nullptr) {
    return;
  }

  snprintf(new_file, FRED_STRING_SIZE, "%s-elev", location_file);
  FILE* newfp = fopen(new_file, "w");

  // update header line
  fgets(label, FRED_STRING_SIZE, fp);
  snprintf(label, FRED_STRING_SIZE, "sp_id\tgq_type\tstcotrbg\tpersons\tlatitude\tlongitude\televation\n");
  fputs(label, newfp);
  fflush(newfp);

  // read first data line
  strcpy(line, "");
  fgets(line, FRED_STRING_SIZE, fp);
  int items = sscanf(line, "%s %s %s %d %lf %lf %lf", label, type_str, admin_code, &persons, &lat, &lon, &elevation);
  
  int n = 0;
  while(6 <= items) {
    elevation = Place::gq[n]->get_elevation();
    fprintf(newfp, "%s\t%c\t%s\t%d\t%0.7lf\t%0.7lf\t%lf\n", label, type_str[0], admin_code, persons, lat, lon, elevation);
    fflush(newfp);

    // get next line
    strcpy(line, "");
    fgets(line, FRED_STRING_SIZE, fp);
    items = sscanf(line, "%s %s %s %d %lf %lf %lf", label, type_str, admin_code, &persons, &lat, &lon, &elevation);
    ++n;
  }
  fclose(fp);
  fclose(newfp);

  // cleanup
  char cmd[FRED_STRING_SIZE];
  snprintf(cmd, FRED_STRING_SIZE, "mv %s %s", new_file, location_file);
  system(cmd);

  Place::place_logger->info("finished updating {:d} group_quarters", n);
  return;
}

/**
 * Gets places of the specified Place_Type around the specified Place. These places are fetched from neighborhood patches surrounding 
 * the target's Neighborhood_Patch, within the max distance of the place type, where the distance is a combination of 
 * adjacent cells in the grid of neighborhood patches.
 *
 * @param target the target place
 * @param type_id the place type ID
 * @return the places
 */
place_vector_t Place::get_candidate_places(Place* target, int type_id) { 
  int max_dist = Place_Type::get_place_type(type_id)->get_max_dist();

  place_vector_t results;
  place_vector_t tmp;
  Neighborhood_Patch* patch = target->get_patch();
  if(patch == nullptr) {
    Place::place_logger->warn("target {:s} has bad patch", target->get_label());
  }
  for(int dist = 0; dist <= max_dist; ++dist) {
    Place::place_logger->debug("get_candidate_places distance = {:d}", dist);
    tmp = patch->get_places_at_distance(type_id, dist);
    for (int i = 0; i < static_cast<int>(tmp.size()); ++i) {
      Place* place = tmp[i];
      Place::place_logger->debug("place {:s} row {:d} col {:d}", place->get_label(), place->get_patch()->get_row(), 
          place->get_patch()->get_col()); fflush(stdout);
    }

    // append results from one distance to overall results
    results.insert(std::end(results), std::begin(tmp), std::end(tmp));
  }
  return results;

}

/**
 * Reports data on school distributions to files for the given day.
 *
 * @param day the day
 */
void Place::report_school_distributions(int day) {
  // original size distribution
  int count[21];
  int osize[21];
  int nsize[21];
  // size distribution of schools
  for(int c = 0; c <= 20; ++c) {
    count[c] = 0;
    osize[c] = 0;
    nsize[c] = 0;
  }
  for(int p = 0; p < Place::get_number_of_schools(); ++p) {
    int os = Place::get_school(p)->get_original_size();
    int ns = Place::get_school(p)->get_size();
    int n = os / 50;
    if(n > 20) {
      n = 20;
    }
    ++count[n];
    osize[n] += os;
    nsize[n] += ns;
  }

  std::stringstream ss;
  ss << "SCHOOL SIZE distribution: ";
  for(int c = 0; c <= 20; ++c) {
    ss << fmt::format("{:d} {:d} {:0.2f} {:0.2f} | ", c, count[c], 
        count[c] ? (1.0 * osize[c]) / (1.0 * count[c]) : 0, 
        count[c] ? (1.0 * nsize[c]) / (1.0 * count[c]) : 0);
  }
  Place::place_logger->info("{:s}", ss.str());

  return;
}

/**
 * Reports data on household distributions to files.
 */
void Place::report_household_distributions() {
  int number_of_households = Place::get_number_of_households();
  {
    int count[20];
    int total = 0;
    // size distribution of households
    for(int c = 0; c <= 10; ++c) {
      count[c] = 0;
    }
    for(int p = 0; p < number_of_households; ++p) {
      int n = Place::get_household(p)->get_size();
      if(n <= 10) {
	      ++count[n];
      } else {
	      ++count[10];
      }
      ++total;
    }

    std::stringstream hh_size_dist_s_str;
    hh_size_dist_s_str << fmt::format("Household size distribution: N = {:d} ", total);
    for(int c = 0; c <= 10; ++c) {
      hh_size_dist_s_str << fmt::format("{:3d}: {:6d} ({:.2f}) ", c, count[c], (100.0 * count[c]) / total);
    }
    Place::place_logger->info("{:s}", hh_size_dist_s_str.str());
    
    // original size distribution
    int hsize[20];
    total = 0;
    // size distribution of households
    for(int c = 0; c <= 10; ++c) {
      count[c] = 0;
      hsize[c] = 0;
    }
    for(int p = 0; p < number_of_households; ++p) {
      int n = Place::get_household(p)->get_original_size();
      int hs = Place::get_household(p)->get_size();
      if(n <= 10) {
        ++count[n];
	      hsize[n] += hs;
      } else {
	      ++count[10];
	      hsize[10] += hs;
      }
      ++total;
    }

    std::stringstream hh_orig_dist_s_str;
    hh_orig_dist_s_str << fmt::format("Household orig distribution: N = {:d} ", total);
    for(int c = 0; c <= 10; ++c) {
      hh_orig_dist_s_str << fmt::format("{:3d}: {:6d} ({:.2f}) {:0.2f} ",
	        c, count[c], 
          (100.0 * count[c]) / total, count[c] ? (static_cast<double>(hsize[c]) / static_cast<double>(count[c])) : 0.0);
    }
    Place::place_logger->info("{:s}", hh_orig_dist_s_str.str());
  }

  return;
}

///
/// QUALITY CONTROL 
///

/**
 * Preforms quality control on the places in the simulation and outputs data on distributions.
 */
void Place::quality_control() {
  //Can't do the quality control until all of the population files have been read
  assert(Person::is_load_completed());

  int number_of_households = Place::get_number_of_households();
  int number_of_schools = Place::get_number_of_schools();
  int number_of_neighborhoods = Place::get_number_of_neighborhoods();
  int number_of_workplaces = Place::get_number_of_workplaces();

  Place::place_logger->info("places quality control check for places");

  {
    int hn = 0;
    int nn = 0;
    int sn = 0;
    int wn = 0;
    double hsize = 0.0;
    double nsize = 0.0;
    double ssize = 0.0;
    double wsize = 0.0;
    // mean size by place type

    for(int p = 0; p < number_of_households; ++p) {
      int n = Place::get_household(p)->get_size();
      ++hn;
      hsize += n;
    }

    for(int p = 0; p < number_of_neighborhoods; ++p) {
      int n = Place::get_neighborhood(p)->get_size();
      ++nn;
      nsize += n;
    }

    for(int p = 0; p < number_of_schools; ++p) {
      int n = Place::get_school(p)->get_size();
      ++sn;
      ssize += n;
    }

    for(int p = 0; p < number_of_workplaces; ++p) {
      int n = Place::get_workplace(p)->get_size();
      ++wn;
      wsize += n;
    }

    if(hn) {
      hsize /= hn;
    }
    if(nn) {
      nsize /= nn;
    }
    if(sn) {
      ssize /= sn;
    }
    if(wn) {
      wsize /= wn;
    }
    Place::place_logger->info("MEAN PLACE SIZE: H {:.2f} N {:.2f} S {:.2f} W {:.2f}",
	      hsize, nsize, ssize, wsize);
  }

  if(Global::Verbose > 1) {
    char filename[FRED_STRING_SIZE];
    snprintf(filename, FRED_STRING_SIZE, "%s/houses.dat", Global::Simulation_directory);
    FILE* fp = fopen(filename, "w");
    for(int p = 0; p < number_of_households; ++p) {
      Place* h = get_household(p);
      double x = Geo::get_x(h->get_longitude());
      double y = Geo::get_y(h->get_latitude());
      fprintf(fp, "%f %f\n", x, y);
    }
    fclose(fp);
  }

  // household type
  int htypes = 21;
  enum htype_t {
    SINGLE_FEMALE,
    SINGLE_MALE,
    OPP_SEX_SIM_AGE_PAIR,
    OPP_SEX_DIF_AGE_PAIR,
    OPP_SEX_TWO_PARENTS,
    SINGLE_PARENT,
    SINGLE_PAR_MULTI_GEN_FAMILY,
    TWO_PAR_MULTI_GEN_FAMILY,
    UNATTENDED_KIDS,
    OTHER_FAMILY,
    YOUNG_ROOMIES,
    OLDER_ROOMIES,
    MIXED_ROOMIES,
    SAME_SEX_SIM_AGE_PAIR,
    SAME_SEX_DIF_AGE_PAIR,
    SAME_SEX_TWO_PARENTS,
    DORM_MATES,
    CELL_MATES,
    BARRACK_MATES,
    NURSING_HOME_MATES,
    UNKNOWN,
  };

  std::string htype[21] = {
    "single-female",
    "single-male",
    "opp-sex-sim-age-pair",
    "opp-sex-dif-age-pair",
    "opp-sex-two-parent-family",
    "single-parent-family",
    "single-parent-multigen-family",
    "two-parent-multigen-family",
    "unattended-minors",
    "other-family",
    "young-roomies",
    "older-roomies",
    "mixed-roomies",
    "same-sex-sim-age-pair",
    "same-sex-dif-age-pair",
    "same-sex-two-parent-family",
    "dorm-mates",
    "cell-mates",
    "barrack-mates",
    "nursing-home_mates",
    "unknown",
  };

  int type[htypes];
  int ttotal[htypes];
  int hnum = 0;
  for(int i = 0; i < htypes; ++i) {
    type[i] = 0;
    ttotal[i] = 0;
  }

  for(int p = 0; p < number_of_households; ++p) {
    ++hnum;
    Household* h = Place::get_household(p);
    int t = h->get_orig_household_structure();
    ++type[t];
    ttotal[t] += h->get_size();
  }

  Place::place_logger->info("HOUSEHOLD_TYPE DISTRIBUTION");
  for(int t = 0; t < htypes; ++t) {
    Place::place_logger->info(
        "HOUSEHOLD TYPE DIST: {:30s}: {:8d} households ({:5.1f}) with {:8d} residents ({:5.1f})",
        htype[t].c_str(), type[t], (100.0 * type[t]) / hnum, ttotal[t], 
        (100.0 * ttotal[t] / Person::get_population_size()));
  }

  {
    int count[20];
    int total = 0;
    // size distribution of households
    for(int c = 0; c < 15; ++c) {
      count[c] = 0;
    }
    for(int p = 0; p < number_of_households; ++p) {
      int n = Place::get_household(p)->get_size();
      if(n < 15) {
        ++count[n];
      } else {
        ++count[14];
      }
      ++total;
    }

    Place::place_logger->debug("Household size distribution: {:d} households", total);
    for(int c = 0; c < 15; ++c) {
      Place::place_logger->debug("{:3d}: {:6d} ({:.2f})", c, count[c], (100.0 * count[c]) / total);
    }
  }

  {
    int count[20];
    int total = 0;
    // adult distribution of households
    for(int c = 0; c < 15; ++c) {
      count[c] = 0;
    }
    for(int p = 0; p < number_of_households; ++p) {
      Household* h = Place::get_household(p);
      int n = h->get_adults();
      if(n < 15) {
        ++count[n];
      } else {
        ++count[14];
      }
      ++total;
    }
    Place::place_logger->debug("Household adult size distribution: {:d} households", total);
    for(int c = 0; c < 15; ++c) {
      Place::place_logger->debug("{:3d}: {:6d} ({:.2f})", c, count[c], (100.0 * count[c]) / total);
    }
  }

  {
    int count[20];
    int total = 0;
    // children distribution of households
    for (int c = 0; c < 15; ++c) {
      count[c] = 0;
    }
    for(int p = 0; p < number_of_households; ++p) {
      Household* h = Place::get_household(p);
      int n = h->get_children();
      if(n < 15) {
        ++count[n];
      } else {
        ++count[14];
      }
      ++total;
    }
    Place::place_logger->debug("Household children size distribution: {:d} households", total);
    for(int c = 0; c < 15; ++c) {
      Place::place_logger->debug("{:3d}: {:6d} ({:.2f})",  c, count[c], (100.0 * count[c]) / total);
    }
  }

  {
    int count[20];
    int total = 0;
    // adult distribution of households with children
    for(int c = 0; c < 15; ++c) {
      count[c] = 0;
    }
    for(int p = 0; p < number_of_households; ++p) {
      Household* h = Place::get_household(p);
      if(h->get_children() == 0) {
        continue;
      }
      int n = h->get_adults();
      if(n < 15) {
        ++count[n];
      } else {
        ++count[14];
      }
      ++total;
    }
    Place::place_logger->debug("Household w/ children, adult size distribution: {:d} households", total);
    for(int c = 0; c < 15; ++c) {
      Place::place_logger->debug("{:3d}: {:6d} ({:.2f})", c, count[c], (100.0 * count[c]) / total);
    }
  }

  {
    int count[100];
    int total = 0;
    // age distribution of heads of households
    for(int c = 0; c < 100; ++c) {
      count[c] = 0;
    }
    for(int p = 0; p < number_of_households; ++p) {
      Person* per = nullptr;
      Household* h = Place::get_household(p);
      for(int i = 0; i < h->get_size(); ++i) {
        if(h->get_member(i)->is_householder()) {
          per = h->get_member(i);
        }
      }
      if(per == nullptr) {
        Place::place_logger->warn(
            "Help! No head of household found for household id {:d} label {:s} size {:d} groupquarters: {:d}",
            h->get_id(), h->get_label(), h->get_size(), h->is_group_quarters() ? 1 : 0);
        ++count[0];
      } else {
        int a = per->get_age();
        if(a < 100) {
          ++count[a];
        } else {
          ++count[99];
        }
        ++total;
      }
    }
    Place::place_logger->debug("Age distribution of heads of households: {:d} households", total);
    for(int c = 0; c < 100; ++c) {
      Place::place_logger->debug("age {:2d}: {:6d} ({:.2f})", c, count[c], (100.0 * count[c]) / total);
    }
  }

  {  
    int count[100];
    int total = 0;
    int children = 0;
    // age distribution of heads of households with children
    for(int c = 0; c < 100; ++c) {
      count[c] = 0;
    }
    for(int p = 0; p < number_of_households; ++p) {
      Person* per = nullptr;
      Household* h = Place::get_household(p);
      if(h->get_children() == 0) {
        continue;
      }
      children += h->get_children();
      for(int i = 0; i < h->get_size(); ++i) {
        if(h->get_member(i)->is_householder()) {
          per = h->get_member(i);
        }
      }
      if(per == nullptr) {
        Place::place_logger->warn(
            "Help! No head of household found for household id {:d} label {:s} groupquarters: {:d}",
            h->get_id(), h->get_label(), h->is_group_quarters() ? 1 : 0);
        ++count[0];
      } else {
        int a = per->get_age();
        if(a < 100) {
          ++count[a];
        } else {
          ++count[99];
        }
        ++total;
      }
    }
    Place::place_logger->debug("Age distribution of heads of households with children: {:d} households", total);
    for(int c = 0; c < 100; ++c) {
      Place::place_logger->debug("age {:2d}: {:6d} ({:.2f})", c, count[c], (100.0 * count[c]) / total);
    }
    Place::place_logger->debug("children = {:d}", children);
  }

  {
    int count[100];
    int total = 0;
    int tot_students = 0;
    // size distribution of schools
    for(int c = 0; c < 20; ++c) {
      count[c] = 0;
    }
    for(int p = 0; p < number_of_schools; ++p) {
      int s = Place::get_school(p)->get_size();
      tot_students += s;
      int n = s / 50;
      if(n < 20) {
        ++count[n];
      } else {
        ++count[19];
      }
      ++total;
    }
    Place::place_logger->debug("School size distribution: {:d} schools {:d} students", total, tot_students);
    for(int c = 0; c < 20; ++c) {
      Place::place_logger->debug("{:3d}: {:6d} ({:.2f})", (c + 1) * 50, count[c], (100.0 * count[c]) / total);
    }
  }

  {
    // age distribution in schools
    Place::place_logger->debug("School age distribution:");
    int count[100];
    for(int c = 0; c < 100; ++c) {
      count[c] = 0;
    }
    for(int p = 0; p < number_of_schools; ++p) {
      for(int c = 0; c < 100; ++c) {
        count[c] += Place::get_school(p)->get_size_by_age(c);
      }
    }
    for(int c = 0; c < 100; ++c) {
      Place::place_logger->debug("age = {:2d}  students = {:6d}", c, count[c]);
    }
  }

  {
    int count[101];
    int small_employees = 0;
    int med_employees = 0;
    int large_employees = 0;
    int xlarge_employees = 0;
    int total_employees = 0;
    int total = 0;
    // size distribution of workplaces
    for(int c = 0; c <= 100; ++c) {
      count[c] = 0;
    }
    for(int p = 0; p < number_of_workplaces; ++p) {
      int s = Place::get_workplace(p)->get_size();
      if(s <= 100) {
        ++count[s];
      } else {
        ++count[100];
      }
      if(s < 50) {
        small_employees += s;
      } else if (s < 100) {
        med_employees += s;
      } else if (s < 500) {
        large_employees += s;
      } else {
        xlarge_employees += s;
      }
      total_employees += s;
      total++;
    }
    for(int p = 0; p < number_of_schools; ++p) {
      int s = Place::get_school(p)->get_staff_size();
      if(s <= 100) {
        ++count[s];
      } else {
        ++count[100];
      }
      if(s < 50) {
        small_employees += s;
      } else if(s < 100) {
        med_employees += s;
      } else if(s < 500) {
        large_employees += s;
      } else {
        xlarge_employees += s;
      }
      total_employees += s;
      ++total;
    }
    Place::place_logger->debug("Workplace size distribution: {:d} workplaces", total);
    for(int c = 0; c <= 100; ++c) {
      Place::place_logger->debug("{:3d}: {:6d} ({:.2f})", (c+1)*1, count[c], (100.0*count[c])/total);
    }

    Place::place_logger->debug("employees at small workplaces (1-49): {:d}", small_employees);
    Place::place_logger->debug("employees at medium workplaces (50-99): {:d}", med_employees);
    Place::place_logger->debug("employees at large workplaces (100-499): {:d}", large_employees);
    Place::place_logger->debug("employees at xlarge workplaces (500-up): {:d}", xlarge_employees);
    Place::place_logger->debug("total employees: {:d}", total_employees);
  }

  {
    int count[60];
    int total = 0;
    // size distribution of offices
    for(int c = 0; c < 60; ++c) {
      count[c] = 0;
    }
    for(int p = 0; p < number_of_workplaces; ++p) {
      Place* w = Place::get_workplace(p);
      for(int off = 0; off < w->get_number_of_partitions(); ++off) {
        int s = w->get_partition(off)->get_size();
        int n = s;
        if(n < 60) {
          ++count[n];
        } else {
          ++count[60 - 1];
        }
        ++total;
      }
    }
    Place::place_logger->debug("Office size distribution: {:d} offices", total);
    for(int c = 0; c < 60; ++c) {
      Place::place_logger->debug("{:3d}: {:6d} ({:.2f})", c, count[c], (100.0 * count[c]) / total);
    }
  }

  Place::place_logger->info("places quality control finished");
}

/**
 * Sets up partitions of the specified Place_Type for this place, with this place as their container. The partition capacity 
 * specifies the max amount of members per partition. The partitions are made based off the partition basis, which either 
 * partitions by age or by random. If the basis is age, partitions are made for members of each age, which fill partitions of that age 
 * up to the capacity until all members of that age are assigned to a partition. The max age partition specifies the the maximum age 
 * to make partitions for. If the basis is random, partitions of the capacity are made until all members are assigned to a partition.
 *
 * @param partition_type_id the place type ID of the partitions
 * @param partition_capacity the partition capacity
 * @param partition_basis the partition basis
 * @param min_age_partition _UNUSED_
 * @param max_age_partition the maximum age to partition to, if basis is age
 */
void Place::setup_partitions(int partition_type_id, int partition_capacity, std::string partition_basis, int min_age_partition, int max_age_partition) {

  if(partition_type_id < 0) {
    return;
  }

  if(partition_capacity == 0) {
    return;
  }

  long long int sp_id = 0;
  int size = this->get_size();

  if(partition_basis == "age") {

    // find size of each age group
    this->original_size_by_age = new int[Demographics::MAX_AGE + 1];
    this->partitions_by_age = new place_vector_t[Demographics::MAX_AGE + 1];
    int next_partition_by_age[Demographics::MAX_AGE + 1];
    for(int a = 0; a <= Demographics::MAX_AGE; ++a) {
      this->original_size_by_age[a] = 0;
      this->partitions_by_age[a].clear();
      next_partition_by_age[a] = 0;
    }
    for(int i = 0; i < size; ++i) {
      this->original_size_by_age[get_member(i)->get_age()]++;
    }

    // create each partition
    for(int a = 0; a <= max_age_partition; ++a) {
      int n = this->original_size_by_age[a];
      if(n == 0) {
        continue;
      }
      int rooms = n / partition_capacity;
      if(n % partition_capacity) {
        ++rooms;
      }
      Place::place_logger->debug("place {:d} {:s} age {:d} number {:d} rooms {:d}", this->get_id(), 
          this->get_label(), a, n, rooms);
      for(int c = 0; c < rooms; ++c) {
        char label[FRED_STRING_SIZE];
        snprintf(label, FRED_STRING_SIZE, "%s-%02d-%02d", this->get_label(), a, c + 1);
        Place* partition = Place::add_place(label, partition_type_id, Place::SUBTYPE_NONE, this->get_longitude(),
            this->get_latitude(), this->get_elevation(), this->get_admin_code());
        sp_id = this->sp_id * 1000000 + 10000 * a + (c + 1);
        partition->set_sp_id(sp_id);
        this->partitions.push_back(partition);
        this->partitions_by_age[a].push_back(partition);
        partition->set_container(this);
        Global::Neighborhoods->add_place(partition);
        Place::place_logger->debug("CREATE PARTITIONS place {:d} {:s} added partition {:s} {:d}", this->get_id(), 
            this->get_label(), partition->get_label(), partition->get_id());
      }
    }

    // assign partition to each member, round robin
    for(int i = 0; i < size; ++i) {
      Person* person = get_member(i);
      int age = person->get_age();
      int room = next_partition_by_age[age];
      if(room < (int)this->partitions_by_age[age].size() - 1) {
        next_partition_by_age[age]++;
      } else {
        next_partition_by_age[age] = 0;
      }
      Place::place_logger->debug("room = {:d} {:s} {:d}", room,
          this->partitions_by_age[age][room]->get_label(),
          this->partitions_by_age[age][room]->get_id());
      person->set_place_of_type(partition_type_id, this->partitions_by_age[age][room]);
    }
  }

  if(partition_basis == "random") {
    // determine number of partitions
    int parts = size / partition_capacity;
    if(size % partition_capacity) {
      ++parts;
    }
    if(parts == 0) {
      ++parts;
    }
    Place::place_logger->debug("CREATE PARTITIONS Place {:d} {:s} number {:d} partitions {:d}  partition_type_id {:d}",
        this->get_id(), this->get_label(), size, parts, partition_type_id);

    // create each partition
    for(int i = 0; i < parts; ++i) {
      char label[FRED_STRING_SIZE];
      snprintf(label, FRED_STRING_SIZE, "%s-%03d", this->get_label(), i + 1);
      Place* partition = Place::add_place(label, partition_type_id, Place::SUBTYPE_NONE,
          this->get_longitude(), this->get_latitude(),
          this->get_elevation(), this->get_admin_code());
      sp_id = this->sp_id * 10000 + (i + 1);
      partition->set_sp_id(sp_id);
      this->partitions.push_back(partition);
      partition->set_container(this);
      Global::Neighborhoods->add_place(partition);
      Place::place_logger->debug("CREATE PARTITIONS place {:d} {:s} added partition {:s} {:d}", this->get_id(), 
          this->get_label(), partition->get_label(), partition->get_id());
    }

    // assign each member to a random partition
    for(int i = 0; i < size; ++i) {
      Person* person = get_member(i);
      select_partition(person);
    }
  }

  return;
}

/**
 * Selects and returns a partition for the specified Person. The partition selected will depend on this place's 
 * partition basis and type ID. If the partition basis is age, a random room of the partitions for the person's age will be returned.
 * If the basis is random, a random partition room will be returned. The partition will also be set as the person's activity group 
 * of the parition's Place_Type.
 *
 * @param person the person
 * @return the partition
 */
Place* Place::select_partition(Person* person) {
  std::string partition_basis = Place_Type::get_place_type(this->type_id)->get_partition_basis();
  int partition_type_id = Place_Type::get_place_type(this->type_id)->get_partition_type_id();
  Place* partition = nullptr;
  int room;
  if(partition_basis == "age") {
    int age = person->get_age();
    room = Random::draw_random_int(0, this->partitions_by_age[age].size() - 1);
    partition = this->partitions_by_age[age][room];
  }
  if(partition_basis == "random") {
    room = Random::draw_random_int(0, this->partitions.size() - 1);
    partition = this->partitions[room];
  }
  Place::place_logger->debug("room = {:d} {:s} {:d}", room, partition->get_label(), partition->get_id());
  person->set_place_of_type(partition_type_id, partition);
  return(partition);
}

/**
 * Checks if this place is open on the given day. This will be true if the place's container is open, the place's Place_Type is open, 
 * and the place does not have admin closure.
 *
 * @param day the day
 * @return if this place is open
 */
bool Place::is_open(int day) {

  Place::place_logger->info("is_open: check place {:s} on day {:d}", this->get_label(), day);

  // place is closed if container is closed:
  if(this->container != nullptr) {
    if(this->container->is_open(day) == false) {
      Place::place_logger->info("day {:d} place {:s} is closed because container {:s} is closed", 
          day, this->get_label(), this->container->get_label());
      return false;
    } else {
      Place::place_logger->info("day {:d} place {:s} container {:s} is open", 
          day, this->get_label(), this->container->get_label());
    }
  }

  // see if base class is open
  return Group::is_open();
}

/**
 * Checks if this place or this place's container has admin closure.
 *
 * @return if this place has admin closure
 */
bool Place::has_admin_closure() {

  int day = Global::Simulation_Day;

  Place::place_logger->debug("has_admin_closure: check place {:s} on day {:d}", this->get_label(), day);

  // place is closed if container is closed:
  if(this->container != nullptr) {
    if(this->container->has_admin_closure()) {
      Place::place_logger->debug("day {:d} place {:s} is closed because container {:s} is closed", 
          day, this->get_label(), this->container->get_label());
      return true;
    } else {
      Place::place_logger->debug("day {:d} place {:s} container {:s} is open", 
          day, this->get_label(), this->container->get_label());
    }
  }

  // see if base class has a closure
  return Group::has_admin_closure();
}

/**
 * Updates the elevations for all places.
 */
void Place::update_elevations() {

  if (Place::Update_elevation) {

    // add elevation sites to the appropriate neighborhood patches
    Place::get_elevation_data();

    // get elevation info for each place
    Neighborhood_Patch* patch;
    for(int p = 0; p < Place::get_number_of_households(); ++p) {
      Place* place = Place::get_household(p);
      fred::geo lat = place->get_latitude();
      fred::geo lon = place->get_longitude();
      patch = Global::Neighborhoods->get_patch(lat, lon);
      if(patch != nullptr) {
        place->set_elevation(patch->get_elevation(lat, lon));
      }
    }
    for(int p = 0; p < Place::get_number_of_schools(); ++p) {
      Place* place = Place::get_school(p);
      fred::geo lat = place->get_latitude();
      fred::geo lon = place->get_longitude();
      patch = Global::Neighborhoods->get_patch(lat, lon);
      if(patch != nullptr) {
        place->set_elevation(patch->get_elevation(lat, lon));
      }
    }
    for(int p = 0; p < Place::get_number_of_workplaces(); ++p) {
      Place* place = Place::get_workplace(p);
      fred::geo lat = place->get_latitude();
      fred::geo lon = place->get_longitude();
      patch = Global::Neighborhoods->get_patch(lat, lon);
      if(patch != nullptr) {
        place->set_elevation(patch->get_elevation(lat, lon));
      }
    }
    for(int p = 0; p < Place::get_number_of_hospitals(); ++p) {
      Place* place = Place::get_hospital(p);
      fred::geo lat = place->get_latitude();
      fred::geo lon = place->get_longitude();
      patch = Global::Neighborhoods->get_patch(lat, lon);
      if(patch != nullptr) {
        place->set_elevation(patch->get_elevation(lat, lon));
      }
    }

    // update input files for each specified location
    int locs = Place::get_number_of_location_ids();
    for(int i = 0; i < locs; ++i) {
      char loc_id[FRED_STRING_SIZE];
      char loc_dir[FRED_STRING_SIZE];
      char location_file[FRED_STRING_SIZE];
      strcpy(loc_id, get_location_id(i));
      snprintf(loc_dir, FRED_STRING_SIZE, "%s/%s/%s/%s", Place::Population_directory, Place::Country, Place::Population_version, loc_id);

      // update household locations
      snprintf(location_file, FRED_STRING_SIZE, "%s/households.txt", loc_dir);
      Place::update_household_file(location_file);
      
      // update school locations
      snprintf(location_file, FRED_STRING_SIZE, "%s/schools.txt", loc_dir);
      Place::update_school_file(location_file);
      
      // update workplace locations
      snprintf(location_file, FRED_STRING_SIZE, "%s/workplaces.txt", loc_dir);
      Place::update_workplace_file(location_file);
      
      // update gq locations
      snprintf(location_file, FRED_STRING_SIZE, "%s/gq.txt", loc_dir);
      Place::update_gq_file(location_file);

      snprintf(location_file, FRED_STRING_SIZE, "touch %s/UPDATED", loc_dir);
      system(location_file);
    }

    Utils::fred_print_lap_time("Places.update_elevations");

    // terminate
    exit(0);
  }
}

/**
 * Sets the elevation for all partitions and partitions of the partitions.
 *
 * @param elev the elevation
 */
void Place::set_partition_elevation(double elev) {
  // set elevation of partitions
  int rooms = this->partitions.size();
  for(int i = 0; i < rooms; ++i) {
    this->partitions[i]->set_elevation(elev);
    this->partitions[i]->set_partition_elevation(elev);
  }
}

/**
 * Checks if this place's vaccination rate is lower than the default vaccination rate for places of this place's Place_Type.
 *
 * @return if the vaccination rate is low
 */
bool Place::is_low_vaccination_place() {
  return this->vaccination_rate < Place_Type::get_place_type(this->type_id)->get_default_vaccination_rate();
}

/**
 * Sets vaccination rates, accounting for exemption rates and vaccine refusals.
 */
void Place::prepare_vaccination_rates() {

  Place_Type* place_type = Place_Type::get_place_type(this->type_id);
  place_type->prepare_vaccination_rates();

  // set vaccination rate for this place
  if(place_type->is_vaccination_rate_enabled()) {

    if(this->vaccination_rate < 0.0) {
      this->vaccination_rate = place_type->get_default_vaccination_rate();
    }

    // randomize the order of processing the members
    std::vector<int> shuffle_index;
    shuffle_index.reserve(get_size());
    for(int i = 0; i < get_size(); ++i) {
      shuffle_index.push_back(i);
    }
    FYShuffle<int>(shuffle_index);
    
    int ineligibles = place_type->get_medical_vacc_exempt_rate() * get_size();
    for(int i = 0; i < ineligibles; ++i) {
      Person* person = this->members[shuffle_index[i]];
      person->set_ineligible_for_vaccine();
    }
    
    int refusers = ((1.0 - place_type->get_medical_vacc_exempt_rate()) - this->vaccination_rate) * get_size();
    if(refusers < 0) {
      refusers = 0;
    }
    for(int i = ineligibles; i < get_size() && i < ineligibles + refusers; ++i) {
      Person* person = this->members[shuffle_index[i]];
      person->set_vaccine_refusal();
    }
    
    int receivers = 0;
    for(int i = ineligibles + refusers; i < get_size(); ++i) {
      Person* person = this->members[shuffle_index[i]];
      person->set_received_vaccine();
      ++receivers;
    }      
    
    Place::place_logger->info(
        "PREP_VAX: place {:s} coverage {:0.2f} size {:d} ineligibles = {:d} refusers = {:d} received = {:d}", 
        this->label, this->vaccination_rate, get_size(), ineligibles, refusers, receivers);
  }
  
}

/**
 * Gets the Place with the specified Synthetic Population ID.
 *
 * @param n the SP ID
 * @return the place
 */
Place* Place::get_place_from_sp_id(long long int n) {
  return static_cast<Place*>(Group::get_group_from_sp_id(n));
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Place::setup_logging() {
  if(Place::is_log_initialized) {
    return;
  }

  if(Parser::does_property_exist("place_log_level")) {
    Parser::get_property("place_log_level", &Place::place_log_level);
  } else {
    Place::place_log_level = "OFF";
  }

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Place::place_logger = std::make_unique<spdlog::logger>("place_logger", 
        sink_list.begin(), sink_list.end());
    Place::place_logger->set_level(
        Utils::get_log_level_from_string(Place::place_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Place::place_logger->trace("<{:s}, {:d}>: Place logger initialized", 
      __FILE__, __LINE__  );
  Place::is_log_initialized = true;
}
