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
// File: Fred_API.cc 
// DEPRECATED, SEE NEW FRED_API
//

#include <stdio.h>
#include <assert.h>
#include <vector>
#include <string>

void update();


// keys
std::vector<std::string> keys;

// values
std::vector<std::string> values;


double get_value(std::string key) {
  int size = keys.size();
  for (int i = 0; i < size; i++) {
    if (keys[i] == key) {
      double val;
      sscanf(values[i].c_str(), "%lf", &val);
      return val;
    }
  }
  return 0.0;
}


void set_value(std::string key, double val) {
  int size = keys.size();
  for(int i = 0; i < size; ++i) {
    if (keys[i] == key) {
      char vstr[64];
      snprintf(vstr, 64, "%f", val);
      values[i] = string(vstr);
      return;
    }
  }
  return;
}


int main(int argc, char* argv[]) {

  // read working directory from command line
  char dir[FRED_STRING_SIZE];
  if(argc > 1) {
    strcpy(dir, argv[1]);
  } else {
    strcpy(dir, "");
  }
  // printf("dir = |%s|\n", dir);

  // requests filename
  char requests[FRED_STRING_SIZE];
  snprintf(requests, FRED_STRING_SIZE, "%s/requests", dir);

  // open request file
  FILE* reqfp = fopen(requests, "r");
  if(reqfp == nullptr) {
    //abort
  }

  char next_filename[FRED_STRING_SIZE];
  char requestfile[FRED_STRING_SIZE];
  
  while(fscanf(reqfp, "%s", next_filename)==1) {

    // request filename
    snprintf(requestfile, FRED_STRING_SIZE, "%s/%s", dir, next_filename);

    // open request file
    FILE* fp = fopen(requestfile, "r");
    if(fp == nullptr) {
      // abort
    }

    // clear keys and values
    keys.clear();
    values.clear();

    char key[FRED_STRING_SIZE];
    char val[FRED_STRING_SIZE];
    while(fscanf(fp, "%s = %s ", key, val) == 2) {
      keys.push_back(string(key));
      values.push_back(string(val));
      // printf("%s == %s\n", key,val);
    }
    fclose(fp);
    if(keys.size() == 0) {
      continue;
    }

    update();

    // set results filename
    char results_file[FRED_STRING_SIZE];
    int id = get_value("person");
    snprintf(results_file, FRED_STRING_SIZE, "%s/results.%d", dir, id);
    
    // open results file
    fp = fopen(results_file, "w");
    int size = keys.size();
    for(int i = 0; i < size; i++) {
      fprintf(fp, "%s = %s\n", keys[i].c_str(), values[i].c_str());
    }
    fclose(fp);
  }

  // send ready signal
  char ready_file[FRED_STRING_SIZE];
  snprintf(ready_file, FRED_STRING_SIZE, "%s/results_ready", dir);
  FILE* fp = fopen(ready_file, "w");
  fclose(fp);

  return 0;
}


////////////////////////////////////////////////////////
//
// DO NOT MODIFY ANY CODE ABOVE THIS POINT
//
// MODIFY THE FOLLOWING AS NEEDED TO UPDATE VALUES OF
// ANY PERSONAL VARIABLES
//
////////////////////////////////////////////////////////


void update() {

  // select FRED variables using format CONDITION.variable
  string var = "STAY_HOME.x";

  // get current values
  double new_x = get_value(var);

  // update as needed
  new_x += 10.0;

  // set the variables to the updated values
  set_value(var, new_x);

}
