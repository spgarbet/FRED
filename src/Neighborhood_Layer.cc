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
// File: Neighborhood_Layer.cc
//
#include <algorithm>
#include <list>
#include <string>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "Geo.h"
#include "Global.h"
#include "Household.h"
#include "Neighborhood_Layer.h"
#include "Neighborhood_Patch.h"
#include "Parser.h"
#include "Place.h"
#include "Place_Type.h"
#include "Random.h"
#include "Regional_Layer.h"
#include "Utils.h"

bool Neighborhood_Layer::is_log_initialized = false;
std::string Neighborhood_Layer::neighborhood_layer_log_level = "";
std::unique_ptr<spdlog::logger> Neighborhood_Layer::neighborhood_layer_logger = nullptr;

/**
 * Creates a Neighborhood_Layer with default variables. Sets up the grid 
 * to cover the global simulation region.
 */
Neighborhood_Layer::Neighborhood_Layer() {
  char property[FRED_STRING_SIZE];

  Regional_Layer* base_grid = Global::Simulation_Region;
  this->min_lat = base_grid->get_min_lat();
  this->min_lon = base_grid->get_min_lon();
  this->max_lat = base_grid->get_max_lat();
  this->max_lon = base_grid->get_max_lon();
  this->min_x = base_grid->get_min_x();
  this->min_y = base_grid->get_min_y();
  this->max_x = base_grid->get_max_x();
  this->max_y = base_grid->get_max_y();

  this->offset = nullptr;
  this->gravity_cdf = nullptr;
  this->max_offset = 0;

  // determine patch size for this layer
  strcpy(property, "Neighborhood.patch_size");
  if(Parser::does_property_exist(property)) {
    Parser::get_property("Neighborhood.patch_size", &this->patch_size);
  } else {
    Parser::get_property("Neighborhood_patch_size", &this->patch_size);
  }

  // determine number of rows and cols
  this->rows = static_cast<double>(this->max_y - this->min_y) / this->patch_size;
  if(this->min_y + this->rows * this->patch_size < this->max_y) {
    ++this->rows;
  }

  this->cols = static_cast<double>(this->max_x - this->min_x) / this->patch_size;
  if(this->min_x + this->cols * this->patch_size < this->max_x) {
    ++this->cols;
  }
    
  if(Global::Compile_FRED && this->rows < 0) {
    this->rows = 1;
  }
      
  if(Global::Compile_FRED && this->cols < 0) {
    this->cols = 1;
  }

  Neighborhood_Layer::neighborhood_layer_logger->debug("Neighborhood_Layer min_lon = {:f}", this->min_lon);
  Neighborhood_Layer::neighborhood_layer_logger->debug("Neighborhood_Layer min_lat = {:f}", this->min_lat);
  Neighborhood_Layer::neighborhood_layer_logger->debug("Neighborhood_Layer max_lon = {:f}", this->max_lon);
  Neighborhood_Layer::neighborhood_layer_logger->debug("Neighborhood_Layer max_lat = {:f}", this->max_lat);
  Neighborhood_Layer::neighborhood_layer_logger->debug("Neighborhood_Layer rows = {:d}  cols = {:d}", 
      this->rows, this->cols);
  Neighborhood_Layer::neighborhood_layer_logger->debug("Neighborhood_Layer min_x = {:f}  min_y = {:f}", 
      this->min_x, this->min_y);
  Neighborhood_Layer::neighborhood_layer_logger->debug("Neighborhood_Layer max_x = {:f}  max_y = {:f}", 
      this->max_x, this->max_y);

  // setup patches
  this->grid = new Neighborhood_Patch*[this->rows];
  for(int i = 0; i < this->rows; ++i) {
    this->grid[i] = new Neighborhood_Patch[this->cols];
    for(int j = 0; j < this->cols; ++j) {
      this->grid[i][j].setup(this, i, j);
    }
  }

  // properties to determine neighborhood visitation patterns
  strcpy(property, "Neighborhood.max_distance");
  if(Parser::does_property_exist(property)) {
    Parser::get_property("Neighborhood.max_distance", &this->max_distance);
  } else {
    Parser::get_property("Neighborhood_max_distance", &this->max_distance);
  }
  strcpy(property, "Neighborhood.max_destinations");
  if(Parser::does_property_exist(property)) {
    Parser::get_property("Neighborhood.max_destinations", &this->max_destinations);
  } else {
    Parser::get_property("Neighborhood_max_destinations", &this->max_destinations);
  }
  strcpy(property, "Neighborhood.min_distance");
  if(Parser::does_property_exist(property)) {
    Parser::get_property("Neighborhood.min_distance", &this->min_distance);
  } else {
    Parser::get_property("Neighborhood_min_distance", &this->min_distance);
  }
  strcpy(property, "Neighborhood.distance_exponent");
  if(Parser::does_property_exist(property)) {
    Parser::get_property("Neighborhood.distance_exponent", &this->dist_exponent);
  } else {
    Parser::get_property("Neighborhood_distance_exponent", &this->dist_exponent);
  }
  strcpy(property, "Neighborhood.population_exponent");
  if(Parser::does_property_exist(property)) {
    Parser::get_property("Neighborhood.population_exponent", &this->pop_exponent);
  } else {
    Parser::get_property("Neighborhood_population_exponent", &this->pop_exponent);
  }
}

/**
 * Sets up a Neighborhood_Patch for each cell in the grid.
 */
void Neighborhood_Layer::setup() {

  int type = Place_Type::get_type_id("Neighborhood");

  // create one neighborhood per patch
  for(int i = 0; i < this->rows; ++i) {
    for(int j = 0; j < this->cols; ++j) {
      if(this->grid[i][j].get_houses() > 0) {
        this->grid[i][j].make_neighborhood(type);
      }
      Neighborhood_Layer::neighborhood_layer_logger->trace("<{:s}, {:d}>: grid[{:d}][{:d}]: {:s}", __FILE__, __LINE__, i, j, this->grid[i][j].to_string());
    }
  }
}

/**
 * Prepares this neighborhood layer.
 */
void Neighborhood_Layer::prepare() {
  Neighborhood_Layer::neighborhood_layer_logger->info("Neighborhood_Layer prepare entered");
  this->record_activity_groups();
  Neighborhood_Layer::neighborhood_layer_logger->info("setup gravity model ...");
  this->setup_gravity_model();
  Neighborhood_Layer::neighborhood_layer_logger->info("setup gravity model complete");
  Neighborhood_Layer::neighborhood_layer_logger->info("Neighborhood_Layer prepare finished");
}

/**
 * Gets the Neighborhood_Patch in the grid in which a specified Place 
 * is located.
 *
 * @param place the place
 * @return the neighborhood patch
 */
Neighborhood_Patch* Neighborhood_Layer::get_patch(Place* place) {
  return this->get_patch(place->get_latitude(), place->get_longitude());
}

/**
 * Gets the Neighborhood_Patch in the grid at the given row and column.
 *
 * @param row the row
 * @param col the column
 * @return the neighborhood patch
 */
Neighborhood_Patch* Neighborhood_Layer::get_patch(int row, int col) {
  if(row >= 0 && col >= 0 && row < this->rows && col < this->cols) {
    return &this->grid[row][col];
  } else {
    return nullptr;
  }
}

/**
 * Gets the Neighborhood_Patch in the grid at the given latitude and longitude.
 *
 * @param lat the latitude
 * @param lon the longitude
 * @return the neighborhood patch
 */
Neighborhood_Patch* Neighborhood_Layer::get_patch(fred::geo lat, fred::geo lon) {
  int row = this->get_row(lat);
  int col = this->get_col(lon);
  return this->get_patch(row, col);
}

/**
 * Performs quality control on the grid.
 */
void Neighborhood_Layer::quality_control() {
  Neighborhood_Layer::neighborhood_layer_logger->info("grid quality control check");

  int popsize = 0;
  int tot_occ_patches = 0;
  for(int row = 0; row < this->rows; ++row) {
    int min_occ_col = this->cols + 1;
    int max_occ_col = -1;
    for(int col = 0; col < this->cols; ++col) {
      this->grid[row][col].quality_control();
      int patch_pop = this->grid[row][col].get_popsize();
      if(patch_pop > 0) {
        if(col > max_occ_col) {
          max_occ_col = col;
        }
        if(col < min_occ_col) {
          min_occ_col = col;
        }
        popsize += patch_pop;
      }
    }
    if(min_occ_col < this->cols) {
      int patches_occ = max_occ_col - min_occ_col + 1;
      tot_occ_patches += patches_occ;
    }
  }

  if(Global::Verbose > 1) {
    char filename[FRED_STRING_SIZE];
    snprintf(filename, FRED_STRING_SIZE, "%s/grid.dat", Global::Simulation_directory);
    FILE *fp = fopen(filename, "w");
    for(int row = 0; row < rows; ++row) {
      if(row % 2) {
        for(int col = this->cols - 1; col >= 0; col--) {
          double x = this->grid[row][col].get_center_x();
          double y = this->grid[row][col].get_center_y();
          fprintf(fp, "%f %f\n", x, y);
        }
      } else {
        for(int col = 0; col < this->cols; col++) {
          double x = this->grid[row][col].get_center_x();
          double y = this->grid[row][col].get_center_y();
          fprintf(fp, "%f %f\n", x, y);
        }
      }
    }
    fclose(fp);
  }

  int total_area = this->rows * this->cols;
  int convex_area = tot_occ_patches;
  
  // The following two lines print to the log file and are needed by fred_job
  fprintf(Global::Statusfp, "Density: popsize = %d total region = %d total_density = %f\n", popsize,
      total_area, (total_area > 0) ? static_cast<double>(popsize) / static_cast<double>(total_area) : 0.0);
  fprintf(Global::Statusfp, "Density: popsize = %d convex region = %d convex_density = %f\n", popsize,
      convex_area, (convex_area > 0) ? static_cast<double>(popsize) / static_cast<double>(convex_area) : 0.0);
  
  Neighborhood_Layer::neighborhood_layer_logger->debug("Density: popsize = {:d} total region = {:d} total_density = {:f}", 
      popsize, total_area, (total_area > 0) ? static_cast<double>(popsize) / static_cast<double>(total_area) : 0.0);
  Neighborhood_Layer::neighborhood_layer_logger->debug("Density: popsize = {:d} convex region = {:d} convex_density = {:f}", 
      popsize, convex_area, (convex_area > 0) ? static_cast<double>(popsize) / static_cast<double>(convex_area) : 0.0);
  Neighborhood_Layer::neighborhood_layer_logger->debug("grid quality control finished");

}

/**
 * Performs quality control on the grid.
 *
 * @param min_x _UNUSED_
 * @param min_y _UNUSED_
 */
void Neighborhood_Layer::quality_control(double min_x, double min_y) {
  Neighborhood_Layer::neighborhood_layer_logger->info("grid quality control check");

  for(int row = 0; row < this->rows; ++row) {
    for(int col = 0; col < this->cols; ++col) {
      this->grid[row][col].quality_control();
    }
  }

  if(Global::Verbose > 1) {
    char filename[FRED_STRING_SIZE];
    snprintf(filename, FRED_STRING_SIZE, "%s/grid.dat", Global::Simulation_directory);
    FILE *fp = fopen(filename, "w");
    for(int row = 0; row < rows; ++row) {
      if(row % 2) {
        for(int col = this->cols - 1; col >= 0; --col) {
          double x = this->grid[row][col].get_center_x();
          double y = this->grid[row][col].get_center_y();
          fprintf(fp, "%f %f\n", x, y);
        }
      } else {
        for(int col = 0; col < this->cols; ++col) {
          double x = this->grid[row][col].get_center_x();
          double y = this->grid[row][col].get_center_y();
          fprintf(fp, "%f %f\n", x, y);
        }
      }
    }
    fclose(fp);
  }

  Neighborhood_Layer::neighborhood_layer_logger->info("grid quality control finished");
}

/**
 * Gets the number of neighborhoods in the grid. This will be the number of patches in the grid 
 * that have at least one Household.
 *
 * @return the number of neighborhoods
 */
int Neighborhood_Layer::get_number_of_neighborhoods() {
  int n = 0;
  for(int row = 0; row < this->rows; ++row) {
    for(int col = 0; col < this->cols; ++col) {
      if(this->grid[row][col].get_houses() > 0) {
        ++n;
      }
    }
  }
  return n;
}

/**
 * Records activity groups and prepares each Neighborhood_Patchh in the grid with at least one Household.
 */
void Neighborhood_Layer::record_activity_groups() {
  Neighborhood_Layer::neighborhood_layer_logger->info("record_daily_activities entered");
  for(int row = 0; row < this->rows; ++row) {
    for(int col = 0; col < this->cols; ++col) {
      Neighborhood_Patch* patch = static_cast<Neighborhood_Patch*>(&this->grid[row][col]);
      if(patch != nullptr) {
        if(patch->get_houses() > 0) {
          patch->record_activity_groups();
          patch->prepare();
        }
      }
    }
  }
  Neighborhood_Layer::neighborhood_layer_logger->info("record_daily_activities finished");
}

/**
 * Compares two pairs. This comparison is used to sort by probability.
 *
 * @param p1 the first pair
 * @param p2 the second pair
 * @return the result of the comparison
 */
static bool compare_pair(const std::pair<double, int>&p1, const std::pair<double, int>&p2) {
  return ((p1.first == p2.first) ? (p1.second < p2.second) : (p1.first > p2.first));
}

/**
 * Sets up the gravity model. This will be used when selecting destination patches; neighborhood residents 
 * will gravitate towards more populated patches when venturing out of their own.
 */
void Neighborhood_Layer::setup_gravity_model() {
  int tmp_offset[256 * 256];
  double tmp_prob[256 * 256];
  int count = 0;

  // print_distances();  // DEBUGGING

  this->offset = new offset_t*[this->rows];
  this->gravity_cdf = new gravity_cdf_t*[this->rows];
  for(int i = 0; i < rows; ++i) {
    this->offset[i] = new offset_t[this->cols];
    this->gravity_cdf[i] = new gravity_cdf_t[this->cols];
  }

  if(this->max_distance < 0) {
    this->setup_null_gravity_model();
    return;
  }

  this->max_offset = this->max_distance / this->patch_size;
  assert(this->max_offset < 128);

  for(int i = 0; i < this->rows; ++i) {
    for(int j = 0; j < this->cols; ++j) {
      // set up gravity model for grid[i][j];
      Neighborhood_Patch* patch = static_cast<Neighborhood_Patch*>(&grid[i][j]);
      assert(patch != nullptr);
      double x_src = patch->get_center_x();
      double y_src = patch->get_center_y();
      int pop_src = patch->get_popsize();
      if (pop_src == 0) continue;
      count = 0;
      for(int ii = i - this->max_offset; ii < this->rows && ii <= i + this->max_offset; ++ii) {
        if(ii < 0) {
          continue;
        }

        for(int jj = j - this->max_offset; jj < this->cols && jj <= j + this->max_offset; ++jj) {
          if(jj < 0) {
            continue;
          }

          Neighborhood_Patch* dest_patch = static_cast<Neighborhood_Patch*>(&this->grid[ii][jj]);
          assert(dest_patch != nullptr);
          int pop_dest = dest_patch->get_popsize();
          if(pop_dest == 0) {
            continue;
          }

          double x_dest = dest_patch->get_center_x();
          double y_dest = dest_patch->get_center_y();
          double dist = sqrt((x_src - x_dest) * (x_src - x_dest) + (y_src - y_dest) * (y_src - y_dest));
          if(this->max_distance < dist) {
            continue;
          }

          double gravity = pow(pop_dest, this->pop_exponent) / (1.0 + pow(dist / this->min_distance, this->dist_exponent));
          int off = 256 * (i - ii + this->max_offset) + (j - jj + this->max_offset);

          /*
           * consider income similarity in gravity model
           * double mean_household_income_dest = dest_patch->get_mean_household_income();
           * double income_similarity = mean_household_income_dest / mean_household_income_src;
           * if(income_similarity > 1.0) {
           *   income_similarity = 1.0 / income_similarity;
           * }
           * gravity = ...;
           */
          tmp_offset[count] = off;
          tmp_prob[count++] = gravity;
        }
      }

      // sort by gravity value
      this->sort_pair.clear();
      for(int k = 0; k < count; ++k) {
        this->sort_pair.push_back(std::pair<double, int>(tmp_prob[k], tmp_offset[k]));
      }
      std::sort(this->sort_pair.begin(), this->sort_pair.end(), compare_pair);

      // keep at most largest max_destinations
      if(count > this->max_destinations) {
        count = this->max_destinations;
      }
      for(int k = 0; k < count; ++k) {
        tmp_prob[k] = this->sort_pair[k].first;
        tmp_offset[k] = this->sort_pair[k].second;
      }
      this->sort_pair.clear();

      // transform gravity values into a prob distribution
      double total = 0.0;
      for(int k = 0; k < count; ++k) {
        total += tmp_prob[k];
      }
      for(int k = 0; k < count; ++k) {
        tmp_prob[k] /= total;
      }

      // convert to cdf
      for(int k = 1; k < count; ++k) {
        tmp_prob[k] += tmp_prob[k - 1];
      }

      // store gravity prob and offsets for this patch
      this->gravity_cdf[i][j].reserve(count);
      this->gravity_cdf[i][j].clear();
      this->offset[i][j].reserve(count);
      this->offset[i][j].clear();
      for(int k = 0; k < count; ++k) {
        this->gravity_cdf[i][j].push_back(tmp_prob[k]);
        this->offset[i][j].push_back(tmp_offset[k]);
      }
    }
  }
}

/**
 * Prints the gravity model.
 */
void Neighborhood_Layer::print_gravity_model() {
  Neighborhood_Layer::neighborhood_layer_logger->info("=== GRAVITY MODEL ========================================================");
  for(int i_src = 0; i_src < rows; ++i_src) {
    for(int j_src = 0; j_src < cols; ++j_src) {
      Neighborhood_Patch* src_patch = static_cast<Neighborhood_Patch*>(&grid[i_src][j_src]);
      double x_src = src_patch->get_center_x();
      double y_src = src_patch->get_center_y();
      int pop_src = src_patch->get_popsize();
      if(pop_src == 0) {
        continue;
      }
      int count = static_cast<int>(this->offset[i_src][j_src].size());
      for(int k = 0; k < count; ++k) {
        int off = this->offset[i_src][j_src][k];
        int i_dest = i_src + this->max_offset - (off / 256);
        int j_dest = j_src + this->max_offset - (off % 256);
        Neighborhood_Patch* dest_patch = this->get_patch(i_dest, j_dest);
        assert(dest_patch != nullptr);
        double x_dest = dest_patch->get_center_x();
        double y_dest = dest_patch->get_center_y();
        double dist = sqrt((x_src-x_dest)*(x_src-x_dest) + (y_src - y_dest) * (y_src - y_dest));
        int pop_dest = dest_patch->get_popsize();
        double gravity_prob = this->gravity_cdf[i_src][j_src][k];
        if(k > 0) {
          gravity_prob -= this->gravity_cdf[i_src][j_src][k-1];
        }
        Neighborhood_Layer::neighborhood_layer_logger->info(
            "GRAVITY_MODEL row {:3d} col {:3d} pop {:5d} count {:4d} k {:4d} offset {:d} row {:3d} col {:3d} pop {:5d} dist {:0.4f} prob {:f}", 
            i_src, j_src, pop_src, count, k, off, i_dest, j_dest, pop_dest, dist, gravity_prob);
      }
    }
  }
}

/**
 * Prints the distances to destination patches to the all_distances.dat file.
 */
void Neighborhood_Layer::print_distances() {
  FILE *fp;
  fp = fopen("all_distances.dat", "w");
  for(int i_src = 0; i_src < this->rows; ++i_src) {
    for(int j_src = 0; j_src < this->cols; ++j_src) {
      Neighborhood_Patch* src_patch = static_cast<Neighborhood_Patch*>(&this->grid[i_src][j_src]);
      double x_src = src_patch->get_center_x();
      double y_src = src_patch->get_center_y();
      int pop_src = src_patch->get_popsize();
      if(pop_src == 0) {
        continue;
      }

      for(int i_dest = 0; i_dest < this->rows; ++i_dest) {
        for(int j_dest = 0; j_dest < this->cols; ++j_dest) {
          if(i_dest < i_src) {
            continue;
          }
          if(i_dest == i_src and j_dest < j_src) {
            continue;
          }
          fprintf(fp,"row %3d col %3d pop %5d ", i_src, j_src, pop_src);
          fprintf(fp,"row %3d col %3d ", i_dest, j_dest);
          Neighborhood_Patch* dest_patch = this->get_patch(i_dest, j_dest);
          assert(dest_patch != nullptr);
          double x_dest = dest_patch->get_center_x();
          double y_dest = dest_patch->get_center_y();
          double dist = sqrt((x_src-x_dest)*(x_src-x_dest) + (y_src - y_dest) * (y_src - y_dest));
          int pop_dest = dest_patch->get_popsize();
          fprintf(fp, "pop %5d dist %0.4f\n", pop_dest, dist);
        }
      }
    }
  }
  fclose(fp);
  exit(0);
}

/**
 * Sets up the gravity model as null.
 */
void Neighborhood_Layer::setup_null_gravity_model() {
  int tmp_offset[256 * 256];
  double tmp_prob[256 * 256];
  int count = 0;

  this->offset = new offset_t*[this->rows];
  this->gravity_cdf = new gravity_cdf_t*[this->rows];
  this->offset[0] = new offset_t[this->cols];
  this->gravity_cdf[0] = new gravity_cdf_t[this->cols];

  this->max_offset = this->rows * this->patch_size;
  assert(this->max_offset < 128);

  for(int i_dest = 0; i_dest < this->rows; ++i_dest) {
    for(int j_dest = 0; j_dest < this->cols; ++j_dest) {
      Neighborhood_Patch* dest_patch = this->get_patch(i_dest, j_dest);
      int pop_dest = dest_patch->get_popsize();
      if(pop_dest == 0) {
        continue;
      }
      // double gravity = pow(pop_dest,pop_exponent);
      double gravity = pop_dest;
      int off = 256 * (0 - i_dest + this->max_offset) + (0 - j_dest + this->max_offset);
      tmp_offset[count] = off;
      tmp_prob[count++] = gravity;
    }
  }

  // transform gravity values into a prob distribution
  double total = 0.0;
  for(int k = 0; k < count; ++k) {
    total += tmp_prob[k];
  }
  for(int k = 0; k < count; ++k) {
    tmp_prob[k] /= total;
  }

  // convert to cdf
  for (int k = 1; k < count; ++k) {
    tmp_prob[k] += tmp_prob[k - 1];
  }

  // store gravity prob and offsets for this patch
  this->gravity_cdf[0][0].reserve(count);
  this->gravity_cdf[0][0].clear();
  this->offset[0][0].reserve(count);
  this->offset[0][0].clear();
  for(int k = 0; k < count; ++k) {
    this->gravity_cdf[0][0].push_back(tmp_prob[k]);
    this->offset[0][0].push_back(tmp_offset[k]);
  }
}

/**
 * Gets the neighborhood of a destination Neighborhood_Patch that is randomly selected based on gravity models 
 * given a source neighborhood.
 *
 * @param src_neighborhood the source neighborhood
 * @return the destination neighborhood
 */
Place* Neighborhood_Layer::select_destination_neighborhood(Place* src_neighborhood) {

  assert(src_neighborhood != nullptr);
  Neighborhood_Patch* src_patch = this->get_patch(src_neighborhood->get_latitude(), src_neighborhood->get_longitude());
  int i_src = src_patch->get_row();
  int j_src = src_patch->get_col();
  if(this->max_distance < 0) {
    // use null gravity model
    i_src = j_src = 0;
  }
  int offset_index = Random::draw_from_cdf_vector(this->gravity_cdf[i_src][j_src]);
  int off = this->offset[i_src][j_src][offset_index];
  int i_dest = i_src + this->max_offset - (off / 256);
  int j_dest = j_src + this->max_offset - (off % 256);

  Neighborhood_Patch* dest_patch = this->get_patch(i_dest, j_dest);
  assert(dest_patch != nullptr);

  return dest_patch->get_neighborhood();
}

/**
 * Adds a specified place to the Neighborhood_Patch that it is located within. This will also set the Place's patch.
 *
 * @param place the place
 */
void Neighborhood_Layer::add_place(Place* place) {
  int row = this->get_row(place->get_latitude());
  int col = this->get_col(place->get_longitude());
  Neighborhood_Patch* patch = Global::Neighborhoods->get_patch(row, col);
  if(patch == nullptr) {
    // Raised the verbosity to a 1 since we really don't want this filling up the LOG file
    Neighborhood_Layer::neighborhood_layer_logger->warn(
        "WARNING: place {:d} {:s} has bad patch,  lat = {:f} (not in [{:f}, {:f}])  lon = {:f} (not in [{:f}, {:f}])", 
        place->get_id(), place->get_label(), place->get_latitude(), this->min_lat, this->max_lat, 
        place->get_longitude(), this->min_lon, this->max_lon);
  } else {
    patch->add_place(place);
  }
  place->set_patch(patch);
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Neighborhood_Layer::setup_logging() {
  if(Neighborhood_Layer::is_log_initialized) {
    return;
  }

  Parser::get_property("neighborhood_layer_log_level", &Neighborhood_Layer::neighborhood_layer_log_level);

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Neighborhood_Layer::neighborhood_layer_logger = std::make_unique<spdlog::logger>("neighborhood_layer_logger", 
        sink_list.begin(), sink_list.end());
    Neighborhood_Layer::neighborhood_layer_logger->set_level(
        Utils::get_log_level_from_string(Neighborhood_Layer::neighborhood_layer_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Neighborhood_Layer::neighborhood_layer_logger->trace("<{:s}, {:d}>: Neighborhood_Layer logger initialized", 
      __FILE__, __LINE__  );
  Neighborhood_Layer::is_log_initialized = true;
}
