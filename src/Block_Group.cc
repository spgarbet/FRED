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
// File: Block_Group.cc
//

#include <spdlog/spdlog.h>

#include "Block_Group.h"
#include "Census_Tract.h"
#include "Parser.h"
#include "Utils.h"

/**
 * Default destructor.
 */
Block_Group::~Block_Group() {
}

/**
 * Creates a Block_Group with the specified admin code. Sets a Census_Tract as the higher division 
 * to this block group, and adds this block group as a subdivison of the census tract.
 *
 * @param admin_code the admin code
 */
Block_Group::Block_Group(long long int admin_code) : Admin_Division(admin_code) {

  // get the census tract associated with this code, creating a new one if necessary
  long long int census_tract_admin_code = get_admin_division_code() / 10;
  Census_Tract* census_tract = Census_Tract::get_census_tract_with_admin_code(census_tract_admin_code);
  this->higher = census_tract;
  census_tract->add_subdivision(this);

  this->adi_national_rank = 0;
  this->adi_state_rank = 0;
  if(Block_Group::Enable_ADI) {
    this->adi_national_rank = adi_national_rank_map[admin_code];
    this->adi_state_rank = adi_state_rank_map[admin_code];
  }

}

//////////////////////////////
//
// STATIC METHODS
//
//////////////////////////////


// static variables
std::vector<Block_Group*> Block_Group::block_groups;
std::unordered_map<long long int,Block_Group*> Block_Group::lookup_map;
std::unordered_map<long long int,int> Block_Group::adi_national_rank_map;
std::unordered_map<long long int,int> Block_Group::adi_state_rank_map;
int Block_Group::Enable_ADI = 0;

bool Block_Group::is_log_initialized = false;
std::string Block_Group::block_group_log_level = "";
std::unique_ptr<spdlog::logger> Block_Group::block_group_logger = nullptr;

/**
 * Gets the Block_Group with the specified admin code. If no such block group exists, one is created
 * with the admin code.
 *
 * @param block_group_admin_code the block group admin code
 * @return the block group
 */
Block_Group* Block_Group::get_block_group_with_admin_code(long long int block_group_admin_code) {
  Block_Group* block_group = nullptr;
  std::unordered_map<long long int,Block_Group*>::iterator itr;
  itr = Block_Group::lookup_map.find(block_group_admin_code);
  if(itr == Block_Group::lookup_map.end()) {
    // this is a new block group
    block_group = new Block_Group(block_group_admin_code);
    Block_Group::block_groups.push_back(block_group);
    Block_Group::lookup_map[block_group_admin_code] = block_group;
  } else {
    block_group = itr->second;
  }
  return block_group;
}

/**
 * Reads the ADI file
 */
void Block_Group::read_adi_file() {
  char adi_file [FRED_STRING_SIZE];

  Block_Group::block_group_logger->info("read_adi_file entered");
  Parser::get_property("enable_adi_rank", &Block_Group::Enable_ADI);
  Parser::get_property("adi_file", adi_file);

  if(Block_Group::Enable_ADI) {
    char line [FRED_STRING_SIZE];
    long long int gis;
    int st;
    long long int fips;
    int state_rank;
    int national_rank;

    Block_Group::block_group_logger->info("read_adi_file {:s}", adi_file);
    FILE* fp = Utils::fred_open_file(adi_file);

    // skip header line
    fgets(line, FRED_STRING_SIZE, fp);

    // read first data line
    strcpy(line, "");
    fgets(line, FRED_STRING_SIZE, fp);

    int items = sscanf(line, "G%lld,%d,%lld,%d,%d", &gis, &st, &fips, &state_rank, &national_rank);
    while(items == 5) {
      Block_Group::adi_national_rank_map[fips] = national_rank;
      Block_Group::adi_state_rank_map[fips] = state_rank;
      // get next line
      strcpy(line, "");
      fgets(line, FRED_STRING_SIZE, fp);
      items = sscanf(line, "G%lld,%d,%lld,%d,%d", &gis, &st, &fips, &state_rank, &national_rank);
    }
    fclose(fp);
  }
  Block_Group::block_group_logger->info("read_adi_file finished");
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Block_Group::setup_logging() {
  if(Block_Group::is_log_initialized) {
    return;
  }

  if(Parser::does_property_exist("block_group_log_level")) {
    Parser::get_property("block_group_log_level",  &Block_Group::block_group_log_level);
  } else {
    Block_Group::block_group_log_level = "OFF";
  }

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Block_Group::block_group_logger = std::make_unique<spdlog::logger>("block_group_logger", 
        sink_list.begin(), sink_list.end());
    Block_Group::block_group_logger->set_level(
        Utils::get_log_level_from_string(Block_Group::block_group_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Block_Group::block_group_logger->trace("<{:s}, {:d}>: Block_Group logger initialized", 
      __FILE__, __LINE__  );
  Block_Group::is_log_initialized = true;
}
