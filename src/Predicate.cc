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

#include <regex>

#include <spdlog/spdlog.h>

#include "Condition.h"
#include "Date.h"
#include "Expression.h"
#include "Global.h"
#include "Group_Type.h"
#include "Household.h"
#include "Network.h"
#include "Parser.h"
#include "Person.h"
#include "Place.h"
#include "Predicate.h"
#include "Place_Type.h"
#include "Utils.h"

std::map<std::string, int> compare_map = {
  {"eq", 1},
  {"neq", 2},
  {"lt", 3},
  {"lte", 4},
  {"gt", 5},
  {"gte", 6}
};

std::map<std::string, int> misc_map = {
  {"range", 1},
  {"date_range", 2},
  {"date", 3}
};

std::map<std::string,int> group_map = {
  {"at", 1},
  {"member", 1},
  {"admins", 1},
  {"hosts", 1},
  {"admin", 1},
  {"host", 1},
  {"open", 1},
  {"exposed_in", 1},
  {"exposed_externally", 1},
  {"is_connected_to", 1},
  {"is_connected_from", 1},
  {"is_connected", 1},
};

bool Predicate::is_log_initialized = false;
std::string Predicate::predicate_log_level = "";
std::unique_ptr<spdlog::logger> Predicate::predicate_logger = nullptr;

/**
 * Finds the first occurrence of a comma in the specified string excluding ones that are inside parentheses.
 *
 * @param s the string
 * @return the index
 */
int pred_find_comma(std::string s) {
  char x [FRED_STRING_SIZE];
  strcpy(x, s.c_str());
  int inside = 0;
  int n = 0;
  while (x[n] != '\0') {
    if(x[n] == ',' && !inside) {
      return n;
    }
    if(x[n] == '(') {
      ++inside;
    }
    if(x[n] == ')') {
      --inside;
    }
    ++n;
  }
  return -1;
}

/**
 * Creates a Predicate with the given string as the name. This string will have it's spaces deleted. 
 * Default variables are initialized.
 *
 * @param s the name
 */
Predicate::Predicate(std::string s) {
  this->name = Utils::delete_spaces(s);
  this->name = s;
  this->predicate_str = "";
  this->predicate_index = 0;
  this->expression1 = nullptr;
  this->expression2 = nullptr;
  this->expression3 = nullptr;
  this->func = nullptr;
  this->group_type_id = -1;
  this->condition_id = -1;
  this->negate = false;
  this->warning = false;
}

/**
 * Gets the value given two Person objects.
 *
 * @param person the first person
 * @param other the other person
 * @return the value
 */
bool Predicate::get_value(Person* person, Person* other) {

  bool result = false;

  if(this->func != nullptr) {
    // evalute this predicate directly using a built-in function
    result = this->func(person, this->condition_id, this->group_type_id);
  } else if(compare_map.find(this->predicate_str) != compare_map.end()) {
    double value1 = this->expression1->get_value(person, other);
    double value2 = this->expression2->get_value(person, other);
    switch(this->predicate_index) {
      case 1:          // "eq"
        result = (value1 == value2);
        break;
      case 2:          // "neq"
        result = (value1 != value2);
        break;
      case 3:          // "lt"
        result = (value1 < value2);
        break;
      case 4:          // "lte"
        result = (value1 <= value2);
        break;
      case 5:          // "gt"
        result = (value1 > value2);
        break;
      case 6:          // "gte"
        result = (value1 >= value2);
        break;
    }
  } else if(this->predicate_str == "range") {
    double value1 = this->expression1->get_value(person, other);
    double value2 = this->expression2->get_value(person, other);
    double value3 = this->expression3->get_value(person, other);
    result = (value2 <= value1 && value1 <= value3);
  } else if(this->predicate_str == "date") {
    int value1 = this->expression1->get_value(person);
    int today = Date::get_date_code();
    result = (value1 == today);
  } else if(this->predicate_str == "date_range") {
    int value1 = this->expression1->get_value(person);
    int value2 = this->expression2->get_value(person);
    int today = Date::get_date_code();
    result = false;
    if(value1 <= value2) {
      result = (value1 <= today && today <= value2);
    } else {
      result = (value1 <= today || today <= value2); 
    }
  } else if(this->predicate_str == "is_connected_to") {
    Group* group = person->get_group_of_type(this->group_type_id);
    if(group == nullptr) {
      result = false;
    } else {
      if(group->is_a_network()) {
        Person* person2 = Person::get_person_with_id(this->expression1->get_value(person));
        if(person != nullptr && person2 != nullptr) {
          result = static_cast<Network*>(group)->is_connected_to(person, person2);
        } else {
          result = false;
        }
      } else {
        result = false;
      }
    }
  } else if(this->predicate_str == "is_connected_from") {
    Group* group = person->get_group_of_type(this->group_type_id);
    if(group == nullptr) {
      result = false;
    } else {
      if(group->is_a_network()) {
        Person* person2 = Person::get_person_with_id(this->expression1->get_value(person));
        if(person != nullptr && person2 != nullptr) {
          result = static_cast<Network*>(group)->is_connected_from(person, person2);
        } else {
          result = false;
        }
      } else {
        result = false;
      }
    }
  } else if(this->predicate_str == "is_connected") {
    Group* group = person->get_group_of_type(this->group_type_id);
    if(group == nullptr) {
      result = false;
    } else {
      if(group->is_a_network()) {
        Person* person2 = Person::get_person_with_id(this->expression1->get_value(person));
        if(person != nullptr && person2 != nullptr) {
          result = static_cast<Network*>(group)->is_connected_from(person, person2);
          result = result || static_cast<Network*>(group)->is_connected_to(person, person2);
        } else {
          result = false;
        }
      } else {
        result = false;
      }
    }
  }

  if(this->negate) {
    return result == false;
  } else {
    return result;
  }
}

/**
 * Parses the predicate.
 *
 * @return if the predicate was parsed successfully.
 */
bool Predicate::parse() {

  std::string predicate = "";
  if(this->name.find("not(") == 0) {
    int pos = static_cast<int>(this->name.find_last_of(")"));
    if(pos == static_cast<int>(std::string::npos)) {
      Predicate::predicate_logger->error("HELP: RULE UNRECOGNIZED PREDICATE = |{:s}|", this->name.c_str());
      return false;
    }
    predicate = this->name.substr(4, pos - 4);
    this->negate = true;
  } else {
    predicate = this->name;
  }
  predicate = Predicate::get_prefix_notation(predicate);
  int pos1 = static_cast<int>(predicate.find("("));
  int pos2 = static_cast<int>(predicate.find_last_of(")"));
  if(pos1 == static_cast<int>(std::string::npos) || pos2 == static_cast<int>(std::string::npos) || pos2 < pos1) {

    // check for zero-arg predicates

    // activity profile
    if(predicate == "is_student") {
      this->func = &Predicate::is_student;
      return true;
    }

    // meta agent
    if(predicate == "is_import_agent") {
      this->func = &Predicate::is_import_agent;
      return true;
    }

    // deprecated:

    if(predicate == "household_is_in_low_vaccination_school") {
      this->func = &Predicate::household_is_in_low_vaccination_school;
      return true;
    }

    if(predicate == "household_refuses_vaccines") {
      this->func = &Predicate::household_refuses_vaccines;
      return true;
    }

    if(predicate == "attends_low_vaccination_school") {
      this->func = &Predicate::attends_low_vaccination_school;
      return true;
    }

    if(predicate == "refuses_vaccine") {
      this->func = &Predicate::refuses_vaccines;
      return true;
    }

    if(predicate == "is_ineligible_for_vaccine") {
      this->func = &Predicate::is_ineligible_for_vaccine;
      return true;
    }

    if(predicate == "has_received_vaccine") {
      this->func = &Predicate::has_received_vaccine;
      return true;
    }

    Predicate::predicate_logger->error("HELP: RULE UNRECOGNIZED PREDICATE = |{:s}|", this->name.c_str());
    return false;
  }

  this->predicate_str = predicate.substr(0, pos1);
  if(compare_map.find(this->predicate_str) == compare_map.end() &&
      misc_map.find(this->predicate_str) == misc_map.end() &&
      group_map.find(this->predicate_str) == group_map.end()) {
    Predicate::predicate_logger->error("HELP: RULE UNRECOGNIZED PREDICATE |{:s}| in |{:s}|", 
        this->predicate_str.c_str(), this->name.c_str());
    return false;
  }

  // discard outer parentheses
  std::string inner = predicate.substr(pos1 + 1, pos2 - pos1 - 1);

  if(compare_map.find(this->predicate_str) != compare_map.end()) {
    this->predicate_index = compare_map[this->predicate_str];
    int pos_comma = pred_find_comma(inner);
    if(-1 < pos_comma) {
      // get args
      std::string first = inner.substr(0, pos_comma);
      this->expression1 = new Expression(first);
      if(this->expression1->parse() == false) {
        Predicate::predicate_logger->error("HELP: RULE BAD 1st ARG for QUAL {:s} = |{:s}|", 
            this->predicate_str.c_str(), this->name.c_str());
        this->warning = this->expression1->is_warning();
        return false;
      }

      std::string second = inner.substr(pos_comma + 1);

      // handle symbolic state names
      if(first.find("current_state_in") == 0) {
        // get condition _id
        int pos1 = strlen("current_state_in_");
        std::string cond_name = first.substr(pos1);
        int cond_id = Condition::get_condition_id(cond_name);
        if(cond_id < 0) {
          Predicate::predicate_logger->error("HELP: RULE BAD 1st ARG for QUAL {:s} = |{:s}|", 
              this->predicate_str.c_str(), this->name.c_str());
          return false;
        }
        int state_id = Condition::get_condition(cond_id)->get_state_from_name(second);
        if(state_id < 0) {
          Predicate::predicate_logger->error("HELP: RULE BAD 2nd ARG for QUAL {:s} = |{:s}|", 
              this->predicate_str.c_str(), this->name.c_str());
          return false;
        }
        second = std::to_string(state_id);
      }
      this->expression2 = new Expression(second);
      if(this->expression2->parse() == false) {
        Predicate::predicate_logger->error("HELP: RULE BAD 2nd ARG for QUAL {:s} = |{:s}|", 
            this->predicate_str.c_str(), this->name.c_str());
        this->warning = this->expression2->is_warning();
        return false;
      }
      return true;
    } else {
      Predicate::predicate_logger->error("HELP: RULE MISSING 2nd ARG for QUAL {:s} = |{:s}|", 
          this->predicate_str.c_str(), this->name.c_str());
      return false;
    }
  } else if(this->predicate_str == "range") {
    int pos_comma = pred_find_comma(inner);
    if(-1 < pos_comma) {
      // get args
      std::string first = inner.substr(0, pos_comma);
      this->expression1 = new Expression(first);
      if(this->expression1->parse() == false) {
        Predicate::predicate_logger->error("HELP: RULE BAD 1st ARG for QUAL {:s} = |{:s}|", 
            this->predicate_str.c_str(), this->name.c_str());
        this->warning = this->expression1->is_warning();
        return false;
      }
      inner = inner.substr(pos_comma + 1);
      pos_comma = pred_find_comma(inner);
      std::string second = inner.substr(0, pos_comma);
      this->expression2 = new Expression(second);
      if(this->expression2->parse() == false) {
        Predicate::predicate_logger->error("HELP: RULE BAD 2nd ARG for QUAL {:s} = |{:s}|", 
            this->predicate_str.c_str(), this->name.c_str());
        this->warning = this->expression2->is_warning();
        return false;
      }
      std::string third = inner.substr(pos_comma + 1);
      this->expression3 = new Expression(third);
      if(this->expression3->parse() == false) {
        Predicate::predicate_logger->error("HELP: RULE BAD 3rd ARG for QUAL {:s} = |{:s}|", 
            this->predicate_str.c_str(), this->name.c_str());
        this->warning = this->expression3->is_warning();
        return false;
      }
      return true;
    } else {
      Predicate::predicate_logger->error("HELP: RULE MISSING 2nd and 3rd ARG for QUAL {:s} = |{:s}|", 
          this->predicate_str.c_str(), this->name.c_str());
      return false;
    }
  } else if(this->predicate_str == "date_range") {
    int pos_comma = pred_find_comma(inner);
    if(-1 < pos_comma) {
      // get args
      std::string first = inner.substr(0, pos_comma);
      int date_code1 = Date::get_date_code(first);
      if(date_code1 < 0) {
        Predicate::predicate_logger->error("HELP: RULE illegal DATE SPEC |{:s}| PREDICATE |{:s}|", 
            first.c_str(), this->name.c_str());
        return false;
      }
      this->expression1 = new Expression(std::to_string(date_code1));
      if(this->expression1->parse() == false) {
        Predicate::predicate_logger->error("HELP: RULE BAD 1st ARG for QUAL {:s} = |{:s}|", 
            this->predicate_str.c_str(), this->name.c_str());
        this->warning = this->expression1->is_warning();
        return false;
      }
      std::string second = inner.substr(pos_comma + 1);
      int date_code2 = Date::get_date_code(second);
      if(date_code2 < 0) {
        Predicate::predicate_logger->error("HELP: RULE illegal DATE SPEC |{:s}| PREDICATE |{:s}|", 
            second.c_str(), this->name.c_str());
        return false;
      }
      this->expression2 = new Expression(std::to_string(date_code2));
      if(this->expression2->parse() == false) {
        Predicate::predicate_logger->error("HELP: RULE BAD 2nd ARG for QUAL {:s} = |{:s}|", 
            this->predicate_str.c_str(), this->name.c_str());
        this->warning = this->expression2->is_warning();
        return false;
      }
      Predicate::predicate_logger->info("OK DATE SPEC |{:s}-{:s}| date_codes {:d} {:d} PREDICATE |{:s}|", 
          first.c_str(), second.c_str(), date_code1, date_code2, this->name.c_str());
      return true;
    } else {
      Predicate::predicate_logger->error("HELP: RULE MISSING 2nd ARG for QUAL {:s} = |{:s}|", 
          this->predicate_str.c_str(), this->name.c_str());
      return false;
    }
  } else if(this->predicate_str == "date") {
    int date_code1 = Date::get_date_code(inner);
    if(date_code1 < 0) {
      Predicate::predicate_logger->error("HELP: RULE illegal DATE SPEC |{:s}| PREDICATE |{:s}|", 
          inner.c_str(), this->name.c_str());
      return false;
    }
    this->expression1 = new Expression(std::to_string(date_code1));
    if(this->expression1->parse() == false) {
      Predicate::predicate_logger->error("HELP: RULE BAD 1st ARG for QUAL {:s} = |{:s}|", 
          this->predicate_str.c_str(), this->name.c_str());
      this->warning = this->expression1->is_warning();
      return false;
    }
    return true;
  } else if(this->predicate_str == "is_connected_to") {
    std::string first = inner.substr(0, inner.find(","));
    this->expression1 = new Expression(first);
    if(this->expression1->parse() == false) {
      Predicate::predicate_logger->error("HELP: RULE BAD 1st ARG for PREDICATE {:s} = |{:s}|", 
          this->predicate_str.c_str(), this->name.c_str());
      this->warning = this->expression1->is_warning();
      return false;
    }
    std::string group_type = inner.substr(inner.find(",") + 1);
    this->group_type_id = Group_Type::get_type_id(group_type);
    if(Group::is_a_network(this->group_type_id) == false) {
      Predicate::predicate_logger->error("HELP: RULE UNRECOGNIZED group_type |{:s}| PREDICATE = |{:s}|", 
          group_type.c_str(), this->name.c_str());
      return false;
    }
    return true;
  } else if(this->predicate_str == "is_connected_from") {
    std::string first = inner.substr(0, inner.find(","));
    this->expression1 = new Expression(first);
    if(this->expression1->parse() == false) {
      Predicate::predicate_logger->error("HELP: RULE BAD 1st ARG for PREDICATE {:s} = |{:s}|", 
          this->predicate_str.c_str(), this->name.c_str());
      this->warning = this->expression1->is_warning();
      return false;
    }
    std::string group_type = inner.substr(inner.find(",") + 1);
    this->group_type_id = Group_Type::get_type_id(group_type);
    if(Group::is_a_network(this->group_type_id) == false) {
      Predicate::predicate_logger->error("HELP: RULE UNRECOGNIZED group_type |{:s}| PREDICATE = |{:s}|", 
          group_type.c_str(), this->name.c_str());
      return false;
    }
    return true;
  } else if(this->predicate_str == "is_connected") {
    std::string first = inner.substr(0, inner.find(","));
    this->expression1 = new Expression(first);
    if(this->expression1->parse() == false) {
      Predicate::predicate_logger->error("HELP: RULE BAD 1st ARG for PREDICATE {:s} = |{:s}|", 
          this->predicate_str.c_str(), this->name.c_str());
      this->warning = this->expression1->is_warning();
      return false;
    }
    std::string group_type = inner.substr(inner.find(",") + 1);
    this->group_type_id = Group_Type::get_type_id(group_type);
    if(Group::is_a_network(this->group_type_id) == false) {
      Predicate::predicate_logger->error("HELP: RULE UNRECOGNIZED group_type |{:s}| PREDICATE = |{:s}|", 
          group_type.c_str(), this->name.c_str());
      return false;
    }
    return true;
  } else {
    if(this->predicate_str == "at") {
      this->group_type_id = Group_Type::get_type_id(inner);
      if(this->group_type_id < 0) {
        Predicate::predicate_logger->error("HELP: RULE UNRECOGNIZED group_type |{:s}| PREDICATE = |{:s}|", 
            inner.c_str(), this->name.c_str());
        return false;
      }
      this->func = &Predicate::is_at;
      return true;
    }

    if(this->predicate_str == "member") {
      this->group_type_id = Group_Type::get_type_id(inner);
      if(this->group_type_id < 0) {
        Predicate::predicate_logger->error("HELP: RULE UNRECOGNIZED group_type |{:s}| PREDICATE = |{:s}|", 
            inner.c_str(), this->name.c_str());
        return false;
      }
      this->func = &Predicate::is_member;
      return true;
    }

    if(this->predicate_str == "admins" || this->predicate_str == "admin") {
      this->group_type_id = Group_Type::get_type_id(inner);
      if(this->group_type_id < 0) {
        Predicate::predicate_logger->error("HELP: RULE UNRECOGNIZED group_type |{:s}| PREDICATE = |{:s}|", 
            inner.c_str(), this->name.c_str());
        return false;
      }
      this->func = &Predicate::is_admin;
      return true;
    }

    if(this->predicate_str == "hosts" || this->predicate_str == "host") {
      this->group_type_id = Group_Type::get_type_id(inner);
      if(this->group_type_id < 0) {
        Predicate::predicate_logger->error("HELP: RULE UNRECOGNIZED group_type |{:s}| PREDICATE = |{:s}|", 
            inner.c_str(), this->name.c_str());
        return false;
      }
      this->func = &Predicate::is_host;
      return true;
    }

    if(this->predicate_str == "open") {
      this->group_type_id = Group_Type::get_type_id(inner);
      if(this->group_type_id < 0) {
        Predicate::predicate_logger->error("HELP: RULE UNRECOGNIZED group_type |{:s}| PREDICATE = |{:s}|", 
            inner.c_str(), this->name.c_str());
        return false;
      }
      this->func = &Predicate::is_open;
      return true;
    }

    if(this->predicate_str == "exposed_in") {
      std::string cond = inner.substr(0, inner.find(","));
      std::string group_type = inner.substr(inner.find(",") + 1);
      this->condition_id = Condition::get_condition_id(cond);
      if(this->condition_id < 0) {
        Predicate::predicate_logger->error("HELP: RULE UNRECOGNIZED condition |{:s}| PREDICATE = |{:s}|", 
            cond.c_str(), this->name.c_str());
        this->warning = true;
        return false;
      }
      this->group_type_id = Group_Type::get_type_id(group_type);
      if(this->group_type_id < 0) {
        Predicate::predicate_logger->error("HELP: RULE UNRECOGNIZED group_type |{:s}| PREDICATE = |{:s}|", 
            group_type.c_str(), this->name.c_str());
        return false;
      }
      this->func = &Predicate::was_exposed_in;
      return true;
    }

     if(this->predicate_str == "exposed_externally") {
      this->condition_id = Condition::get_condition_id(inner);
      if(this->condition_id < 0) {
        Predicate::predicate_logger->error("HELP: RULE UNRECOGNIZED condition |{:s}| PREDICATE = |{:s}|", 
            inner.c_str(), this->name.c_str());
        this->warning = true;
        return false;
      }
      this->group_type_id = -1;
      this->func = &Predicate::was_exposed_externally;
      return true;
     }
     
     Predicate::predicate_logger->error("HELP: RULE UNRECOGNIZED predicate |{:s}| PREDICATE = |{:s}|", 
        inner.c_str(), this->name.c_str());
     return false;

  }
  Predicate::predicate_logger->error("HELP: RULE UNRECOGNIZED PROBLEM WITH PREDICATE = |{:s}|", 
      this->name.c_str());
  return false;
}

/**
 * Checks if the specified Person is currently present in their Group of the specified Group_Type.
 *
 * @param person the person
 * @param condition_id _UNUSED_
 * @param group_type_id the group type ID
 * @return if the person is at the group
 */
bool Predicate::is_at(Person* person, int condition_id, int group_type_id) {
  if(is_open(person, condition_id, group_type_id)) {
    Group* group = person->get_group_of_type(group_type_id);
    return person->is_present(Global::Simulation_Day, group);
  }
  return false;
}

/**
 * Checks if the specified Person is a member of a Group with the specified Group_Type.
 *
 * @param person the person
 * @param condition_id _UNUSED_
 * @param group_type_id the group type ID
 * @return if the person is a member
 */
bool Predicate::is_member(Person* person, int condition_id, int group_type_id) {
  Group* group = person->get_group_of_type(group_type_id);
  if(group == nullptr) {
    return false;
  }
  return true;
}

/**
 * Checks if the specified Person is an admin of a Group with the specified Group_Type.
 *
 * @param person the person
 * @param condition_id _UNUSED_
 * @param group_type_id the group type ID
 * @return if the person is an admin
 */
bool Predicate::is_admin(Person* person, int condition_id, int group_type_id) {
  Group* group = person->get_admin_group();
  if(group == nullptr) {
    return false;
  }
  return group->get_type_id() == group_type_id;
}

/**
 * Checks if the specified Person is a host of a Group with the specified Group_Type.
 *
 * @param person the person
 * @param condition_id _UNUSED_
 * @param group_type_id the group type ID
 * @return if the person is a host
 */
bool Predicate::is_host(Person* person, int condition_id, int group_type_id) {
  Group* group = Group_Type::get_group_hosted_by(person);
  if(group == nullptr) {
    return false;
  } else {
    return group->get_type_id() == group_type_id;
  }
}

/**
 * Checks if the specified Person's Group of the specified Group_Type is currently open.
 *
 * @param person the person
 * @param condition_id _UNUSED_
 * @param group_type_id the group type ID
 * @return if it is open
 */
bool Predicate::is_open(Person* person, int condition_id, int group_type_id) {
  Group* group = person->get_group_of_type(group_type_id);
  if(group == nullptr) {
    return false;
  }
  if(group->is_a_place()) {
    return static_cast<Place*>(group)->is_open(Global::Simulation_Day);
  } else {
    return group->is_open();
  }
}

/**
 * Checks if the specified Person was exposed to the specified Condition in a Group of the specified Group_Type.
 *
 * @param person the person
 * @param condition_id the condition ID
 * @param group_type_id the group type ID
 * @return if this person was exposed in that group type
 */
bool Predicate::was_exposed_in(Person* person, int condition_id, int group_type_id) {
  return (group_type_id == person->get_exposure_group_type_id(condition_id));
}

/**
 * Checks if the specified Person was exposed to the specified Condition externally.
 *
 * @param person the person
 * @param condition_id the condition ID
 * @param group_type_id _UNUSED_
 * @return if the person was exposed externally
 */
bool Predicate::was_exposed_externally(Person* person, int condition_id, int group_type_id) {
  return person->was_exposed_externally(condition_id);
}

/**
 * Checks if the specified Person is a student.
 *
 * @param person the person
 * @param condition_id _UNUSED_
 * @param group_type_id _UNUSED_
 * @return if the person is a student
 */
bool Predicate::is_student(Person* person, int condition_id, int group_type_id) {
  return person->is_student();
}

/**
 * Checks if the specified Person is the import agent.
 *
 * @param person the person
 * @param condition_id _UNUSED_
 * @param group_type_id _UNUSED_
 * @return if the person is the import agent
 */
bool Predicate::is_import_agent(Person* person, int condition_id, int group_type_id) {
  return (person == Person::get_import_agent());
}

/**
 * Checks if the specified Person is employed.
 *
 * @param person the person
 * @param condition_id _UNUSED_
 * @param group_type_id _UNUSED_
 * @return if the person is employed
 */
bool Predicate::is_employed(Person* person, int condition_id, int group_type_id) {
  return person->is_employed();
}

/**
 * Checks if the specified Person is unemployed.
 *
 * @param person the person
 * @param condition_id _UNUSED_
 * @param group_type_id _UNUSED_
 * @return if the person is unemployed
 */
bool Predicate::is_unemployed(Person* person, int condition_id, int group_type_id) {
  return person->is_employed() == false;
}

/**
 * Checks if the specified Person is a teacher.
 *
 * @param person the person
 * @param condition_id _UNUSED_
 * @param group_type_id _UNUSED_
 * @return if the person is a teacher
 */
bool Predicate::is_teacher(Person* person, int condition_id, int group_type_id) {
  return person->is_teacher();
}

/**
 * Checks if the specified Person is retired.
 *
 * @param person the person
 * @param condition_id _UNUSED_
 * @param group_type_id _UNUSED_
 * @return if the person is retired
 */
bool Predicate::is_retired(Person* person, int condition_id, int group_type_id) {
  return person->is_retired();
}

/**
 * Checks if the specified Person lives in group quarters.
 *
 * @param person the person
 * @param condition_id _UNUSED_
 * @param group_type_id _UNUSED_
 * @return if the person lives in group quarters
 */
bool Predicate::lives_in_group_quarters(Person* person, int condition_id, int group_type_id) {
  return person->lives_in_group_quarters();
}

/**
 * Checks if the specified Person is a college dorm resident.
 *
 * @param person the person
 * @param condition_id _UNUSED_
 * @param group_type_id _UNUSED_
 * @return if the person is a college dorm resident
 */
bool Predicate::is_college_dorm_resident(Person* person, int condition_id, int group_type_id) {
  return person->is_college_dorm_resident();
}

/**
 * Checks if the specified Person is a nursing home resident.
 *
 * @param person the person
 * @param condition_id _UNUSED_
 * @param group_type_id _UNUSED_
 * @return if the person is a nursing home resident
 */
bool Predicate::is_nursing_home_resident(Person* person, int condition_id, int group_type_id) {
  return person->is_nursing_home_resident();
}

/**
 * Checks if the specified Person is a military base resident.
 *
 * @param person the person
 * @param condition_id _UNUSED_
 * @param group_type_id _UNUSED_
 * @return if the person is a military base resident
 */
bool Predicate::is_military_base_resident(Person* person, int condition_id, int group_type_id) {
  return person->is_military_base_resident();
}

/**
 * Checks if the specified Person is a prisoner.
 *
 * @param person the person
 * @param condition_id _UNUSED_
 * @param group_type_id _UNUSED_
 * @return if the person is a prisoner
 */
bool Predicate::is_prisoner(Person* person, int condition_id, int group_type_id) {
  return person->is_prisoner();
}

/**
 * Checks if the specified Person is a householder.
 *
 * @param person the person
 * @param condition_id _UNUSED_
 * @param group_type_id _UNUSED_
 * @return if the person is a householder
 */
bool Predicate::is_householder(Person* person, int condition_id, int group_type_id) {
  return person->is_householder();
}

/**
 * Checks if the specified Person's Household is in a low vaccination school.
 *
 * @param person the person
 * @param condition_id _UNUSED_
 * @param group_type_id _UNUSED_
 * @return if the person's household is in a low vaccination school
 */
bool Predicate::household_is_in_low_vaccination_school(Person* person, int condition_id, int group_type_id) {
  if(person->get_household() != nullptr) {
    return person->get_household()->is_in_low_vaccination_school();
  } else {
    return false;
  }
}

/**
 * Checks if the specified Person's Household refuses vaccines.
 *
 * @param person the person
 * @param condition_id _UNUSED_
 * @param group_type_id _UNUSED_
 * @return if the person's household refuses vaccines
 */
bool Predicate::household_refuses_vaccines(Person* person, int condition_id, int group_type_id) {
  if(person->get_household() != nullptr) {
    return person->get_household()->refuses_vaccines();
  } else {
    return false;
  }
}

/**
 * Checks if the specified Person attends a low vaccination school.
 *
 * @param person the person
 * @param condition_id _UNUSED_
 * @param group_type_id _UNUSED_
 * @return if the person attends a low vaccination school
 */
bool Predicate::attends_low_vaccination_school(Person* person, int condition_id, int group_type_id) {
  if(person->get_school() != nullptr) {
    return person->get_school()->is_low_vaccination_place();
  } else {
    return false;
  }
}

/**
 * Checks if the specified Person refuses vaccines.
 *
 * @param person the person
 * @param condition_id _UNUSED_
 * @param group_type_id _UNUSED_
 * @return if the person refuses vaccines
 */
bool Predicate::refuses_vaccines(Person* person, int condition_id, int group_type_id) {
  return person->refuses_vaccines();
}

/**
 * Checks if the specified Person is ineligible for vaccines.
 *
 * @param person the person
 * @param condition_id _UNUSED_
 * @param group_type_id _UNUSED_
 * @return if the person is ineligible for vaccines
 */
bool Predicate::is_ineligible_for_vaccine(Person* person, int condition_id, int group_type_id) {
  return person->is_ineligible_for_vaccine();
}

/**
 * Checks if the specified Person has received a vaccine.
 *
 * @param person the person
 * @param condition_id _UNUSED_
 * @param group_type_id _UNUSED_
 * @return if the person has received a vaccine
 */
bool Predicate::has_received_vaccine(Person* person, int condition_id, int group_type_id) {
  return person->has_received_vaccine();
}

/**
 * Gets the prefix notation of the specified string.
 *
 * @param s the string
 * @return the prefix notation
 */
std::string Predicate::get_prefix_notation(std::string s) {
  int pos = static_cast<int>(s.find("=="));
  if(pos != static_cast<int>(std::string::npos)) {
    std::string first = s.substr(0, pos);
    std::string second = s.substr(pos + 2);
    std::string result = "eq(" + first + "," + second + ")";
    return result;
  }

  pos = static_cast<int>(s.find("!="));
  if(pos != static_cast<int>(std::string::npos)) {
    std::string first = s.substr(0, pos);
    std::string second = s.substr(pos + 2);
    std::string result = "neq(" + first + "," + second + ")";
    return result;
  }

  pos = static_cast<int>(s.find("<="));
  if(pos != static_cast<int>(std::string::npos)) {
    std::string first = s.substr(0, pos);
    std::string second = s.substr(pos + 2);
    std::string result = "lte(" + first + "," + second + ")";
    return result;
  }

  pos = static_cast<int>(s.find(">="));
  if(pos != static_cast<int>(std::string::npos)) {
    std::string first = s.substr(0, pos);
    std::string second = s.substr(pos + 2);
    std::string result = "gte(" + first + "," + second + ")";
    return result;
  }

  pos = static_cast<int>(s.find(">"));
  if(pos != static_cast<int>(std::string::npos)) {
    std::string first = s.substr(0, pos);
    std::string second = s.substr(pos + 1);
    std::string result = "gt(" + first + "," + second + ")";
    return result;
  }

  pos = static_cast<int>(s.find("<"));
  if(pos != static_cast<int>(std::string::npos)) {
    std::string first = s.substr(0, pos);
    std::string second = s.substr(pos + 1);
    std::string result = "lt(" + first + "," + second + ")";
    return result;
  }

  return s;
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Predicate::setup_logging() {
  if(Predicate::is_log_initialized) {
    return;
  }

  if(Parser::does_property_exist("predicate_log_level")) {
    Parser::get_property("predicate_log_level", &Predicate::predicate_log_level);
  } else {
    Predicate::predicate_log_level = "OFF";
  }

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Predicate::predicate_logger = std::make_unique<spdlog::logger>("predicate_logger", 
        sink_list.begin(), sink_list.end());
    Predicate::predicate_logger->set_level(
        Utils::get_log_level_from_string(Predicate::predicate_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Predicate::predicate_logger->trace("<{:s}, {:d}>: Predicate logger initialized", 
      __FILE__, __LINE__  );
  Predicate::is_log_initialized = true;
}
