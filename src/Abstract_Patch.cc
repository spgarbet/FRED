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

#include <spdlog/fmt/fmt.h>

#include "Abstract_Patch.h"

/**
 * Converts the patch to a string representation.
 */
std::string Abstract_Patch::to_string() {
  std::stringstream ss;
  ss << fmt::format("patch {:d} {:d}: {:f}, {:f}, {:f}, {:f}", this->row, this->col, 
      this->min_x, this->min_y, this->max_x, this->max_y);

  return ss.str();
}