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

#include <string>
#include <sstream>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>

#include "Clause.h"
#include "Condition.h"
#include "Expression.h"
#include "Global.h"
#include "Network.h"
#include "Parser.h"
#include "Person.h"
#include "Preference.h"
#include "Rule.h"
#include "Utils.h"

///////// STATIC MEMBERS

std::vector<std::string> Rule::rule_list;
int Rule::first_rule = 1;
std::vector<Rule*> Rule::rules;
std::vector<Rule*> Rule::compiled_rules;

std::vector<std::string> Rule::action_string =
  {
    "wait",
    "wait_until",
    "give_birth",
    "die",
    "fatal",
    "sus", 
    "trans",
    "join",
    "quit",
    "add_edge_from",
    "add_edge_to",
    "delete_edge_from",
    "delete_edge_to",
    "set",
    "set_list",
    "set_state",
    "change_state",
    "set_weight",
    "set_sus",
    "set_trans",
    "report",
    "absent",
    "present",
    "close",
    "set_contacts",
    "randomize_network",
    "import_count",
    "import_per_capita",
    "import_location",
    "import_admin_code",
    "import_ages",
    "count_all_import_attempts",
    "import_list",
  };

bool Rule::is_log_initialized = false;
std::string Rule::rule_log_level = "";
std::unique_ptr<spdlog::logger> Rule::rule_logger = nullptr;

/**
 * Adds the specified line to the rule list. If this is the first rule, the rule list will first be cleared.
 *
 * @param line the line
 */
void Rule::add_rule_line(std::string line) {
  if(Rule::first_rule) {
    Rule::rule_list.clear();
    Rule::first_rule = 0;
  }
  Rule::rule_list.push_back(line);
}

/**
 * Prepares the rules. First, parses each rule in the rule list vector. If the rule parses successfully, it will be 
 * added to the rules vector. Then each parsed rule will be compiled.
 */
void Rule::prepare_rules() {

  for(int n = 0; n < static_cast<int>(Rule::rule_list.size()); ++n) {
    std::string linestr = Rule::rule_list[n];

    Rule* rule = new Rule(linestr);
    if(rule->parse()) {
      Rule::rule_logger->info("Good RULE: {:s}", rule->to_string());
      Rule::rules.push_back(rule);
    } else {
      Rule::rule_logger->error("RULE parse failed: {:s}", rule->get_err_msg().c_str());
      Rule::rule_logger->info("BAD RULE: {:s}", rule->to_string());
      if(rule->is_warning()) {
        Utils::print_warning(rule->get_err_msg().c_str());
      } else {
        Utils::print_error(rule->get_err_msg().c_str());
      }
    }
  }

  Rule::rule_logger->info("RULES found = {:d}", static_cast<int>(Rule::rules.size()));
  for(int i = 0; i < static_cast<int>(Rule::rules.size()); ++i) {
    Rule::rule_logger->info("RULE[{:d}]: {:s}", i, Rule::rules[i]->to_string());
  }

  Rule::compiled_rules.clear();

  for(int i = 0; i < static_cast<int>(Rule::rules.size()); ++i) {
    Rule::rules[i]->compile();
  }

  Rule::rule_logger->info("COMPILED RULES size = {:d}", static_cast<int>(Rule::compiled_rules.size()));
  for(int i = 0; i < static_cast<int>(Rule::compiled_rules.size()); ++i) {
    Rule::rule_logger->info("COMPILED RULE[{:d}]: {:s}", i, Rule::compiled_rules[i]->to_string());
  }

}

/**
 * Prints a warning for each rule that is not used, hidden by another rule, or does not have a state of Start or 
 * Excluded.
 */
void Rule::print_warnings() {
  std::string msg;
  for(int i = 0; i < static_cast<int>(Rule::rules.size()); ++i) {
    if(Rule::rules[i]->is_used() == false && Rule::rules[i]->get_hidden_by_rule() == nullptr &&
       (Rule::rules[i]->state != "Start" && Rule::rules[i]->state != "Excluded")) {
      msg = "Ignoring rule (check for typos):\n  " + Rule::rules[i]->get_name();
      Utils::print_warning(msg);
    }
  }
}


///////// END STATIC MEMBERS

/**
 * Creates a Rule with the specified string as it's name. Initializes default variables.
 *
 * @param s the name
 */
Rule::Rule(std::string s) {
  this->name = s;
  this->cond = "";
  this->cond_id = -1;
  this->state = "";
  this->state_id = -1;
  this->clause_str = "";
  this->clause = nullptr;
  this->next_state = "";
  this->next_state_id = -1;
  this->action = "";
  this->action_id = -1;
  this->expression_str = "";
  this->expression = nullptr;
  this->expression_str2 = "";
  this->expression2 = nullptr;
  this->expression3 = nullptr;
  this->var = "";
  this->var_id = -1;
  this->list_var = "";
  this->list_var_id = -1;
  this->source_cond = "";
  this->source_cond_id = -1;
  this->source_state = "";
  this->source_state_id = -1;
  this->dest_state = "";
  this->dest_state_id = -1;
  this->network = "";
  this->network_id = -1;
  this->group = "";
  this->group_type_id = -1;
  this->err = "";
  this->preference = nullptr;
  this->used = false;
  this->warning = false;
  this->global = false;
  this->hidden_by = nullptr;

  this->action_rule = false;
  this->wait_rule = false;
  this->exposure_rule = false;
  this->next_rule = false;
  this->default_rule = false;
  this->schedule_rule = false;
}


/**
 * Checks if this rule applies to the two specified Person objects. 
 * This will be true if this rule does not have a clause, or if the rule's clause's value given the two persons 
 * is true.
 *
 * @param person the first person
 * @param other the other person
 * @return if this rule applies
 */
bool Rule::applies(Person* person, Person* other) {
  return (this->clause == nullptr || this->clause->get_value(person, other));
}

/**
 * Gets the value given two Person objects.
 *
 * @param person the first person
 * @param other the other person
 * @return the value
 */
double Rule::get_value(Person* person, Person* other) {

  if(this->action_id == Rule_Action::SET) {
    double value = 0.0;
    if(this->expression != nullptr) {
      value = this->expression->get_value(person, other);
      Rule::rule_logger->debug("Rule::get_value expr = |{:s}| person {:d} value {:f}", 
          this->expression->get_name().c_str(), person->get_id(), value);
    } else {
      value = 0.0;
      Rule::rule_logger->debug("Rule::get_value expr = nullptr  person {:d} value {:f}", 
          person->get_id(), value);
    }
    return value;
  }

  // test the clause for next_rules:
  if(this->next_rule) {
    if(this->clause==nullptr || this->clause->get_value(person)) {
      double value = 1.0;
      if(this->expression != nullptr) {
        value = this->expression->get_value(person);
      }
      return value;
    } else {
      return 0.0;
    }
  }

  return 0.0;
}

/**
 * Parses the rule.
 *
 * @return if the rule was parsed successfully
 */
bool Rule::parse() {

  Rule::rule_logger->info("RULE parse? |{:s}|", this->name.c_str());

  // parse the line into separate strings
  this->parts.clear();
  this->parts = Utils::get_string_vector(this->name, ' ');
  
  if(this->name.find("then wait(") != std::string::npos) {
    return this->parse_wait_rule();
  }

  if(this->name.find("if exposed(") == 0) {
    return this->parse_exposure_rule();
  }

  if(this->name.find("then next(") != std::string::npos) {
    return this->parse_next_rule();
  }

  if(this->name.find("then default(") != std::string::npos) {
    return this->parse_default_rule();
  }

  if(this->name.find(" then ") == std::string::npos) {
    this->err = "No THEN clause found\n  " + this->name;
    Utils::print_error(this->get_err_msg().c_str());
  }

  // if not one of the above, must be a action rule
  return this->parse_action_rule();
}

/**
 * Converts the rule to a string representation.
 */
std::string Rule::to_string() {

  std::stringstream ss;
  std::string current = this->cond + "," + this->state;

  if(this->is_wait_rule()) {
    if(this->clause) {
      ss << fmt::format("if state({:s}) and({:s}) then wait({:s})", 
          current, this->clause_str, this->expression_str);
    } else {
      ss << fmt::format("if state({:s}) then wait({:s})", current, this->expression_str);
    }
  }

  if(this->exposure_rule) {
    ss << fmt::format("if exposed({:s}) then next({:s})", this->cond, this->next_state);
  }

  if(this->next_rule) {
    ss << fmt::format("if state({:s},{:s}) and({:s}) then next({:s}) with prob({:s})", 
        this->cond, this->state, this->clause_str, this->next_state, this->expression_str);
  }

  if(this->is_default_rule()) {
    ss << fmt::format("if state({:s}) then default({:s})", current, this->next_state);
  }

  if(this->is_action_rule()) {
    ss << this->name;
  }

  return ss.str();
}

/**
 * Compiles the rule.
 */
bool Rule::compile() {

  this->action_id = -1;
  for(int i = 0; i < static_cast<int>(Rule::action_string.size()); ++i) {
    if(this->action == Rule::action_string[i]) {
      this->action_id = i;
      break;
    }
  }

  Rule::rule_logger->info("COMPILING RULE {:s} action |{:s}| action_id {:d}", 
      this->name.c_str(), this->action.c_str(), this->action_id);

  // get cond_id
  this->cond_id = Condition::get_condition_id(this->cond);
  if(this->cond_id < 0) {
    Rule::rule_logger->error("COMPILE BAD COND: RULE {:s}", this->name.c_str());
    this->err = "Can't parse rule:\n  " + this->name;
    return false;
  }

  // EXPOSURE RULE
  if(this->is_exposure_rule()) {
    this->next_state_id = Condition::get_condition(this->cond_id)->get_state_from_name(this->next_state);
    if(0 <= this->next_state_id) {
      Rule::compiled_rules.push_back(this);
      Rule::rule_logger->info("COMPILED EXPOSURE RULE {:s}", this->name.c_str());
      return true;
    } else {
      Rule::rule_logger->error("COMPILE BAD NEXT_STATE: EXPOSURE RULE {:s}", this->name.c_str());
      this->err = "Can't parse rule:\n  " + this->name;
      return false;
    }
  }

  // get state id for all other rules
  this->state_id = Condition::get_condition(this->cond_id)->get_state_from_name(this->state);
  if(this->state_id < 0) {
    Rule::rule_logger->error("COMPILE BAD STATE: RULE {:s}", this->name.c_str());
    this->err = "Can't parse rule:\n  " + this->name;
    return false;
  }

  // WAIT RULE
  if(this->is_wait_rule()) {
    if(this->clause_str != "") {
      this->clause = new Clause(this->clause_str);
      if(this->clause->parse() == false) {
        Rule::rule_logger->error("COMPILE BAD CLAUSE: RULE {:s}", this->name.c_str());
        this->err = "Can't parse rule:\n  " + this->name;
        return false;
      }
    }
    if(this->action == "wait") {
      Rule::rule_logger->info("COMPILE WAIT RULE {:s}", this->name.c_str());
      this->expression = new Expression(this->expression_str);
      if(this->expression->parse()) {
        Rule::compiled_rules.push_back(this);
        Rule::rule_logger->info("COMPILED WAIT RULE {:s}", this->name.c_str());
        return true;
      } else{
        this->warning = this->expression->is_warning();
        delete this->expression;
        this->err = "Expression  " + this->expression_str + " not recognized:\n  " + this->name;
        if(is_warning()) {
          Utils::print_warning(this->get_err_msg().c_str());
        } else {
          Utils::print_error(this->get_err_msg().c_str());
        }
        return false;
      }
    }
    if(this->action == "wait_until") {
      Rule::rule_logger->info("COMPILE WAIT_UNTIL RULE {:s}", this->name.c_str());
      Rule::compiled_rules.push_back(this);
      Rule::rule_logger->info("COMPILED WAIT_UNTIL RULE {:s}", this->name.c_str());
      return true;
    }
  }

  // NEXT RULE
  if(this->is_next_rule()) {
    this->next_state_id = Condition::get_condition(this->cond_id)->get_state_from_name(this->next_state);
    if(0 <= this->next_state_id) {
      this->expression = new Expression(this->expression_str);
      if(this->expression->parse() == false) {
        Rule::rule_logger->error("COMPILE BAD EXPR: RULE {:s}", this->name.c_str());
        this->err = "Can't parse rule:\n  " + this->name;
        return false;
      }
      if(this->clause_str != "") {
        this->clause = new Clause(this->clause_str);
        if(this->clause->parse()==false) {
          Rule::rule_logger->error("COMPILE BAD CLAUSE: RULE {:s}", this->name.c_str());
          this->err = "Can't parse rule:\n  " + this->name;
          return false;
        }
      }
      Rule::compiled_rules.push_back(this);
      Rule::rule_logger->info("COMPILED NEXT RULE {:s}", this->name.c_str());
      return true;
    } else {
      Rule::rule_logger->error("COMPILE BAD NEXT_STATE: NEXT RULE {:s}", this->name.c_str());
      this->err = "Can't parse rule:\n  " + this->name;
      return false;
    }
  }

  // DEFAULT RULE
  if(this->is_default_rule()) {
    this->next_state_id = Condition::get_condition(this->cond_id)->get_state_from_name(this->next_state);
    if(0 <= this->next_state_id) {
      Rule::compiled_rules.push_back(this);
      Rule::rule_logger->info("COMPILED DEFAULT RULE {:s}", this->name.c_str());
      return true;
    } else {
      Rule::rule_logger->error("COMPILE BAD NEXT_STATE: DEFAULT RULE {:s}", this->name.c_str());
      this->err = "Can't parse rule:\n  " + this->name;
      return false;
    }
  }

  // ACTION RULES
  if(this->is_action_rule()) {
    return compile_action_rule();
  }

  Rule::rule_logger->error("COMPILE RULE UNKNOWN TYPE: |{:s}|", this->name.c_str());
  this->err = "Can't parse rule:\n  " + this->name;
  return false;
}

/**
 * Sets this rule as hidden by the specified rule.
 *
 * @param rule the rule
 */
void Rule::set_hidden_by_rule(Rule* rule) {
  this->hidden_by = rule;
  char msg[FRED_STRING_SIZE];
  snprintf(msg, FRED_STRING_SIZE, "Ignoring duplicate rule:\n  %s\n     is hidden by:\n  %s", get_name().c_str(), rule->get_name().c_str());
  Utils::print_warning(msg);
}

/**
 * Parses the action rule.
 *
 * @return if the action rule was parsed successfully
 */
bool Rule::parse_action_rule() {
  Rule::rule_logger->info("ENTERED PARSE ACTION RULE: {:s}", this->name.c_str());
  if(parse_state() && this->parts[2] == "then" && this->parts.size() == 4) {

    // re-write if() then sus()
    if(this->parts[3].find("sus(") == 0) {
      this->parts[3].replace(0, 4, "set_sus(" + this->cond + ",");
      this->name = this->parts[0] + " " + this->parts[1] + " " + this->parts[2] + " " + this->parts[3];
      Rule::rule_logger->info("REWROTE RULE: |{:s}|", this->name.c_str());
    }

    // re-write if() then trans()
    if(this->parts[3].find("trans(") == 0) {
      this->parts[3].replace(0,6,"set_trans(" + this->cond + ",");
      this->name = this->parts[0] + " " + this->parts[1] + " " + this->parts[2] + " " + this->parts[3];
      Rule::rule_logger->info("REWROTE RULE: |{:s}|", this->name.c_str());
    }

    // re-write if() then mult_sus()
    if(this->parts[3].find("mult_sus(") == 0) {
      int pos = this->parts[3].find(",");
      if(pos == static_cast<int>(std::string::npos)) {
        this->err = "Can't parse rule:\n  " + this->name;
        return false;
      }
      std::string source = this->parts[3].substr(9, pos - 9);
      this->parts[3] = "set_sus(" + source + "," + "susceptibility_to_" + source + "*" + this->parts[3].substr(pos+1);
      this->name = this->parts[0] + " " + this->parts[1] + " " + this->parts[2] + " " + this->parts[3];
      Rule::rule_logger->info("REWROTE RULE: |{:s}|", this->name.c_str());
    }

    // re-write if() then mult_trans()
    if(this->parts[3].find("mult_trans(") == 0) {
      int pos = this->parts[3].find(",");
      if(pos == static_cast<int>(std::string::npos)) {
        this->err = "Can't parse rule:\n  " + this->name;
        return false;
      }
      std::string source = this->parts[3].substr(11, pos - 11);
      this->parts[3] = "set_trans(" + source + "," + "transmissibility_for_" + source + "*" + this->parts[3].substr(pos+1);
      this->name = this->parts[0] + " " + this->parts[1] + " " + this->parts[2] + " " + this->parts[3];
      Rule::rule_logger->info("REWROTE RULE: |{:s}|", this->name.c_str());
    }

    std::string str = this->parts[3];
    int pos = str.find("(");
    if(pos != static_cast<int>(std::string::npos) && str[str.length() - 1] == ')') {
      this->action = str.substr(0, pos);
      this->expression_str = str.substr(pos + 1, str.length() - pos - 2);
      this->action_rule = true;
      return true;
    }
  }
  if(parse_state() && this->parts[3] == "then" && this->parts.size() == 5) {

    // re-write if() then sus()
    if(this->parts[4].find("sus(") == 0) {
      this->parts[4].replace(0,4,"set_sus(" + this->cond + ",");
      this->name = this->parts[0] + " " + this->parts[1] + " " + this->parts[2] + " " + this->parts[3] + " " + this->parts[4];
      Rule::rule_logger->info("REWROTE RULE: |{:s}|", this->name.c_str());
    }

    // re-write if() then trans()
    if(this->parts[4].find("trans(") == 0) {
      this->parts[4].replace(0, 6, "set_trans(" + this->cond + ",");
      this->name = this->parts[0] + " " + this->parts[1] + " " + this->parts[2] + " " + this->parts[3] + " " + this->parts[4];
      Rule::rule_logger->info("REWROTE RULE: |{:s}|", this->name.c_str());
    }

    // re-write if() then mult_sus()
    if(this->parts[4].find("mult_sus(") == 0) {
      int pos = this->parts[4].find(",");
      if(pos == static_cast<int>(std::string::npos)) {
        this->err = "Can't parse rule:\n  " + this->name;
        return false;
      }
      std::string source = this->parts[4].substr(9,pos-9);
      this->parts[4] = "set_sus(" + source + "," + "susceptibility_to_" + source + "*" + this->parts[4].substr(pos+1);
      this->name = this->parts[0] + " " + this->parts[1] + " " + this->parts[2] + " " + this->parts[3] + " " + this->parts[4];
      Rule::rule_logger->info("REWROTE RULE: |{:s}|", this->name.c_str());
    }

    // re-write if() then mult_trans()
    if(this->parts[4].find("mult_trans(") == 0) {
      int pos = this->parts[4].find(",");
      if(pos == static_cast<int>(std::string::npos)) {
        this->err = "Can't parse rule:\n  " + this->name;
        return false;
      }
      std::string source = this->parts[4].substr(11, pos - 11);
      this->parts[4] = "set_trans(" + source + "," + "transmissibility_for_" + source + "*" + this->parts[4].substr(pos + 1);
      this->name = this->parts[0] + " " + this->parts[1] + " " + this->parts[2] + " " + this->parts[3] + " " + this->parts[4];
      Rule::rule_logger->info("REWROTE RULE: |{:s}|", this->name.c_str());
    }

    std::string str = this->parts[2];
    if(str.find("and(") == 0 && str[str.length() - 1] == ')') {
      this->clause_str = str.substr(4, str.length() - 5);
    } else {
      Rule::rule_logger->error("FAILED PARSE ACTION RULE: {:s}", this->name.c_str());
      this->err = "Can't parse rule:\n  " + this->name;
      return false;
    }

    str = this->parts[4];
    int pos = str.find("(");
    if(pos != static_cast<int>(std::string::npos) && str[str.length() - 1]==')') {
      this->action = str.substr(0,pos);
      this->expression_str = str.substr(pos + 1, str.length() - pos - 2);
      this->action_rule = true;
      return true;
    }
  }
  this->err = "Can't parse action rule\n  " + this->name;
  Utils::print_error(this->get_err_msg().c_str());
  Rule::rule_logger->error("FAILED PARSE ACTION RULE: {:s}", this->name.c_str());
  return false;
}

/**
 * Parses the wait rule.
 *
 * @return if the wait rule was parsed successfully
 */
bool Rule::parse_wait_rule() {
  Rule::rule_logger->info("ENTERED PARSE WAIT RULE: {:s}", this->name.c_str());
  if(parse_state()) {
    if(this->parts[2] == "then") {
      std::string str = this->parts[3];
      if(str.find("wait(") == 0 && str[str.length() - 1]==')') {
        std::string arg = str.substr(5, str.length() - 6);
        if(arg == "") {
          arg = "999999";
        }
        this->wait_rule = true;
        if(str.find("wait(until_") == 0) {
          this->expression_str = arg.substr(6);
          this->action = "wait_until";
        } else {
          this->expression_str = arg;
          this->action = "wait";
        }
        return true;
      }
    } else {
      std::string str = this->parts[2];
      if(str.find("and(") == 0 && str[str.length() - 1] == ')') {
        this->clause_str = str.substr(4, str.length() - 5);
        if(this->parts[3] == "then") {
          std::string str = this->parts[4];
          if(str.find("wait(") == 0 && str[str.length() - 1] == ')') {
            std::string arg = str.substr(5,str.length()-6);
            if(arg == "") {
              arg = "999999";
            }
            this->wait_rule = true;
            if(str.find("wait(until_") == 0) {
              this->expression_str = arg.substr(6);
              this->action = "wait_until";
            } else {
              this->expression_str = arg;
              this->action = "wait";
            }
            return true;
          }
        }
      }
    }
  }
  this->err = "Can't parse wait rule\n  " + this->name;
  Utils::print_error(this->get_err_msg().c_str());
  Rule::rule_logger->error("FAILED WAIT RULE: {:s}", this->name.c_str());
  return false;
}

/**
 * Parses the exposure rule.
 *
 * @return if the wait rule was parsed successfully
 */
bool Rule::parse_exposure_rule() {
  Rule::rule_logger->info("ENTERED PARSE EXPOSURE RULE: {:s}", this->name.c_str());
  for(int i = 0; i < static_cast<int>(this->parts.size()); ++i) {
    Rule::rule_logger->info("parts[{:d}] = |{:s}|", i, this->parts[i].c_str());
  }
  if(this->parts.size() != 4 || this->parts[0] != "if" || this->parts[2]!="then") {
    Rule::rule_logger->info("parts size {:d} parts[0] |{:s}| parts[2] |{:s}|", 
        static_cast<int>(this->parts.size()), this->parts[0].c_str(), this->parts[2].c_str());
    this->err = "Can't parse rule:\n  " + this->name;
    return false;
  }
  std::string str = this->parts[1];
  if(str.find("exposed(") == 0 && str[str.length() - 1] == ')') {
    this->cond = str.substr(8, str.length() - 9);
    Rule::rule_logger->info("exposed cond = {:s}", this->cond.c_str());

    // get next state
    str = this->parts[3];
    if(str.find("next(") == 0 && str[str.length() - 1] == ')') {
      this->next_state = str.substr(5, str.length() - 6);
      if(this->next_state == "") {
        this->err = "No Next State:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      Rule::rule_logger->info("exposure next_state = {:s}", this->next_state.c_str());
      this->exposure_rule = true;
      return true;
    }
  }
  this->err = "Can't parse exposure rule\n  " + this->name;
  Utils::print_error(this->get_err_msg().c_str());
  return false;
}

/**
 * Parses the next rule.
 *
 * @return if the next rule was parsed successfully
 */
bool Rule::parse_next_rule() {
  Rule::rule_logger->info("ENTERED PARSE NEXT RULE: {:s}", this->name.c_str());
  if(parse_state()) {
    this->expression_str = "";
    this->clause_str = "";
    int next_part = 3;
    std::string str = this->parts[2];
    if(str.find("and(") == 0 && str[str.length() - 1] == ')') {
      this->clause_str = str.substr(4,str.length()-5);
      ++next_part;
    }
    if(this->parts[next_part - 1] != "then") {
      this->err = "Can't parse rule:\n  " + this->name;
      return false;
    }
    str = this->parts[next_part];
    if(str.find("next(") == 0 && str[str.length() - 1] == ')') {
      this->next_state = str.substr(5, str.length() - 6);
      if(this->next_state == "") {
        this->err = "No Next State:\n  " + this->name;
        return false;
      }
    } else {
      this->err = "Can't parse rule:\n  " + this->name;
      return false;
    }
    if(static_cast<int>(this->parts.size()) == next_part + 1) {
      this->expression_str = "1";
      this->next_rule = true;
      return true;
    } else {
      if(static_cast<int>(this->parts.size()) != next_part + 3) {
        this->err = "Bad Next Rule:\n  " + this->name;
        return false;
      }
      if(this->parts[next_part +1 ] != "with") {
        this->err = "Bad Next Rule:\n  " + this->name;
        return false;
      }
      str = this->parts[next_part + 2];
      if(str.find("prob(") == 0 && str[str.length() - 1] == ')') {
        this->expression_str = str.substr(5, str.length() - 6);
        if(this->expression_str =="") {
          this->err = "Bad Next Rule:\n  " + this->name;
          return false;
        } else {
          this->next_rule = true;
          return true;
        }
      } else {
        this->err = "Bad Next Rule:\n  " + this->name;
        return false;
      }
    }
  }
  this->err = "Can't parse rule:\n  " + this->name;
  return false;
}

/**
 * Parses the default rule.
 *
 * @return if the default rule was parsed successfully
 */
bool Rule::parse_default_rule() {
  Rule::rule_logger->info("ENTERED PARSE DEFAULT RULE: {:s}", this->name.c_str());
  if(parse_state() && this->parts[2] == "then") {
    std::string str = this->parts[3];
    if(str.find("default(") == 0 && str[str.length() - 1] == ')') {
      this->next_state = str.substr(8, str.length() - 9);
      if(this->next_state == "") {
        this->err = "No Next State:\n  " + this->name;
        return false;
      }
      this->default_rule = true;
      return true;
    }
  }
  this->err = "Can't parse default rule:\n  " + this->name;
  Utils::print_error(this->get_err_msg().c_str());
  return false;
}

/**
 * Parses the condition state.
 *
 * @return if the condition state was parsed successfully
 */
bool Rule::parse_state() {
  if(this->parts.size() < 4 || this->parts[0] != "if") {
    this->err = "Can't parse state in rule:\n  " + this->name;
    Utils::print_error(this->get_err_msg().c_str());
    return false;
  }
  std::string str = this->parts[1];;
  if((str.find("state(") == 0 || str.find("enter(") == 0) && str[str.length() - 1] == ')') {
    std::string arg = str.substr(6, str.length()-7);
    int pos = arg.find(".");
    if(pos != static_cast<int>(std::string::npos)) {
      this->cond = arg.substr(0, pos);
      this->state = arg.substr(pos + 1);
      return true;
    }
    pos = arg.find(",");
    if(pos != static_cast<int>(std::string::npos)) {
      this->cond = arg.substr(0, pos);
      this->state = arg.substr(pos + 1);
      return true;
    }
  }
  this->err = "Can't parse state rule:\n  " + this->name;
  Utils::print_error(this->get_err_msg().c_str());
  return false;
}

/**
 * Compiles the action rule.
 *
 * @return if the action rule was compiled successfully
 */
bool Rule::compile_action_rule() {

  Rule::rule_logger->info("COMPILING ACTION RULE {:s} action {:s} action_id {:d}", 
      this->name.c_str(), this->action.c_str(), this->action_id);

  if(this->clause_str != "") {
    this->clause = new Clause(this->clause_str);
    if(this->clause->parse() == false) {
      Rule::rule_logger->error("COMPILE BAD CLAUSE: RULE {:s}", this->name.c_str());
      this->err = "Bad AND clause::\n  " + this->name;
      Utils::print_error(this->get_err_msg().c_str());
      return false;
    }
  }

  switch(this->action_id) {

  case Rule_Action::GIVE_BIRTH:
    {
      Rule::compiled_rules.push_back(this);
      Rule::rule_logger->info("COMPILED ACTION RULE {:s}", this->name.c_str());
      return true;
    }
    break;

  case Rule_Action::DIE:
  case Rule_Action::DIE_OLD:
    {
      Rule::compiled_rules.push_back(this);
      Rule::rule_logger->info("COMPILED ACTION RULE {:s}", this->name.c_str());
      return true;
    }
    break;

  case Rule_Action::JOIN:
    {
      string_vector_t args = Utils::get_top_level_parse(this->expression_str,',');
      if(args.size() > 1) {
        this->expression2 = new Expression(args[1]);
        if(this->expression2->parse()==false) {
          this->err = "Second arg to join " + args[1] + " not recognized:\n  " + this->name;
          Utils::print_error(this->get_err_msg().c_str());
          return false;
        }
      }
      this->group = args[0];
      this->group_type_id = Group_Type::get_type_id(group);
      if(Group::is_a_network(group_type_id)) {
        this->network = this->group;
      }
      if(Group::is_a_network(group_type_id) || Group::is_a_place(group_type_id)) {
        Rule::compiled_rules.push_back(this);
        Rule::rule_logger->info("COMPILED ACTION RULE {:s}", this->name.c_str());
        return true;
      } else {
        this->err = "Group " + this->group + " not recognized:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
    }
    break;

  case Rule_Action::QUIT:
    {
      this->group = this->expression_str;
      this->group_type_id = Group_Type::get_type_id(group);
      if(Group::is_a_network(group_type_id)) {
        this->network = this->group;
      }
      if(Group::is_a_network(group_type_id) || Group::is_a_place(group_type_id)) {
        Rule::compiled_rules.push_back(this);
        Rule::rule_logger->info("COMPILED ACTION RULE {:s}", this->name.c_str());
        return true;
      } else {
        this->err = "Group " + this->group + " not recognized:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
    }
    break;

  case Rule_Action::ADD_EDGE_FROM:
  case Rule_Action::ADD_EDGE_TO:
  case Rule_Action::DELETE_EDGE_FROM:
  case Rule_Action::DELETE_EDGE_TO:
    {
      string_vector_t args = Utils::get_top_level_parse(this->expression_str, ',');
      if(args.size() != 2) {
        this->err = "Needs 2 arguments:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      this->network = args[0];
      if(Network::get_network(this->network) == nullptr) {
        this->err = "Network " + this->network + " not recognized:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      this->expression_str = args[1];
      this->expression = new Expression(this->expression_str);
      if(this->expression->parse()) {
        Rule::compiled_rules.push_back(this);
        Rule::rule_logger->info("COMPILED ACTION RULE {:s}", this->name.c_str());
        return true;
      } else {
        this->err = "Expression " + this->expression_str + " not recognized:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
    }
    break;

  case Rule_Action::SET_LIST:
    {
      string_vector_t args = Utils::get_top_level_parse(this->expression_str,',');
      if(args.size() != 2) {
        this->err = "Needs 2 arguments:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      this->list_var = args[0];
      this->list_var_id = Person::get_list_var_id(list_var);
      if(this->list_var_id < 0) {
        this->list_var_id = Person::get_global_list_var_id(list_var);
        if(this->list_var_id < 0) {
          this->err = "List_var " + this->list_var + " not recognized:\n  " + this->name;
          Utils::print_error(this->get_err_msg().c_str());
          return false;
        }
        else {
          this->global = true;
        }
      }
      this->expression_str = args[1];
      this->expression = new Expression(this->expression_str);
      if(this->expression->parse()) {
        if(this->expression->is_list_expression()) {
          Rule::compiled_rules.push_back(this);
          Rule::rule_logger->info("COMPILED ACTION RULE {:s}", this->name.c_str());
          return true;
        } else {
          this->err = "Need a list-valued expression: " + this->expression_str + " not recognized:\n  " + this->name;
          Utils::print_error(this->get_err_msg().c_str());
          return false;
        }
      } else {
        this->err = "Expression " + this->expression_str + " not recognized:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
    }
    break;

  case Rule_Action::SET_WEIGHT:
    {
      string_vector_t args = Utils::get_top_level_parse(this->expression_str, ',');
      if(args.size() != 3) {
        this->err = "Needs 3 arguments:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      this->network = args[0];
      if(Network::get_network(this->network) == nullptr) {
        this->err = "Network " + this->network + " not recognized:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      this->expression_str = args[1];
      this->expression = new Expression(this->expression_str);
      if(this->expression->parse() == false) {
        this->err = "Person Expression " + this->expression_str + " not recognized:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      this->expression_str2 = args[2];
      this->expression2 = new Expression(this->expression_str2);
      if(this->expression2->parse()==false) {
        this->err = "Value Expression " + this->expression_str2 + " not recognized:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      Rule::compiled_rules.push_back(this);
      Rule::rule_logger->info("COMPILED ACTION RULE {:s}", this->name.c_str());
      return true;
    }
    break;

  case Rule_Action::REPORT:
    {
      this->expression = new Expression(this->expression_str);
      if(this->expression->parse()) {
        Rule::compiled_rules.push_back(this);
        Rule::rule_logger->info("COMPILED ACTION RULE {:s}", this->name.c_str());
        return true;
      } else {
        this->err = "Expression " + this->expression_str + " not recognized:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
    }
    break;

  case Rule_Action::ABSENT:
  case Rule_Action::PRESENT:
  case Rule_Action::CLOSE:
    {
      if(this->expression_str == "") {
        // add all group types
        int types = Group_Type::get_number_of_group_types();
        for(int k = 0; k < types; ++k) {
          this->expression_str += Group_Type::get_group_type_name(k);
          if(k < types - 1) {
            this->expression_str += ",";
          }
        }
      }
      // verify group names in expression_str
      string_vector_t group_type_vec = Utils::get_string_vector(this->expression_str, ',');
      for(int k = 0; k < static_cast<int>(group_type_vec.size()); ++k) {
        std::string group_name = group_type_vec[k];
        int type_id = Group_Type::get_type_id(group_name);
        if(type_id < 0) {
          this->err = "Group name " + group_name + " not recognized:\n  " + this->name;
          Utils::print_error(this->get_err_msg().c_str());
          return false;
        }
      }
      this->schedule_rule = true;
      Rule::compiled_rules.push_back(this);
      Rule::rule_logger->info("COMPILED ACTION RULE {:s}", this->name.c_str());
      return true;
    }
    break;

  case Rule_Action::SET_CONTACTS:
    {
      Rule::rule_logger->info("COMPILE SET_CONTACTS RULE |{:s}|  expr |{:s}|", 
          this->name.c_str(), this->expression_str.c_str());
      string_vector_t args = Utils::get_top_level_parse(this->expression_str, ',');
      if(args.size() != 1) {
        this->err = "set_contacts rule needs 1 argument:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      this->expression = nullptr;
      this->expression = new Expression(this->expression_str);
      this->expression_str = args[0];
      if(this->expression->parse()) {
        Rule::compiled_rules.push_back(this);
        Rule::rule_logger->info("COMPILED SET_CONTACTS RULE {:s}", this->name.c_str());
        this->action = "set_contacts";
        return true;
      } else {
        this->warning = this->expression->is_warning();
        delete this->expression;
        this->err = "Expression  " + this->expression_str + " not recognized:\n  " + this->name;
        if(is_warning()) {
          Utils::print_warning(this->get_err_msg().c_str());
        } else {
          Utils::print_error(this->get_err_msg().c_str());
        }
        this->err = "Can't parse rule:\n  " + this->name;
        return false;
      }
      break;
    }

  case Rule_Action::RANDOMIZE_NETWORK:
    {
      string_vector_t args = Utils::get_top_level_parse(this->expression_str, ',');
      if(args.size() != 3) {
        this->err = "Needs 3 arguments:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      this->network = args[0];
      if(Network::get_network(this->network) == nullptr) {
        this->err = "Network " + this->network + " not recognized:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      this->expression = new Expression(args[1]);
      if(this->expression->parse() == false) {
        this->err = "Mean degree expression " + this->expression->get_name() + " not recognized:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      this->expression2 = new Expression(args[2]);
      if(this->expression2->parse() == false) {
        this->err = "Max degree expression " + this->expression2->get_name() + " not recognized:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      Rule::compiled_rules.push_back(this);
      Rule::rule_logger->info("COMPILED RANDOMIZE RULE {:s}", this->name.c_str());
      return true;
    }
    break;


  case Rule_Action::SET:
    {
      Rule::rule_logger->info("COMPILE SET RULE {:s}", this->name.c_str());
      this->global = false;
      string_vector_t args = Utils::get_top_level_parse(this->expression_str, ',');
      if(args.size() < 2) {
        this->err = "Action set needs two arguments::\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      this->var = args[0];
      Rule::rule_logger->info("COMPILE SET RULE var |{:s}|", this->var.c_str());
      this->global = false;
      this->var_id = Person::get_var_id(var);
      if(this->var_id < 0) {
        this->var_id = Person::get_global_var_id(var);
        if(this->var_id < 0) {
          this->err = "Var " + this->var + " not recognized:\n  " + this->name;
          Utils::print_error(this->get_err_msg().c_str());
          return false;
        } else {
          this->global = true;
        }
      }
      this->expression_str = args[1];
      Rule::rule_logger->info("COMPILE SET RULE expression_str |{:s}", this->expression_str.c_str());
      this->expression = new Expression(this->expression_str);
      if(this->expression->parse()) {
        if(args.size() == 2) {
          Rule::compiled_rules.push_back(this);
          Rule::rule_logger->info("COMPILED SET RULE {:s} with expression |{:s}", 
              this->name.c_str(), this->expression->get_name().c_str());
          return true;
        } else {
          this->expression_str2 = args[2];
          Rule::rule_logger->info("COMPILE SET RULE expression_str |{:s}|  expression_str2 |{:s}", 
              this->expression_str.c_str(), this->expression_str2.c_str());
          this->expression2 = new Expression(this->expression_str2);
          if(this->expression2->parse()) {
            Rule::compiled_rules.push_back(this);
            Rule::rule_logger->info("COMPILED SET RULE {:s} with expressions |{:s}| |{:s}", 
                this->name.c_str(), this->expression->get_name().c_str(), 
                this->expression2->get_name().c_str());
            return true;
          } else {
            this->warning = this->expression2->is_warning();
            delete this->expression2;
            this->err = "Expression  " + this->expression_str2 + " not recognized:\n  " + this->name;
            if(is_warning()) {
              Utils::print_warning(this->get_err_msg().c_str());
            } else {
              Utils::print_error(this->get_err_msg().c_str());
            }
            return false;
          }
        }
      } else {
        this->warning = this->expression->is_warning();
        delete this->expression;
        this->err = "Expression  " + this->expression_str + " not recognized:\n  " + this->name;
        if(is_warning()) {
          Utils::print_warning(this->get_err_msg().c_str());
        } else {
          Utils::print_error(this->get_err_msg().c_str());
        }
        return false;
      }
      break;
    }

  case Rule_Action::SET_STATE:
  case Rule_Action::CHANGE_STATE:
    {
      this->action_id = Rule_Action::SET_STATE;
      this->action = "set_state";
      string_vector_t args = Utils::get_top_level_parse(this->expression_str, ',');
      if(args.size() != 2 && args.size() != 3) {
        this->err = "Set_state expression  " + this->expression_str + " not recognized:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      this->source_cond = args[0];
      this->source_cond_id = Condition::get_condition_id(this->source_cond);
      if(this->source_cond_id < 0) {
        this->err = "Source condition  " + source_cond + " not recognized:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      if(args.size() == 3) {
        this->source_state = args[1];
        this->dest_state = args[2];
      } else {
        this->source_state = "*";
        this->dest_state = args[1];
      }
      this->source_state_id = Condition::get_condition(this->source_cond_id)->get_state_from_name(this->source_state);
      if(this->source_state_id < 0 && this->source_state!="*") {
        this->err = "Source state  " + source_state + " not recognized:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      this->dest_state_id = Condition::get_condition(this->source_cond_id)->get_state_from_name(this->dest_state);
      if(this->dest_state_id < 0) {
        this->err = "Destination state  " + this->dest_state + " not recognized:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      Rule::compiled_rules.push_back(this);
      return true;
    }
    break;

  case Rule_Action::SUS:
    {
      Rule::rule_logger->info("COMPILE SUS RULE {:s}", this->name.c_str());
      this->expression = new Expression(this->expression_str);
      if(this->expression->parse()) {
        Rule::compiled_rules.push_back(this);
        Rule::rule_logger->info("COMPILED SUS RULE {:s}", this->name.c_str());
        return true;
      } else {
        this->warning = this->expression->is_warning();
        delete this->expression;
        this->err = "Expression  " + this->expression_str + " not recognized:\n  " + this->name;
        if(is_warning()) {
          Utils::print_warning(this->get_err_msg().c_str());
        } else {
          Utils::print_error(this->get_err_msg().c_str());
        }
        return false;
      }
    }
    break;

  case Rule_Action::SET_SUS:
    {
      Rule::rule_logger->info("COMPILE SET_SUS RULE {:s}", this->name.c_str());
      string_vector_t args = Utils::get_top_level_parse(this->expression_str,',');
      if(args.size() != 2) {
        this->err = "Can't parse rule:\n  " + this->name;
        return false;
      }
      this->source_cond = args[0];
      Rule::rule_logger->info("COMPILE SET_SUS RULE {:s}  cond |{:s}|", 
          this->name.c_str(), this->source_cond.c_str());
      this->source_cond_id = Condition::get_condition_id(this->source_cond);
      if(this->source_cond_id < 0) {
        this->err = "Source condition  " + source_cond + " not recognized:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      this->expression_str2 = args[1];
      this->expression2 = new Expression(this->expression_str2);
      if(this->expression2->parse()) {
        Rule::compiled_rules.push_back(this);
        Rule::rule_logger->info("COMPILED SET_SUS RULE {:s}", this->name.c_str());
        return true;
      } else {
        this->warning = this->expression2->is_warning();
        delete this->expression2;
        this->err = "Expression  " + this->expression_str2 + " not recognized:\n  " + this->name;
        if(is_warning()) {
          Utils::print_warning(this->get_err_msg().c_str());
        }
        else {
          Utils::print_error(this->get_err_msg().c_str());
        }
        return false;
      }
    }
    break;

  case Rule_Action::SET_TRANS:
    {
      Rule::rule_logger->info("COMPILE SET_TRANS RULE {:s}", this->name.c_str());
      string_vector_t args = Utils::get_top_level_parse(this->expression_str,',');
      if(args.size() != 2) {
        this->err = "Expression  " + this->expression_str + " not recognized:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      this->source_cond = args[0];
      Rule::rule_logger->info("COMPILE SET_TRANS RULE {:s}  cond |{:s}|", 
          this->name.c_str(), this->source_cond.c_str());
      this->source_cond_id = Condition::get_condition_id(this->source_cond);
      if(this->source_cond_id < 0) {
        this->err = "Source condition  " + source_cond + " not recognized:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      this->expression_str2 = args[1];
      this->expression2 = new Expression(this->expression_str2);
      if(this->expression2->parse()) {
        Rule::compiled_rules.push_back(this);
        Rule::rule_logger->info("COMPILED SET_TRANS RULE {:s}", this->name.c_str());
        return true;
      } else {
        this->warning = this->expression2->is_warning();
        delete this->expression2;
        this->err = "Expression  " + this->expression_str2 + " not recognized:\n  " + this->name;
        if(is_warning()) {
          Utils::print_warning(this->get_err_msg().c_str());
        } else {
          Utils::print_error(this->get_err_msg().c_str());
        }
        return false;
      }
    }
    break;

  case Rule_Action::TRANS:
    {
      Rule::rule_logger->info("COMPILE TRANS RULE {:s}", this->name.c_str());
      this->expression = new Expression(this->expression_str);
      if(this->expression->parse()) {
        Rule::compiled_rules.push_back(this);
        Rule::rule_logger->info("COMPILED TRANS RULE {:s}", this->name.c_str());
        return true;
      } else {
        this->warning = this->expression->is_warning();
        delete this->expression;
        this->err = "Expression  " + this->expression_str + " not recognized:\n  " + this->name;
        if(is_warning()) {
          Utils::print_warning(this->get_err_msg().c_str());
        } else {
          Utils::print_error(this->get_err_msg().c_str());
        }
        return false;
      }
    }
    break;

  case Rule_Action::IMPORT_COUNT:
    {
      Rule::rule_logger->info("COMPILE IMPORT RULE |{:s}|  expr |{:s}|", 
          this->name.c_str(), this->expression_str.c_str());
      string_vector_t args = Utils::get_top_level_parse(this->expression_str, ',');
      if(args.size() != 1) {
        this->err = "Import_count rule needs 1 argument:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      this->expression = nullptr;
      this->expression = new Expression(this->expression_str);
      this->expression_str = args[0];
      if(this->expression->parse()) {
        Rule::compiled_rules.push_back(this);
        Rule::rule_logger->info("COMPILED IMPORT_COUNT RULE {:s}", this->name.c_str());
        this->action = "import_count";
        return true;
      } else {
        this->warning = this->expression->is_warning();
        delete this->expression;
        this->err = "Expression  " + this->expression_str + " not recognized:\n  " + this->name;
        if(is_warning()) {
          Utils::print_warning(this->get_err_msg().c_str());
        } else {
          Utils::print_error(this->get_err_msg().c_str());
        }
        return false;
      }
      break;
    }

  case Rule_Action::IMPORT_PER_CAPITA:
    {
      Rule::rule_logger->info("COMPILE IMPORT RULE {:s}", this->name.c_str());
      string_vector_t args = Utils::get_top_level_parse(this->expression_str, ',');
      if(args.size() != 1) {
        this->err = "Import_per_capita rule needs 1 argument:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      this->expression_str = args[0];
      this->expression = new Expression(this->expression_str);
      if(this->expression->parse()) {
        Rule::compiled_rules.push_back(this);
        Rule::rule_logger->info("COMPILED IMPORT_PER_CAPITA RULE {:s}", this->name.c_str());
        this->action = "import_per_capita";
        return true;
      } else {
        this->warning = this->expression->is_warning();
        delete this->expression;
        this->err = "Expression  " + this->expression_str + " not recognized:\n  " + this->name;
        if(is_warning()) {
          Utils::print_warning(this->get_err_msg().c_str());
        } else {
          Utils::print_error(this->get_err_msg().c_str());
        }
        return false;
      }
      break;
    }

  case Rule_Action::IMPORT_LOCATION :
    {
      Rule::rule_logger->info("COMPILE IMPORT RULE {:s}", this->name.c_str());
      string_vector_t args = Utils::get_top_level_parse(this->expression_str, ',');
      if(args.size() != 3) {
        this->err = "import_location rule needs 3 argument:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      this->expression_str = args[0];
      this->expression_str2 = args[1];
      this->expression_str3 = args[2];
      this->expression = new Expression(this->expression_str);
      this->expression2 = new Expression(this->expression_str2);
      this->expression3 = new Expression(this->expression_str3);
      if(this->expression->parse()) {
        if(this->expression2->parse()) {
          if(this->expression3->parse()) {
            Rule::compiled_rules.push_back(this);
            Rule::rule_logger->info("COMPILED IMPORT_LOCATION RULE {:s}", this->name.c_str());
            this->action = "import_location";
            return true;
          } else {
            this->warning = this->expression3->is_warning();
            delete this->expression3;
            this->err = "Expression  " + this->expression_str3 + " not recognized:\n  " + this->name;
            if(is_warning()) {
              Utils::print_warning(this->get_err_msg().c_str());
            } else {
              Utils::print_error(this->get_err_msg().c_str());
            }
            return false;
          }
        } else {
          this->warning = this->expression2->is_warning();
          delete this->expression2;
          this->err = "Expression  " + this->expression_str2 + " not recognized:\n  " + this->name;
          if(is_warning()) {
            Utils::print_warning(this->get_err_msg().c_str());
          } else {
            Utils::print_error(this->get_err_msg().c_str());
          }
          return false;
        }
      }else {
        this->warning = this->expression->is_warning();
        delete this->expression;
        this->err = "Expression  " + this->expression_str + " not recognized:\n  " + this->name;
        if(is_warning()) {
          Utils::print_warning(this->get_err_msg().c_str());
        } else {
          Utils::print_error(this->get_err_msg().c_str());
        }
        return false;
      }
      break;
    }

  case Rule_Action::IMPORT_ADMIN_CODE:
    {
      Rule::rule_logger->info("COMPILE IMPORT RULE {:s}", this->name.c_str());
      string_vector_t args = Utils::get_top_level_parse(this->expression_str, ',');
      if(args.size() != 1) {
        this->err = "Import_census_tract rule needs 1 argument:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      this->expression_str = args[0];
      this->expression = new Expression(this->expression_str);
      if(this->expression->parse()) {
        Rule::compiled_rules.push_back(this);
        Rule::rule_logger->info("COMPILED IMPORT_ADMIN_CODE RULE {:s}", this->name.c_str());
        this->action = "import_admin_code";
        return true;
      } else {
        this->warning = this->expression->is_warning();
        delete this->expression;
        this->err = "Expression  " + this->expression_str + " not recognized:\n  " + this->name;
        if(is_warning()) {
          Utils::print_warning(this->get_err_msg().c_str());
        } else {
          Utils::print_error(this->get_err_msg().c_str());
        }
        return false;
      }
      break;
    }

  case Rule_Action::IMPORT_AGES:
    {
      Rule::rule_logger->info("COMPILE IMPORT RULE {:s}", this->name.c_str());
      string_vector_t args = Utils::get_top_level_parse(this->expression_str, ',');
      if(args.size() != 2) {
        this->err = "import_ages rule needs 2 argument:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      this->expression_str = args[0];
      this->expression = new Expression(this->expression_str);
      if(this->expression->parse()) {
        this->expression_str2 = args[1];
        this->expression2 = new Expression(this->expression_str2);
        if(this->expression2->parse()) {
          Rule::compiled_rules.push_back(this);
          Rule::rule_logger->info("COMPILED IMPORT_AGES RULE {:s}", this->name.c_str());
          this->action = "import_ages";
          return true;
        } else {
          this->warning = this->expression2->is_warning();
          delete this->expression2;
          this->err = "Expression  " + this->expression_str2 + " not recognized:\n  " + this->name;
          if(is_warning()) {
            Utils::print_warning(this->get_err_msg().c_str());
          } else {
            Utils::print_error(this->get_err_msg().c_str());
          }
          return false;
        }
      } else {
        this->warning = this->expression->is_warning();
        delete this->expression;
        this->err = "Expression  " + this->expression_str + " not recognized:\n  " + this->name;
        if(is_warning()) {
          Utils::print_warning(this->get_err_msg().c_str());
        } else {
          Utils::print_error(this->get_err_msg().c_str());
        }
        return false;

      }
      break;
    }

  case Rule_Action::COUNT_ALL_IMPORT_ATTEMPTS:
    {
      Rule::rule_logger->info("COMPILE COUNT_ALL_IMPORT_ATTEMPTS RULE {:s}", this->name.c_str());
      if(this->expression_str=="") {
        Rule::compiled_rules.push_back(this);
        Rule::rule_logger->info("COMPILED COUNT_ALL_IMPORT_ATTEMPTS RULE {:s}", this->name.c_str());
        this->action = "count_all_import_attempts";
        return true;
      } else {
        this->err = "Count_all_import_attempts takes no arguments:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
    }

  case Rule_Action::IMPORT_LIST:
    {
      Rule::rule_logger->info("COMPILE IMPORT RULE {:s}", this->name.c_str());
      string_vector_t args = Utils::get_top_level_parse(this->expression_str, ',');
      if(args.size() != 1) {
        this->err = "Import_list rule needs 1 argument:\n  " + this->name;
        Utils::print_error(this->get_err_msg().c_str());
        return false;
      }
      this->expression_str = args[0];
      this->expression = new Expression(this->expression_str);
      if(this->expression->parse() && this->expression->is_list_expression()) {
        Rule::compiled_rules.push_back(this);
        Rule::rule_logger->info("COMPILED IMPORT_LIST RULE {:s}", this->name.c_str());
        this->action = "import_list";
        return true;
      } else {
        this->warning = this->expression->is_warning();
        delete this->expression;
        this->err = "Expression  " + this->expression_str + " not recognized:\n  " + this->name;
        if(is_warning()) {
          Utils::print_warning(this->get_err_msg().c_str());
        }
        else {
          Utils::print_error(this->get_err_msg().c_str());
        }
        return false;
      }
      break;
    }

  default:
    Rule::rule_logger->error("COMPILE RULE UNKNOWN ACTION ACTION: |{:s}|", this->name.c_str());
    this->err = "Unknown Rule Action:\n  " + this->name;
    Utils::print_error(this->get_err_msg().c_str());
    return false;
    
    break;
  }

  return false;
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Rule::setup_logging() {
  if(Rule::is_log_initialized) {
    return;
  }

  if(Parser::does_property_exist("rule_log_level")) {
    Parser::get_property("rule_log_level", &Rule::rule_log_level);
  } else {
    Rule::rule_log_level = "OFF";
  }

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Rule::rule_logger = std::make_unique<spdlog::logger>("rule_logger", 
        sink_list.begin(), sink_list.end());
    Rule::rule_logger->set_level(
        Utils::get_log_level_from_string(Rule::rule_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Rule::rule_logger->trace("<{:s}, {:d}>: Rule logger initialized", 
      __FILE__, __LINE__  );
  Rule::is_log_initialized = true;
}
