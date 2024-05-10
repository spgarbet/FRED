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

#include "Clause.h"
#include "Parser.h"
#include "Person.h"
#include "Predicate.h"

bool Clause::is_log_initialized = false;
std::string Clause::clause_log_level = "";
std::unique_ptr<spdlog::logger> Clause::clause_logger = nullptr;

/**
 * Creates a default Clause with no name.
 */
Clause::Clause() {
  this->name = "";
  this->warning = false;
  this->predicates.clear();
}

/**
 * Creates a Clause with the specified name.
 *
 * @param s the name
 */
Clause::Clause(std::string s) {
  this->name = s;
  this->warning = false;
  this->predicates.clear();
}

/**
 * Gets the name of this clause.
 *
 * @return the name
 */
std::string Clause::get_name() {
  return this->name;
}

/**
 * Parses the clause.
 *
 * @return if the clause was parsed successfully
 */
bool Clause::parse() {

  if(this->name == "") {
    return true;
  }

  Clause::clause_logger->info("RULE CLAUSE: recognizing clause |{:s}|", this->name.c_str());

  // change top level commas to semicolons
  char x [FRED_STRING_SIZE];
  strcpy(x,this->name.c_str());
  int inside = 0;
  char* s = x;
  while (*s!='\0') {
    if(*s==',' && !inside) {
      *s = ';';
    }
    if(*s=='(') {
      ++inside;
    }
    if(*s==')') {
      --inside;
    }
    ++s;
  }

  // find predicates
  std::string predicate_string = std::string(x);
  Predicate* predicate = nullptr;
  while(predicate_string != "") {
    int pos = static_cast<int>(predicate_string.find(";"));
    if(pos == static_cast<int>(std::string::npos)) {
      predicate = new Predicate(predicate_string);
      predicate_string = "";
    } else {
      predicate = new Predicate(predicate_string.substr(0, pos));
      predicate_string = predicate_string.substr(pos + 1);;
    }
    if(predicate != nullptr && predicate->parse()) {
      this->predicates.push_back(predicate);
    } else{
      if(predicate != nullptr) {
        this->warning = predicate->is_warning();
        delete predicate;
        for(int i = 0; i < static_cast<int>(this->predicates.size()); ++i) {
          delete this->predicates[i];
        }
      }
      Clause::clause_logger->error("HELP: UNRECOGNIZED PREDICATE = |{:s}|", this->name.c_str());
      return false;
    }
  }

  return true;
}

/**
 * Checks if all predicate values of the two specified Person objects are true.
 *
 * @param person the first person
 * @param other the other person
 * @return if all predicate values are true
 */
bool Clause::get_value(Person* person, Person* other) {
  for(int i = 0; i < static_cast<int>(this->predicates.size()); ++i) {
    if(this->predicates[i]->get_value(person, other) == false) {
      return false;
    }
  }
  return true;
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Clause::setup_logging() {
  if(Clause::is_log_initialized) {
    return;
  }

  if(Parser::does_property_exist("clause_log_level")) {
    Parser::get_property("clause_log_level",  &Clause::clause_log_level);
  } else {
    Clause::clause_log_level = "OFF";
  }

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Clause::clause_logger = std::make_unique<spdlog::logger>("clause_logger", 
        sink_list.begin(), sink_list.end());
    Clause::clause_logger->set_level(
        Utils::get_log_level_from_string(Clause::clause_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Clause::clause_logger->trace("<{:s}, {:d}>: Clause logger initialized", 
      __FILE__, __LINE__  );
  Clause::is_log_initialized = true;
}
