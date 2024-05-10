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

#include <spdlog/spdlog.h>

#include "Factor.h"

#include "Condition.h"
#include "Date.h"
#include "Global.h"
#include "Group.h"
#include "Group_Type.h"
#include "Network.h"
#include "Network_Type.h"
#include "Parser.h"
#include "Person.h"
#include "Place.h"
#include "Place_Type.h"
#include "Random.h"
#include "Utils.h"

bool Factor::is_log_initialized = false;
std::string Factor::factor_log_level = "";
std::unique_ptr<spdlog::logger> Factor::factor_logger = nullptr;

/**
 * Creates a Factor with the specified string as the name. Initializes default 
 * variables.
 *
 * @param s the string
 */
Factor::Factor(std::string s) {
  this->name = s;
  this->number_of_args = 0;
  this->number = 0.0;
  this->f0 = nullptr;
  this->f1 = nullptr;
  this->f2 = nullptr;
  this->f3 = nullptr;
  this->f4 = nullptr;
  this->f5 = nullptr;
  this->f6 = nullptr;
  this->f7 = nullptr;
  this->f8 = nullptr;
  this->F2 = nullptr;
  this->F3 = nullptr;
  this->arg2 = 0;
  this->arg3 = 0;
  this->arg4 = 0;
  this->arg5 = 0;
  this->arg6 = 0;
  this->arg7 = 0;
  this->arg8 = 0;
  this->is_constant = false;
  this->warning = false;
}

/**
 * Gets the name of this factor.
 *
 * @return the name
 */
std::string Factor::get_name() {
  return this->name;
}


/////////////////////////////////////////////////////////
//
//

/// Factors drawn from a statistical distribution

/**
 * Gets a random number between -1.0 and 1.0.
 *
 * @return the random number
 */
double Factor::get_random() {
  return Random::draw_random(-1.0, 1.0);
}

/**
 * Gets a random number from the normal distribution with a mean of 0.0 and a 
 * standard deviation of 1.0.
 *
 * @return the random number
 */
double Factor::get_normal() {
  return Random::draw_normal(0.0, 1.0);
}

/**
 * Gets a random number from an exponential distrubtion with a lambda of 1.0.
 *
 * @return the random number
 */
double Factor::get_exponential() {
  return Random::draw_exponential(1.0);
}

/// Factors based on simulation run

/**
 * Gets the run number of the simulation.
 *
 * @return the simulation run number
 */
double Factor::get_sim_run() {
  return Global::Simulation_run_number;
}

/// Factors based on time and dates

/**
 * Gets the current day of the simulation.
 *
 * @return the simulation day
 */
double Factor::get_sim_day() {
  return Global::Simulation_Day;
}

/**
 * Gets the current week of the simulation.
 *
 * @return the simulation week
 */
double Factor::get_sim_week() {
  return static_cast<int>(Global::Simulation_Day / 7);
}

/**
 * Gets the current month of the simulation.
 *
 * @return the simulation month
 */
double Factor::get_sim_month() {
  return static_cast<int>(Global::Simulation_Day / 30);
}

/**
 * Gets the current year of the simulation.
 *
 * @return the simulation year
 */
double Factor::get_sim_year() {
  return static_cast<int>(Global::Simulation_Day / 365);
}

/**
 * Gets the current day of the week.
 *
 * @return the day of the week
 */
double Factor::get_day_of_week() {
  return Date::get_day_of_week();
}

/**
 * Gets the current day of the month.
 *
 * @return the day of the month
 */
double Factor::get_day_of_month() {
  return Date::get_day_of_month();
}

/**
 * Gets the current day of the year.
 *
 * @return the day of the year
 */
double Factor::get_day_of_year() {
  return Date::get_day_of_year();
}

/**
 * Gets the current year of the date.
 *
 * @return the year
 */
double Factor::get_year() {
  return Date::get_year();
}

/**
 * Gets the current month of the date.
 *
 * @return the month
 */
double Factor::get_month() {
  return Date::get_month();
}

/**
 * Gets the current date in the form of an integer to allow comparisons. The 
 * date is formed using this formula: 100 * month + day
 *
 * @return the date
 */
double Factor::get_date() {
  int month = Date::get_month();
  int day = Date::get_day_of_month();
  return 100 * month + day;
}

/**
 * Gets the current hour of the simulation.
 *
 * @return the simulation hour
 */
double Factor::get_hour() {
  return Global::Simulation_Hour;
}

/**
 * Gets the current EPI week.
 *
 * @return the EPI week
 */
double Factor::get_epi_week() {
  return Date::get_epi_week();
}

/**
 * gets the current EPI year.
 *
 * @return the EPI year
 */
double Factor::get_epi_year() {
  return Date::get_epi_year();
}

/// Factors based on the agent's demographics

/**
 * Gets the ID of a specified Person.
 *
 * @param person the person
 * @return the ID
 */
double Factor::get_id(Person* person) {
  return person->get_id();
}

/**
 * Gets the birth year of a specified Person.
 *
 * @param person the person
 * @return the birth year
 */
double Factor::get_birth_year(Person* person) {
  return person->get_birth_year();
}

/**
 * Gets the age in days of a specified Person.
 *
 * @param person the person
 * @return the birth year
 */
double Factor::get_age_in_days(Person* person) {
  return person->get_age_in_days();
}

/**
 * Gets the age in weeks of a specified Person.
 *
 * @param person the person
 * @return the age in weeks
 */
double Factor::get_age_in_weeks(Person* person) {
  return person->get_age_in_weeks();
}

/**
 * Gets the age in months of a specified Person.
 *
 * @param person the person
 * @return the age in months
 */
double Factor::get_age_in_months(Person* person) {
  return person->get_age_in_months();
}

/**
 * Gets the age in years of a specified Person.
 *
 * @param person the person
 * @return the age in years
 */
double Factor::get_age_in_years(Person* person) {
  return person->get_age_in_years();
}

/**
 * Gets the age of a specified Person.
 *
 * @param person the person
 * @return the age
 */
double Factor::get_age(Person* person) {
  return person->get_age();
}

/**
 * Gets the sex of a specified Person. Will return a boolean casted to a double 
 * of if the person is a male.
 *
 * @param person the person
 * @return the sex
 */
double Factor::get_sex(Person* person) {
  return (person->get_sex() == 'M');
}

/**
 * Gets the race of a specified Person.
 *
 * @param person the person
 * @return the race
 */
double Factor::get_race(Person* person) {
  return person->get_race();
}

/**
 * Gets the profile of a specified Person.
 *
 * @param person the person
 * @return the profile
 */
double Factor::get_profile(Person* person) {
  return person->get_profile();
}

/**
 * Gets the household relationship of a specified Person.
 *
 * @param person the person
 * @return the household relationship
 */
double Factor::get_household_relationship(Person* person) {
  return person->get_household_relationship();
}

/**
 * Gets the number of children of a specified Person.
 *
 * @param person the person
 * @return the number of children
 */
double Factor::get_number_of_children(Person* person) {
  return person->get_number_of_children();
}

/// Factors based on agent's current state

/**
 * Gets the current condition state of a given Person for a specified Condition.
 *
 * @param person the person
 * @param condition_id the condition ID
 * @return the current condition state
 */
double Factor::get_current_state(Person* person, int condition_id) {
  return person->get_state(condition_id);
}

/**
 * Gets the time in hours since a given Person entered a specified condition state for a specified Condition.
 *
 * @param person the person
 * @param condition_id the condition ID
 * @param state the condition state
 * @return the time since entering the state
 */
double Factor::get_time_since_entering_state(Person* person, int condition_id, int state) {
  int result;
  int entered = person->get_time_entered(condition_id, state);
  if(entered < 0) {
    result = entered;
  } else {
    result = 24 * Global::Simulation_Day + Global::Simulation_Hour - entered;
  }

  return result;
}

/**
 * Gets the susceptibility of a given Person to a specified Condition.
 *
 * @param person the person
 * @param condition_id the condition ID
 * @return the susceptibility
 */
double Factor::get_susceptibility(Person* person, int condition_id) {
  return person->get_susceptibility(condition_id);
}

/**
 * Gets the transmissibility of a specified Condition. If a Person is given, it will be that person's transmissiblity 
 * of the condition; if not, it will be the condition's overall transmissibility.
 *
 * @param person the person
 * @param condition_id the condition ID
 * @return the transmissibility
 */
double Factor::get_transmissibility(Person* person, int condition_id) {
  if(0 <= person->get_id()) {
    return person->get_transmissibility(condition_id);
  } else {
    return Condition::get_condition(condition_id)->get_transmissibility();
  }
}

/**
 * Gets the number of transmissions of a specified Condition for a given Person.
 *
 * @param person the person
 * @param condition_id the condition ID
 * @return the number of transmissions
 */
double Factor::get_transmissions(Person* person, int condition_id) {
  return person->get_transmissions(condition_id);
}

/**
 * Gets the variable at a specified index. If is_global equates to true, this function will return 
 * the global variable at the index. If not, it will return the given Person's personal 
 * variable at the index.
 *
 * @param person the person
 * @param var_index the variable index
 * @param is_global if the variable is global
 * @return the variable
 */
double Factor::get_var(Person* person, int var_index, int is_global) {
  if(is_global) {
    return Person::get_global_var(var_index);
  } else {
    return person->get_var(var_index);
  }
}

/**
 * Gets the size of the list variable at a specified index. If is_global equates to true, this function will 
 * return the size of the global list at the index. If not, it will return the size of the specified Person's personal 
 * list at the index.
 *
 * @param person the person
 * @param list_var_index the list variable index
 * @param is_global if the list variable is global
 * @return the variable
 */
double Factor::get_list_size(Person* person, int list_var_index, int is_global) {
  double value = 0.0;
  if(is_global) {
    value = Person::get_global_list_size(list_var_index);
  } else {
    value = person->get_list_size(list_var_index);
  }
  return value;
}

/**
 * Gets the ID of the Person who transmitted the specified Condition to the given Person.
 *
 * @param person the person
 * @param condition_id the condition ID
 * @return the person ID
 */
double Factor::get_id_of_transmission_source(Person* person, int condition_id) {
  Person* source = person->get_source(condition_id);
  if(source == nullptr) {
    return -999999;
  } else {
    return source->get_id();
  }
}

/// Factors based on other agents

/**
 * Gets the count of people in a specified Condition and condition state. If a Group_Type is specified, the count will 
 * only include those who share a Group of the specified group type with the given Person. 
 * The verb parameter specifies which type of count to receive: 
 *  if verb is 1, an incidence count for that state will be returned; 
 *  if verb is 2, a current count for that state will be returned; 
 *  if verb is 3, a total count for that state will be returned. 
 * The parameter is_count will specify if this function should return a count of people in that state, or a percentage of 
 * people in that state compared to the entire population. If except_me equates to true, the given person will not be counted.
 *
 * @param person the person
 * @param verb the type of count to receive
 * @param is_count if a count or percentage should be returned
 * @param group_type_id the group type ID
 * @param condition_id the condition ID
 * @param state the condition state
 * @param except_me if the person should be counted
 * @return the count
 */
double Factor::get_state_count(Person* person, int verb, int is_count,
           int group_type_id, int condition_id, int state, int except_me) {

  Factor::factor_logger->info(
      "GET_CURRENT_COUNT person {:d} cond {:s} state {:s} verb {:d} is_count {:d} group_type {:d} {:s}",
      person->get_id(), Condition::get_condition(condition_id)->get_name(),
      Condition::get_condition(condition_id)->get_state_name(state).c_str(),
      verb, is_count, group_type_id, Group_Type::get_group_type_name(group_type_id).c_str());
  double value = 0.0;
  if(group_type_id < 0) {
    int count = 0;
    if(verb == 1) {
      count = Condition::get_condition(condition_id)->get_incidence_count(state);
      Factor::factor_logger->debug("GET_CURRENT_COUNT cond {:s} state {:s} count = {:d}",
          Condition::get_condition(condition_id)->get_name(),
          Condition::get_condition(condition_id)->get_state_name(state).c_str(), count);
    }
    if(verb == 2) {
      count = Condition::get_condition(condition_id)->get_current_count(state);
    }
    if(verb == 3) {
      count = Condition::get_condition(condition_id)->get_total_count(state);
    }
    if(except_me && (person->get_state(condition_id) == state)) {
      --count;
    }
    if(is_count) {
      value = count;
    } else {
      value = static_cast<double>(count) * 100.0 / static_cast<double>(Person::get_population_size());
    }
    Factor::factor_logger->debug("GET_CURRENT_COUNT cond {:s} state {:s} value = {:f}",
        Condition::get_condition(condition_id)->get_name(),
        Condition::get_condition(condition_id)->get_state_name(state).c_str(), value);
  } else {
    if(Group::is_a_place(group_type_id)) {
      Place* place = nullptr;
      place = person->get_place_of_type(group_type_id);
      if(place == nullptr) {
        Factor::factor_logger->debug("get_current_count cond {:s} state {:s} place {:s} = {:d}",
            Condition::get_condition(condition_id)->get_name(),
            Condition::get_condition(condition_id)->get_state_name(state).c_str(), "NULL", 0);
        return 0;
      }
      Factor::factor_logger->debug("get_current_count cond {:s} state {:s} place {:s}",
          Condition::get_condition(condition_id)->get_name(),
          Condition::get_condition(condition_id)->get_state_name(state).c_str(), place->get_label());
      int count = 0;
      if(verb == 1) {
        count = Condition::get_condition(condition_id)->get_incidence_group_state_count(place, state);
      }
      if(verb == 2) {
        count = Condition::get_condition(condition_id)->get_current_group_state_count(place,state);
        Factor::factor_logger->debug("get_current_count cond {:s} state {:s} place {:s} = {:d}",
            Condition::get_condition(condition_id)->get_name(),
            Condition::get_condition(condition_id)->get_state_name(state).c_str(), place->get_label(), count);
      }
      if(verb == 3) {
        count = Condition::get_condition(condition_id)->get_total_group_state_count(place, state);
        Factor::factor_logger->debug("get_total_count cond {:s} state {:s} place {:s} = {:d}",
            Condition::get_condition(condition_id)->get_name(),
            Condition::get_condition(condition_id)->get_state_name(state).c_str(), place->get_label(), count);
      }
      if(except_me && (person->get_state(condition_id) == state)) {
        --count;
      }
      if(is_count) {
        value = count;
      } else {
        if(place->get_size() > 0) {
          value = (double)count * 100.0 / (double)place->get_size();
          Factor::factor_logger->debug("get_current_percent cond {:d} state {:d} place {:s} size {:d} = {:f}",
              condition_id, state, place->get_label(), place->get_size(), value);
        } else {
          value = 0.0;
        }
      }
    } else {
      Network* network = nullptr;
      network = person->get_network_of_type(group_type_id);
      if(network == nullptr) {
        return 0;
      }
      int count = 0;
      if(verb == 1) {
        count = Condition::get_condition(condition_id)->get_incidence_group_state_count(network, state);
      }
      if(verb == 2) {
        count = Condition::get_condition(condition_id)->get_current_group_state_count(network,state);
        Factor::factor_logger->debug("get_current_count cond {:d} state {:d} network {:s} = {:d}",
            condition_id, state, network->get_label(), count);
      }
      if(verb == 3) {
        count = Condition::get_condition(condition_id)->get_total_group_state_count(network, state);
      }
      if(except_me && (person->get_state(condition_id) == state)) {
        --count;
      }
      if(is_count) {
        value = count;
      } else {
        if(network->get_size() > 0) {
          value = (double)count * 100.0 / (double)network->get_size();
          Factor::factor_logger->debug("get_current_percent cond {:d} state {:d} network {:s} size {:d} = {:f}",
              condition_id, state, network->get_label(), network->get_size(), value);
        } else {
          value = 0.0;
        }
      }
    }
  }
  Factor::factor_logger->info(
      "GET_STATE_COUNT day {:d} person {:d} verb {:d} group_type {:d} cond_id {:d} state {:d} except_me {:d} value {:f}",
      Global::Simulation_Day, person->get_id(), verb, group_type_id, condition_id, state, except_me, value);

  return value;
}

/**
 * Gets the sum of all personal variables at the specified index of members who share a Group of the specified Group_Type 
 * with the given Person.
 *
 * @param person the person
 * @param var_id the variable index
 * @param group_type_id the group type ID
 * @return the sum
 */
double Factor::get_sum_of_vars_in_group(Person* person, int var_id, int group_type_id) {
  if(group_type_id < Place_Type::get_number_of_place_types()) {
    Place* place = nullptr;
    place = person->get_place_of_type(group_type_id);
    if(place == nullptr) {
      return 0;
    }
    double value = place->get_sum_of_var(var_id);
    return value;
  } else {
    Network* network = nullptr;
    network = person->get_network_of_type(group_type_id);
    if(network == nullptr) {
      return 0;
    }
    double value = network->get_sum_of_var(var_id);
    return value;
  }
}

/**
 * Gets the average of all personal variables at the specified index of members who share a Group
 * of the specified Group_Type with the given Person.
 *
 * @param person the person
 * @param var_id the variable index
 * @param group_type_id the group type ID
 * @return the average
 */
double Factor::get_ave_of_vars_in_group(Person* person, int var_id, int group_type_id) {
  if(group_type_id < Place_Type::get_number_of_place_types()) {
    Place* place = nullptr;
    place = person->get_place_of_type(group_type_id);
    if(place == nullptr) {
      return 0;
    }
    double value = place->get_sum_of_var(var_id);
    int size = static_cast<int>(place->get_size());
    if(size > 0) {
      value = value / size;
    }
    return value;
  } else {
    Network* network = nullptr;
    network = person->get_network_of_type(group_type_id);
    if(network == nullptr) {
      return 0;
    }
    double value = network->get_sum_of_var(var_id);
    int size = static_cast<int>(network->get_size());
    if(size > 0) {
      value = value / size;
    }
    return value;
  }
}

/**
 * Gets the admin code of the block group of a Place of the specified Place_Type that a given Person is linked to.
 *
 * @param person the person
 * @param place_type_id the place type ID
 * @return the admin code
 */
double Factor::get_block_group_admin_code(Person* person, int place_type_id) {
  Place* place = nullptr;
  place = person->get_place_of_type(place_type_id);
  if(place == nullptr) {
    return 0;
  }
  return place->get_block_group_admin_code();
}

/**
 * Gets the admin code of the census tract of a Place of the specified Place_Type that a given Person is linked to.
 *
 * @param person the person
 * @param place_type_id the place type ID
 * @return the admin code
 */
double Factor::get_census_tract_admin_code(Person* person, int place_type_id) {
  Place* place = nullptr;
  place = person->get_place_of_type(place_type_id);
  if(place == nullptr) {
    return 0;
  }
  return place->get_census_tract_admin_code();
}

/**
 * Gets the admin code of the county of a Place of the specified Place_Type that a given Person is linked to.
 *
 * @param person the person
 * @param place_type_id the place type ID
 * @return the admin code
 */
double Factor::get_county_admin_code(Person* person, int place_type_id) {
  Place* place = nullptr;
  place = person->get_place_of_type(place_type_id);
  if(place == nullptr) {
    return 0;
  }
  return place->get_county_admin_code();
}

/**
 * Gets the admin code of the state of a Place of the specified Place_Type that a given Person is linked to.
 *
 * @param person the person
 * @param place_type_id the place type ID
 * @return the admin code
 */
double Factor::get_state_admin_code(Person* person, int place_type_id) {
  Place* place = nullptr;
  place = person->get_place_of_type(place_type_id);
  if(place == nullptr) {
    return 0;
  }
  return place->get_state_admin_code();
}

/// Factors based on groups

/**
 * Gets the SP ID of a Group of the specified Group_Type that a given Person is a member of.
 *
 * @param person the person
 * @param group_type_id the group type ID
 * @return the SP ID
 */
double Factor::get_group_id(Person* person, int group_type_id) {
  if(group_type_id < 0) {
    return -1;
  }
  Group* group = nullptr;
  group = person->get_group_of_type(group_type_id);
  if(group == nullptr) {
    return -1;
  } else {
    return group->get_sp_id();
  }
}

/**
 * Gets the ID of the administrator of a Group of the specified Group_Type that a given Person is a member of.
 *
 * @param person the person
 * @param group_type_id the group type ID
 * @return the ID
 */
double Factor::get_admin_id(Person* person, int group_type_id) {
  if(Group::is_a_place(group_type_id)) {
    Place* place = nullptr;
    place = person->get_place_of_type(group_type_id);
    if(place == nullptr) {
      return -1;
    } else {
      Person* admin = place->get_administrator();
      if(admin) {
        return admin->get_id();
      } else {
        return -1;
      }
    }
  }
  if(Group::is_a_network(group_type_id)) {
    Network* network = nullptr;
    network = person->get_network_of_type(group_type_id);
    if(network == nullptr) {
      return -1;
    } else {
      Person* admin = network->get_administrator();
      if(admin) {
        return admin->get_id();
      } else {
        return -1;
      }
    }
  }
  return -1;
}

/**
 * Gets varying data on a Place of the specified Place_Type that a given Person is linked to, depending on the selection: 
 *  A selection of 1 will get the size of the place. 
 *  A selection of 2 will get the income of the place. 
 *  A selection of 3 will get the elevation of the place. 
 *  A selection of 4 will get the size quartile of the place. 
 *  A selection of 5 will get the income quartile of the place. 
 *  A selection of 6 will get the elevation quartile of the place. 
 *  A selection of 7 will get the size quintile of the place. 
 *  A selection of 8 will get the income quintile of the place. 
 *  A selection of 9 will get the elevation quintile of the place. 
 *  A selection of 10 will get the latitude of the place. 
 *  A selection of 11 will get the longitude of the place.
 *
 * @param person the person
 * @param selection the selection
 * @param place_type_id the ID of the place type
 * @return the result
 */
double Factor::get_group_level(Person* person, int selection, int place_type_id) {

  Factor::factor_logger->info("GET_PLACE_LEVEL day {:d} person {:d} place_type {:d}",
      Global::Simulation_Day, person->get_id(), place_type_id);

  if(selection == 1) {
    Group* group = person->get_group_of_type(place_type_id);
    if(group) {
      return group->get_size();
    } else {
      return 0;
    }
  }

  if(selection == 2) {
    Group* group = person->get_group_of_type(place_type_id);
    if(group) {
      return group->get_income();
    }
    else {
      return 0;
    }
  }

  Place* place = nullptr;
  place = person->get_place_of_type(place_type_id);
  if(place == nullptr) {
    Factor::factor_logger->info("GET_PLACE_LEVEL day {:d} person {:d} place_type {:d} nullptr PLACE RETURN 0",
        Global::Simulation_Day, person->get_id(), place_type_id);
    return 0;
  }

  double value;
  switch(selection) {
    /*
      case 1:
      value = place->get_size();
      break;
      case 2:
      value = place->get_income();
      break;
    */
    case 3:
      value = place->get_elevation();
      break;
    case 4:
      value = Place_Type::get_place_type(place_type_id)->get_size_quartile(place->get_size());
      break;
    case 5:
      value = Place_Type::get_place_type(place_type_id)->get_income_quartile(place->get_income());
      break;
    case 6:
      value = Place_Type::get_place_type(place_type_id)->get_elevation_quartile(place->get_elevation());
      break;
    case 7:
      value = Place_Type::get_place_type(place_type_id)->get_size_quintile(place->get_size());
      break;
    case 8:
      value = Place_Type::get_place_type(place_type_id)->get_income_quintile(place->get_income());
      break;
    case 9:
      value = Place_Type::get_place_type(place_type_id)->get_elevation_quintile(place->get_elevation());
      break;
    case 10:
      value = place->get_latitude();
      break;
    case 11:
      value = place->get_longitude();
      break;
  }

  Factor::factor_logger->info("GET_PLACE_LEVEL day {:d} person {:d} place_type {:d} VALUE {:f}",
      Global::Simulation_Day, person->get_id(), place_type_id, value);
  return value;
}

/**
 * Gets the ADI state rank of the block group of a Place of the specified Place_Type that a given Person is linked to.
 *
 * @param person the person
 * @param place_type_id the place type ID
 * @return the ADI state rank
 */
double Factor::get_adi_state_rank(Person* person, int place_type_id) {
  Place* place = person->get_place_of_type(place_type_id);
  int rank = 0;
  if(place != nullptr) {
    rank = place->get_adi_state_rank();
  }
  return rank;
}

/**
 * Gets the ADI national rank of the block group of a Place of the specified Place_Type that a given Person is linked to.
 *
 * @param person the person
 * @param place_type_id the place type ID
 * @return the ADI national rank
 */
double Factor::get_adi_national_rank(Person* person, int place_type_id) {
  Place* place = person->get_place_of_type(place_type_id);
  int rank = 0;
  if(place != nullptr) {
    rank = place->get_adi_national_rank();
  }
  return rank;
}

/// Factors based on network
/**
 * Gets a given Person's in-degree in a Network of the specified Network_Type that they are linked to.
 *
 * @param person the person
 * @param network_type_id the network type ID
 * @return the in-degree
 */
double Factor::get_network_in_degree(Person* person, int network_type_id) {
  Network_Type* network_type = Network_Type::get_network_type(network_type_id);
  if(network_type == nullptr) {
    return 0;
  }
  Network* network = network_type->get_network();
  int degree = person->get_in_degree(network);

  return degree;
}

/**
 * Gets a given Person's out-degree in a Network of the specified Network_Type that they are linked to.
 *
 * @param person the person
 * @param network_type_id the network type ID
 * @return the out-degree
 */
double Factor::get_network_out_degree(Person* person, int network_type_id) {
  Network_Type* network_type = Network_Type::get_network_type(network_type_id);
  if(network_type == nullptr) {
    return 0;
  }
  Network* network = network_type->get_network();
  int degree = person->get_out_degree(network);
  return degree;
}

/**
 * Gets a given Person's degree in a Network of the specified Network_Type that they are linked to. 
 * If the network type is undirected, this will simply be the person's in-degree. Otherwise, the degree will 
 * be the sum of the person's in-degree and out-degree.
 *
 * @param person the person
 * @param network_type_id the network type ID
 */
double Factor::get_network_degree(Person* person, int network_type_id) {
  if(Network_Type::get_network_type(network_type_id)->is_undirected()) {
    return Factor::get_network_in_degree(person, network_type_id);
  } else {
    return Factor::get_network_in_degree(person, network_type_id)
        + Factor::get_network_out_degree(person, network_type_id);
  }
}

/**
 * Gets the network weight from a given Person to a second given Person in a Network of the specified Network_Type 
 * that the first person is linked to.
 *
 * @param person1 the first person
 * @param person2 the second person
 * @param network_type_id the network type ID
 * @return the network weight
 */
double Factor::get_network_weight(Person* person1, Person* person2, int network_type_id) {
  double result = 0.0;
  Network* network = person1->get_network_of_type(network_type_id);
  if(network != nullptr) {
    result = person1->get_weight_to(person2, network);
  }
  return result;
}

/**
 * Gets the network timestamp from a given Person to a second given Person in a Network of the specified Network_Type 
 * that the first person is linked to.
 *
 * @param person1 the first person
 * @param person2 the second person
 * @param network_type_id the network type ID
 * @return the network timestamp
 */
double Factor::get_network_timestamp(Person* person1, Person* person2, int network_type_id) {
  double result = -1.0;
  Network* network = person1->get_network_of_type(network_type_id);
  if(network != nullptr) {
    result = person1->get_timestamp_to(person2, network);
  }
  return result;
}

/**
 * Gets the ID of the Person with the maximum inward edge weight in a Network of the specified Network_Type 
 * that the given Person is linked to.
 *
 * @param person the person
 * @param network_type_id the network type ID
 * @return the ID
 */
double Factor::get_id_of_max_weight_inward_edge_in_network(Person* person, int network_type_id) {
  double result = -999999;
  Network* network = person->get_network_of_type(network_type_id);
  if(network != nullptr) {
    result = person->get_id_of_max_weight_inward_edge_in_network(network);
  }
  return result;
}

/**
 * Gets the ID of the Person with the maximum outward edge weight in a Network of the specified Network_Type 
 * that the given Person is linked to.
 *
 * @param person the person
 * @param network_type_id the network type ID
 * @return the ID
 */
double Factor::get_id_of_max_weight_outward_edge_in_network(Person* person, int network_type_id) {
  double result = -999999;
  Network* network = person->get_network_of_type(network_type_id);
  if(network != nullptr) {
    result = person->get_id_of_max_weight_outward_edge_in_network(network);
  }
  return result;
}

/**
 * Gets the ID of the Person with the minimum inward edge weight in a Network of the specified Network_Type 
 * that the given Person is linked to.
 *
 * @param person the person
 * @param network_type_id the network type ID
 * @return the ID
 */
double Factor::get_id_of_min_weight_inward_edge_in_network(Person* person, int network_type_id) {
  double result = -999999;
  Network* network = person->get_network_of_type(network_type_id);
  if(network != nullptr) {
    result = person->get_id_of_min_weight_inward_edge_in_network(network);
  }
  return result;
}

/**
 * Gets the ID of the Person with the minimum outward edge weight in a Network of the specified Network_Type 
 * that the given Person is linked to.
 *
 * @param person the person
 * @param network_type_id the network type ID
 * @return the ID
 */
double Factor::get_id_of_min_weight_outward_edge_in_network(Person* person, int network_type_id) {
  double result = -999999;
  Network* network = person->get_network_of_type(network_type_id);
  if(network != nullptr) {
    result = person->get_id_of_min_weight_outward_edge_in_network(network);
  }
  return result;
}

/**
 * Gets the ID of the Person who was last connected to a given Person in a Network of the specified Network_Type 
 * that the given person is linked to.
 *
 * @param person the person
 * @param network_type_id the network type ID
 * @return the ID
 */
double Factor::get_id_of_last_inward_edge_in_network(Person* person, int network_type_id) {
  double result = -999999;
  Network* network = person->get_network_of_type(network_type_id);
  if(network != nullptr) {
    result = person->get_id_of_last_inward_edge_in_network(network);
  }
  return result;
}

/**
 * Gets the ID of the Person who was last connected from a given Person in a Network of the specified Network_Type
 * that the given person is linked to.
 *
 * @param person the person
 * @param network_type_id the network type ID
 * @return the ID
 */
double Factor::get_id_of_last_outward_edge_in_network(Person* person, int network_type_id) {
  double result = -999999;
  Network* network = person->get_network_of_type(network_type_id);
  if(network != nullptr) {
    result = person->get_id_of_last_outward_edge_in_network(network);
  }
  return result;
}


/////////////////////////////////////////////////////////

/**
 * Gets the value given a Person.
 *
 * @param person the person
 * @return the value
 */
double Factor::get_value(Person* person) {
  double value = 0.0;
  if(this->is_constant) {
    return this->number;
  }
  switch(this->number_of_args) {
  case 0:
    value = this->f0();
    break;
  case 1:
    value = this->f1(person);
    break;
  case 2:
    value = this->f2(person, this->arg2);
    break;
  case 3:
    value = this->f3(person, this->arg2, this->arg3);
    break;
  case 4:
    value = this->f4(person, this->arg2, this->arg3, this->arg4);
    break;
  case 5:
    value = this->f5(person,this->arg2, this->arg3, this->arg4, this->arg5);
    break;
  case 6:
    value = this->f6(person, this->arg2, this->arg3, this->arg4, this->arg5, this->arg6);
    break;
  case 7:
    value = this->f7(person, this->arg2, this->arg3, this->arg4, this->arg5, this->arg6, this->arg7);
    break;
  case 8:
    value = this->f8(person, this->arg2, this->arg3, this->arg4, this->arg5, this->arg6, this->arg7, this->arg8);
    break;
  }

  return value;
}

/**
 * Gets the value of the factor given two specified Person objects.
 *
 * @param person1 the first person
 * @param person2 the second person
 * @return the value
 */
double Factor::get_value(Person* person1, Person* person2) {
  double value = 0.0;
  if(this->is_constant) {
    return this->number;
  }
  switch(this->number_of_args) {
  case 2:
    value = this->F2(person1, person2);
    break;
  case 3:
    value = this->F3(person1, person2, this->arg3);
    break;
  default:
    value = this->get_value(person1);
  }
  return value;
}

/**
 * Parses the factor.
 *
 * @return if the factor was parsed successfully
 */
bool Factor::parse() {

  Factor::factor_logger->info("FACTOR: parsing factor |{:s}|", this->name.c_str());

  if(this->name == "") {
    this->number_of_args = 0;
    return true;
  }

  if(Utils::is_number(this->name)) {
    this->is_constant = true;
    this->number = strtod(this->name.c_str(), nullptr);
    return true;
  }

  int var_id = Person::get_var_id(this->name);
  if(0 <= var_id) {
    this->arg2 = var_id;
    this->arg3 = 0;
    this->number_of_args = 3;
    this->f3 = Factor::get_var;
    return true;
  }

  var_id = Person::get_global_var_id(this->name);
  if(0 <= var_id) {
    this->arg2 = var_id;
    this->arg3 = 1;
    this->number_of_args = 3;
    this->f3 = Factor::get_var;
    return true;
  }

  int group_type_id = Group_Type::get_type_id(this->name);
  if(0 <= group_type_id) {
    this->f2 = &Factor::get_group_id;
    this->arg2 = group_type_id;
    this->number_of_args = 2;
    return true;
  }

  if(this->name.find("list_size_of_") == 0) {
    // get list_var _id
    int pos1 = strlen("list_size_of_");
    std::string list_var_name = this->name.substr(pos1);
    int list_var_id = Person::get_list_var_id(list_var_name);
    if(0 <= list_var_id) {
      this->f3 = Factor::get_list_size;
      this->arg2 = list_var_id;
      this->arg3 = 0;
      this->number_of_args = 3;
      return true;
    }
    list_var_id = Person::get_global_list_var_id(list_var_name);
    if(0 <= list_var_id) {
      this->f3 = Factor::get_list_size;
      this->arg2 = list_var_id;
      this->arg3 = 1;
      this->number_of_args = 3;
      return true;
    }
    return false;
  }

  // random distributions
  if(this->name == "random") {
    this->f0 = &Factor::get_random;
    this->number_of_args = 0;
    return true;
  }

  if(this->name == "normal") {
    this->f0 = &Factor::get_normal;
    this->number_of_args = 0;
    return true;
  }

  if(this->name == "exponential") {
    this->f0 = &Factor::get_exponential;
    this->number_of_args = 0;
    return true;
  }

  // simulation run
  if(this->name == "sim_run") {
    this->f0 = &Factor::get_sim_run;
    this->number_of_args = 0;
    return true;
  }

  // time and dates
  if(this->name == "sim_day") {
    this->f0 = &Factor::get_sim_day;
    this->number_of_args = 0;
    return true;
  }

  if(this->name == "sim_week") {
    this->f0 = &Factor::get_sim_week;
    this->number_of_args = 0;
    return true;
  }

  if(this->name == "sim_month") {
    this->f0 = &Factor::get_sim_month;
    this->number_of_args = 0;
    return true;
  }

  if(this->name == "sim_year") {
    this->f0 = &Factor::get_sim_year;
    this->number_of_args = 0;
    return true;
  }

  if(this->name == "day_of_week") {
    this->f0 = &Factor::get_day_of_week;
    this->number_of_args = 0;
    return true;
  }

  if(this->name == "day_of_month") {
    this->f0 = &Factor::get_day_of_month;
    this->number_of_args = 0;
    return true;
  }

  if(this->name == "day_of_year") {
    this->f0 = &Factor::get_day_of_year;
    this->number_of_args = 0;
    return true;
  }

  if(this->name == "month") {
    this->f0 = &Factor::get_month;
    this->number_of_args = 0;
    return true;
  }

  if(this->name == "year") {
    this->f0 = &Factor::get_year;
    this->number_of_args = 0;
    return true;
  }

  if(this->name == "date") {
    this->f0 = &Factor::get_date;
    this->number_of_args = 0;
    return true;
  }

  if(this->name == "hour") {
    this->f0 = &Factor::get_hour;
    this->number_of_args = 0;
    return true;
  }

  if(this->name == "epi_week") {
    this->f0 = &Factor::get_epi_week;
    this->number_of_args = 0;
    return true;
  }

  if(this->name == "epi_year") {
    this->f0 = &Factor::get_epi_year;
    this->number_of_args = 0;
    return true;
  }

  // the agent's demographics
  if(this->name == "id") {
    this->f1 = &Factor::get_id;
    this->number_of_args = 1;
    return true;
  }

  if(this->name == "birth_year") {
    this->f1 = &Factor::get_birth_year;
    this->number_of_args = 1;
    return true;
  }

  if(this->name == "age_in_days") {
    this->f1 = &Factor::get_age_in_days;
    this->number_of_args = 1;
    return true;
  }

  if(this->name == "age_in_weeks") {
    this->f1 = &Factor::get_age_in_weeks;
    this->number_of_args = 1;
    return true;
  }

  if(this->name == "age_in_months") {
    this->f1 = &Factor::get_age_in_months;
    this->number_of_args = 1;
    return true;
  }

  if(this->name == "age_in_years") {
    this->f1 = &Factor::get_age_in_years;
    this->number_of_args = 1;
    return true;
  }

  if(this->name == "age") {
    this->f1 = &Factor::get_age;
    this->number_of_args = 1;
    return true;
  }

  if(this->name == "sex") {
    this->f1 = &Factor::get_sex;
    this->number_of_args = 1;
    return true;
  }

  if(this->name == "race") {
    this->f1 = &Factor::get_race;
    this->number_of_args = 1;
    return true;
  }

  if(this->name == "profile") {
    this->f1 = &Factor::get_profile;
    this->number_of_args = 1;
    return true;
  }

  if(this->name == "household_relationship") {
    this->f1 = &Factor::get_household_relationship;
    this->number_of_args = 1;
    return true;
  }

  if(this->name == "number_of_children") {
    this->f1 = &Factor::get_number_of_children;
    this->number_of_args = 1;
    return true;
  }

  // the agent's current state

  if(this->name.find("current_state_in_") == 0) {
    // get condition _id
    int pos1 = strlen("current_state_in_");
    std::string cond_name = this->name.substr(pos1);
    int cond_id = Condition::get_condition_id(cond_name);
    if(cond_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED CONDITION = |{:s}|", this->name.c_str());
      this->warning = true;
      return false;
    }
    this->f2 = &Factor::get_current_state;
    this->arg2 = cond_id;
    this->number_of_args = 2;
    return true;
  }

  if(this->name.find("time_since") == 0) {
    // get condition _id
    int pos1 = strlen("time_since_entering_");
    int pos2 = this->name.find(".",pos1);
    if(pos1 == static_cast<int>(std::string::npos) || pos2 == static_cast<int>(std::string::npos)) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}|", this->name.c_str());
      return false;
    }
    std::string cond_name = this->name.substr(pos1,pos2-pos1);
    int cond_id = Condition::get_condition_id(cond_name);
    if(cond_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED CONDITION = |{:s}|", this->name.c_str());
      this->warning = true;
      return false;
    }
    Factor::factor_logger->debug("PARSING SINCE FACTOR = |{:s}| cond {:s} {:d}", this->name.c_str(), 
        cond_name.c_str(), cond_id);
    std::string state_name = this->name.substr(pos2+1);
    int state_id = Condition::get_condition(cond_id)->get_state_from_name(state_name);
    if(state_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED STATE = |{:s}|", this->name.c_str());
      this->warning = true;
      return false;
    }
    this->f3 = &Factor::get_time_since_entering_state;
    this->arg2 = cond_id;
    this->arg3 = state_id;
    this->number_of_args = 3;
    return true;
  }

  if(this->name.find("susceptibility") == 0) {
    // get condition _id
    std::string cond_name = this->name.substr(strlen("susceptibility_to_"));
    int cond_id = Condition::get_condition_id(cond_name);
    if(cond_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED CONDITION = |{:s}|", this->name.c_str());
      this->warning = true;
      return false;
    }
    this->f2 = &Factor::get_susceptibility;
    this->arg2 = cond_id;
    this->number_of_args = 2;
    return true;
  }

  if(this->name.find("transmissibility") == 0) {
    // get condition _id
    std::string cond_name = this->name.substr(strlen("transmissibility_for_"));
    int cond_id = Condition::get_condition_id(cond_name);
    if(cond_id < 0) {
      cond_name = this->name.substr(strlen("transmissibility_of_"));
      cond_id = Condition::get_condition_id(cond_name);
      if(cond_id < 0) {
        Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED CONDITION = |{:s}|", this->name.c_str());
        this->warning = true;
        return false;
      }
    }
    this->f2 = &Factor::get_transmissibility;
    this->arg2 = cond_id;
    this->number_of_args = 2;
    return true;
  }

  if(this->name.find("transmissions_of_") == 0) {
    // get condition _id
    std::string cond_name = this->name.substr(strlen("transmissions_of_"));
    int cond_id = Condition::get_condition_id(cond_name);
    if(cond_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED CONDITION = |{:s}|", this->name.c_str());
      this->warning = true;
      return false;
    }
    this->f2 = &Factor::get_transmissions;
    this->arg2 = cond_id;
    this->number_of_args = 2;
    return true;
  }

  if(this->name.find("id_of_source_of_") == 0) {
    // get condition _id
    std::string cond_name = this->name.substr(strlen("id_of_source_of_"));
    int cond_id = Condition::get_condition_id(cond_name);
    if(cond_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED CONDITION = |{:s}|", this->name.c_str());
      this->warning = true;
      return false;
    }
    this->f2 = &Factor::get_id_of_transmission_source;
    this->arg2 = cond_id;
    this->number_of_args = 2;
    return true;
  }

  // factors based on other agents

  if((this->name.find("incidence_") != std::string::npos || this->name.find("current_") != std::string::npos || this->name.find("total_") != std::string::npos) &&
     (this->name.find("_count_") != std::string::npos || this->name.find("_percent_") != std::string::npos)) {

    // find verb
    int verb = 0;
    if(this->name.find("incidence_") != std::string::npos) {
      verb = 1;
    } else if(this->name.find("current_") != std::string::npos) {
      verb = 2;
    } else if(this->name.find("total_") != std::string::npos) {
      verb = 3;
    }
    if(verb == 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}|", this->name.c_str());
      return false;
    }

    // see if we're looking at count or percent
    int is_count = (this->name.find("_count_") != std::string::npos);

    // get condition name
    int pos = static_cast<int>(this->name.find("_of_")) + 4;
    int next = static_cast<int>(this->name.find(".", pos));
    std::string cond_name = this->name.substr(pos, next - pos);
    int cond_id = Condition::get_condition_id(cond_name);
    if(cond_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED CONDITION = |{:s}|", this->name.c_str());
      this->warning = true;
      return false;
    }

    // get state name
    pos = static_cast<int>(this->name.find("_", next));
    std::string state_name = this->name.substr(next + 1, pos - next - 1);
    int state_id = Condition::get_condition(cond_id)->get_state_from_name(state_name);
    if(state_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED STATE = |{:s}|", this->name.c_str());
      this->warning = true;
      return false;
    }

    // get group_type_name if any
    std::string group_type_name = "";
    int group_type_id = -1;
    if(this->name.find("_in_") != std::string::npos) {
      pos = static_cast<int>(this->name.find("_in_")) + 4;
      next = static_cast<int>(this->name.find("_", pos));
      group_type_name = this->name.substr(pos, next-pos);
      group_type_id = Group_Type::get_type_id(group_type_name);
      if(group_type_id < 0) {
        Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED PLACE OR NETWORK NAME = |{:s}|", 
            this->name.c_str());
        return false;
      }
    }

    // looking at others
    int except_me = (this->name.find("_excluding_me") != std::string::npos);

    if(0 <= group_type_id && group_type_id < Place_Type::get_number_of_place_types() + Network_Type::get_number_of_network_types()) {
      Condition::get_condition(cond_id)->track_group_state_counts(group_type_id, state_id);
    }

    this->arg2 = verb;
    this->arg3 = is_count;
    this->arg4 = group_type_id;
    this->arg5 = cond_id;
    this->arg6 = state_id;
    this->arg7 = except_me;
    this->number_of_args = 7;
    this->f7 = &Factor::get_state_count;
    return true;
  }

  if(static_cast<int>(this->name.find("sum_of_")) == 0 || static_cast<int>(this->name.find("ave_of_")) == 0) {

    // get verb: 0 = "sum_of", 1 = "ave_of"
    int verb = (this->name.find("ave_of_") == 0);

    // get var name
    int pos = static_cast<int>(this->name.find("_of_")) + 4;
    int next = static_cast<int>(this->name.find("_", pos));
    std::string var_name = this->name.substr(pos, next - pos);

    Factor::factor_logger->debug("SET GET_VAR_IN_PLACE var |{:s}|", var_name.c_str());

    int var_id = Person::get_var_id(var_name);
    if(var_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}|", this->name.c_str());
      return false;
    }

    // get group_type_name
    std::string group_type_name = "";
    if(this->name.find("_in_") != std::string::npos) {
      pos = static_cast<int>(this->name.find("_in_")) + 4;
      next = static_cast<int>(this->name.find("_", pos));
      group_type_name = this->name.substr(pos, next - pos);
    }
    int group_type_id = Group_Type::get_type_id(group_type_name);
    if(group_type_id < 0) {
      group_type_id = Group_Type::get_type_id(group_type_name);
    }
    if(group_type_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED PLACE OR NETWORK NAME = |{:s}|", 
          this->name.c_str());
      return false;
    }

    this->arg2 = var_id;
    this->arg3 = group_type_id;
    this->number_of_args = 3;
    if(verb == 0) {
      this->f3 = Factor::get_sum_of_vars_in_group;
    }
    else {
      this->f3 = Factor::get_ave_of_vars_in_group;
    }
    return true;
  }

  // factors based on the agent's places

  // admin id
  if(this->name.find("admin_of_") == 0) {

    // find place type
    int pos = this->name.find("_of_") + 4;
    std::string name = this->name.substr(pos);
    int group_type_id = Group_Type::get_type_id(name);
    if(group_type_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}|", this->name.c_str());
      return false;
    }

    this->f2 = &Factor::get_admin_id;
    this->arg2 = group_type_id;
    this->number_of_args = 2;
    return true;
  }

  // place size, income or elevation
  if(this->name.find("size_") == 0 ||
      this->name.find("latitude_") == 0 ||
      this->name.find("longitude_") == 0 ||
      this->name.find("income_") == 0 ||
      this->name.find("elevation_") == 0) {

    // find verb
    int verb = 0;
    if(this->name.find("size_of_") == 0) {
      verb = 1;
    } else if(this->name.find("income_of_") == 0) {
      verb = 2;
    } else if(this->name.find("elevation_of_") == 0) {
      verb = 3;
    }

    if(this->name.find("size_quartile_of_") == 0) {
      verb = 4;
    } else if(this->name.find("income_quartile_of_") == 0) {
      verb = 5;
    } else if(this->name.find("elevation_quartile_of_") == 0) {
      verb = 6;
    }

    if(this->name.find("size_quintile_of_") == 0) {
      verb = 7;
    } else if(this->name.find("income_quintile_of_") == 0) {
      verb = 8;
    } else if(this->name.find("elevation_quintile_of_") == 0) {
      verb = 9;
    } else if(this->name.find("latitude_of_") == 0) {
      verb = 10;
    } else if(this->name.find("longitude_of_") == 0) {
      verb = 11;
    }

    // find place type
    int pos = this->name.find("_of_") + 4;
    std::string name = this->name.substr(pos);
    int place_type_id = Group_Type::get_type_id(name);
    if(place_type_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}|", this->name.c_str());
      return false;
    }

    this->f3 = &Factor::get_group_level;
    this->arg2 = verb;
    this->arg3 = place_type_id;
    this->number_of_args = 3;
    return true;
  }

  // adi rank
  if(this->name.find("adi_state_rank_") == 0) {
    int pos = this->name.find("_of_") + 4;
    std::string name = this->name.substr(pos);
    int place_type_id = Group_Type::get_type_id(name);
    if(place_type_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}|", this->name.c_str());
      return false;
    }
    this->number_of_args = 2;
    this->arg2 = place_type_id;
    this->f2 = &Factor::get_adi_state_rank;
    return true;
  }

  if(this->name.find("adi_national_rank_") == 0) {
    int pos = this->name.find("_of_") + 4;
    std::string name = this->name.substr(pos);
    int place_type_id = Group_Type::get_type_id(name);
    if(place_type_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}|", this->name.c_str());
      return false;
    }
    this->number_of_args = 2;
    this->arg2 = place_type_id;
    this->f2 = &Factor::get_adi_national_rank;
    return true;
  }

  // admin_code of block_group
  if(this->name.find("block_group") == 0) {
    int pos = this->name.find("_of_") + 4;
    std::string name = this->name.substr(pos);
    int place_type_id = Group_Type::get_type_id(name);
    if(place_type_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}|", this->name.c_str());
      return false;
    }
    this->number_of_args = 2;
    this->arg2 = place_type_id;
    this->f2 = &Factor::get_block_group_admin_code;
    return true;
  }

  // admin_code of census_tract
  if(this->name.find("census_tract") == 0) {
    int pos = this->name.find("_of_") + 4;
    std::string name = this->name.substr(pos);
    int place_type_id = Group_Type::get_type_id(name);
    if(place_type_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}|", this->name.c_str());
      return false;
    }
    this->number_of_args = 2;
    this->arg2 = place_type_id;
    this->f2 = &Factor::get_census_tract_admin_code;
    return true;
  }

  // admin_code of county
  if(this->name.find("county") == 0) {
    int pos = this->name.find("_of_") + 4;
    std::string name = this->name.substr(pos);
    int place_type_id = Group_Type::get_type_id(name);
    if(place_type_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}|", this->name.c_str());
      return false;
    }
    this->number_of_args = 2;
    this->arg2 = place_type_id;
    this->f2 = &Factor::get_county_admin_code;
    return true;
  }

  // admin_code of state
  if(this->name.find("state") == 0) {
    int pos = this->name.find("_of_") + 4;
    std::string name = this->name.substr(pos);
    int place_type_id = Group_Type::get_type_id(name);
    if(place_type_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}|", this->name.c_str());
      return false;
    }
    this->number_of_args = 2;
    this->arg2 = place_type_id;
    this->f2 = &Factor::get_state_admin_code;
    return true;
  }

  // network in_degree
  if(this->name.find("in_degree") == 0) {
    int pos = this->name.find("_of_") + 4;
    std::string name = this->name.substr(pos);
    int network_type_id = Group_Type::get_type_id(name);
    if(network_type_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}|", this->name.c_str());
      return false;
    }
    this->number_of_args = 2;
    this->arg2 = network_type_id;
    this->f2 = &Factor::get_network_in_degree;
    return true;
  }

  // network out_degree
  if(this->name.find("out_degree") == 0) {
    int pos = this->name.find("_of_") + 4;
    std::string net_name = this->name.substr(pos);
    int network_type_id = Group_Type::get_type_id(net_name);
    if(network_type_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}|", this->name.c_str());
      return false;
    }
    this->number_of_args = 2;
    this->arg2 = network_type_id;
    this->f2 = &Factor::get_network_out_degree;
    return true;
  }

  // network degree
  if(this->name.find("degree_of_") == 0) {
    int pos = this->name.find("_of_") + 4;
    std::string net_name = this->name.substr(pos);
    int network_type_id = Group_Type::get_type_id(net_name);
    if(network_type_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}| net_name {:s}", 
          this->name.c_str(), net_name.c_str());
      return false;
    }
    this->number_of_args = 2;
    this->arg2 = network_type_id;
    this->f2 = &Factor::get_network_degree;
    return true;
  }

  if(this->name.find("id_of_max_weight_inward_edge_in_") == 0) {
    int pos = this->name.find("_in_") + 4;
    std::string net_name = this->name.substr(pos);
    int network_type_id = Group_Type::get_type_id(net_name);
    if(network_type_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}| net_name {:s}", 
          this->name.c_str(), net_name.c_str());
      return false;
    }
    if(Group::is_a_network(network_type_id)) {
      this->number_of_args = 2;
      this->arg2 = network_type_id;
      this->f2 = &Factor::get_id_of_max_weight_inward_edge_in_network;
      return true;
    } else {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}| group {:s} is not a network", 
          this->name.c_str(), net_name.c_str());
      return false;
    }
  }

  if(this->name.find("id_of_max_weight_outward_edge_in_") == 0) {
    int pos = this->name.find("_in_") + 4;
    std::string net_name = this->name.substr(pos);
    int network_type_id = Group_Type::get_type_id(net_name);
    if(network_type_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}| net_name {:s}", 
          this->name.c_str(), net_name.c_str());
      return false;
    }
    if(Group::is_a_network(network_type_id)) {
      this->number_of_args = 2;
      this->arg2 = network_type_id;
      this->f2 = &Factor::get_id_of_max_weight_outward_edge_in_network;
      return true;
    } else {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}| group {:s} is not a network", 
          this->name.c_str(), net_name.c_str());
      return false;
    }
  }

  if(this->name.find("id_of_min_weight_inward_edge_in_") == 0) {
    int pos = this->name.find("_in_") + 4;
    std::string net_name = this->name.substr(pos);
    int network_type_id = Group_Type::get_type_id(net_name);
    if(network_type_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}| net_name {:s}", 
          this->name.c_str(), net_name.c_str());
      return false;
    }
    if(Group::is_a_network(network_type_id)) {
      this->number_of_args = 2;
      this->arg2 = network_type_id;
      this->f2 = &Factor::get_id_of_min_weight_inward_edge_in_network;
      return true;
    } else {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}| group {:s} is not a network", 
          this->name.c_str(), net_name.c_str());
      return false;
    }
  }

  if(this->name.find("id_of_min_weight_outward_edge_in_") == 0) {
    int pos = this->name.find("_in_") + 4;
    std::string net_name = this->name.substr(pos);
    int network_type_id = Group_Type::get_type_id(net_name);
    if(network_type_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}| net_name {:s}", 
          this->name.c_str(), net_name.c_str());
      return false;
    }
    if(Group::is_a_network(network_type_id)) {
      this->number_of_args = 2;
      this->arg2 = network_type_id;
      this->f2 = &Factor::get_id_of_min_weight_outward_edge_in_network;
      return true;
    } else {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}| group {:s} is not a network", 
          this->name.c_str(), net_name.c_str());
      return false;
    }
  }

  if(this->name.find("id_of_last_inward_edge_in_") == 0) {
    int pos = this->name.find("_in_") + 4;
    std::string net_name = this->name.substr(pos);
    int network_type_id = Group_Type::get_type_id(net_name);
    if(network_type_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}| net_name {:s}", 
          this->name.c_str(), net_name.c_str());
      return false;
    }
    if(Group::is_a_network(network_type_id)) {
      this->number_of_args = 2;
      this->arg2 = network_type_id;
      this->f2 = &Factor::get_id_of_last_inward_edge_in_network;
      return true;
    } else {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}| group {:s} is not a network", 
          this->name.c_str(), net_name.c_str());
      return false;
    }
  }

  if(this->name.find("id_of_last_outward_edge_in_") == 0) {
    int pos = this->name.find("_in_") + 4;
    std::string net_name = this->name.substr(pos);
    int network_type_id = Group_Type::get_type_id(net_name);
    if(network_type_id < 0) {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}| net_name {:s}", 
          this->name.c_str(), net_name.c_str());
      return false;
    }
    if(Group::is_a_network(network_type_id)) {
      this->number_of_args = 2;
      this->arg2 = network_type_id;
      this->f2 = &Factor::get_id_of_last_outward_edge_in_network;
      return true;
    } else {
      Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}| group {:s} is not a network", 
          this->name.c_str(), net_name.c_str());
      return false;
    }
  }

  // unrecognized factor
  Factor::factor_logger->error("HELP: FACTOR UNRECOGNIZED FACTOR = |{:s}|", this->name.c_str());
  return false;
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Factor::setup_logging() {
  if(Factor::is_log_initialized) {
    return;
  }

  if(Parser::does_property_exist("factor_log_level")) {
    Parser::get_property("factor_log_level", &Factor::factor_log_level);
  } else {
    Factor::factor_log_level = "OFF";
  }

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Factor::factor_logger = std::make_unique<spdlog::logger>("factor_logger", 
        sink_list.begin(), sink_list.end());
    Factor::factor_logger->set_level(
        Utils::get_log_level_from_string(Factor::factor_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Factor::factor_logger->trace("<{:s}, {:d}>: Factor logger initialized", 
      __FILE__, __LINE__  );
  Factor::is_log_initialized = true;
}
