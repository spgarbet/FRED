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
// File: Preference.cc
//

#include <unordered_set>

#include <spdlog/spdlog.h>

#include "Expression.h"
#include "Global.h"
#include "Parser.h"
#include "Person.h"
#include "Preference.h"
#include "Random.h"

bool Preference::is_log_initialized = false;
std::string Preference::preference_log_level = "";
std::unique_ptr<spdlog::logger> Preference::preference_logger = nullptr;

/**
 * Default constructor.
 */
Preference::Preference() {
  this->expressions.clear();
}

/**
 * Default destructor.
 */
Preference::~Preference() {
  this->expressions.clear();
}

/**
 * Gets the name of this preference. This will be a combination of the names of this preference's expressions.
 *
 * @return the name
 */
std::string Preference::get_name(){
  std::string result;
  result += "pref: ";
  for(int i = 0; i < static_cast<int>(this->expressions.size()); ++i) {
    result += this->expressions[i]->get_name();
    result += "|";
  }
  result += "\n";
  return result;
}

/**
 * Adds expressions contained in the specified expression string to the expressions vector if they parse successfully.
 *
 * @param expr_str the expression string
 */
void Preference::add_preference_expressions(std::string expr_str) {
  if(expr_str != "") {
    string_vector_t expression_strings = Utils::get_top_level_parse(expr_str, ',');
    for(int i = 0; i < static_cast<int>(expression_strings.size());++i) {
      std::string e = expression_strings[i];
      Expression* expression = new Expression(e);
      if(expression->parse() == false) {
        char msg[FRED_STRING_SIZE];
        snprintf(msg, FRED_STRING_SIZE, "Bad expression: |%s|", e.c_str());
        Utils::print_error(msg);
        return;
      } else {
        this->expressions.push_back(expression);
      }
    }
  }
}

/**
 * Selects a random Person from the given person vector based off a distribution created from the 
 * Predicate values of the specified Person and each person in the person vector.
 *
 * @param person the person
 * @param people the person vector
 * @return the random person
 */
Person* Preference::select_person(Person* person, person_vector_t &people) {
  
  Preference::preference_logger->info(
      "select_person entered for person {:d} age {:d} sex {:c} people size {:d}",
      person->get_id(), person->get_age(), person->get_sex(), (int)people.size());

  int psize = people.size();
  if(psize == 0) {
    return nullptr;
  }

  // create a cdf based on preference values
  double cdf[psize];
  double total = 0.0;
  for (int i = 0; i < psize; ++i) {
    cdf[i] = this->get_value(person, people[i]);
    total += cdf[i];
  }

  // create a cumulative density distribution
  for(int i = 0; i < psize; ++i) {
    if(total > 0) {
      cdf[i] /= total;
    } else {
      cdf[i] = 1.0 / psize;
    }
    if(i > 0) {
      cdf[i] += cdf[i - 1];
    }
  }

  // select
  double r = Random::draw_random();
  int p;
  for(p = 0; p < psize; ++p) {
    if(r <= cdf[p]) {
      break;
    }
  }
  if(p == psize) {
    p = psize - 1;
  }
  Person* other = people[p];
  return other;
}

/**
 * Gets the value given two Person objects.
 *
 * @param person the first person
 * @param other the other person
 * @return the value
 */
double Preference::get_value(Person* person, Person* other) {
  int size = this->expressions.size();
  double numerator = 1.0;
  double denominator = 1.0;
  for(int p = 0; p < size; ++p) {
    double value = this->expressions[p]->get_value(person, other);
    if(value > 0) {
      numerator += value;
    } else {
      denominator += fabs(value);
    }
  }
  // note: denominator >= 1 and numerator >= 0
  double result = numerator / denominator;
  return result;
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Preference::setup_logging() {
  if(Preference::is_log_initialized) {
    return;
  }

  if(Parser::does_property_exist("preference_log_level")) {
    Parser::get_property("preference_log_level", &Preference::preference_log_level);
  } else {
    Preference::preference_log_level = "OFF";
  }

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Preference::preference_logger = std::make_unique<spdlog::logger>("preference_logger", 
        sink_list.begin(), sink_list.end());
    Preference::preference_logger->set_level(
        Utils::get_log_level_from_string(Preference::preference_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Preference::preference_logger->trace("<{:s}, {:d}>: Preference logger initialized", 
      __FILE__, __LINE__  );
  Preference::is_log_initialized = true;
}
