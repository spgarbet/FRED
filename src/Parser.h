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
// File Parser.h
//
#ifndef _FRED_PARSER_H
#define _FRED_PARSER_H

#include "Global.h"

class Rule;

class Parser {

public:

  static std::string remove_leading_whitespace(std::string str);

  static std::string delete_whitespace(std::string str);

  static bool does_property_exist(std::string name);

  static void read_program_file(char* program_file);

  static void parse_statement(std::string statement, int linenum, char* program_file);
  
//  static void secondary_parse_statement(std::string statement);

  static void remove_comments();

  static void remove_continuations();

  static void break_on_brackets();

  static void break_on_semicolons();

  static void remove_blank_lines();

  static int verify_brackets();

  static void remove_excess_whitespace();

  static void parse_state_blocks();

  static void parse_condition_blocks();

  static void parse_place_blocks();

  static void parse_network_blocks();

  static void find_unmatched_brackets();

  static void print_program();
  
  static void pre_parse(char* program_file);

  static int parse(char* program_file);

  static std::string find_property(std::string name);

  static int get_next_property(std::string name, std::string* value, int start);

  static int get_property(std::string name, bool* value);

  static int get_property(std::string name, int* value);

  static int get_property(std::string name, unsigned long* value);

  static int get_property(std::string name, long long int* value);

  static int get_property(std::string name, double* value);

  static int get_property(std::string name, float* value);

  static int get_property(std::string name, char* value);

  static int get_property(std::string name, std::string* p);

  static int get_property(std::string name, std::string &value);

  // the following form a property name from the first n-1 args

  template <typename T>
  static int get_property(std::string s, int index, T* value){
    char name[256];
    snprintf(name, 256, "%s[%d]", s.c_str(), index);
    int found = Parser::get_property(name, value);
    return found;
  }

  template <typename T>
  static int get_property(std::string s, int index_i, int index_j, T* value){
    char name[256];
    snprintf(name, 256, "%s[%d][%d]", s.c_str(), index_i, index_j);
    int found = Parser::get_property(name, value);
    return found;
  }

  template <typename T>
  static int get_property(std::string s1, std::string s2, T* value){
    char name[256];
    snprintf(name, 256, "%s.%s", s1.c_str(), s2.c_str());
    int found = Parser::get_property(name, value);
    return found;
  }

  template <typename T>
  static int get_property(std::string s1, std::string s2, std::string s3, T* value){
    char name[256];
    snprintf(name, 256, "%s.%s.%s", s1.c_str(), s2.c_str(), s3.c_str());
    int found = Parser::get_property(name, value);
    return found;
  }

  template <typename T>
  static int get_property(std::string s1, std::string s2, std::string s3, std::string s4, T* value) {
    char name[256];
    snprintf(name, 256, "%s.%s.%s.%s", s1.c_str(), s2.c_str(), s3.c_str(), s4.c_str());
    int found = Parser::get_property(name, value);
    return found;
  }

  // property vectors

  /**
   * @property s the program name
   * @property p a pointer to the vector of ints that will be set
   * @return 1 if found
   */
  static int get_property_vector(char* s, std::vector<int> &p);

  /**
   * @property s the program name
   * @property p a pointer to the vector of doubles that will be set
   * @return 1 if found
   */
  static int get_property_vector(char* s, std::vector<double> &p);

  /**
   * @property s char* with value to be vectorized
   * @property p a pointer to the vector of doubles that will be set
   * @return 1 if found
   */
  static int get_property_vector_from_string(char *s, std::vector<double> &p);

  static int get_property_vector(char* s, double* p);

  static int get_property_vector(char* s, int* p);

  static int get_property_vector(char* s, std::string* p);

  /**
   * @property s the program name
   * @property p a pointer to the 2D array of doubles that will be set
   * @return 1 if found
   */
  static int get_property_matrix(char* s, double*** p);

  template <typename T>
  static int get_indexed_property_vector(std::string s, int index, T* p) {
    char name[256];
    snprintf(name, 256, "%s[%d]",s.c_str(),index);
    int found = Parser::get_property_vector(name,p);
    return found;
  }

  template <typename T>
  static int get_indexed_property_vector(std::string s1, std::string s2, T* p) {
    char name[256];
    snprintf(name, 256, "%s.%s", s1.c_str(), s2.c_str());
    int found = Parser::get_property_vector(name, p);
    return found;
  }

  template <typename T>
  static int get_double_indexed_property_vector(std::string s, int index_i, int index_j, T* p) {
    char name[256];
    snprintf(name, 256, "%s[%d][%d]", s.c_str(), index_i, index_j);
    int found = Parser::get_property_vector(name, p);
    return found;
  }

  // determine whether property is required or optional

  /**
   * Sets the parser to abort on failure.
   */
  static void set_abort_on_failure() {
    Parser::abort_on_failure = 1;
  }

  /**
   * Disables the parser from aborting on failure.
   */
  static void disable_abort_on_failure() {
    Parser::abort_on_failure = 0;
  }

  static void print_errors(char* filename);
  static void print_warnings(char* filename);
  static void report_parameter_check();
  static int check_properties;
  static edge_vector_t get_edges(std::string network_name);

private:
  static std::vector<std::string> model_file;
  static std::vector<std::string> property_names;
  static std::vector<std::string> property_values;
  static std::vector<std::string> property_not_found;
  static std::vector<int> property_lineno;
  static std::vector<int> property_is_duplicate;
  static std::vector<int> property_is_used;
  static std::vector<int> property_is_default;
  static std::unordered_map<std::string, int> property_map;
  static int number_of_properties;
  static int abort_on_failure;
  static int default_properties;
  static std::string error_string;
};

#endif // _FRED_PARSER_H
