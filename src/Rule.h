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

#ifndef _FRED_RULE_H
#define _FRED_RULE_H

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "Clause.h"
#include "Expression.h"
#include "Person.h"
#include "Preference.h"

namespace Rule_Action {
  enum e { NONE = -1,
	   WAIT,
	   WAIT_UNTIL,
	   GIVE_BIRTH,
	   DIE, 
	   DIE_OLD, 
	   SUS,
	   TRANS,
	   JOIN,
	   QUIT,
	   ADD_EDGE_FROM,
	   ADD_EDGE_TO,
	   DELETE_EDGE_FROM,
	   DELETE_EDGE_TO,
	   SET,
	   SET_LIST,
	   SET_STATE,
	   CHANGE_STATE,
	   SET_WEIGHT, 
	   SET_SUS,
	   SET_TRANS,
	   REPORT,
	   ABSENT,
	   PRESENT, 
	   CLOSE,
	   SET_CONTACTS,
	   RANDOMIZE_NETWORK,
	   IMPORT_COUNT,
	   IMPORT_PER_CAPITA,
	   IMPORT_LOCATION,
	   IMPORT_ADMIN_CODE,
	   IMPORT_AGES,
	   COUNT_ALL_IMPORT_ATTEMPTS,
	   IMPORT_LIST,
	   RULE_ACTIONS };
};

/**
 * This class represents a rule in the FRED language.
 */
class Rule {
public:

  Rule(std::string s);

  /**
   * Gets the name of the rule.
   *
   * @return the name
   */
  std::string get_name() {
    return this->name;
  }

  /**
   * Gets this rule's condition as a string.
   *
   * @return the condition name
   */
  std::string get_cond() {
    return this->cond;
  }

  /**
   * Gets this rule's condition ID.
   *
   * @return the condition ID
   */
  int get_cond_id() {
    return this->cond_id;
  }

  /**
   * Gets this rule's condition state as a name.
   *
   * @return the state name
   */
  std::string get_state() {
    return this->state;
  }

  /**
   * Gets this rule's condition state ID.
   *
   * @return the state ID
   */
  int get_state_id() {
    return this->state_id;
  }

  /**
   * Gets this rule's clause string.
   *
   * @return the clause string
   */
  std::string get_clause_str() {
    return this->clause_str;
  }

  /**
   * Gets this rule's clause.
   *
   * @return the clause
   */
  Clause* get_clause() {
    return this->clause;
  }

  /**
   * Gets this rule's next condition state as a string.
   *
   * @return the next state name
   */
  std::string get_next_state() {
    return this->next_state;
  }

  /**
   * Gets this rule's next condition state ID.
   *
   * @return the next state ID
   */
  int get_next_state_id() {
    return this->next_state_id;
  }

  /**
   * Gets this rule's action.
   *
   * @return the action name
   */
  std::string get_action() {
    return this->action;
  }

  /**
   * Gets this rule's action ID.
   *
   * @return the action ID
   */
  int get_action_id() {
    return this->action_id;
  }

  /**
   * Gets this rule's first expression string.
   *
   * @return the expression string
   */
  std::string get_expression_str() {
    return this->expression_str;
  }

  /**
   * Gets this rule's first expression.
   *
   * @return the expression
   */
  Expression* get_expression() {
    return this->expression;
  }

  /**
   * Gets this rule's second expression string.
   *
   * @return the expression string
   */
  std::string get_expression_str2() {
    return this->expression_str2;
  }

  /**
   * Gets this rule's second expression.
   *
   * @return the expression
   */
  Expression* get_expression2() {
    return this->expression2;
  }

  /**
   * Gets this rule's third expression string.
   *
   * @return the expression string
   */
  std::string get_expression_str3() {
    return this->expression_str3;
  }

  /**
   * Gets this rule's third expression.
   *
   * @return the expression
   */
  Expression* get_expression3() {
    return this->expression3;
  }

  /**
   * Gets this rule's variable.
   *
   * @return the variable
   */
  std::string get_var() {
    return this->var;
  }

  /**
   * Gets this rule's variable ID.
   *
   * @return the variable ID
   */
  int get_var_id() {
    return this->var_id;
  }

  /**
   * Gets this rule's list variable.
   *
   * @return the list variable
   */
  std::string get_list_var() {
    return this->list_var;
  }

  /**
   * Gets this rule's list variable ID.
   *
   * @return the list variable ID
   */
  int get_list_var_id() {
    return this->list_var_id;
  }

  /**
   * Gets this rule's source condition as a string.
   *
   * @return the source condition name
   */
  std::string get_source_cond() {
    return this->source_cond;
  }

  /**
   * Gets this rule's source condition ID.
   *
   * @return the source condition ID
   */
  int get_source_cond_id() {
    return this->source_cond_id;
  }

  /**
   * Gets this rule's source condition state as a string.
   *
   * @return the source state name
   */
  std::string get_source_state() {
    return this->source_state;
  }

  /**
   * Gets this rule's source condition state ID.
   *
   * @return the source state ID
   */
  int get_source_state_id() {
    return this->source_state_id;
  }

  /**
   * Gets this rule's destination condition state as a string.
   *
   * @return the destination state name
   */
  std::string get_dest_state() {
    return this->dest_state;
  }

  /**
   * Gets this rule's destination condition state ID.
   *
   * @return the destination state ID
   */
  int get_dest_state_id() {
    return this->dest_state_id;
  }

  /**
   * Gets this rule's network as a string.
   *
   * @return the network name
   */
  std::string get_network() {
    return this->network;
  }

  /**
   * Gets this rule's network ID.
   *
   * @return the network ID
   */
  int get_network_id() {
    return this->network_id;
  }

  /**
   * Gets this rule's group as a string.
   *
   * @return the group name
   */
  std::string get_group() {
    return this->group;
  }

  /**
   * Gets this rule's group ID.
   *
   * @return the group ID
   */
  int get_group_type_id() {
    return this->group_type_id;
  }

  /**
   * Sets this rule's error message.
   *
   * @param msg the error message
   */
  void set_err_msg(std::string msg) {
    this->err = msg;
  }

  /**
   * Gets this rule's error message.
   *
   * @return the error message
   */
  std::string get_err_msg() {
    return this->err;
  }

  /**
   * Checks if this rule is a warning.
   *
   * @return if this rule is a warning
   */
  bool is_warning() {
    return this->warning;
  }

  /**
   * Checks if this rule's action is join.
   *
   * @return if this rule is a join rule
   */
  bool is_join_rule() {
    return this->action == "join";
  }

  /**
   * Checks if this rule is a wait rule.
   *
   * @return if this rule is a wait rule
   */
  bool is_wait_rule() {
    return this->wait_rule;
  }

  /**
   * Checks if this rule is a exposure rule.
   *
   * @return if this rule is a exposure rule
   */
  bool is_exposure_rule() {
    return this->exposure_rule;
  }

  /**
   * Checks if this rule is a default rule.
   *
   * @return if this rule is a default rule
   */
  bool is_default_rule() {
    return this->default_rule;
  }

  /**
   * Checks if this rule is a next rule.
   *
   * @return if this rule is a next rule
   */
  bool is_next_rule() {
    return this->next_rule;
  }

  /**
   * Checks if this rule is a schedule rule.
   *
   * @return if this rule is a schedule rule
   */
  bool is_schedule_rule() {
    return this->schedule_rule;
  }

  /**
   * Checks if this rule is an action rule.
   *
   * @return if this rule is an action rule
   */
  bool is_action_rule() {
    return this->action_rule;
  }

  /**
   * Checks if this rule is global.
   *
   * @return if this rule is global
   */
  bool is_global() {
    return this->global;
  }

  void parse(std::string str);

  bool parse();

  bool parse_action_rule();
  bool parse_wait_rule();
  bool parse_exposure_rule();
  bool parse_next_rule();
  bool parse_default_rule();
  bool parse_state();

  std::string to_string();

  bool applies(Person* person, Person* other = nullptr);

  double get_value(Person* person, Person* other = nullptr);

  /**
   * Marks this rule as used.
   */
  void mark_as_used() {
    this->used = true;
  }

  /**
   * Marks this rule as not used.
   */
  void mark_as_unused() {
    this->used = false;
  }

  /**
   * Checks if this rule is used.
   *
   * @return if this rule is used
   */
  bool is_used() {
    return this->used;
  }

  bool compile();
  bool compile_action_rule();

  void set_hidden_by_rule(Rule* rule);

  /**
   * Gets the rule this rule is hidden by.
   *
   * @return the rule
   */
  Rule* get_hidden_by_rule() {
    return this->hidden_by;
  }

  static void print_warnings();

  static void add_rule_line(std::string line);

  static void prepare_rules();

  /**
   * Gets the number of rules in the static rules rule vector.
   *
   * @return the number of rules
   */
  static int get_number_of_rules() {
    return Rule::rules.size();
  }

  /**
   * Gets the rule at the specified index in the static rules rule vector.
   *
   * @param n the index
   * @return the rule
   */
  static Rule* get_rule(int n) {
    return Rule::rules[n];
  }

  /**
   * Gets the number of compiled rules in the static compiled rules rule vector.
   *
   * @return the number of compiled rules
   */
  static int get_number_of_compiled_rules() {
    return Rule::compiled_rules.size();
  }

  /**
   * Gets the compiled rule at the specified index in the static compiled rules rule vector.
   *
   * @param n the index
   * @return the compiled rule
   */
  static Rule* get_compiled_rule(int n) {
    return Rule::compiled_rules[n];
  }
  
  static void setup_logging();

private:
  static int first_rule;
  static std::vector<std::string> rule_list;
  static std::vector<Rule*> rules;
  static std::vector<Rule*> compiled_rules;
  static std::vector<std::string> action_string;

  std::string name;
  std::string cond;
  int cond_id;
  std::string state;
  int state_id;
  std::string clause_str;
  Clause* clause;
  std::string next_state;
  int next_state_id;
  std::string action;
  int action_id;
  std::string expression_str;
  Expression* expression;
  std::string expression_str2;
  Expression* expression2;
  std::string expression_str3;
  Expression* expression3;
  std::string var;
  int var_id;
  std::string list_var;
  int list_var_id;
  std::string source_cond;
  int source_cond_id;
  std::string source_state;
  int source_state_id;
  std::string dest_state;
  int dest_state_id;
  std::string network;
  int network_id;
  std::string group;
  int group_type_id;

  std::string err;
  std::vector<std::string>parts;

  bool used;
  bool warning;
  bool global;

  // Rule Types:
  bool action_rule;
  bool wait_rule;
  bool exposure_rule;
  bool next_rule;
  bool default_rule;
  bool schedule_rule;

  Preference* preference;

  Rule* hidden_by;

  static bool is_log_initialized;
  static std::string rule_log_level;
  static std::unique_ptr<spdlog::logger> rule_logger;
};

#endif
