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
// File: Household.cc
//
#include <algorithm>
#include <climits>
#include <set>

#include <spdlog/spdlog.h>

#include "Global.h"
#include "Household.h"
#include "Neighborhood_Layer.h"
#include "Neighborhood_Patch.h"
#include "Parser.h"
#include "Person.h"
#include "Random.h"
#include "Regional_Layer.h"
#include "Utils.h"

bool Household::is_log_initialized = false;
std::string Household::household_log_level = "";
std::unique_ptr<spdlog::logger> Household::household_logger = nullptr;

/**
 * Creates a Household with the given properties. Passes properties into the Place constructor, setting this 
 * place's Place_Type as a household. Initializes default properties.
 *
 * @param lab the label
 * @param _subtype the subtype
 * @param lon the longitude
 * @param lat the latitude
 */
Household::Household(const char* lab, char _subtype, fred::geo lon, fred::geo lat) : Place(lab, Place_Type::get_type_id("Household"), lon, lat) {
  this->set_subtype(_subtype);
  this->orig_household_structure = UNKNOWN;
  this->household_structure = UNKNOWN;
  this->vaccination_decision = 0;
  this->vaccination_probability = 0.0;
  this->race = Race::UNKNOWN_RACE;
  this->group_quarters_units = 0;
  this->group_quarters_workplace = nullptr;
  this->migration_admin_code = 0;
  this->in_low_vaccination_school = false;
  this->refuse_vaccine = false;
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Household::setup_logging() {
  if(Household::is_log_initialized) {
    return;
  }
  
  // Get the log level for Household.cc from the properties
  if(Parser::does_property_exist("household_log_level")) {
    Parser::get_property("household_log_level", &Household::household_log_level);
  } else {
    Household::household_log_level = "OFF";
  }
    
  try {
    spdlog::sinks_init_list sink_list = { Global::StdoutSink, Global::ErrorFileSink, Global::DebugFileSink, Global::TraceFileSink };
    Household::household_logger = std::make_unique<spdlog::logger>("household_logger", sink_list.begin(), sink_list.end());
    Household::household_logger->set_level(Utils::get_log_level_from_string(Household::household_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }
  Household::household_logger->trace("<{:s}, {:d}>: Household logger initialized", __FILE__, __LINE__);
  Household::is_log_initialized = true;
}

/**
 * _UNUSED_
 */
void Household::get_properties() {
}

/**
 * Sets the household structure and household structure label of this household. 
 * The household structure relates to the type of household and relationship 
 * between household members.
 */
void Household::set_household_structure () {

  if(this->is_college_dorm()) {
    this->household_structure = DORM_MATES;
    strcpy(this->household_structure_label, htype[this->household_structure].c_str());
    return;
  }

  if(this->is_prison_cell()) {
    this->household_structure = CELL_MATES;
    strcpy(this->household_structure_label, htype[this->household_structure].c_str());
    return;
  }

  if(this->is_military_barracks()) {
    this->household_structure = BARRACK_MATES;
    strcpy(this->household_structure_label, htype[this->household_structure].c_str());
    return;
  }

  if(this->is_nursing_home()) {
    this->household_structure = NURSING_HOME_MATES;
    strcpy(this->household_structure_label, htype[this->household_structure].c_str());
    return;
  }

  Person* person = nullptr;
  int count[76];
  for(int i = 0; i < 76; ++i) {
    count[i] = 0;
  }
  int hsize = 0;
  int male_adult = 0;
  int female_adult = 0;
  int male_minor = 0;
  int female_minor = 0;
  int max_minor_age = -1;
  int max_adult_age = -1;
  int min_minor_age = 100;
  int min_adult_age = 100;
  int size = get_size();
  // std::vector<int> ages;
  // ages.clear();
  for(int i = 0; i < size; ++i) {
    person = get_member(i);
    if(person != nullptr) {
      ++hsize;
      int a = person->get_age();
      // ages.push_back(a);
      if (a > 75) {
	      a = 75;
      }
      ++count[a];
      if(a >= 18 && a < min_adult_age) {
        min_adult_age = a;
      }
      if(a >= 18 && a > max_adult_age) {
        max_adult_age = a;
      }
      if(a < 18 && a < min_minor_age) {
        min_minor_age = a;
      }
      if(a < 18 && a > max_minor_age) {
        max_minor_age = a;
      }
      if(a >= 18) {
        if(person->get_sex() == 'F') {
          ++female_adult;
        } else {
          ++male_adult;
        }
      } else {
        if(person->get_sex() == 'F') {
          ++female_minor;
        } else {
          ++male_minor;
        }
      }
    }
  }
  htype_t t = htype_t::UNKNOWN;

  if(max_minor_age > -1) {
    // households with minors:

    // more than two adults
    if (male_adult+female_adult>2) {
      // check for adult children in household
      int max_child_age = -1;
      for(int i = 0; i < hsize; ++i) {
        person = get_member(i);
        int age = person->get_age();
        // find age of oldest "child"
        if(age < 30 && age < max_minor_age + 15 && age > max_child_age) {
          max_child_age = age;
        }
      }
      // count potential parents
      int males = 0;
      int females = 0;
      int max_par_age = -1;
      int min_par_age = 100;
      for(int i = 0; i < hsize; ++i) {
        person = get_member(i);
        int age = person->get_age();
        // count potential parents
        if (age >= max_child_age + 15) {
          if(age > max_par_age) {
            max_par_age = age;
          }

          if(age < min_par_age) {
            min_par_age = age;
          }

          if(person->get_sex() == 'F') {
            ++females;
          } else {
            ++males;
          }
        }
      }
      if(0 < males + females && males + females <= 2 && max_par_age < min_par_age + 15) {
        // we have at least one potential biological parent
        if(males == 1 && females == 1) {
          t = htype_t::OPP_SEX_TWO_PARENTS;
        } else if(males + females == 2) {
          t = htype_t::SAME_SEX_TWO_PARENTS;
        }
        else if(males + females == 1) {
          t = htype_t::SINGLE_PARENT;
        }
      } else if(max_par_age >= min_par_age + 15) {
        // multi-gen family:
        // delete the older generation
        int ma = males;
        int fa = females;
        for(int i = 0; i < hsize; ++i) {
          person = get_member(i);
          int age = person->get_age();
          if(age >= min_par_age+15) {
            if(person->get_sex() == 'F') {
              --fa;
            } else {
              --ma;
            }
          }
        }
        if(ma + fa == 1) {
          t = htype_t::SINGLE_PAR_MULTI_GEN_FAMILY;
        } else if(ma + fa == 2) {
          t = htype_t::TWO_PAR_MULTI_GEN_FAMILY;
        } else {
          t = htype_t::OTHER_FAMILY;
        }
      } else {
        t = htype_t::OTHER_FAMILY;
      }
    } // end more than 2 adults
    
    // two adults
    if(male_adult+female_adult == 2) {
      if(max_adult_age < min_adult_age + 15) {
        if(male_adult == 1 && female_adult == 1) {
          t = htype_t::OPP_SEX_TWO_PARENTS;
        } else {
          t = htype_t::SAME_SEX_TWO_PARENTS;
        }
      } else {
        t = htype_t::SINGLE_PAR_MULTI_GEN_FAMILY;
      }
    } // end two adults

    // one adult
    if(male_adult+female_adult == 1) {
      t = htype_t::SINGLE_PARENT;
    } // end one adult

    // no adults
    if(male_adult + female_adult == 0) {
      if(hsize == 2 && max_minor_age >= 15 && min_minor_age <= max_minor_age - 14) {
        t = htype_t::SINGLE_PARENT;
      } else if(count[15] + count[16] + count[17] == 2 && min_minor_age <= max_minor_age - 14) {
        t = htype_t::OPP_SEX_TWO_PARENTS;
      } else if(hsize == 2 && count[15] + count[16] + count[17] == 2) {
        if(female_minor && male_minor) {
          t = htype_t::OPP_SEX_SIM_AGE_PAIR;
        } else {
          t = htype_t::SAME_SEX_SIM_AGE_PAIR;
        }
      } else if(hsize == 1 && max_minor_age > 14) {
        if(female_minor) {
          t = htype_t::SINGLE_FEMALE;
        } else {
          t = htype_t::SINGLE_MALE;
        }
      } else if (hsize > 2 && count[17]==hsize) {
        t = htype_t::YOUNG_ROOMIES;
      } else {
        t = htype_t::UNATTENDED_KIDS;
      }
    }
  } else { // end households with minors
    // single adults
    if(hsize == 1) {
      if(female_adult || female_minor) {
        t = htype_t::SINGLE_FEMALE;
      } else {
        t = htype_t::SINGLE_MALE;
      }
    }

    // pairs of adults
    if(hsize == 2) {
      if(max_adult_age < min_adult_age + 15) {
        if(male_adult && female_adult) {
          t = htype_t::OPP_SEX_SIM_AGE_PAIR;
        } else {
          t = htype_t::SAME_SEX_SIM_AGE_PAIR;
        }
      } else {
        if(male_adult && female_adult) {
          t = htype_t::OPP_SEX_DIF_AGE_PAIR;
        } else {
          t = htype_t::SAME_SEX_DIF_AGE_PAIR;
        }
      }
    } // end adults pairs

    // more than 2 adults
    if(hsize > 2) {
      if(max_adult_age < 30) {
        t = htype_t::YOUNG_ROOMIES;
      } else if(min_adult_age >= 30) {
        t = htype_t::OLDER_ROOMIES;
      } else {
        t = htype_t::MIXED_ROOMIES;
      }
    }
  } // end adult-only households

  this->household_structure = t;
  strcpy(this->household_structure_label, htype[t].c_str());
}

/**
 * If enable_Household_vaccination_rates = 1, then the household will set its
 * vaccination rate to match that of its children's school rate
 */
void Household::set_household_vaccination() {
  Household::household_logger->trace("<{:s}, {:d}>: VAX REFUSAL hh {:s} size {:d} set_household_vacc_prob entered", __FILE__, __LINE__, this->label, this->get_size());
  int n = 0;
  int age = 100;
  for(int i = 0; i < this->get_size(); ++i) {
    Person* person = this->members[i];
    Place* school = person->get_school();
    if(school != nullptr) {
      if(school->is_low_vaccination_place()) {
        double rate = school->get_vaccination_rate();
        if(rate < Random::draw_random(0,1)) {
          person->set_vaccine_refusal();
          ++n;
          if(person->get_age() < age) {
            age = person->get_age();
          }
        } else {
          Household::household_logger->trace("<{:s}, {:d}>: NO_VAX_REFUSAL: hh {:s} person {:d} age {:d} school {:s} rate {:f}", __FILE__, __LINE__,
              this->label, person->get_id(), person->get_age(), school->get_label(), rate);
        }
      }
    }
  }

  // refuse vacination for younger children if any child has refused
  if(n > 0) {
    for(int i = 0; i < static_cast<int>(this->members.size()); ++i) {
      Person* person = this->members[i];
      if(person->get_age() < age) {
        person->set_vaccine_refusal();
        Household::household_logger->trace("<{:s}, {:d}>: YOUNGER_REFUSAL: hh {:s} person {:d} age {:d}", __FILE__, __LINE__, this->label, person->get_id(), person->get_age());
      }
    }
  }
}
