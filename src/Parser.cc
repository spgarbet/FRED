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
// File Parser.cc
//

#include <algorithm>
#include <math.h>
#include <sstream>
#include <stdio.h>
#include <string>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>

#include "Condition.h"
#include "Global.h"
#include "Natural_History.h"
#include "Network_Type.h"
#include "Parser.h"
#include "Person.h"
#include "Place_Type.h"
#include "Rule.h"
#include "Utils.h"

// static variables
int Parser::abort_on_failure = 1;
std::vector<std::string> Parser::property_names;
std::vector<std::string> Parser::property_values;
std::vector<std::string> Parser::property_not_found;
std::vector<std::string> Parser::model_file;
std::vector<int> Parser::property_lineno;
std::vector<int> Parser::property_is_duplicate;
std::vector<int> Parser::property_is_used;
std::vector<int> Parser::property_is_default;
std::unordered_map<std::string, int> Parser::property_map;
int Parser::number_of_properties = 0;
int Parser::check_properties = 0;
int Parser::default_properties = 1;
std::string Parser::error_string = "";

string_vector_t program;


std::string Parser::remove_leading_whitespace(std::string str) {
  char copy[FRED_STRING_SIZE];
  char tmp[FRED_STRING_SIZE];
  strcpy(copy, str.c_str());
  char* s = copy;
  if(copy[0]==' ') {
    while(*s != '\0' && *s == ' ') {
      ++s;
    } 
  }
  strcpy(tmp,s);
  std::string result = std::string(tmp);
  return result;
}

std::string Parser::delete_whitespace(std::string str) {

  // replace tabs with spaces
  std::replace(str.begin(), str.end(), '\t', ' ');
  // printf("delete_ws: |%s|\n", str.c_str());

  // replace newline with spaces
  std::replace(str.begin(), str.end(), '\n', ' ');
  // printf("delete_ws: |%s|\n", str.c_str());

  // replace multiple spaces with one space
  for(int i = static_cast<int>(str.size()) - 1; i > 0; --i) {
    if(str[i] == ' ' && str[i] == str[i - 1]) {
      str.erase(str.begin() + i);
    }
  }
  // printf("|%s|\n", str.c_str());

  // replace initial space if present
  if(static_cast<int>(str.size()) > 0 && str[0] == ' ') {
    str.erase(str.begin());
  }
  // printf("|%s|\n", str.c_str());

  // replace final space if present
  if(static_cast<int>(str.size()) > 0 && str[static_cast<int>(str.size()) - 1] == ' ') {
    str.erase(str.begin() + str.size() - 1);
  }
  // printf("|%s|\n", str.c_str());

  // erase space before left paren
  for(int i = static_cast<int>(str.size()) - 2; i >= 0; --i) {
    if(str[i] == ' ' && str[i + 1] == '(') {
      str.erase(str.begin() + i);
    }
  }
  // printf("|%s|\n", str.c_str());

  // erase any spaces within paren
  int inner = 0;
  for(int i = static_cast<int>(str.size()) - 1; i >= 0; --i) {
    if(str[i] == ')') {
      ++inner;
    }
    if(str[i] == '(') {
      --inner;
    }
    if(str[i] == ' ' && inner > 0) {
      str.erase(str.begin() + i);
    }
  }

  return str;
}


void Parser::remove_comments() {
  string_vector_t tmp;
  tmp.clear();

  for(int i = 0; i < static_cast<int>(program.size()); ++i) {
    std::string str = program[i];
    // printf("%d: |%s|\n", i, program[i].c_str());
    std::size_t comment = str.find("#");
    if(comment != std::string::npos) {
      str = str.substr(0, static_cast<int>(comment));
      // printf("%d: |%s|\n", i, str.c_str());
    }
    tmp.push_back(str);
  }
  program.clear();
  for(int i = 0; i < static_cast<int>(tmp.size()); ++i) {
    program.push_back(tmp[i]);
  }
}

void Parser::remove_continuations() {
  //handle continuation lines
  std::string contin = "\\";
  int i = 0;
  while(i < static_cast<int>(program.size())) {
    int len = static_cast<int>(program[i].length());
    if(len > 0 && program[i].substr(len - 1, 1) == contin) {
      // printf("length = %d |%s| %s\n", len, program[i].c_str(), program[i].substr(len-1,1).c_str());
      while(len > 0 && program[i].substr(len - 1, 1) == contin) {
        if(i + 1 < static_cast<int>(program.size())) {
          std::string tmp = program[i].substr(0, len - 1) + program[i + 1];
          program[i] = " ";
          ++i;
          program[i] = tmp;
          len = program[i].length();
        }
      }
    }
    ++i;
  }
}

void Parser::break_on_brackets() {
  string_vector_t tmp;
  tmp.clear();

  for(int i = 0; i < static_cast<int>(program.size()); ++i) {
    std::string str = program[i];
    int bracket = static_cast<int>(str.find("{"));
    if(bracket != static_cast<int>(std::string::npos)) {
      tmp.push_back(str.substr(0, bracket + 1));
      tmp.push_back(str.substr(bracket + 1));
    } else {
      tmp.push_back(str);
    }
  }
  program.clear();
  program = tmp;
  string_vector_t tmp2;
  tmp2.clear();

  for(int i = 0; i < static_cast<int>(program.size()); ++i) {
    std::string str = program[i];
    int bracket = static_cast<int>(str.find("}"));
    if(bracket != static_cast<int>(std::string::npos) && static_cast<int>(str.length()) > 1) {
      tmp2.push_back(str.substr(0, bracket));
      tmp2.push_back(str.substr(bracket));
    } else {
      tmp2.push_back(str);
    }
  }
  program.clear();
  program = tmp2;
}

void Parser::break_on_semicolons() {
  string_vector_t tmp;
  tmp.clear();

  int i = 0;
  while(i < static_cast<int>(program.size())) {
    std::string str = program[i];
    int bracket = static_cast<int>(str.find(";"));
    if(bracket != static_cast<int>(std::string::npos)) {
      tmp.push_back(str.substr(0, bracket));
      program[i] = str.substr(bracket + 1);
    } else {
      tmp.push_back(str);
      ++i;
    }
  }
  program.clear();
  program = tmp;
}

void Parser::remove_excess_whitespace() {
  for(int i = 0; i < static_cast<int>(program.size()); ++i) {
    program[i] = Parser::delete_whitespace(program[i]);
  }
}

void Parser::remove_blank_lines() {
  string_vector_t tmp;
  tmp.clear();
  for(int i = 0; i < static_cast<int>(program.size()); ++i) {
    if(static_cast<int>(program[i].size()) > 0 && program[i] != " ") {
      tmp.push_back(program[i]);
    }
  }
  program.clear();
  program = tmp;
}

int Parser::verify_brackets() {

  // make sure left bracket not on line alone
  for(int i = 1; i < static_cast<int>(program.size()); ++i) {
    if(program[i] == "{") {
      program[i - 1] = program[i - 1] + " {";
      program[i] = "";
    }
  }
  remove_blank_lines();
  
  string_vector_t tmp;
  tmp.clear();
  
  int i = 0;
  while(i < static_cast<int>(program.size())) {
    if((static_cast<int>(program[i].find("State")) == 0 ||
        static_cast<int>(program[i].find("state")) == 0 ||
        static_cast<int>(program[i].find("if state")) == 0) &&
        program[i].find("{") != std::string::npos) {
      tmp.push_back(program[i]);
      // printf("BLOCK START: %s\n", program[i].c_str());
      int empty_block = 1;
      int found_end = 0;
      int found_nest = 0;
      int j = i + 1;
      while(found_nest == 0 && found_end == 0 && j < static_cast<int>(program.size())) {
        if(static_cast<int>(program[j].find("}")) == 0) {
          i = j + 1;
          found_end = 1;
          if(empty_block) {
            tmp.push_back("action()");
            tmp.push_back("wait()");
            tmp.push_back("next()");
          }
          tmp.push_back(program[j]);
          break;
        } else {
          if(program[j].find("{") != std::string::npos) {
            found_nest = 1;
          } else {
            empty_block  = 0;
            tmp.push_back(program[j]);
            ++j;
          }
        }
      }
      if(found_nest == 1) {
        // nest block error
        Parser::error_string += "Nested block found starting at:\n  " + program[i];
        printf("Nest Block found in line |%s|\n", program[i].c_str());
        return 0;
      }
      if(found_end == 0) {
        // unterminated block error
        Parser::error_string += "Unterminated block found starting at:\n  " + program[i];
        printf("Unterminated block found in line |%s|\n", program[i].c_str());
        return 0;
      }
      // otherwise continue outer whileloop with new value of i
    } else {
      tmp.push_back(program[i]);
      ++i;
    }
  }
  program.clear();
  program = tmp;
  return 1;
}

void Parser::parse_state_blocks() {

  // handle block code
  int i = 0;
  while(i < static_cast<int>(program.size())) {
    string_vector_t parts = Utils::get_string_vector(program[i], ' ');
    std::string state = "";
    std::string start_block = "";
    
    if(((parts[0] == "state" || parts[0] == "State" ) && parts[2] == "{") ||
       ((static_cast<int>(program[i].find("if state(")) == 0 || static_cast<int>(program[i].find("state(")) == 0 ||
         static_cast<int>(program[i].find("State(")) == 0) && program[i].find("{") != std::string::npos)) {

      start_block = program[i];

      if((parts[0] == "state" || parts[0] == "State" ) && parts[2] == "{") {
        state = parts[1];
      } else {
        int pos1 = static_cast<int>(program[i].find("tate("));
        int pos = static_cast<int>(program[i].find(")", pos1));
        // printf("%s pos = %d\n", start_block.c_str(), (int) pos);
        if(pos == static_cast<int>(std::string::npos)) {
          Parser::error_string += "Bad state in line:\n  " + program[i];
          printf("Error in line |%s|\n", program[i].c_str());
          start_block = "";
          program[i] = "";
          ++i;
          continue;
        }
        state = program[i].substr(pos1 + 5, pos - pos1 - 5);
      }
      
      program[i] = " ";
      ++i;
      while(i < static_cast<int>(program.size())) {
        std::string tmp = "";
        if(static_cast<int>(program[i].find("}")) == 0) {
          program[i] = " ";
          break;
        }
        if(static_cast<int>(program[i].find("action()")) == 0 || static_cast<int>(program[i].find("effect()")) == 0) {
          tmp = "";
        } else if(static_cast<int>(program[i].find("next()")) == 0 || static_cast<int>(program[i].find("default()")) == 0) {
          std::string st = "";
          if(state.find(",") != std::string::npos) {
            st = state.substr(state.find(",") + 1);
          } else {
            st = state.substr(state.find(".") + 1);
          }
          tmp = "if state(" + state + ") then next(" + st + ")";
        } else if(static_cast<int>(program[i].find("if(")) == 0) {
          program[i].replace(0, 2, "and");
          tmp = "if state(" + state + ") " + program[i];
        } else {
          tmp = "if state(" + state + ") then " + program[i];
        }
        program[i] = tmp;
        ++i;
      }
    }
    ++i;
  }
  Parser::remove_blank_lines();
}

void Parser::parse_condition_blocks() {

  string_vector_t tmp;
  tmp.clear();

  int i = 0;
  while(i < static_cast<int>(program.size())) {
    string_vector_t parts = Utils::get_string_vector(program[i], ' ');

    if(static_cast<int>(parts.size()) == 2 && (parts[0] == "Condition" || parts[0] == "condition")) {
      // printf("%s\n", program[i].c_str());
      tmp.push_back("include_condition = " + parts[1]);
      // printf("%s\n", program[i].c_str());
      ++i;
    } else if(static_cast<int>(parts.size()) == 3 && (parts[0] == "Condition" || parts[0] == "condition") && parts[2] == "{") {
      // printf("%s\n", program[i].c_str());
      tmp.push_back("include_condition = " + parts[1]);
      // printf("%s\n", program[i].c_str());
      std::string cond = parts[1];
      int j = i + 1;
      int open_block = 1;
      while(j < static_cast<int>(program.size()) && open_block) {
        if(static_cast<int>(program[j].find("}")) == 0) {
          open_block = 0;
        } else {
          tmp.push_back(cond + "." + program[j]);
        }
        ++j;
      }
      i = j;
    } else {
      tmp.push_back(program[i]);
      ++i;
    }
  }
  program.clear();
  program = tmp;
  Parser::remove_blank_lines();
}

void Parser::parse_place_blocks() {

  string_vector_t tmp;
  tmp.clear();

  int i = 0;
  while(i < static_cast<int>(program.size())) {
    string_vector_t parts = Utils::get_string_vector(program[i], ' ');

    if(static_cast<int>(parts.size()) == 2 && (parts[0] == "Place" || parts[0] == "place")) {
      // printf("%s\n", program[i].c_str());
      tmp.push_back("include_place = " + parts[1]);
      // printf("%s\n", program[i].c_str());
      ++i;
    } else if(static_cast<int>(parts.size()) == 3 && (parts[0] == "Place" || parts[0] == "place") && parts[2] == "{") {
      // printf("%s\n", program[i].c_str());
      tmp.push_back("include_place = " + parts[1]);
      // printf("%s\n", program[i].c_str());
      std::string place_type = parts[1];
      int j = i + 1;
      int open_block = 1;
      while(j < static_cast<int>(program.size()) && open_block) {
        if(static_cast<int>(program[j].find("}")) == 0) {
          open_block = 0;
        } else {
          tmp.push_back(place_type + "." + program[j]);
        }
        ++j;
      }
      i = j;
    } else {
      tmp.push_back(program[i]);
      ++i;
    }
  }
  program.clear();
  program = tmp;
  Parser::remove_blank_lines();
}

void Parser::parse_network_blocks() {

  string_vector_t tmp;
  tmp.clear();

  int i = 0;
  while(i < static_cast<int>(program.size())) {
    string_vector_t parts = Utils::get_string_vector(program[i], ' ');

    if(static_cast<int>(parts.size()) == 2 && (parts[0] == "Network" || parts[0] == "network")) {
      // printf("%s\n", program[i].c_str());
      tmp.push_back("include_network = " + parts[1]);
      // printf("%s\n", program[i].c_str());
      ++i;
    } else if(static_cast<int>(parts.size()) == 3 && (parts[0] == "Network" || parts[0] == "network") && parts[2] == "{") {
      // printf("%s\n", program[i].c_str());
      tmp.push_back("include_network = " + parts[1]);
      // printf("%s\n", program[i].c_str());
      std::string net_type = parts[1];
      int j = i + 1;
      int open_block = 1;
      while(j < static_cast<int>(program.size()) && open_block) {
        if(static_cast<int>(program[j].find("}")) == 0) {
          open_block = 0;
        } else {
          tmp.push_back(net_type + "." + program[j]);
        }
        ++j;
      }
      i = j;
    } else {
      tmp.push_back(program[i]);
      ++i;
    }
  }
  program.clear();
  program = tmp;
  Parser::remove_blank_lines();
}

void Parser::find_unmatched_brackets() {
  //they should be no brackets at this point
  for(int i = 1; i < static_cast<int>(program.size()); ++i) {
    if(program[i].find("{") != std::string::npos || program[i].find("}") != std::string::npos) {
      Parser::error_string += "Illegal bracket in line:\n  " + program[i];
    }
  }
}

void Parser::print_program() {
  for(int i = 0; i < static_cast<int>(program.size()); ++i) {
    printf("%s\n", program[i].c_str());
  }
}

void Parser::pre_parse(char* program_file) {
  char filename[FRED_STRING_SIZE];
  
  program.clear();
  Parser::property_names.clear();
  Parser::property_values.clear();
  strcpy(filename, "$FRED_HOME/data/config.fred");
  Parser::default_properties = 1;
  Parser::read_program_file(filename);
  Parser::default_properties = 0;
  Parser::read_program_file(program_file);
  
  std::unordered_map<std::string, std::string> pre_parse_key_val_map;
  
  // Load all key = value  pairs into a string/string map
  for(int i = 0; i < static_cast<int>(program.size()); ++i) {
    std::vector<std::string> fields;
    boost::algorithm::split(fields, program[i], boost::is_any_of("="));
    if(static_cast<int>(fields.size()) == 2) {
      boost::algorithm::trim(fields[0]);
      boost::algorithm::trim(fields[1]);
      pre_parse_key_val_map.insert_or_assign(fields[0], fields[1]);
    }
  }
  
  // Now, any that need to be processed before actual parsing occurs should be handled
  auto search = pre_parse_key_val_map.find("outdir");
  if(search != pre_parse_key_val_map.end()) {
    strcpy(Global::Output_directory, search->second.c_str());
  }
  
  search = pre_parse_key_val_map.find("enable_health_records");
  if(search != pre_parse_key_val_map.end()) {
    std::istringstream(search->second) >> Global::Enable_Records;
  }
}

int Parser::parse(char* program_file) {
  char filename[FRED_STRING_SIZE];
  
  program.clear();
  Parser::property_names.clear();
  Parser::property_values.clear();
  strcpy(filename, "$FRED_HOME/data/config.fred");
  Parser::default_properties = 1;
  Parser::read_program_file(filename);
  Parser::default_properties = 0;
  Parser::read_program_file(program_file);

  Parser::remove_comments();
  Parser::remove_continuations();
  Parser::break_on_semicolons();
  Parser::break_on_brackets();
  Parser::remove_excess_whitespace();
  Parser::remove_blank_lines();
  Parser::verify_brackets();
  
  Parser::parse_state_blocks();
  Parser::parse_condition_blocks();
  Parser::parse_place_blocks();
  Parser::parse_network_blocks();
  // Parser::find_unmatched_brackets();
  
  // handle pre-processing aliases
  for(int i = 0; i < static_cast<int>(program.size()); ++i) {
    // printf("PERSONAL pre-procesing program[%d]: |%s|\n", i, program[i].c_str());
    string_vector_t parts = Utils::get_string_vector(program[i], ' ');

    if(static_cast<int>(parts.size()) > 0 && (parts[0] == "my" || parts[0] == "Var" || parts[0] == "var")) {
      program[i] = "include_variable = ";
      for(int j = 1; j < static_cast<int>(parts.size()); ++j) {
        program[i] = program[i] + " " + parts[j];
      }
      // printf("PERSONAL include_variable statement |%s|\n", program[i].c_str()); 
    }

    if(static_cast<int>(parts.size()) > 0 && (parts[0] == "my_list" || parts[0] == "List" || parts[0] == "list")) {
      program[i] = "include_list_variable = ";
      for(int j = 1; j < static_cast<int>(parts.size()); ++j) {
        program[i] = program[i] + " " + parts[j];
      }
    }

    if(static_cast<int>(parts.size()) > 0 && (parts[0] == "Global" || parts[0] == "global")) {
      program[i] = "include_global_variable = ";
      for(int j = 1; j < static_cast<int>(parts.size()); ++j) {
        program[i] = program[i] + " " + parts[j];
      }
    }

    if(static_cast<int>(parts.size()) > 0 && (parts[0] == "Global_List" || parts[0] == "global_list")) {
      program[i] = "include_global_list_variable = ";
      for(int j = 1; j < static_cast<int>(parts.size()); ++j) {
        program[i] = program[i] + " " + parts[j];
      }
    }

  }
  Parser::remove_blank_lines();
  printf("==== PARSED PROGRAM FILE %s =======================\n", program_file);
  Parser::print_program();
  printf("==== END PARSED PROGRAM FILE %s =======================\n\n", program_file);
  for(int i = 0; i < static_cast<int>(program.size()); ++i) {
    Parser::parse_statement(program[i], i, program_file);
  }
  Parser::get_property("check_properties", &Parser::check_properties);

  return Parser::property_names.size();
}

void Parser::print_errors(char* filename) {
  if(Parser::error_string != "") {
    FILE* fp = fopen(filename, "a");
    fprintf(fp, "%s", Parser::error_string.c_str());
    fclose(fp);
    Global::Error_found = 1;
  }
}

void Parser::print_warnings(char* filename) {
  int size = static_cast<int>(Parser::property_names.size());
  int found = 0;
  for(int i = 0; i < size && found == 0; ++i) {
    found = (Parser::property_is_default[i] == 0 && Parser::property_is_duplicate[i] == 0 && Parser::property_is_used[i] == 0)
      || (Parser::property_is_default[i] == 0 && Parser::property_is_duplicate[i] == 1);
  }
  if(found) {
    FILE* fp = fopen(filename, "a");
    for(int i = 0; i < size; ++i) {
      if(Parser::property_is_default[i] == 0 && Parser::property_is_duplicate[i] == 0 && Parser::property_is_used[i] == 0) {
        fprintf(fp, "FRED Warning (file %s, line %d) Unrecognized property statement: %s = %s\n",
          Parser::model_file[i].c_str(),
          Parser::property_lineno[i],
          Parser::property_names[i].c_str(),
          Parser::property_values[i].c_str());
        int pos = static_cast<int>(Parser::property_names[i].find(".states"));
        if(pos != static_cast<int>(std::string::npos)) {
          std::string cond = Parser::property_names[i].substr(0, pos);
          fprintf(fp, "  Is %s a missing condition?\n", cond.c_str());
        }
      } else if(Parser::property_is_default[i] == 0 && Parser::property_is_duplicate[i] == 1) {
//        fprintf(fp, "FRED Warning (file %s, line %d) Ignored duplicate property statement: %s = %s\n",
//            Parser::model_file[i].c_str(), Parser::property_lineno[i],
//            Parser::property_names[i].c_str(), Parser::property_values[i].c_str());
      }
    }
    fclose(fp);
  }
}

void Parser::report_parameter_check() {
  int size = static_cast<int>(Parser::property_names.size());

  FILE* fp = fopen("CHECK_PARAMETERS.txt", "w");

  fprintf(fp, "### BEGIN CHECK PARAMETERS\n\n");

  fprintf(fp, "### These user entries were not used to set values in this run of FRED.\n"); 
  fprintf(fp, "### WARNING: CHECK THESE ENTRIES CAREFULLY. THEY MAY BE MISSPELLED.\n"); 
  fprintf(fp, "### BEGIN UNUSED USER-DEFINED PROPERTIES\n");
  for(int i = 0; i < size; i++) {
    if(Parser::property_is_default[i] == 0 && Parser::property_is_duplicate[i] == 0 && Parser::property_is_used[i] == 0) {
      fprintf(fp, "%s = %s\n", Parser::property_names[i].c_str(), Parser::property_values[i].c_str());
    }
  }
  fprintf(fp, "### END   UNUSED USER-DEFINED PROPERTIES\n\n");

  fprintf(fp, "### These user entries appear in the FRED program but are overridden by later entries.\n"); 
  fprintf(fp, "### BEGIN DUPLICATE USER-DEFINED PROPERTIES\n");
  for(int i = 0; i < size; ++i) {
    if(Parser::property_is_default[i] == 0 && Parser::property_is_duplicate[i] == 1) {
      fprintf(fp, "%s = %s\n", Parser::property_names[i].c_str(), Parser::property_values[i].c_str());
    }
  }
  fprintf(fp, "### END   DUPLICATE USER-DEFINED PROPERTIES\n\n");

  fprintf(fp, "### These user entries appear in the FRED program and are not overridden by later entries.\n"); 
  fprintf(fp, "### BEGIN AVAILABLE USER-DEFINED PROPERTIES\n");
  for(int i = 0; i < size; ++i) {
    if(Parser::property_is_default[i] == 0 && Parser::property_is_duplicate[i] == 0) {
      fprintf(fp, "%s = %s\n", Parser::property_names[i].c_str(),  Parser::property_values[i].c_str());
    }
  }
  fprintf(fp, "### END   AVAILABLE USER-DEFINED PROPERTIES\n\n");

  fprintf(fp, "### These user entries were used to set values in this run of FRED.\n"); 
  fprintf(fp, "### BEGIN USED USER-DEFINED PROPERTIES\n");
  for(int i = 0; i < size; ++i) {
    if(Parser::property_is_default[i] == 0 && Parser::property_is_used[i] == 1) {
      fprintf(fp, "%s = %s\n", Parser::property_names[i].c_str(), Parser::property_values[i].c_str());
    }
  }
  fprintf(fp, "### END   USED USER-DEFINED PROPERTIES\n\n");

  fprintf(fp, "### These entries appear in the config file but are overridden by later entries.\n"); 
  fprintf(fp, "### BEGIN OVERRIDDEN DEFAULT PROPERTIES\n");
  for(int i = 0; i < size; ++i) {
    if(Parser::property_is_default[i] && Parser::property_is_duplicate[i] == 1) {
      fprintf(fp, "%s = %s\n", Parser::property_names[i].c_str(), Parser::property_values[i].c_str());
    }
  }
  fprintf(fp, "### END   OVERRIDDEN DEFAULT PROPERTIES\n\n");

  fprintf(fp, "### These entries appear in the config file and are not overridden by later entries.\n"); 
  fprintf(fp, "### BEGIN AVAILABLE DEFAULT PROPERTIES\n");
  for(int i = 0; i < size; ++i) {
    if(Parser::property_is_default[i] && Parser::property_is_duplicate[i] == 0) {
      fprintf(fp, "%s = %s\n", Parser::property_names[i].c_str(), Parser::property_values[i].c_str());
    }
  }
  fprintf(fp, "### END   AVAILABLE DEFAULT PROPERTIES\n\n");

  fprintf(fp, "### These entries from the config file were used to set values in this run of FRED.\n"); 
  fprintf(fp, "### BEGIN USED DEFAULT PROPERTIES\n");
  for(int i = 0; i < size; ++i) {
    if(Parser::property_is_default[i] && Parser::property_is_used[i] == 1) {
      fprintf(fp, "%s = %s\n", Parser::property_names[i].c_str(), Parser::property_values[i].c_str());
    }
  }
  fprintf(fp, "### END   USED DEFAULT PROPERTIES\n\n");

  fprintf(fp, "### The following entries in the config file were ignored in this run of FRED.\n"); 
  fprintf(fp, "### BEGIN UNUSED DEFAULT PROPERTIES\n");
  for(int i = 0; i < size; ++i) {
    if(Parser::property_is_default[i] && Parser::property_is_duplicate[i] == 0 && Parser::property_is_used[i] == 0) {
      fprintf(fp, "%s = %s\n", Parser::property_names[i].c_str(),  Parser::property_values[i].c_str());
    }
  }
  fprintf(fp, "### END   UNUSED DEFAULT PROPERTIES\n\n");

  fprintf(fp, "### The following properties were not found in the property files, but\n");
  fprintf(fp, "### default values are specified in the source code.\n"); 
  fprintf(fp, "### \n");
  fprintf(fp, "### BEGIN PROPERTIES NOT FOUND\n");
  for(int i = 0; i < static_cast<int>(Parser::property_not_found.size()); ++i) {
    fprintf(fp, "%s\n", Parser::property_not_found[i].c_str());
  }
  fprintf(fp, "### END   PROPERTIES NOT FOUND\n\n");

  fprintf(fp, "### END CHECK PARAMETERS\n\n");
  fclose(fp);
}

void Parser::read_program_file(char* program_file) {
  FILE* fp;
  char* line = nullptr;
  std::size_t linecap = 0;
  std::size_t linelen;

  fp = Utils::fred_open_file(program_file);
  if(fp != nullptr) {
    fprintf(stdout, "FRED reading program file %s\n", program_file);
    fflush(stdout);

    while((linelen = getline(&line, &linecap, fp)) > 0) {
      if(static_cast<int>(linelen) == -1) {
        break;
      }
      std::string current = std::string(line);
      int len = current.length();

      // replace newline with spaces
      if(static_cast<int>(len) > 0 && current.substr(len - 1, 1) == "\n") {
        current = current.substr(0, len - 1);
      }

      if(static_cast<int>(current.find("include ")) == 0) {
        char includefilename[FRED_STRING_SIZE];
        sscanf(current.c_str(), "include %s", includefilename);
        // printf("INCLUDE found: include |%s|\n", includefilename);
        Parser::read_program_file(includefilename);
        continue;
      }

      if(static_cast<int>(current.find("use FRED::")) == 0) {
        char includefilename[FRED_STRING_SIZE];
        std::string library_name = current.substr(strlen("use FRED::"));
        boost::algorithm::trim(library_name);
        snprintf(includefilename, FRED_STRING_SIZE, "$FRED_HOME/library/%s/%s.fred", library_name.c_str(), library_name.c_str());
        Parser::read_program_file(includefilename);
        continue;
      }
      program.push_back(current);
    }
    
    program.push_back(" ");
    fclose(fp);
  } else {
    fprintf(stdout, "FRED failed reading program file %s\n", program_file);
    fflush(stdout);
    abort();
    // Utils::fred_abort("Help!  Can't read program_file %s\n", program_file);
  }

  fprintf(stdout, "FRED finished reading program file %s\n", program_file);
  fflush(stdout);
}


void Parser::parse_statement(std::string statement, int linenum, char* filename) {

  // statement = Parser::delete_whitespace(statement);
  if(static_cast<int>(statement.length()) == 0) {
    return;
  }
      
  if(static_cast<int>(statement.find("if ")) == 0) {
    Rule::add_rule_line(statement);
    Global::Use_rules = 1;
    return;
  }

  std::string orig_statement = statement;
  std::string property = "";
  std::string value = "";

  // the line is a property statement of it contains = but not any comparison operator
  int pos = static_cast<int>(statement.find("="));
  if(pos != static_cast<int>(std::string::npos)) {

    // remove whitespace from property name
    property = statement.substr(0, pos);
    value = statement.substr(pos + 1);
    value = Parser::remove_leading_whitespace(value);

    std::string::iterator end_pos = std::remove(property.begin(), property.end(), ' ');
    property.erase(end_pos, property.end());    

    // now reformulate the statement:
    statement = property + " = " + value;

    if(property.find_first_of("=!<>") != std::string::npos) {
      Parser::error_string += "Bad property statement [2]:\n  " + orig_statement + "\n";
      printf("ERROR: Bad property statement %s\n", orig_statement.c_str());
      return;
    }

    if(value.find_first_of("=!<>") != std::string::npos) {
      Parser::error_string += "Bad property statement [3]:\n  " + orig_statement + "\n";
      printf("ERROR: Bad property statement %s\n", orig_statement.c_str());
      return;
    }
  } else {
    Parser::error_string += "Bad property statement [1]:\n  " + orig_statement + "\n";
    printf("ERROR: Bad property statement %s\n", orig_statement.c_str());
    return;
  }

  if(static_cast<int>(value.length()) == 0) {
    Parser::error_string += "Bad property statement [4]:\n  " + orig_statement + "\n";
    printf("ERROR: Bad property statement %s\n", orig_statement.c_str());
    return;
  }
      
  printf("READ_property: %s = |%s|\n", property.c_str(), value.c_str());
  Parser::model_file.push_back(std::string(filename));
  Parser::property_lineno.push_back(linenum);
  Parser::property_names.push_back(property);
  Parser::property_values.push_back(value);
  Parser::property_is_duplicate.push_back(0);
  Parser::property_is_used.push_back(0);
  Parser::property_is_default.push_back(Parser::default_properties);

  if(static_cast<int>(property.find("include_")) != 0 && static_cast<int>(property.find("exclude_"))!= 0 && property.find(".add") == std::string::npos) {
    std::unordered_map<std::string, int>::const_iterator got = Parser::property_map.find(property);
    if(got != Parser::property_map.end()) {
      Parser::property_is_duplicate[got->second] = 1;
    }
  }
  int n = Parser::number_of_properties;
  Parser::property_map[property] = Parser::number_of_properties++;
  if(property == "include_condition") {
    Condition::include_condition(value);
    Parser::property_is_used[n] = 1;
    Parser::property_is_duplicate[n] = 0;
  }
  
  if(property == "exclude_condition") {
    Condition::exclude_condition(value);
    Parser::property_is_used[n] = 1;
    Parser::property_is_duplicate[n] = 0;
  }

  if(property == "include_variable") {
    Person::include_variable(value);
    Parser::property_is_used[n] = 1;
    Parser::property_is_duplicate[n] = 0;
  }

  if(property == "exclude_variable") {
    Person::exclude_variable(value);
    Parser::property_is_used[n] = 1;
    Parser::property_is_duplicate[n] = 0;
  }

  if(property == "include_list_variable") {
    Person::include_list_variable(value);
    Parser::property_is_used[n] = 1;
    Parser::property_is_duplicate[n] = 0;
  }

  if(property == "exclude_list_variable") {
    Person::exclude_list_variable(value);
    Parser::property_is_used[n] = 1;
    Parser::property_is_duplicate[n] = 0;
  }

  if(property == "include_global_variable") {
    Person::include_global_variable(value);
    Parser::property_is_used[n] = 1;
    Parser::property_is_duplicate[n] = 0;
  }

  if(property == "exclude_global_variable") {
    Person::exclude_global_variable(value);
    Parser::property_is_used[n] = 1;
    Parser::property_is_duplicate[n] = 0;
  }

  if(property == "include_global_list_variable") {
    Person::include_global_list_variable(value);
    Parser::property_is_used[n] = 1;
    Parser::property_is_duplicate[n] = 0;
  }

  if(property == "exclude_global_list_variable") {
    Person::exclude_global_list_variable(value);
    Parser::property_is_used[n] = 1;
    Parser::property_is_duplicate[n] = 0;
  }

  if(property == "include_place") {
    Place_Type::include_place_type(value);
    Parser::property_is_used[n] = 1;
    Parser::property_is_duplicate[n] = 0;
  }

  if(property == "exclude_place") {
    Place_Type::exclude_place_type(value);
    Parser::property_is_used[n] = 1;
    Parser::property_is_duplicate[n] = 0;
  }

  if(property == "include_network") {
    Network_Type::include_network_type(value);
    Parser::property_is_used[n] = 1;
    Parser::property_is_duplicate[n] = 0;
  }

  if(property == "exclude_network") {
    Network_Type::exclude_network_type(value);
    Parser::property_is_used[n] = 1;
    Parser::property_is_duplicate[n] = 0;
  }

  if(property.find(".add") != std::string::npos) {
    Parser::property_is_used[n] = 1;
    Parser::property_is_duplicate[n] = 0;
  }

  return;
}

//void Parser::secondary_parse_statement(std::string statement) {
//
//  if(static_cast<int>(statement.length()) == 0) {
//    return;
//  }
//
//  if(static_cast<int>(statement.find("if ")) == 0) {
//    Rule::add_rule_line(statement);
//    Global::Use_rules = 1;
//    return;
//  }
//
//  std::string orig_statement = statement;
//  std::string property = "";
//  std::string value = "";
//
//  // the line is a property statement of it contains = but not any comparison operator
//  int pos = static_cast<int>(statement.find("="));
//  if(pos != static_cast<int>(std::string::npos)) {
//
//    // remove whitespace from property name
//    property = statement.substr(0, pos);
//    value = statement.substr(pos + 1);
//    value = Parser::remove_leading_whitespace(value);
//
//    std::string::iterator end_pos = std::remove(property.begin(), property.end(), ' ');
//    property.erase(end_pos, property.end());
//
//    // now reformulate the statement:
//    statement = property + " = " + value;
//
//    if(property.find_first_of("=!<>") != std::string::npos) {
//      Parser::error_string += "Bad property statement [2]:\n  " + orig_statement + "\n";
//      printf("ERROR: Bad property statement %s\n", orig_statement.c_str());
//      return;
//    }
//
//    if(value.find_first_of("=!<>") != std::string::npos) {
//      Parser::error_string += "Bad property statement [3]:\n  " + orig_statement + "\n";
//      printf("ERROR: Bad property statement %s\n", orig_statement.c_str());
//      return;
//    }
//  } else {
//    Parser::error_string += "Bad property statement [1]:\n  " + orig_statement + "\n";
//    printf("ERROR: Bad property statement %s\n", orig_statement.c_str());
//    return;
//  }
//
//  if(static_cast<int>(value.length()) == 0) {
//    Parser::error_string += "Bad property statement [4]:\n  " + orig_statement + "\n";
//    printf("ERROR: Bad property statement %s\n", orig_statement.c_str());
//    return;
//  }
//
//  printf("Secondary_READ_property: %s = |%s|\n", property.c_str(), value.c_str());
////  Parser::model_file.push_back(std::string(filename));
////  Parser::property_lineno.push_back(linenum);
////  Parser::property_names.push_back(property);
////  Parser::property_values.push_back(value);
////  Parser::property_is_duplicate.push_back(0);
////  Parser::property_is_used.push_back(0);
////  Parser::property_is_default.push_back(Parser::default_properties);
//
////  if(static_cast<int>(property.find("include_")) != 0 && static_cast<int>(property.find("exclude_"))!= 0 && property.find(".add") == std::string::npos) {
////    std::unordered_map<std::string, int>::const_iterator got = Parser::property_map.find(property);
////    if(got != Parser::property_map.end()) {
////      Parser::property_is_duplicate[got->second] = 1;
////    }
////  }
////  int n = Parser::number_of_properties;
//  Parser::property_map[property] = Parser::number_of_properties++;
//  if(property == "include_condition") {
//    Condition::include_condition(value);
////    Parser::property_is_used[n] = 1;
////    Parser::property_is_duplicate[n] = 0;
//  }
//
//  if(property == "exclude_condition") {
//    Condition::exclude_condition(value);
////    Parser::property_is_used[n] = 1;
////    Parser::property_is_duplicate[n] = 0;
//  }
//
//  if(property == "include_variable") {
//    Person::include_variable(value);
////    Parser::property_is_used[n] = 1;
////    Parser::property_is_duplicate[n] = 0;
//  }
//
//  if(property == "exclude_variable") {
//    Person::exclude_variable(value);
////    Parser::property_is_used[n] = 1;
////    Parser::property_is_duplicate[n] = 0;
//  }
//
//  if(property == "include_list_variable") {
//    Person::include_list_variable(value);
////    Parser::property_is_used[n] = 1;
////    Parser::property_is_duplicate[n] = 0;
//  }
//
//  if(property == "exclude_list_variable") {
//    Person::exclude_list_variable(value);
////    Parser::property_is_used[n] = 1;
////    Parser::property_is_duplicate[n] = 0;
//  }
//
//  if(property == "include_global_variable") {
//    Person::include_global_variable(value);
////    Parser::property_is_used[n] = 1;
////    Parser::property_is_duplicate[n] = 0;
//  }
//
//  if(property == "exclude_global_variable") {
//    Person::exclude_global_variable(value);
////    Parser::property_is_used[n] = 1;
////    Parser::property_is_duplicate[n] = 0;
//  }
//
//  if(property == "include_global_list_variable") {
//    Person::include_global_list_variable(value);
////    Parser::property_is_used[n] = 1;
////    Parser::property_is_duplicate[n] = 0;
//  }
//
//  if(property == "exclude_global_list_variable") {
//    Person::exclude_global_list_variable(value);
////    Parser::property_is_used[n] = 1;
////    Parser::property_is_duplicate[n] = 0;
//  }
//
//  if(property == "include_place") {
//    Place_Type::include_place_type(value);
////    Parser::property_is_used[n] = 1;
////    Parser::property_is_duplicate[n] = 0;
//  }
//
//  if(property == "exclude_place") {
//    Place_Type::exclude_place_type(value);
////    Parser::property_is_used[n] = 1;
////    Parser::property_is_duplicate[n] = 0;
//  }
//
//  if(property == "include_network") {
//    Network_Type::include_network_type(value);
////    Parser::property_is_used[n] = 1;
////    Parser::property_is_duplicate[n] = 0;
//  }
//
//  if(property == "exclude_network") {
//    Network_Type::exclude_network_type(value);
////    Parser::property_is_used[n] = 1;
////    Parser::property_is_duplicate[n] = 0;
//  }
//
////  if(property.find(".add") != std::string::npos) {
////    Parser::property_is_used[n] = 1;
////    Parser::property_is_duplicate[n] = 0;
////  }
//
//  return;
//}

bool Parser::does_property_exist(std::string name) {
  std::unordered_map<std::string, int>::const_iterator found = Parser::property_map.find(name);
  return (found != Parser::property_map.end());
}


std::string Parser::find_property(std::string name) {
  std::unordered_map<std::string, int>::const_iterator found = Parser::property_map.find(name);
  if(found != Parser::property_map.end()) {
    int i = found->second;
    Parser::property_is_used[i] = 1;
    return Parser::property_values[Parser::property_map[name]];
  } else {
    Parser::property_not_found.push_back(name);
    if(Parser::abort_on_failure) {
      Utils::fred_abort("params: %s not found\n", name.c_str()); 
    }
    return "";
  }
}


int Parser::get_next_property(std::string name, std::string* value, int start) {
  // FRED_VERBOSE(0, "get_next_property %s start %d\n", name.c_str(), start);
  *value = "";
  for(int i = start; i < static_cast<int>(Parser::property_names.size()); ++i) {
    if(name == Parser::property_names[i]) {
      *value = Parser::property_values[i];
      return i;
    }
  }
  return -1;
}

int Parser::get_property(std::string name, bool* p) {
  int temp;
  int n = Parser::get_property(name, &temp);
  *p = temp;
  return n;
}


int Parser::get_property(std::string name, int* p) {
  // printf("GET_PARAM: look for %s\n", name.c_str()); fflush(stdout);
  std::string value = Parser::find_property(name);
  if(value == "") {
    return 0;
  }
  sscanf(value.c_str(), "%d", p);
  if(Global::Debug > 1) {
    printf("GET_PARAM: %s = %d\n", name.c_str(), *p);
    fflush( stdout);
  }
  return 1;
}

int Parser::get_property(std::string name, unsigned long* p) {
  // printf("GET_PARAM: look for %s\n", name.c_str()); fflush(stdout);
  std::string value = Parser::find_property(name);
  if(value == "") {
    return 0;
  }
  sscanf(value.c_str(), "%lu", p);
  if(Global::Debug > 1) {
    printf("GET_PARAM: %s = %lu\n", name.c_str(), *p);
    fflush( stdout);
  }
  return 1;
}


int Parser::get_property(std::string name, long long int * p) {
  // printf("GET_PARAM: look for %s\n", name.c_str()); fflush(stdout);
  std::string value = Parser::find_property(name);
  if(value == "") {
    return 0;
  }
  sscanf(value.c_str(), "%lld", p);
  if(Global::Debug > 1) {
    printf("GET_PARAM: %s = %lld\n", name.c_str(), *p);
    fflush( stdout);
  }
  return 1;
}


int Parser::get_property(std::string name, double* p) {
  // printf("GET_PARAM: look for %s\n", name.c_str()); fflush(stdout);
  std::string value = Parser::find_property(name);
  if(value == "") {
    return 0;
  }
  sscanf(value.c_str(), "%lf", p);
  if(Global::Debug > 1) {
    printf("GET_PARAM: %s = %f\n", name.c_str(), *p);
    fflush( stdout);
  }
  return 1;
}

int Parser::get_property(std::string name, float* p) {
  // printf("GET_PARAM: look for %s\n", name.c_str()); fflush(stdout);
  std::string value = Parser::find_property(name);
  if(value == "") {
    return 0;
  }
  sscanf(value.c_str(), "%f", p);
  if(Global::Debug > 1) {
    printf("GET_PARAM: %s = %f\n", name.c_str(), *p);
    fflush( stdout);
  }
  return 1;
}

int Parser::get_property(std::string name, std::string* p) {
  // printf("GET_PARAM: look for %s\n", name.c_str()); fflush(stdout);
  std::string value = Parser::find_property(name);
  if(value == "") {
    return 0;
  }
  *p = value;
  if(Global::Debug > 1) {
    printf("GET_PARAM: %s = |%s|\n", name.c_str(), p->c_str());
    fflush( stdout);
  }
  return 1;
}

int Parser::get_property(std::string name, std::string &p) {
  // printf("GET_PARAM: look for %s\n", name.c_str()); fflush(stdout);
  std::string value = Parser::find_property(name);
  if(value == "") {
    return 0;
  }
  p = value;
  if(Global::Debug > 1) {
    printf("GET_PARAM: %s = |%s|\n", name.c_str(), p.c_str());
    fflush( stdout);
  }
  return 1;
}

int Parser::get_property(std::string name, char* p) {
  // printf("GET_PARAM: look for %s\n", name.c_str()); fflush(stdout);
  std::string value = Parser::find_property(name);
  if(value == "") {
    return 0;
  }
  strcpy(p, value.c_str());
  if(Global::Debug > 1) {
    printf("GET_PARAM: %s = |%s|\n", name.c_str(), p);
    fflush( stdout);
  }
  return 1;
}


int Parser::get_property_vector(char* name, std::vector<int> &p){
  char str[FRED_STRING_SIZE];
  int n;
  char* pch;
  int v;
  Parser::get_property(name, str);
  pch = strtok(str, " ");
  if(sscanf(pch, "%d", &n) == 1){
    for(int i = 0; i < n; ++i){
      pch = strtok(nullptr," ");
      if(pch == nullptr) {
        Utils::fred_abort("Help! bad property vector: %s\n", name);
      }
      sscanf(pch,"%d",&v);
      p.push_back(v);
    }
  } else {
    Utils::fred_abort("Incorrect format for vector %s\n", name); 
  }
  return n;
}

int Parser::get_property_vector(char* s, std::vector<double> &p){
  char str[FRED_STRING_SIZE];
  int n = Parser::get_property(s, str);
  if(n == 0) {
    return n;
  }
  char* pch = strtok(str," ");
  if(sscanf(pch, "%d", &n) == 1) {
    for(int i = 0; i < n; ++i) {
      double v;
      pch = strtok (nullptr, " ");
      if(pch == nullptr) {
        Utils::fred_abort("Help! bad property vector: %s\n", s);
      }
      sscanf(pch, "%lf", &v);
      p.push_back(v);
    }
  } else {
    Utils::fred_abort("Incorrect format for vector %s\n", s); 
  }
  return n;
}

int Parser::get_property_vector(char* s, double* p) {
  char str[FRED_STRING_SIZE];
  int n;
  char* pch;
  Parser::get_property(s, str);
  pch = strtok(str, " ");
  if(sscanf(pch, "%d", &n) == 1) {
    for(int i = 0; i < n; ++i) {
      pch = strtok(nullptr, " ");
      if(pch == nullptr) {
        Utils::fred_abort("Help! bad property vector: %s\n", s);
      }
      sscanf(pch, "%lf", &p[i]);
    }
  } else {
    Utils::fred_abort("params: %s not found\n", s); 
  }
  return n;
}

int Parser::get_property_vector(char* s, int* p) {
  char str[FRED_STRING_SIZE];
  int n;
  char* pch;
  Parser::get_property(s, str);
  pch = strtok(str, " ");
  if(sscanf(pch, "%d", &n) == 1) {
    for(int i = 0; i < n; ++i) {
      pch = strtok(nullptr, " ");
      if(pch == nullptr) {
        Utils::fred_abort("Help! bad property vector: %s\n", s);
      }
      sscanf(pch, "%d", &p[i]);
    }
  } else {
    Utils::fred_abort(""); 
  }
  return n;
}


int Parser::get_property_vector(char* s, std::string* p) {
  char str[FRED_STRING_SIZE];
  int n;
  char* pch;
  Parser::get_property(s, str);
  pch = strtok(str, " ");
  if(sscanf(pch, "%d", &n) == 1) {
    for(int i = 0; i < n; ++i) {
      pch = strtok(nullptr, " ");
      if(pch == nullptr) {
        Utils::fred_abort("Help! bad property vector: %s\n", s);
      }
      p[i] = pch;
    }
  } else {
    Utils::fred_abort(""); 
  }
  return n;
}

//added
int Parser::get_property_vector_from_string(char *s, std::vector<double> &p){
  int n;
  char *pch;
  double v;
  pch = strtok(s," ");
  if(sscanf(pch, "%d", &n) == 1) {
    for(int i = 0; i < n; ++i) {
      pch = strtok (nullptr, " ");
      if(pch == nullptr) {
        Utils::fred_abort("Help! bad property vector: %s\n", s);
      }
      sscanf(pch, "%lf", &v);
      p.push_back(v);
    }
    for(std::vector<double>::const_iterator i = p.begin(); i != p.end(); ++i) {
      printf("age!! %f \n", *i); //std::cout << *i << ' ';
    }
    fflush(stdout);
  } else {
    Utils::fred_abort("Incorrect format for vector %s\n", s); 
  }
  return n;
}

int Parser::get_property_matrix(char* s, double*** p) {
  int n = 0;
  Parser::get_property((char*)s, &n);
  if(n > 0) {
    double* tmp;
    tmp = new double[n];
    Parser::get_property_vector((char*)s, tmp);
    int temp_n = static_cast<int>(sqrt(static_cast<double>(n)));
    if(n != temp_n * temp_n) {
      Utils::fred_abort("Improper matrix dimensions: matricies must be square found dimension %i\n", n); 
    }
    n = temp_n;
    (*p) = new double*[n];
    for(int i = 0; i < n; ++i) {
      (*p)[i] = new double[n];
    }
    for(int i = 0; i < n; ++i) {
      for(int j = 0; j < n; ++j) {
        (*p)[i][j] = tmp[i * n + j];
      }
    }
    delete[] tmp;
    return n;
  }
  return -1;
}

edge_vector_t Parser::get_edges(std::string network_name) {
  char values[FRED_STRING_SIZE];
  edge_vector_t result;
  result.clear();
  int size = static_cast<int>(Parser::property_names.size());
  for(int i = 0; i < size; ++i) {
    if(Parser::property_names[i] == network_name + ".add_edge") {
      strcpy(values, Parser::property_values[i].c_str());
      int p1;
      int p2;
      double weight;
      int matched_char_count = sscanf(values, "%d %d %lf", &p1, &p2, &weight);
      if(matched_char_count == EOF) {
        continue;
      } else if(matched_char_count == 2) {
        if(Global::Compile_FRED == 0) {
          edge_info_t edge_info;
          edge_info.from_idx = p1;
          edge_info.to_idx = p2;
          edge_info.weight = 1.0;
          result.push_back(edge_info);
        }
        Parser::property_is_used[i] = 1;
        Parser::property_is_duplicate[i] = 0;
      } else if(matched_char_count == 3) {
        if(Global::Compile_FRED == 0) {
           edge_info_t edge_info;
           edge_info.from_idx = p1;
           edge_info.to_idx = p2;
           edge_info.weight = weight;
           result.push_back(edge_info);
        }
        Parser::property_is_used[i] = 1;
        Parser::property_is_duplicate[i] = 0;
      }
    }
  }
  return result;
}
