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
// File: Block_Group.h
//


#ifndef _FRED_BLOCK_GROUP_H
#define _FRED_BLOCK_GROUP_H

#include <vector>
#include <unordered_map>

#include <spdlog/spdlog.h>

#include "Admin_Division.h"

/**
 * This class represents a block group division, which is a subdivision of a Census_Tract.
 *
 * Block groups have ADI national and state ranks, which stands for Area Deprivation Index.
 *
 * This class inherits from Admin_Division.
 */
class Block_Group : public Admin_Division {
public:

  Block_Group(long long int _admin_code);

  ~Block_Group();

  /**
   * Gets the ADI national rank.
   *
   * @return the ADI national rank
   */
  int get_adi_national_rank() {
    return this->adi_national_rank;
  }

  /**
   * Gets the ADI state rank.
   *
   * @return the ADI state rank
   */
  int get_adi_state_rank() {
    return this->adi_state_rank;
  }

  /**
   * Gets the number of Block_Group objects in the static block groups vector.
   *
   * @return the number of block groups
   */
  static int get_number_of_block_groups() {
    return Block_Group::block_groups.size();
  }

  /**
   * Gets the Block_Group at the specified index in the block groups vector.
   *
   * @param i the index
   * @return the block group
   */
  static Block_Group* get_block_group_with_index(int i) {
    return Block_Group::block_groups[i];
  }

  static Block_Group* get_block_group_with_admin_code(long long int block_group_code);

  static void read_adi_file();
  static void setup_logging();

private:

  int adi_national_rank;
  int adi_state_rank;

  // static variables
  static std::vector<Block_Group*> block_groups;
  static std::unordered_map<long long int,Block_Group*> lookup_map;
  static std::unordered_map<long long int,int> adi_national_rank_map;
  static std::unordered_map<long long int,int> adi_state_rank_map;
  static int Enable_ADI;

  static bool is_log_initialized;
  static std::string block_group_log_level;
  static std::unique_ptr<spdlog::logger> block_group_logger;
};

#endif // _FRED_BLOCK_GROUP_H
