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
#include "Expression.h"
#include "Factor.h"
#include "Geo.h"
#include "Global.h"
#include "Parser.h"
#include "Person.h"
#include "Place.h"
#include "Place_Type.h"
#include "Preference.h"
#include "Random.h"
#include "Utils.h"

#define TWOARGS 14

std::map<std::string, int> Expression::op_map = {
  {"add", 1},
  {"sub", 2},
  {"mult", 3},
  {"div", 4},
  {"mod", 5},
  {"equal", 6},
  {"dist", 7},
  {"min", 8},
  {"max", 9},
  {"uniform", 10},
  {"normal", 11},
  {"lognormal", 12},
  {"binomial", 13},
  {"negbinomial", 14},
  {"poisson", 15},
  {"exponential", 16},
  {"geometric", 17},
  {"pow", 18},
  {"log", 19},
  {"exp", 20},
  {"abs", 21},
  {"sin", 22},
  {"cos", 23},
  {"pool", 24},
  {"filter", 25},
  {"list", 26},
  {"value", 27},
  {"distance", 28},
  {"select", 29},
};

std::map<std::string, int> Expression::value_map = {
  {"male", 1},
  {"female", 0},
  {"householder", 0},
  {"spouse", 1},
  {"child", 2},
  {"sibling", 3},
  {"parent", 4},
  {"grandchild", 5},
  {"in_law", 6},
  {"other_relative", 7},
  {"boarder", 8},
  {"housemate", 9},
  {"partner", 10},
  {"foster_child", 11},
  {"other_non_relative", 12},
  {"institutionalized_group_quarters_pop", 13},
  {"noninstitutionalized_group_quarters_pop", 14},
  {"unknown_race", -1},
  {"white", 1},
  {"african_american", 2},
  {"american_indian", 3},
  {"alaska_native", 4},
  {"tribal", 5},
  {"asian", 6},
  {"hawaiian_native", 7},
  {"other_race", 8},
  {"multiple_race", 9},
  {"infant", 0},
  {"preschool", 1},
  {"student", 2},
  {"teacher", 3},
  {"worker", 4},
  {"weekend_worker", 5},
  {"unemployed", 6},
  {"retired", 7},
  {"prisoner", 8},
  {"college_student", 9},
  {"military", 10},
  {"nursing_home_resident", 11},
  {"Sun", 0},
  {"Mon", 1},
  {"Tue", 2},
  {"Wed", 3},
  {"Thu", 4},
  {"Fri", 5},
  {"Sat", 6},
  {"Jan", 1},
  {"Feb", 2},
  {"Mar", 3},
  {"Apr", 4},
  {"May", 5},
  {"Jun", 6},
  {"Jul", 7},
  {"Aug", 8},
  {"Sep", 9},
  {"Oct", 10},
  {"Nov", 11},
  {"Dec", 12},
};

bool Expression::is_log_initialized = false;
std::string Expression::expression_log_level = "";
std::unique_ptr<spdlog::logger> Expression::expression_logger = nullptr;

/**
 * Creates an Expression with a specified string. If this string has balanced 
 * parentheses, it will be converted to prefix notation and set as the name of 
 * this expression. Default variables are initialized.
 *
 * @param s the string
 */
Expression::Expression(std::string s) {
  if(Expression::unbalanced_parens(s)) {
    this->name = "???";
  } else {
    this->name = convert_infix_to_prefix(Utils::delete_spaces(s));
  }
  this->expr1 = nullptr;
  this->expr2 = nullptr;
  this->expr3 = nullptr;
  this->expr4 = nullptr;
  this->factor = nullptr;
  this->op = "";
  this->op_index = 0;
  this->number_of_expressions = 0;
  this->number = 0.0;
  this->use_other = false;
  this->warning = false;
  this->is_select = false;
  this->list_var = "";
  this->list_var_id = -1;
  this->pref_str = "";
  this->preference = nullptr;
  this->is_list_var = false;
  this->is_global = false;
  this->is_pool = false;
  this->pool.clear();
  this->is_filter = false;
  this->is_list_expr = false;
  this->is_list = false;
  this->is_value = false;
  this->is_distance = false;  
}

/**
 * Gets the name of the expression.
 *
 * @return the name
 */
std::string Expression::get_name() {
  return this->name;
}

/**
 * Checks whether or not the specified string has unbalanced parentheses.
 *
 * @param str the string
 * @return if the string has unbalanced parentheses
 */
bool Expression::unbalanced_parens(std::string str) {
  int inner = 0;
  for(int i = str.size() - 1; i >= 0; --i) {
    if(str[i]==')') {
      ++inner;
    }
    if(str[i]=='(') {
      --inner;
      if(inner < 0) {
        return true;
      }
    }
  }
  return (inner != 0);
}

/**
 * Recursively expands all unary minuses, denoted by a hash symbol (#), into 0-. This simplifies 
 * the expression for further manipulation. 
 * Assumes all unary minuses have already been replaced in the replace_unary_minus method.
 *
 * @param s the string
 * @return the new string
 */
std::string Expression::expand_minus(std::string s) {
  std::string result = "";
  int size = static_cast<int>(s.length());
  if(size == 0) {
    return result;
  }
  // position of next token
  int next_pos = 0;
  while(next_pos < static_cast<int>(s.length())) {
    std::string token = this->get_next_token(s, next_pos);
    next_pos += token.length();
    if(token != "#") {
      // case: non-#
      result += token;
    } else if(size <= next_pos) {
      this->minus_err = "unary minus at end of string: |" + s + "|";
      return "";
    } else if(s[next_pos] == '(') {
      // case: #(...)
      // find matching right paren
      int j = next_pos + 1;
      int level = 1;
      while(j < size && level != 0) {
        if(s[j] == '(') {
          ++level;
        }
        if(s[j] == ')') {
          --level;
        }
        ++j;
      }
      if(j == size && level != 0) {
        this->minus_err = "ill-formed expression missing right paren: |" + s + "|";
        return "";
      }
      std::string sub = "";
      if(j < size) {
        sub = s.substr(next_pos + 1, j - next_pos - 1);
      } else {
        sub = s.substr(next_pos + 1);
      }
      result += "(0-" + this->expand_minus(sub) + ")";
      next_pos += sub.length();
    } else {
      // case: ## or #operand or #function(...)
      std::string next = this->get_next_token(s, next_pos);
      next_pos += next.length();
      std::string sub = "";
      if(next == "#") {
        // case: ##
        while(next == "#") {
          sub += next;
          next = this->get_next_token(s, next_pos);
          next_pos += next.length();
        }
        sub += next;
        if(this->is_function(next)) {
          // find args of func
          int start = next_pos;
          int end = start;
          while(end < size && s[end] != ')') {
            ++end;
          }
          if(end == size) {
            this->minus_err = "ill-formed expression missing right paren: |" + s + "|";
            return "";
          }
          std::string arg = s.substr(next_pos + 1, end - next_pos - 1);
          sub +=  "(" + this->expand_minus(arg) + ")";
          next_pos += (end + 1);
        }
        std::string inner = this->expand_minus(sub);
        result += "(0-" + inner + ")";
      } else {
        if(this->is_function(next)) {
          // case: #function(...)
          // find args of func
          int start = next_pos;
          int end = start;
          while(end < size && s[end] != ')') {
            ++end;
          }
          if(end == size) {
            this->minus_err = "ill-formed expression missing right paren: |" + s + "|";
            return "";
          }
          std::string arg = s.substr(next_pos + 1, end - next_pos - 1);
          result += "(0-" + next + "(" + this->expand_minus(arg) + "))";
          next_pos += (end + 1);
        } else {
          // case: #operand
          result += "(0-" + next + ")";
        }
      }
    }
  }
  return result;
}

/**
 * Gets the next token of a string after a specified position. A token will either be an operator 
 * or a sequence of characters that do not include an operator.
 *
 * @param s the string
 * @param pos the position
 * @return the next token
 */
std::string Expression::get_next_token(std::string s, int pos) {
  int pos2 = static_cast<int>(s.find_first_of(",+-*/%^()#", pos));
  std::string token = "";
  if(pos2 != static_cast<int>(std::string::npos)) {
    if(pos == pos2) {
      token = s.substr(pos, 1);
    } else {
      token = s.substr(pos, pos2 - pos);
    }
  } else {
    token = s.substr(pos);
  }
  return token;
}

/**
 * Expands a string of an operator token to its letter counterpart. Example conversions: 
 *  "+" -> "add"; 
 *  "-" -> "sub"; 
 *  "*" -> "mult"; 
 *  "/" -> "div"; 
 *  "%" -> "mod"; 
 *  "^" -> "pow". 
 * Returns the string unmodified if the string is not a valid token.
 *
 * @param s the string
 * @return the result of the conversion
 */
std::string Expression::expand_operator(std::string s) {
  if(s == "+") {
    return "add";
  }
  if(s == "-") {
    return "sub";
  }
  if(s == "*") {
    return "mult";
  }
  if(s == "/") {
    return "div";
  }
  if(s == "%") {
    return "mod";
  }
  if(s == "^") {
    return "pow";
  }
  return s;
}

/**
 * Checks whether a given string is an operator or not.
 *
 * @param s the string
 * @return if the string is an operator
 */
bool Expression::is_operator(std::string s) {
  bool result = (s == "+" || s == "-" || s == "*" || s == "/" || s == "%" || s == "^" || s == "#");
  return result;
}

/**
 * Checks whether a given string is a function or not.
 *
 * @param s the string
 * @return if the string is a function
 */
bool Expression::is_function(std::string s) {
  bool result = (s == "," || s == "select" || s == "pref" || Expression::is_known_function(s));
  return result;
}

/**
 * Used to check the number of arguments in a function. If the string inputted is a comma, 
 * there are two arguments. If not, there is one argument.
 *
 * @param s the string
 * @return the number of arguments
 */
int Expression::get_number_of_args(std::string s) {
  if(s == ",") {
    return 2;
  } else {
    return 1;
  }
}

/**
 * Gets the priority of a given operator using order of operations.
 *
 * @param s the string
 * @return the priority
 */
int Expression::get_priority(std::string s) {
  if(s=="#" || s == "-" || s == "+") {
    return 2;
  } else if(s == "*" || s == "/") {
    return 3;
  } else if(s == "^" || s == "%") {
    return 4;
  } else if(s == ",") {
    return 1;
  } else if(this->is_function(s)) {
    return 5;
  }
  return 0;
}

/**
 * Converts an infix expression to a prefix expression for simpler calculation. 
 * For information on infix and prefix, see: 
 * https://www.cs.colostate.edu/~cs165/.Fall19/recitations/L15/doc/traversal-order.html
 *
 * @param infix the infix expression
 * @return the prefix expression
 */
std::string Expression::infix_to_prefix(std::string infix) {

  // stack for operators.
  std::stack<std::string> operators;

  // stack for operands.
  std::stack<std::string> operands;

  // position of next token
  int next_pos = 0;
  while(next_pos < static_cast<int>(infix.length())) {
    std::string token = this->get_next_token(infix,next_pos);
    next_pos += token.length();

    // if current token is an opening bracket, then push into the
    // operators stack.
    if(token == "(") {
      operators.push(token);
    } else if(token == ")") {
      // if current token is a closing bracket, then pop from both stacks
      // and push result in operands stack until matching opening bracket
      // is not found.
      while(!operators.empty() && operators.top() != "(") {
        // operator
        if(operators.empty()) {
          this->minus_err = "ill-formed expression missing operator: |"+infix+"|";
          return "";
        }
        std::string op = operators.top();
        operators.pop();

        if(this->is_function(op)) {
          int nargs = this->get_number_of_args(op);
          if(nargs == 1) {
            if(operands.empty()) {
              this->minus_err = "ill-formed expression missing operand (ERR 1): |" + infix + "|";
              return "";
            }
            std::string op1 = operands.top();
            operands.pop();
            std::string tmp = op + "(" + op1 + ")";
            operands.push(tmp);
          }
          if(nargs == 2) {
            if(operands.empty()) {
              this->minus_err = "ill-formed expression missing operand (ERR 2): |" + infix + "|";
              return "";
            }
            std::string op1 = operands.top();
            operands.pop();
            if(operands.empty()) {
              this->minus_err = "ill-formed expression missing operand (ERR 3): |" + infix + "|";
              return "";
            }
            std::string op2 = operands.top();
            operands.pop();
            std::string tmp;
            if(op == ",") {
              tmp = op2 + "," + op1;
            } else {
              tmp = op + "(" + op2 + "," + op1 + ")";
            }
            operands.push(tmp);
          }
        } else {
          std::string oper = this->expand_operator(op);
          if(operands.empty()) {
            this->minus_err = "ill-formed expression missing operand (ERR 4): |" + infix + "|";
            return "";
          }
          std::string op1 = operands.top();
          operands.pop();
          std::string tmp;
          if(operands.empty()) {
            this->minus_err = "ill-formed expression missing operand (ERR 5): |"+infix+"|";
            return "";
          }
          std::string op2 = operands.top();
          operands.pop();
          tmp = oper + "(" + op2 + "," + op1 + ")";
          operands.push(tmp);
        }
      }

      // Pop opening bracket from stack.
      operators.pop();
    } else if(!is_operator(token) && (!this->is_function(token))) {
      // If current token is an operand then push it into operands stack.
      operands.push(token);
    } else {
      // If current token is an operator, then push it into operators
      // stack after popping high priority operators from operators stack
      // and pushing result in operands stack.
      while(!operators.empty() && get_priority(token) <= get_priority(operators.top())) {
        std::string op = operators.top();
        operators.pop();

        if(this->is_function(op)) {
          int nargs = this->get_number_of_args(op);
          if(nargs == 1) {
            if(operands.empty()) {
              this->minus_err = "ill-formed expression missing operand (ERR 6): |" + infix + "|";
              return "";
            }
            std::string op1 = operands.top();
            operands.pop();
            std::string tmp = op + "(" + op1 + ")";
            operands.push(tmp);
          }
          if(nargs == 2) {
            if(operands.empty()) {
              this->minus_err = "ill-formed expression missing operand (ERR 7): |" + infix + "|";
              return "";
            }
            std::string op1 = operands.top();
            operands.pop();
            if(operands.empty()) {
              this->minus_err = "ill-formed expression missing operand (ERR 8): |" + infix + "|";
              return "";
            }
            std::string op2 = operands.top();
            operands.pop();
            std::string tmp;
            if(op == ",") {
              tmp = op2 + "," + op1;
            } else {
              tmp = op + "(" + op2 + "," + op1 + ")";
            }
            operands.push(tmp);
          }
        } else {
          std::string oper = this->expand_operator(op);
          if(operands.empty()) {
            this->minus_err = "ill-formed expression missing operand (ERR 9): |" + infix + "|";
            return "";
          }
          std::string op1 = operands.top();
          operands.pop();
          if(operands.empty()) {
            this->minus_err = "ill-formed expression missing operand (ERR 10): |" + infix + "|";
            return "";
          }
          std::string op2 = operands.top();
          operands.pop();
          std::string tmp = oper + "(" + op2 + "," + op1 + ")";
          operands.push(tmp);
        }
      }
      operators.push(token);
    }
  }

  // pop operators from operators stack until it is empty and add result
  // of each pop operation in operands stack.
  while(!operators.empty()) {
    std::string op = operators.top();
    operators.pop();
    if(this->is_function(op)) {
      int nargs = this->get_number_of_args(op);
      if(nargs == 1) {
        if(operands.empty()) {
          this->minus_err = "ill-formed expression missing operand (ERR 11): |" + infix + "|";
          return "";
        }
        std::string op1 = operands.top();
        operands.pop();
        std::string tmp = op + "(" + op1 + ")";
        operands.push(tmp);
      }
      if(nargs == 2) {
        if(operands.empty()) {
          this->minus_err = "ill-formed expression missing operand (ERR 12): |" + infix + "|";
          return "";
        }
        std::string op1 = operands.top();
        operands.pop();
        if(operands.empty()) {
          this->minus_err = "ill-formed expression missing operand (ERR 13): |" + infix + "|";
          return "";
        }
        std::string op2 = operands.top();
        operands.pop();
        std::string tmp;
        if(op == ",") {
          tmp = op2 + "," + op1;
        } else {
          tmp = op + "(" + op2 + "," + op1 + ")";
        }
        operands.push(tmp);
      }
    } else {
      std::string oper = this->expand_operator(op);
      if(operands.empty()) {
        this->minus_err = "ill-formed expression missing operand (ERR 14): |" + infix + "|";
        return "";
      }
      std::string op1 = operands.top();
      operands.pop();
      if(operands.empty()) {
        this->minus_err = "ill-formed expression missing operand (ERR 15): |" + infix + "|";
        return "";
      }
      std::string op2 = operands.top();
      operands.pop();
      std::string tmp = oper + "(" + op2 + "," + op1 + ")";
      operands.push(tmp);
    }
  }

  // final prefix expression is present in operands stack.
  if(operands.empty()) {
    this->minus_err = "ill-formed expression missing operand (ERR 16): |" + infix + "|";
    return "";
  }
  return operands.top();
}

/**
 * Replaces all unary minuses in a given string with hash symbols (#). A unary minus is 
 * a minus operator that is used to show a negative number.
 *
 * @param s the string
 * @return the new string
 */
std::string Expression::replace_unary_minus(std::string s) {
  std::string result = "";
  // position of next token
  bool last = true;
  int next_pos = 0;
  while(next_pos < static_cast<int>(s.length())) {
    std::string token = this->get_next_token(s, next_pos);
    next_pos += token.length();
    if(last && token=="-") {
      token = "#";
    } else {
      last = is_operator(token) || token == "(" || token == ",";
    }
    result += token;
  }
  return result;
}

/**
 * Prepares an infix expression for conversion by replacing and expanding unary minuses, ensuring the expression is valid. 
 * Then, converts the infix expression to postfix via the infix_to_postfix method.
 *
 * @param infix the infix expression
 * @return the prefix expresssion
 */
std::string Expression::convert_infix_to_prefix(std::string infix) {
  if(infix == "") {
    return infix;
  }
  this->minus_err = "";
  std::string changed = this->replace_unary_minus(infix);
  if(this->minus_err != "") {
    Expression::expression_logger->error("Error: {:s}", this->minus_err.c_str());
    return "???";
  } else {
    std::string expanded = this->expand_minus(changed);
    if(this->minus_err != "") {
      Expression::expression_logger->error("Error: {:s}", this->minus_err.c_str());
      return "???";
    } else {
      std::string prefix = this->infix_to_prefix(expanded);
      if(this->minus_err != "") {
        Expression::expression_logger->error("Error: {:s}", this->minus_err.c_str());
        return "???";
      } else {
        return prefix;
      }
    }
  }
}

/**
 * Finds the index of the first comma in a given string that is not inside parentheses.
 *
 * @param s the string
 * @return the index of the first comma
 */
int Expression::find_comma(std::string s) {
  char x[FRED_STRING_SIZE];
  strcpy(x, s.c_str());
  int inside = 0;
  int n = 0;
  while(x[n] != '\0') {
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
 * Gets the value given two persons.
 *
 * @param person the first person
 * @param other the other person
 * @return the value
 */
double Expression::get_value(Person* person, Person* other) {

  Expression::expression_logger->info(
      "Expr::get_value entered person {:d} other {:d} number_expr {:d} name {:s} factor {:s}",
      person ? person->get_id() : -1, other ? other->get_id() : -1, this->number_of_expressions, 
      this->name.c_str(), this->factor ? this->factor->get_name().c_str() : "NULL");

  if(this->is_value) {
    int agent_id = this->expr1->get_value(person, other);
    Person* agent = Person::get_person_with_id(agent_id);
    double value = 0.0;
    if(agent != nullptr) {
      value = this->expr2->get_value(agent, nullptr);
    }
    return value;
  }

  if(this->is_distance) {
    double lat1 = this->expr1->get_value(person, other);
    double lon1 = this->expr2->get_value(person, other);
    double lat2 = this->expr3->get_value(person, other);
    double lon2 = this->expr4->get_value(person, other);
    double value = Geo::xy_distance(lat1, lon1, lat2, lon2);
    return value;
  }

  if(this->is_select) {
    // this is a select expression

    double_vector_t id_vec = this->expr1->get_list_value(person, other);
    int size = id_vec.size();

    if(this->preference == nullptr ) {
      //this is a select-by-index expression
      int index = this->expr2->get_value(person, other);
      if(index < size) {
        return id_vec[index];
      } else {
        return -99999999;
      }
    } else {
      // this is a select by preference expression
      person_vector_t people;
      people.clear();
      for(int i = 0; i < size; ++i) {
        people.push_back(Person::get_person_with_id(id_vec[i]));
      }
      Person* selected = this->preference->select_person(person, people);
      if(selected != nullptr) {
        return selected->get_id();
      } else {
        return -99999999;
      }
    }
  }

  if(this->number_of_expressions == 0) {
    if(this->factor != nullptr) {
      if(this->use_other) {
        // assert(other != nullptr);
        if(other == nullptr) {
          return 0.0;
        }
      }
      return this->factor->get_value(this->use_other ? other : person);
    } else {
      return this->number;
    }
  }

  Place* place1 = nullptr;
  Place* place2 = nullptr;
  double value1 = 0.0;
  double value2 = 0.0;
  double result = 0.0;
  double sigma, mu;

  value1 = this->expr1->get_value(person, other);
  if(this->number_of_expressions == 2) {
    value2 = this->expr2->get_value(person, other);
  }

  switch(this->op_index) {
    case 0:
      result = value1;
      break;
    case 1:
      result = value1 + value2;
      break;
    case 2:
      result = value1 - value2;
      break;
    case 3:
      result = value1 * value2;
      break;
    case 4:
      if(value2 == 0.0) {
        result = 0.0;
      }
      else {
        result = value1 / value2;
      }
      break;
    case 5:
      result = (static_cast<int>(value2) <= 0) ? 0 : (static_cast<int>(value1) % static_cast<int>(value2));
      break;

    case 6:
      result = (value1 == value2);
      break;
    case 7:
      // value1 and value2 should evaluate to place ids
      place1 = Place::get_place_from_sp_id((long long int)value1);
      place2 = Place::get_place_from_sp_id((long long int)value2);
      if(place1 && place2) {
        result = Place::distance_between_places(place1, place2);
      } else {
        result = 9999999.0;
      }
      break;
    case 8:
      result = std::min(value1,value2);
      break;
    case 9:
      result = std::max(value1,value2);
      break;
    case 10:
      result = Random::draw_random(value1, value2);
      break;
    case 11:
      result = Random::draw_normal(value1, value2);
      break;
    case 12:
      sigma = log(value2);
      if(sigma == 0.0) {
        result = value1;
      } else {
        mu = log(value1);
        result = Random::draw_lognormal(mu,sigma);
      }
      break;
    case 13:
      result = Random::draw_binomial(static_cast<int>(value1), value2);
      break;
    case 14:
      result = Random::draw_negative_binomial(static_cast<int>(value1), value2);
      break;
    case 15:
      result = Random::draw_poisson(value1);
      break;
    case 16:
      result = Random::draw_exponential(value1);
      break;
    case 17:
      if(value1 <= 0.0) {
        result = 0;
      } else {
        result = Random::draw_geometric(1.0 / value1);
      }
      break;
    case 18:
      result = pow(value1, value2);
      break;
    case 19:
      if(value1 <= 0) {
        result = -1.0e100;
      }
      else {
        result = log(value1);
      }
      break;
    case 20:
      result = exp(value1);
      break;
    case 21:
      result = fabs(value1);
      break;
    case 22:
      result = sin(value1);
      break;
    case 23:
      result = cos(value1);
      break;
    default:
      Expression::expression_logger->error("unknown function code");
  }
  return result;
}

/**
 * Parses the expression.
 *
 * @return if the expression was parsed successfully
 */
bool Expression::parse() {

  if(this->minus_err != "") {
    Expression::expression_logger->error("HELP: EXPRESSION |{:s}| PROBLEM WITH UNARY MINUS: {:s}", 
        this->name.c_str(), this->minus_err.c_str());
    return false;
  }

  // process real numbers
  if(Utils::is_number(this->name)) {
    this->number = strtod(this->name.c_str(), nullptr);
    this->number_of_expressions = 0;
    return true;
  }

  // process symbolic values
  if(Expression::value_map.find(this->name) != Expression::value_map.end()) {
    this->number = Expression::value_map[this->name];
    this->number_of_expressions = 0;
    return true;
  }

  // process select expression
  if(this->name.find("select(") == 0) {
    Expression::expression_logger->info("PARSE select expression |{:s}|", this->name.c_str());
    this->expr1 = nullptr;
    this->expr2 = nullptr;
    this->pref_str = "1";
    int pos1 = static_cast<int>(this->name.find(","));
    std::string list_expr = "";
    if(pos1 == static_cast<int>(std::string::npos)) {
      Expression::expression_logger->error("HELP: BAD 1st ARG for SELECT |{:s}|", this->name.c_str());
      Utils::print_error("Select function needs 2 arguments:\n  " + this->name);
      return false;
    }
    list_expr = this->name.substr(7, pos1 - 7);
    this->expr1 = new Expression(list_expr);
    if(this->expr1->parse() == false || this->expr1->is_list_expression() == false) {
      Expression::expression_logger->error("HELP: BAD 1st ARG for SELECT |{:s}|", this->name.c_str());
      Utils::print_error("List expression " + list_expr + " not recognized:\n  " + this->name);
      return false;
    }
    if(this->name.substr(pos1 + 1, 5) == "pref(") {
      this->pref_str = this->name.substr(pos1 + 6, this->name.length() - pos1 - 8);
      this->preference = new Preference();
      this->preference->add_preference_expressions(this->pref_str);
    } else {
      std::string index_expr = this->name.substr(pos1 + 1, this->name.length() - pos1 - 2);
      Expression::expression_logger->info("index_expr |{:s}|", index_expr.c_str());
      this->expr2 = new Expression(index_expr);
      if(this->expr2->parse() == false || this->expr2->is_list_expression() == true) {
        Expression::expression_logger->error("HELP: BAD 2nd ARG for SELECT |{:s}|", this->name.c_str());
        Utils::print_error("List index expression " +  index_expr + " not recognized:\n  " + this->name);
        return false;
      }
    }
    this->is_select = true;
    return true;
  }

  if(this->name.find("value(") == 0) {
    Expression::expression_logger->info("PARSE value expression |{:s}|", this->name.c_str());
    this->expr1 = nullptr;
    this->expr2 = nullptr;
    std::string inner = this->name.substr(6, this->name.length() - 6);
    string_vector_t exp_strings = Utils::get_top_level_parse(inner,',');
    if(exp_strings.size() != 2) {
      Expression::expression_logger->error("HELP: Need two arguments for VALUE |{:s}|", 
          this->name.c_str());
      Utils::print_error("Value function needs 2 arguments:\n  " + this->name);
      return false;
    }
    // first expression is agent id
    std::string index_expr = exp_strings[0];
    if(Group_Type::get_group_type(index_expr)!=nullptr) {
      index_expr = "admin_of_" + index_expr;
    }
    Expression::expression_logger->info("index_expr = |{:s}| |{:s}|", index_expr.c_str(), this->name.c_str());
    this->expr1 = new Expression(index_expr);
    if(this->expr1->parse()==false || this->expr1->is_list_expression()) {
      Expression::expression_logger->error("HELP: BAD 1st ARG for VALUE |{:s}|", this->name.c_str());
      Utils::print_error("Index expression " + index_expr + " not recognized:\n  " + this->name);
      return false;
    }
    std::string value_expr = exp_strings[1].substr(0, exp_strings[1].length() - 1);
    Expression::expression_logger->info("value_expr |{:s}|", value_expr.c_str());
    this->expr2 = new Expression(value_expr);
    if(this->expr2->parse() == false || this->expr2->is_list_expression() == true) {
      Expression::expression_logger->error("HELP: BAD 2nd ARG for VALUE |{:s}|", this->name.c_str());
      Utils::print_error("Value expression " +  value_expr + " not recognized:\n  " + this->name);
      return false;
    }
    this->is_value = true;
    return true;
  }

  if(this->name.find("distance(") == 0) {
    Expression::expression_logger->info("PARSE distance expression |{:s}|", this->name.c_str());
    this->expr1 = nullptr;
    this->expr2 = nullptr;
    this->expr3 = nullptr;
    this->expr4 = nullptr;
    std::string inner = this->name.substr(9, this->name.length() - 10);
    string_vector_t exp_strings = Utils::get_top_level_parse(inner, ',');
    if(exp_strings.size() == 4) {
      this->expr1 = new Expression(exp_strings[0]);
      if(this->expr1->parse()==false || this->expr1->is_list_expression()) {
        Expression::expression_logger->error("HELP: BAD 1st ARG for DISTANCE |{:s}|", this->name.c_str());
        Utils::print_error("Distance expression " + exp_strings[0] + " not recognized:\n  " + this->name);
        return false;
      }
      this->expr2 = new Expression(exp_strings[1]);
      if(this->expr2->parse() == false || this->expr2->is_list_expression()) {
        Expression::expression_logger->error("HELP: BAD 2nd ARG for DISTANCE |{:s}|", this->name.c_str());
        Utils::print_error("Distance expression " + exp_strings[1] + " not recognized:\n  " + this->name);
        return false;
      }
      this->expr3 = new Expression(exp_strings[2]);
      if(this->expr3->parse() == false || this->expr3->is_list_expression()) {
        Expression::expression_logger->error("HELP: BAD 3rd ARG for DISTANCE |{:s}|", this->name.c_str());
        Utils::print_error("Distance expression " + exp_strings[2] + " not recognized:\n  " + this->name);
        return false;
      }
      this->expr4 = new Expression(exp_strings[3]);
      if(this->expr4->parse() == false || this->expr4->is_list_expression()) {
        Expression::expression_logger->error("HELP: BAD 4th ARG for DISTANCE |{:s}|", this->name.c_str());
        Utils::print_error("Distance expression " + exp_strings[3] + " not recognized:\n  " + this->name);
        return false;
      }
      this->is_distance = true;
      return true;
    } else {
      Expression::expression_logger->error("HELP: PROBLEM DISTANCE FUNCTION NEED 4 ARGS |{:s}|", this->name.c_str());
      return false;
    }
  }

  int pos1 = static_cast<int>(this->name.find("("));
  if(pos1 == static_cast<int>(std::string::npos)) {
    if(this->name.find("other:") == 0) {
      this->use_other = true;
      this->name = this->name.substr(strlen("other:"));
    }

    // is this a list_variable?
    this->list_var_id = Person::get_list_var_id(this->name);
    if(0 <= this->list_var_id) {
      this->number_of_expressions = 0;
      this->is_list_var = true;
      this->is_list_expr = true;
      return true;
    } else {
      this->list_var_id = Person::get_global_list_var_id(this->name);
      if(0 <= this->list_var_id) {
        this->number_of_expressions = 0;
        this->is_list_var = true;
        this->is_list_expr = true;
        this->is_global = true;
        return true;
      }
    }

    // create new Factor and parse the name
    this->factor = new Factor(this->name);
    if(factor->parse()) {
      this->number_of_expressions = 0;
      return true;
    } else {
      this->warning = factor->is_warning();
      delete this->factor;
      Expression::expression_logger->error("HELP: EXPRESSION UNRECOGNIZED FACTOR = |{:s}|", 
          this->name.c_str());
      return false;
    }
  } else {
    this->op = this->name.substr(0, pos1);
    if(Expression::op_map.find(this->op) != Expression::op_map.end()) {
      // record the index of the operator
      this->op_index = Expression::op_map[this->op];
      // process args
      int pos2 = static_cast<int>(this->name.find_last_of(")"));
      if(pos2 == static_cast<int>(std::string::npos) || pos2 < pos1) {
        Expression::expression_logger->error("HELP: UNRECOGNIZED EXPRESSION = |{:s}|", this->name.c_str());
        return false;
      }
      // discard outer parentheses

      std::string inner = this->name.substr(pos1 + 1, pos2 - pos1 - 1);
      if(this->op == "pool") {
        string_vector_t groups = Utils::get_string_vector(inner, ',');
        for(int j = 0; j < static_cast<int>(groups.size()); ++j) {
          int group_type_id = Group_Type::get_type_id(groups[j]);
          if(group_type_id == -1) {
            Expression::expression_logger->error("HELP: BAD group type |{:s}| in {:s}", 
                groups[j].c_str(), this->name.c_str());
            return false;
          }
          this->pool.push_back(group_type_id);
        }
        this->is_pool = true;
        this->is_list_expr = true;
        return true;
      }
      int pos_comma = find_comma(inner);

      // LIST
      if(pos_comma < 0 && this->op == "list") {
        Expression::expression_logger->info("parsing list expression |{:s}|", this->name.c_str());
        this->expr1 = new Expression(inner);
        if(this->expr1->parse() == false) {
          Expression::expression_logger->error("HELP: BAD 1st ARG for OP %s = |{:s}|", 
              this->op.c_str(), this->name.c_str());
          return false;
        }
        this->is_list = true;
        this->is_list_expr = true;
        return true;
      }

      if(-1 < pos_comma) {
        // get args
        std::string first = inner.substr(0, pos_comma);
        this->expr1 = new Expression(first);
        if(this->expr1->parse() == false) {
          Expression::expression_logger->error("HELP: BAD 1st ARG for OP %s = |{:s}|", 
              this->op.c_str(), this->name.c_str());
          return false;
        }

        // LIST
        if(this->op == "list") {
          Expression::expression_logger->info("parsing list expression |{:s}|", this->name.c_str());
          if(inner.substr(pos_comma + 1) != "") {
            std::string remainder = "list(" + inner.substr(pos_comma+1) + ")";
            this->expr2 = new Expression(remainder);
            if(this->expr2->parse()==false) {
              Expression::expression_logger->error("HELP: BAD remainder ARG for OP %s = |{:s}|", 
                  this->op.c_str(), this->name.c_str());
              return false;
            }
          }
          this->is_list = true;
          this->is_list_expr = true;
          return true;
        }

        // FILTER
        if(this->op == "filter") {
          if(this->expr1->is_list_expression()==false) {
            Expression::expression_logger->error("First arg is not a list expression: {:s}", 
                this->name.c_str());
            return false;
          }
          this->clause = new Clause(inner.substr(pos_comma + 1));
          if(this->clause->parse() == false) {
            Expression::expression_logger->error("BAD CLAUSE in Expression {:s}", this->name.c_str());
            return false;
          }
          this->is_filter = true;
          this->is_list_expr = true;
          return true;
        }

        std::string second = inner.substr(pos_comma + 1);
        this->expr2 = new Expression(second);
        if(this->expr2->parse() == false) {
          Expression::expression_logger->error("HELP: BAD 2nd ARG for OP {:s} = |{:s}|", 
              this->op.c_str(), this->name.c_str());
          return false;
        }
        this->number_of_expressions = 2;
        return true;
      } else if(this->op_index > TWOARGS) {
        // get single args
        this->expr1 = new Expression(inner);
        if(this->expr1->parse()==false) {
          Expression::expression_logger->error("HELP: BAD ARG for OP {:s} = |{:s}|", 
              this->op.c_str(), this->name.c_str());
          return false;
        }
        this->number_of_expressions = 1;
        return true;
      } else {
        Expression::expression_logger->error("HELP: MISSING ARG for OP {:s} = |{:s}|", 
            this->op.c_str(), this->name.c_str());
        return false;
      }
    } else {
      Expression::expression_logger->error("HELP: EXPRESSION UNRECOGNIZED OPERATOR = |{:s}| in |{:s}|", 
          this->op.c_str(), this->name.c_str());
      return false;
    }
  }
  Expression::expression_logger->error("HELP: PROBLEM WITH EXPRESSION IS_RECOGNIZED = |{:s}|", 
      this->name.c_str());
  return false;
}

/**
 * Gets the list value given two specified person objects.
 *
 * @param person the first person
 * @param person the other person
 * @return the list value
 */
double_vector_t Expression::get_list_value(Person* person, Person* other) {
  double_vector_t results;
  results.clear();

  Expression::expression_logger->info(
      "get_list_value person {:d} other {:d} list_var {:d} is_pool {:d} is_filter {:d} use_other {:d}",
      (person ? person->get_id() : -999), (other ? other->get_id() : -999), this->is_list_var, 
      this->is_pool, this->is_filter, this->use_other);

  if(this->is_list) {
    double_vector_t list1;
    double_vector_t list2;
    list1.clear();
    if(this->expr1->is_list_expression()) {
      list1 = this->expr1->get_list_value(person, other);
    } else {
      list1.push_back(this->expr1->get_value(person, other));
    }
    // int size1 = list1.size();

    list2.clear();
    if(this->expr2 != nullptr) {
      if(this->expr2->is_list_expression()) {
        list2 = this->expr2->get_list_value(person, other);
      } else {
        list2.push_back(this->expr2->get_value(person, other));
      }
    }
    int size2 = list2.size();

    for(int i = 0; i < size2; ++i) {
      list1.push_back(list2[i]);
    }
    // int size = list1.size();
    return list1;
  }

  if(this->is_list_var) {
    if(this->is_global) {
      if(this->use_other) {
        return other->get_global_list_var(this->list_var_id);
      } else {
        return person->get_global_list_var(this->list_var_id);
      }
    } else {
      if(this->use_other) {
        return other->get_list_var(this->list_var_id);
      } else {
        return person->get_list_var(this->list_var_id);
      }
    }
  }

  if(this->is_pool) {
    if(this->use_other) {
      return get_pool(other);
    } else {
      return get_pool(person);
    }
  }

  if(this->is_filter) {
    double_vector_t initial_list = this->expr1->get_list_value(person,other);
    return get_filtered_list(person, initial_list);
  }

  return results;
}

/**
 * Gets a pool of people who belong to an activity group that a specified person is also a part of, without duplicates.
 *
 * @param person the person
 * @return the pool
 */
double_vector_t Expression::get_pool(Person* person) {

  std::unordered_set<int> found = {};
  double_vector_t people;
  people.clear();
  for(int i = 0; i < static_cast<int>(this->pool.size()); ++i) {
    int group_type_id = this->pool[i];
    Group* group = person->get_activity_group(group_type_id);
    if(group!=nullptr) {
      int size = group->get_size();
      for(int j = 0; j < size; j++) {
        int other_id = group->get_member(j)->get_id();
        if(found.insert(other_id).second) {
          people.push_back(other_id);
        }
      }
    }
  }

  return people;
}

double_vector_t Expression::get_filtered_list(Person* person, double_vector_t &list) {

  // create a filtered list of qualified people
  std::unordered_set<int> found = {};
  double_vector_t filtered;
  filtered.clear();

  // filter out anyone who fails any requirement
  for(int j = 0; j < static_cast<int>(list.size()); ++j) {
    int other_id = list[j];
    Person* other = Person::get_person_with_id(other_id);
    if(this->clause->get_value(person, other)) {
      if(found.insert(other_id).second) {
        filtered.push_back(other_id);
      }
    }
  }
  return filtered;
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Expression::setup_logging() {
  if(Expression::is_log_initialized) {
    return;
  }
  
  if(Parser::does_property_exist("expression_log_level")) {
    Parser::get_property("expression_log_level", &Expression::expression_log_level);
  } else {
    Expression::expression_log_level = "OFF";
  }

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Expression::expression_logger = std::make_unique<spdlog::logger>("expression_logger", 
        sink_list.begin(), sink_list.end());
    Expression::expression_logger->set_level(
        Utils::get_log_level_from_string(Expression::expression_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Expression::expression_logger->trace("<{:s}, {:d}>: Expression logger initialized", 
      __FILE__, __LINE__  );
  Expression::is_log_initialized = true;
}
