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
// File: Place_Type.cc
//

#include "Condition.h"
#include "Group_Type.h"
#include "Neighborhood_Layer.h"
#include "Parser.h"
#include "Person.h"
#include "Place.h"
#include "Place_Type.h"
#include "Random.h"
#include "Utils.h"

//////////////////////////
//
// STATIC VARIABLES
//
//////////////////////////

std::vector <Place_Type*> Place_Type::place_types;
std::vector<std::string> Place_Type::names;
std::unordered_map<Person*, Place*> Place_Type::host_place_map;

bool Place_Type::is_log_initialized = false;
std::string Place_Type::place_type_log_level = "";
std::unique_ptr<spdlog::logger> Place_Type::place_type_logger = nullptr;

/**
 * Creates a Place_Type with the given properties. The name is passed into the Group_Type constructor. 
 * Default variables are initialized.
 *
 * @param _id the place type ID
 * @param _name the name
 */
Place_Type::Place_Type(int _id, std::string _name) : Group_Type(_name) {
  this->places.clear();
  this->base_type_id = -1;
  this->enable_visualization = 0;
  this->max_dist = 0.0;
  this->max_size = 999999999;
  this->report_size = -1;
  this->partition_name = "NONE";
  this->partition_type_id = -1;
  this->partition_capacity = 999999999;
  this->partition_basis = "none";
  this->min_age_partition = 0;
  this->max_age_partition = Demographics::MAX_AGE;
  this->medical_vacc_exempt_rate = 0.0;
  this->enable_vaccination_rates = 0;
  this->default_vaccination_rate = 95.0;
  this->need_to_get_vaccination_rates = 0;
  this->elevation_cutoffs = cutoffs_t{"", 0, 0, 0, 0, 0, 0, 0};
  this->size_cutoffs = cutoffs_t{"", 0, 0, 0, 0, 0, 0, 0};
  this->income_cutoffs = cutoffs_t{"", 0, 0, 0, 0, 0, 0, 0};
  this->next_sp_id = 700000000 + _id * 1000000 + 1;
  Group_Type::add_group_type(this);
}

/**
 * Default destructor.
 */
Place_Type::~Place_Type() {
}

/**
 * Gets properties of this place type.
 */
void Place_Type::get_properties(){

  // first get the base class properties
  Group_Type::get_properties();

  Place_Type::place_type_logger->info("place_type {:s} read_properties entered", this->name.c_str());
  
  // property name to fill in
  char property_name[FRED_STRING_SIZE];

  // optional properties:
  Parser::disable_abort_on_failure();

  this->enable_visualization = 0;
  snprintf(property_name, FRED_STRING_SIZE, "%s.enable_visualization", this->name.c_str());
  if(Parser::does_property_exist(property_name)) {
    Parser::get_property(property_name, &this->enable_visualization);
  } else {
    snprintf(property_name, FRED_STRING_SIZE, "%s_enable_visualization", this->name.c_str());
    Parser::get_property(property_name, &this->enable_visualization);
  }

  this->report_size = 0;
  /*
  snprintf(property_name, FRED_STRING_SIZE, "%s.report_size", this->name.c_str());
  if (Parser::does_property_exist(property_name)) {
    Parser::get_property(property_name, &this->report_size);
  }
  */

  this->base_type_id = 0;
  std::string base_type = "Household";
  snprintf(property_name, FRED_STRING_SIZE, "%s.base_type", this->name.c_str());
  if(Parser::does_property_exist(property_name)) {
    Parser::get_property(property_name, &base_type);
    this->base_type_id = Place_Type::get_type_id(base_type);
  }

  // control for gravity model
  this->max_dist = 99.0;
  snprintf(property_name, FRED_STRING_SIZE, "%s.max_dist", this->name.c_str());
  if(Parser::does_property_exist(property_name)) {
    Parser::get_property(property_name, &this->max_dist);
  } else {
    snprintf(property_name, FRED_STRING_SIZE, "%s_max_dist", this->name.c_str());
    Parser::get_property(property_name, &this->max_dist);
  }

  // max place size
  this->max_size = 999999999;
  snprintf(property_name, FRED_STRING_SIZE, "%s.max_size", this->name.c_str());
  if(Parser::does_property_exist(property_name)) {
    Parser::get_property(property_name, &this->max_size);
  }

  // partition place_type
  char type_str[FRED_STRING_SIZE];

  snprintf(type_str, FRED_STRING_SIZE, "NONE");
  snprintf(property_name, FRED_STRING_SIZE, "%s.partition", this->name.c_str());
  if(Parser::does_property_exist(property_name)) {
    Parser::get_property(property_name, type_str);
  } else {
    snprintf(property_name, FRED_STRING_SIZE, "%s_partition", this->name.c_str());
    Parser::get_property(property_name, type_str);
  }
  this->partition_name = std::string(type_str);

  snprintf(property_name, FRED_STRING_SIZE, "%s.partition_basis", this->name.c_str());
  if(Parser::does_property_exist(property_name)) {
    Parser::get_property(property_name, &this->partition_basis);
  } else {
    snprintf(property_name, FRED_STRING_SIZE, "%s_partition_basis", this->name.c_str());
    Parser::get_property(property_name, &this->partition_basis);
  }

  snprintf(property_name, FRED_STRING_SIZE, "%s.partition_min_age", this->name.c_str());
  if(Parser::does_property_exist(property_name)) {
    Parser::get_property(property_name, &this->min_age_partition);
  } else {
    snprintf(property_name, FRED_STRING_SIZE, "%s_%s_min_age", this->name.c_str(), type_str);
    Parser::get_property(property_name, &this->min_age_partition);
  }

  snprintf(property_name, FRED_STRING_SIZE, "%s.partition_max_age", this->name.c_str());
  if(Parser::does_property_exist(property_name)) {
    Parser::get_property(property_name, &this->max_age_partition);
  } else {
    snprintf(property_name, FRED_STRING_SIZE, "%s_%s_max_age", this->name.c_str(), type_str);
    Parser::get_property(property_name, &this->max_age_partition);
  }

  snprintf(property_name, FRED_STRING_SIZE, "%s.partition_size", this->name.c_str());
  if(Parser::does_property_exist(property_name)) {
    Parser::get_property(property_name, &this->partition_capacity);
  } else {
    snprintf(property_name, FRED_STRING_SIZE, "%s_%s_size", this->name.c_str(), type_str);
    Parser::get_property(property_name, &this->partition_capacity);
  }

  snprintf(property_name, FRED_STRING_SIZE, "%s.partition_capacity", this->name.c_str());
  if(Parser::does_property_exist(property_name)) {
    Parser::get_property(property_name, &this->partition_capacity);
  } else {
    snprintf(property_name, FRED_STRING_SIZE, "%s_%s_capacity", this->name.c_str(), type_str);
    Parser::get_property(property_name, &this->partition_capacity);
  }

  // vaccination rates
  snprintf(property_name, FRED_STRING_SIZE, "enable_%s_vaccination_rates", this->name.c_str());
  Parser::get_property(property_name, &this->enable_vaccination_rates);
  if(this->enable_vaccination_rates) {
    snprintf(property_name, FRED_STRING_SIZE, "%s_vaccination_rate_file",  this->name.c_str());
    Parser::get_property(property_name, this->vaccination_rate_file);
    this->need_to_get_vaccination_rates = 1;
  } else {
    strcpy(this->vaccination_rate_file, "none");
    this->need_to_get_vaccination_rates = 0;
  }

  snprintf(property_name, FRED_STRING_SIZE, "default_%s_vaccination_rate", this->name.c_str());
  Parser::get_property(property_name, &this->default_vaccination_rate);

  snprintf(property_name, FRED_STRING_SIZE, "medical_vacc_exempt_rate");
  Parser::get_property(property_name, &this->medical_vacc_exempt_rate);

  Parser::set_abort_on_failure();

  Place_Type::place_type_logger->info("place_type {:s} read_properties finished", this->name.c_str());
}

/**
 * Prepares all Place_Type objects in the static place types vector.
 */
void Place_Type::prepare_place_types() {
  for(int type_id = 0; type_id < static_cast<int>(Place_Type::place_types.size()); ++type_id) {
    Place_Type::place_types[type_id]->prepare(); 
  }
}

/**
 * Adds the specified Place to the vector of places of this type. Also sets the place's index as 
 * the index it was added into in the places vector.
 *
 * @param place the place
 */
void Place_Type::add_place(Place* place) {
  place->set_index(static_cast<int>(this->places.size()));
  this->places.push_back(place);
}

/**
 * Prepares this place type. Updates size, elevation, and income cutoffs.
 */
void Place_Type::prepare() {

  Place_Type::place_type_logger->info("place_type {:s} prepare entered", this->name.c_str());

  // this needs to be done after all places are available

  int number = this->get_number_of_places();
  double* values = new double[number];

  // update size boundaries
  snprintf(this->size_cutoffs.name, FRED_STRING_SIZE, "%s_%s", this->name.c_str(), "size");
  for(int p = 0; p < number; ++p) {
    values[p] = static_cast<int>(this->get_place(p)->get_size());
  }
  Place_Type::set_cutoffs(&this->size_cutoffs, values, number);

  // update elevation boundaries
  snprintf(this->elevation_cutoffs.name, FRED_STRING_SIZE, "%s_%s", this->name.c_str(), "elevation");
  for(int p = 0; p < number; ++p) {
    values[p] = this->get_place(p)->get_elevation();
  }
  Place_Type::set_cutoffs(&this->elevation_cutoffs, values, number);

  // update income boundaries
  snprintf(this->income_cutoffs.name, FRED_STRING_SIZE, "%s_%s", this->name.c_str(), "income");
  for(int p = 0; p < number; ++p) {
    values[p] = this->get_place(p)->get_income();
  }

  Place_Type::set_cutoffs(&this->income_cutoffs, values, number);

  Place_Type::place_type_logger->info("place_type {:s} prepare finished", this->name.c_str());
}

/**
 * Sets the admin list for each Place_Type.
 */
void Place_Type::set_place_type_admin_lists() {
  for(int type_id = 0; type_id < static_cast<int>(Place_Type::place_types.size()); ++type_id) {
    Place_Type::place_types[type_id]->set_admin_list(); 
  }
}

/**
 * Sets the admin list for this place type. This will push back a list of the admin of each Place 
 * of the place type to the global list variables.
 */
void Place_Type::set_admin_list() {
  std::string var_name = this->name + "List";
  Place_Type::place_type_logger->debug("ADMIN place_type {:s} list_var {:s}", 
      this->name.c_str(), var_name.c_str());
  int vid = Person::get_global_list_var_id(var_name);
  int vsize = -1;
  if(0 <= vid) {
    vsize = Person::get_global_list_size(vid);
  }
  Place_Type::place_type_logger->info("ADMIN_LIST {:s} start size = {:d}", 
      var_name.c_str(), vsize);
  int number = this->get_number_of_places();
  for(int p = 0; p < number; ++p) {
    Place* place = this->get_place(p);
    Person* admin = place->get_administrator();
    if (admin != nullptr) {
      int admin_id = admin->get_id();
      Person::push_back_global_list_var(vid, admin_id);
      Place_Type::place_type_logger->debug("adding ADMIN {:d} for place {:s}", 
          admin_id, place->get_label());
    }
  }
  vsize = Person::get_global_list_size(vid);
  Place_Type::place_type_logger->info("ADMIN_LIST {:s} final size = {:d}", 
      var_name.c_str(), vsize);
}

//////////////////////////
//
// STATIC METHODS
//
//////////////////////////

/**
 * Creates and sets up a Place_Type for each place type name, gets it's properties, and 
 * sets up partitions.
 */
void Place_Type::get_place_type_properties() {

  Place_Type::place_types.clear();

//  // read in the list of place_type names
//  char property_name[FRED_STRING_SIZE];
//  char property_value[FRED_STRING_SIZE];

  for(int type_id = 0; type_id < static_cast<int>(Place_Type::names.size()); ++type_id) {

    // create new Place_Type object
    Place_Type* place_type = new Place_Type(type_id, Place_Type::names[type_id]);

    // add to place_type list
    Place_Type::place_types.push_back(place_type);

    // get properties for this place type
    place_type->get_properties();

    Place_Type::place_type_logger->info("CREATED_PLACE_TYPE place_type {:d} = {:s}", type_id, 
        Place_Type::names[type_id].c_str());

  }

  // setup partitions
  for(int type_id = 0; type_id < static_cast<int>(Place_Type::place_types.size()); ++type_id) {
    Place_Type* place_type = Place_Type::place_types[type_id];
    place_type->partition_type_id = Place_Type::get_type_id(place_type->partition_name);
    if(place_type->partition_type_id > -1) {
      Place_Type::place_type_logger->info("PARTITION for {:s} name = {:s} p_id = {:d}", 
          place_type->name.c_str(), place_type->partition_name.c_str(), place_type->partition_type_id);
    }
  }
}

/**
 * Reads places for place types that are not predefined to the specified directory.
 *
 * @param pop_dir the directory
 */
void Place_Type::read_places(const char* pop_dir) {

  Place_Type::place_type_logger->info("read_places from {:s} entered", pop_dir);

  for(int type_id = 0; type_id < static_cast<int>(Place_Type::place_types.size()); ++type_id) {
    Place_Type* place_type = Place_Type::get_place_type(type_id);
    if(Place_Type::is_predefined(place_type->name)) {
      continue;
    }

    if(place_type->file_available) {
      std::string filename = Utils::str_tolower(place_type->name);
      char location_file[FRED_STRING_SIZE];
      snprintf(location_file, FRED_STRING_SIZE, "%s/%ss.txt", pop_dir, filename.c_str());
      Place_Type::place_type_logger->info("place_type name {:s} filename {:s} location_file {:s}",
          place_type->name.c_str(), filename.c_str(), location_file);
      Place_Type::place_type_logger->info("read_place_file {:s}", location_file);
      Place::read_place_file(location_file, type_id);
    } else {
      Place_Type::place_type_logger->info("place_type name {:s} no location_file available", 
          place_type->name.c_str());

      // read any location for this place_type from the FRED program
      std::string prop_name = place_type->name + ".add";
      std::string value = "";
      int n = Parser::get_next_property(prop_name, &value, 0);
      while(0 <= n) {
        long long int sp_id;
        double lat, lon, elevation;
        char label[FRED_STRING_SIZE];
        char place_subtype = Place::SUBTYPE_NONE;
        sscanf(value.c_str(), "%lld %lf %lf %lf", &sp_id, &lat, &lon, &elevation);
        if(sp_id == 0) {
          sp_id = place_type->get_next_sp_id();
        }
        if(!Group::sp_id_exists(sp_id)) {
          snprintf(label, FRED_STRING_SIZE, "%s-%lld", place_type->name.c_str(), sp_id);
          Place_Type::place_type_logger->info(
              "ADD_PLACE {:s} |{:s}| {:s}, sp_id {:d} lat {:f} lon {:f} elev {:f}", 
              place_type->name.c_str(), value.c_str(), label, sp_id, lat, lon, elevation);
          Place* place = Place::add_place(label, type_id, place_subtype, lon, lat, elevation, 0);
          place->set_sp_id(sp_id);
        }

        // get next place location
        n = Parser::get_next_property(prop_name, &value, n + 1);
      }
    }
  }

  Place_Type::place_type_logger->info("read_places from {:s} finished", pop_dir);
  return;
}

/**
 * Adds all places of all place types to their corresponding Neighborhood_Patch on the Neighborhood_Layer.
 */
void Place_Type::add_places_to_neighborhood_layer() {
  Place_Type::place_type_logger->info("add_place_to_neighborhood_layer entered place_types {:d}", 
      static_cast<int>(place_types.size()));

  for(int type_id = 0; type_id < static_cast<int>(Place_Type::place_types.size()); ++type_id) {
    int size = static_cast<int>(Place_Type::place_types[type_id]->places.size());
    for(int p = 0; p < size; ++p) {
      Place* place = Place_Type::place_types[type_id]->places[p];
      if(place != nullptr) {
        Global::Neighborhoods->add_place(place);
      }
    }
  }

  Place_Type::place_type_logger->info("add_place_to_neighborhood_layer finished");
}

/**
 * Sets up partitions for each Place of this place type with this place type's partition type ID, capacity, 
 * basis, and age range.
 */
void Place_Type::setup_partitions() {
  Place_Type::place_type_logger->info(
      "setup_partitions entered for type {:s} partition_type {:d} basis {:s}",
      this->name.c_str(), this->partition_type_id, this->partition_basis.c_str());
  if(this->partition_type_id > -1) {
    int number_of_places = get_number_of_places();
    for(int p = 0; p < number_of_places; ++p) {
      this->places[p]->setup_partitions(this->partition_type_id, this->partition_capacity,
          this->partition_basis, this->min_age_partition, this->max_age_partition);
    }
  }
}

/**
 * Prepares vaccination rates if needed. These will be read from the vaccination rate file.
 */
void Place_Type::prepare_vaccination_rates() {

  if(this->need_to_get_vaccination_rates) {

    // do this just once for each place type
    this->need_to_get_vaccination_rates = 0;

    if(strcmp(this->vaccination_rate_file, "none") != 0) {
      FILE *fp = Utils::fred_open_file(this->vaccination_rate_file);
      if(fp != nullptr) {
        char label[FRED_STRING_SIZE];
        double rate;
        while(fscanf(fp, "%s %lf", label, &rate) == 2) {
          // find place and set it vaccination rates
          Place* place = Place::get_school_from_label(label);
          if(place != nullptr) {
            place->set_vaccination_rate(rate);
            Place_Type::place_type_logger->info("VAX: school {:s} {:s} rate {:f} {:f} size {:d}",
                place->get_label(), label,
                place->get_vaccination_rate(), rate,
                place->get_size());
          } else {
            Place_Type::place_type_logger->info("VAX: {:s} {:f} -- label not found", label, rate);
          }
        }	fclose(fp);
      }
    }
  }
}

/**
 * Generates a new Place with the specified Person as host if the specified Place_Type is a valid place type.
 *
 * @param place_type_id the place type ID
 * @param person the person
 * @return the place
 */
Place* Place_Type::generate_new_place(int place_type_id, Person* person) {
  if(place_type_id < 0 || static_cast<int>(Place_Type::place_types.size()) <= place_type_id) {
    return nullptr;
  } else {
    return Place_Type::place_types[place_type_id]->generate_new_place(person);
  }
}

/**
 * Generates a Place's properties and sets the given Person as the host. If the person is already a host of a 
 * place, that place will be returned. If the person is not linked to any place, a null pointer will be returned.
 *
 * @param person the person
 * @return the place
 */
Place* Place_Type::generate_new_place(Person* person) {
  Place* place = Place_Type::get_place_hosted_by(person);
  if(place != nullptr) {
    return place;
  }

  Place* source = person->get_place_of_type(this->base_type_id);
  if(source == nullptr) {
    return nullptr;
  }

  double lon = source->get_longitude();
  double lat = source->get_latitude();
  double elevation = source->get_elevation();
  long long int census_tract_admin_code = source->get_census_tract_admin_code();
  char label[FRED_STRING_SIZE];
  long long int sp_id = get_next_sp_id();
  snprintf(label, FRED_STRING_SIZE, "%s-%lld", this->name.c_str(), sp_id);
      
  // create a new place
  place = Place::add_place(label, Group_Type::get_type_id(this->name), 'x', lon, lat, elevation, census_tract_admin_code);
  place->set_sp_id(sp_id);
  place->set_host(person);
  Place_Type::host_place_map[person] = place;
  // create an administrator if needed
  if(this->has_admin) {
    place->create_administrator();
    // setup admin agents in epidemics
    Condition::initialize_person(place->get_administrator());
  }
  Place_Type::place_type_logger->debug(
      "GENERATE_NEW_PLACE place {:s} type {:d} {:d} lat {:f} lon {:f} elev {:f} admin_code {:d}  age of host = {:d}",
      place->get_label(), get_type_id(this->name), place->get_type_id(), place->get_latitude(), 
      place->get_longitude(), place->get_elevation(), place->get_census_tract_admin_code(), 
      place->get_host()->get_age());

  return place;
}

/**
 * Reports contacts for each Place_Type.
 */
void Place_Type::report_contacts() {
  for(int id = 0; id < Place_Type::get_number_of_place_types(); ++id) {
    Place_Type* place_type = Place_Type::get_place_type(id);
    place_type->report_contacts_for_place_type();
  }
}

/**
 * Reports contacts for this place type. This will output details on the ages of each member 
 * of the places of this place type to a file.
 */
void Place_Type::report_contacts_for_place_type() {
  char filename[FRED_STRING_SIZE];
  int size = get_number_of_places();
  snprintf(filename, FRED_STRING_SIZE, "%s/age-age-%s.txt", Global::Simulation_directory, get_name());
  FILE* fp = fopen(filename, "w");
  for(int i = 0; i < size; ++i) {
    Place* place = get_place(i);
    int n = place->get_size();
    int ages[n];
    for(int p = 0; p < n; ++p) {
      ages[p] = place->get_member(p)->get_age();
    }
    for(int j = 0; j < n; ++j) {
      for(int k = 0; k < n; ++k) {
        if(j != k) {
          fprintf(fp, "%d %d\n", ages[j], ages[k]);
        }
      }
    }
  }
  fclose(fp);
}

/**
 * Selects a random Place from the places of this place type.
 *
 * @param person _UNUSED_
 * @return the place
 */
Place* Place_Type::select_place(Person* person) {
  int size = get_number_of_places();
  int n = Random::draw_random_int(0, size - 1);
  return get_place(n);
}

/**
 * Selects a random Place of the specified Place_Type.
 *
 * @param place_type_id the place type ID
 * @param person _UNUSED_ in select_place(person)
 * @return the place
 */
Place* Place_Type::select_place_of_type(int place_type_id, Person* person) {
  return Place_Type::get_place_type(place_type_id)->select_place(person);
}

/**
 * Reports the size for each Place of this place type for the given day.
 *
 * @param day the day
 */
void Place_Type::report(int day) {
  if(this->report_size) {
    int number = get_number_of_places();
    for(int p = 0; p < number; ++p) {
      if(get_place(p)->is_reporting_size()) {
        get_place(p)->report_size(day);
      }
    }
  }
}

/**
 * Finishes each Place_Type.
 */
void Place_Type::finish_place_types() {
  for(int type_id = 0; type_id < static_cast<int>(Place_Type::place_types.size()); ++type_id) {
    Place_Type::place_types[type_id]->finish(); 
  }
}

/**
 * Finishes this place type. This will output reports on the size of each Place of this place type 
 * over the course of the simulation to a file.
 */
void Place_Type::finish() {

  if(this->report_size == 0) {
    return;
  }

  char dir[FRED_STRING_SIZE];
  char outfile[FRED_STRING_SIZE];
  FILE* fp;

  snprintf(dir, FRED_STRING_SIZE, "%s/RUN%d/DAILY", Global::Simulation_directory, Global::Simulation_run_number);
  Utils::fred_make_directory(dir);

  for (int i = 0; i < Place_Type::get_number_of_places(); ++i) {
    snprintf(outfile, FRED_STRING_SIZE, "%s/%s.SizeOf%s%03d.txt", dir, this->name.c_str(), this->name.c_str(), i);
    fp = fopen(outfile, "w");
    if(fp == nullptr) {
      Utils::fred_abort("Fred: can't open file %s\n", outfile);
    }
    for(int day = 0; day < Global::Simulation_Days; ++day) {
      fprintf(fp, "%d %d\n", day, this->get_place(i)->get_size_on_day(day));
    }
    fclose(fp);
  }

  // create a csv file for this place_type

  // this joins two files with same value in column 1, from
  // https://stackoverflow.com/questions/14984340/using-awk-to-process-input-from-multiple-files
  char awkcommand[FRED_STRING_SIZE];
  snprintf(awkcommand, FRED_STRING_SIZE, "awk 'FNR==NR{a[$1]=$2 FS $3;next}{print $0, a[$1]}' ");

  char command[FRED_STRING_SIZE];
  char dailyfile[FRED_STRING_SIZE];
  snprintf(outfile, FRED_STRING_SIZE, "%s/RUN%d/%s.csv", Global::Simulation_directory, Global::Simulation_run_number, this->name.c_str());

  snprintf(dailyfile, FRED_STRING_SIZE, "%s/%s.SizeOf%s000.txt", dir, this->name.c_str(), this->name.c_str());
  snprintf(command, FRED_STRING_SIZE, "cp %s %s", dailyfile, outfile);
  system(command);

  for(int i = 0; i < Place_Type::get_number_of_places(); ++i) {
    if(i > 0) {
      snprintf(dailyfile, FRED_STRING_SIZE, "%s/%s.SizeOf%s%03d.txt", dir, this->name.c_str(), this->name.c_str(), i);
      snprintf(command, FRED_STRING_SIZE, "%s %s %s > %s.tmp; mv %s.tmp %s", awkcommand, dailyfile, outfile, outfile, outfile, outfile);
      system(command);
    }
  }  
  
  // create a header line for the csv file
  char headerfile[FRED_STRING_SIZE];
  snprintf(headerfile, FRED_STRING_SIZE, "%s/RUN%d/%s.header", Global::Simulation_directory, Global::Simulation_run_number, this->name.c_str());
  fp = fopen(headerfile, "w");
  fprintf(fp, "Day ");
  for(int i = 0; i < get_number_of_places(); ++i) {
    fprintf(fp, "%s.SizeOf%s%03d ", this->name.c_str(), this->name.c_str(), i);
  }
  fprintf(fp, "\n");
  fclose(fp);
  
  // concatenate header line
  snprintf(command, FRED_STRING_SIZE, "cat %s %s > %s.tmp; mv %s.tmp %s; unlink %s", headerfile, outfile, outfile, outfile, outfile, headerfile);
  system(command);
  
  // replace spaces with commas
  snprintf(command, FRED_STRING_SIZE, "sed -E 's/ +/,/g' %s | sed -E 's/,$//' | sed -E 's/,/ /' > %s.tmp; mv %s.tmp %s", outfile, outfile, outfile, outfile);
  system(command);
  
}

/**
 * Enables the specified Place_Type to report it's size.
 *
 * @param place_type_id the place type ID
 */
void Place_Type::report_place_size(int place_type_id) {
  Place_Type::get_place_type(place_type_id)->report_size = 1;
}

/**
 * Checks that the Place_Type with the specified value, or name, is predefined. This will return true if 
 * the place type does not have a user defined value, i.e. if it is one of the default place types: household, 
 * neighborhood, school, classroom, workplace, office, or hospital.
 *
 * @param value the value
 * @return if the place type is predefined
 */
bool Place_Type::is_predefined(std::string value) {
  return (Utils::str_tolower(value) == "household" ||
      Utils::str_tolower(value) == "neighborhood" ||
      Utils::str_tolower(value) == "school" ||
      Utils::str_tolower(value) == "classroom" ||
      Utils::str_tolower(value) == "workplace" ||
      Utils::str_tolower(value) == "office" ||
      Utils::str_tolower(value) == "hospital");
}

/**
 * Sets the specified cutoffs based on the given values and size. The cutoffs will specify the type of cutoff to set: 
 * size, income, or elevation. The values represent the respective size, income, or elevation of each place of this 
 * place type, while the size defines how many of these places there are. These values will then be sorted, and the 
 * cutoffs will be located at every 20% of the size for quintiles, and every 25% of the size for quartiles.
 *
 * @param cutoffs the cutoffs
 * @param values the values
 * @param size the size
 */
void Place_Type::set_cutoffs(cutoffs_t* cutoffs, double* values, int size) {
  std::sort(values, values + size);
  if(size > 0) {
    cutoffs->first_quintile = values[int(0.2 * size)];
    cutoffs->second_quintile = values[int(0.4 * size)];
    cutoffs->third_quintile = values[int(0.6 * size)];
    cutoffs->fourth_quintile = values[int(0.8 * size)];
    cutoffs->first_quartile = values[int(0.25 * size)];
    cutoffs->second_quartile = values[int(0.5 * size)];
    cutoffs->third_quartile = values[int(0.75 * size)];
  } else {
    cutoffs->first_quintile = 0.0;
    cutoffs->second_quintile = 0.0;
    cutoffs->third_quintile = 0.0;
    cutoffs->fourth_quintile = 0.0;
    cutoffs->first_quartile = 0.0;
    cutoffs->second_quartile = 0.0;
    cutoffs->third_quartile = 0.0;
  }
  Place_Type::place_type_logger->info(
      "CUTOFFS set_cutoffs for {:s} | quartiles {:.1f} {:.1f} {:.1f} | quintiles {:.1f} {:.1f} {:.1f} {:.1f}", 
      cutoffs->name, cutoffs->first_quartile, cutoffs->second_quartile, cutoffs->third_quartile, 
      cutoffs->first_quintile, cutoffs->second_quintile, cutoffs->third_quintile, cutoffs->fourth_quintile);
}

/**
 * Gets the size quartile that the given size falls into dependent on the size cutoffs for the quartiles.
 *
 * @param n the size
 * @return the quartile
 */
int Place_Type::get_size_quartile(int n) {
  if(n <= this->size_cutoffs.first_quartile) {
    return 1;
  }
  if(n <= this->size_cutoffs.second_quartile) {
    return 2;
  }
  if(n <= this->size_cutoffs.third_quartile) {
    return 3;
  }
  return 4;
}

/**
 * Gets the size quintile that the given size falls into dependent on the size cutoffs for the quintiles.
 *
 * @param n the size
 * @return the quintile
 */
int Place_Type::get_size_quintile(int n) {
  if(n <= this->size_cutoffs.first_quintile) {
    return 1;
  }
  if(n <= this->size_cutoffs.second_quintile) {
    return 2;
  }
  if(n <= this->size_cutoffs.third_quintile) {
    return 3;
  }
  if(n <= this->size_cutoffs.fourth_quintile) {
    return 4;
  }
  return 5;
}

/**
 * Gets the income quartile that the given income falls into dependent on the income cutoffs for the quartiles.
 *
 * @param n the income
 * @return the quartile
 */
int Place_Type::get_income_quartile(int n) {
  if(n <= this->income_cutoffs.first_quartile) {
    return 1;
  }
  if(n <= this->income_cutoffs.second_quartile) {
    return 2;
  }
  if(n <= this->income_cutoffs.third_quartile) {
    return 3;
  }
  return 4;
}

/**
 * Gets the income quintile that the given income falls into dependent on the income cutoffs for the quintiles.
 *
 * @param n the income
 * @return the quintile
 */
int Place_Type::get_income_quintile(int n) {
  if(n <= this->income_cutoffs.first_quintile) {
    return 1;
  }
  if(n <= this->income_cutoffs.second_quintile) {
    return 2;
  }
  if(n <= this->income_cutoffs.third_quintile) {
    return 3;
  }
  if(n <= this->income_cutoffs.fourth_quintile) {
    return 4;
  }
  return 5;
}

/**
 * Gets the elevation quartile that the given elevation falls into dependent on the elevation cutoffs for the quartiles.
 *
 * @param n the elevation
 * @return the quartile
 */
int Place_Type::get_elevation_quartile(double n) {
  if(n <= this->elevation_cutoffs.first_quartile) {
    return 1;
  }
  if(n <= this->elevation_cutoffs.second_quartile) {
    return 2;
  }
  if(n <= this->elevation_cutoffs.third_quartile) {
    return 3;
  }
  return 4;
}

/**
 * Gets the elevation quintile that the given elevation falls into dependent on the elevation cutoffs for the quintiles.
 *
 * @param n the elevation
 * @return the quintile
 */
int Place_Type::get_elevation_quintile(double n) {
  if(n <= this->elevation_cutoffs.first_quintile) {
    return 1;
  }
  if(n <= this->elevation_cutoffs.second_quintile) {
    return 2;
  }
  if(n <= this->elevation_cutoffs.third_quintile) {
    return 3;
  }
  if(n <= this->elevation_cutoffs.fourth_quintile) {
    return 4;
  }
  return 5;
}

void Place_Type::setup_logging() {
  if(Place_Type::is_log_initialized) {
    return;
  }
  
  if(Parser::does_property_exist("place_type_log_level")) {
    Parser::get_property("place_type_log_level", &Place_Type::place_type_log_level);
  } else {
    Place_Type::place_type_log_level = "OFF";
  }

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Place_Type::place_type_logger = std::make_unique<spdlog::logger>("place_type_logger", 
        sink_list.begin(), sink_list.end());
    Place_Type::place_type_logger->set_level(
        Utils::get_log_level_from_string(Place_Type::place_type_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Place_Type::place_type_logger->trace("<{:s}, {:d}>: Place_Type logger initialized", 
      __FILE__, __LINE__  );
  Place_Type::is_log_initialized = true;
}
