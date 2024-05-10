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

#ifndef _FRED_EXPRESSION_H
#define _FRED_EXPRESSION_H

#include <spdlog/spdlog.h>

#include "Global.h"

class Factor;
class Person;
class Preference;

/**
 * This class represents an expression in the FRED language.
 */
class Expression {
public:

  Expression(std::string s);
  std::string get_name();
  double get_value(Person* person, Person* other = nullptr);
  double_vector_t get_list_value(Person* person, Person* other = nullptr);
  bool parse();

  /**
   * Checks whether a string is a known function included in the operator map op_map.
   *
   * @param str the string
   * @return if the string is a known function
   */
  static bool is_known_function(std::string str) {
    return Expression::op_map.find(str)!=Expression::op_map.end();
  }

  static bool unbalanced_parens(std::string str);
  std::string get_next_token(std::string s, int pos);
  std::string expand_minus(std::string s);
  std::string expand_operator(std::string s);
  bool is_operator(std::string s);
  bool is_function(std::string s);
  int get_number_of_args(std::string s);
  int get_priority(std::string s);
  std::string infix_to_prefix(std::string infix);
  std::string replace_unary_minus(std::string s);
  std::string convert_infix_to_prefix(std::string infix);
  int find_comma(std::string s);
  double_vector_t get_pool(Person* person);
  double_vector_t get_filtered_list(Person* person, double_vector_t &list);
  bool is_warning() {
    return this->warning;
  }
  bool is_list_expression() {
    return this->is_list_expr;
  }
  
  static void setup_logging();

private:
  std::string name;
  std::string op;
  int op_index;
  Expression* expr1;
  Expression* expr2;
  Expression* expr3;
  Expression* expr4;
  Factor* factor;
  double number;
  int number_of_expressions;
  std::string minus_err;
  std::string list_var;
  int list_var_id;
  std::string pref_str;
  Preference* preference;
  bool is_select;
  bool use_other;
  bool warning;
  bool is_list_expr;
  bool is_list_var;
  bool is_global;
  bool is_pool;
  bool is_filter;
  bool is_list;
  bool is_value;
  bool is_distance;
  int_vector_t pool;
  Clause* clause;

  static std::map<std::string, int> op_map;
  static std::map<std::string, int> value_map;

  static bool is_log_initialized;
  static std::string expression_log_level;
  static std::unique_ptr<spdlog::logger> expression_logger;
};

#endif
