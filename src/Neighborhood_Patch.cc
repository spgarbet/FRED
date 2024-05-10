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
// File: Neighborhood_Patch.cc
//


#include <new>
#include <string>
#include <sstream>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>

#include "Geo.h"
#include "Global.h"
#include "Household.h"
#include "Neighborhood_Layer.h"
#include "Neighborhood_Patch.h"
#include "Parser.h"
#include "Person.h"
#include "Place.h"
#include "Place_Type.h"
#include "Random.h"

bool Neighborhood_Patch::is_log_initialized = false;
std::string Neighborhood_Patch::neighborhood_patch_log_level = "";
std::unique_ptr<spdlog::logger> Neighborhood_Patch::neighborhood_patch_logger = nullptr;

/**
 * Adds the specified place to the given place vector if it is not already included.
 *
 * @param vec the place vector
 * @param p the place
 */
void insert_if_unique(place_vector* vec, Place* p) {
  for(place_vector::iterator itr = vec->begin(); itr != vec->end(); ++itr) {
    if(*itr == p) {
      return;
    }
  }
  vec->push_back(p);
}

/**
 * Creates a Neighborhood_Patch with default variables.
 */
Neighborhood_Patch::Neighborhood_Patch() {
  this->grid = nullptr;
  this->row = -1;
  this-> col = -1;
  this->min_x = 0.0;
  this->max_x = 0.0;
  this->min_y = 0.0;
  this->max_y = 0.0;
  this->center_x = 0.0;
  this->center_y = 0.0;

  this->grid = nullptr;
  this->neighborhood = nullptr;
  this->person.clear();
  this->popsize = 0;
  this->admin_code = 0;
  this->elevation_sites.clear();

  this->schools_attended_by_neighborhood_residents.clear();
  this->workplaces_attended_by_neighborhood_residents.clear();
  this->places = nullptr;
}

/**
 * Sets up the patch in the specified Neighbor_Hood with the given row and column, 
 * as well as other properties of the patch.
 *
 * @param grd the grid
 * @param i the row
 * @param j the column
 */
void Neighborhood_Patch::setup(Neighborhood_Layer* grd, int i, int j) {
  this->grid = grd;
  this->row = i;
  this->col = j;
  double patch_size = this->grid->get_patch_size();
  double grid_min_x = this->grid->get_min_x();
  double grid_min_y = this->grid->get_min_y();
  this->min_x = grid_min_x + this->col * patch_size;
  this->min_y = grid_min_y + this->row * patch_size;
  this->max_x = grid_min_x + (this->col + 1) * patch_size;
  this->max_y = grid_min_y + (this->row + 1) * patch_size;
  this->center_y = (this->min_y + this->max_y) / 2.0;
  this->center_x = (this->min_x + this->max_x) / 2.0;
  this->popsize = 0;
  this->neighborhood = nullptr;
  this->admin_code = 0;
  int number_of_place_types = Place_Type::get_number_of_place_types();
  this->places = new place_vector_t[number_of_place_types];
  for(int i = 0; i < number_of_place_types; ++i) {
    this->places[i].clear();
  }
  this->schools_attended_by_neighborhood_residents.clear();
  this->workplaces_attended_by_neighborhood_residents.clear();
}

/**
 * Prepares the neighborhood patch as well as its associated neighborhood.
 */
void Neighborhood_Patch::prepare() {
  this->neighborhood->prepare();
  this->neighborhood->set_elevation(0.0);
  int houses = this->get_number_of_households();
  if(houses > 0) {
    double sum = 0.0;
    for(int i = 0; i < houses; ++i) {
      double elevation = this->get_household(i)->get_elevation();
      sum += elevation;
    }
    double mean = sum / static_cast<double>(houses);
    this->neighborhood->set_elevation(mean);
  }

  // find median income
  int size = houses;
  std::vector<int> income_list;
  income_list.reserve(size);
  for(int i = 0; i < houses; ++i) {
    income_list[i] = this->get_household(i)->get_income();
  }
  std::sort(income_list.begin(), income_list.end());
  int median = income_list[size / 2];
  this->neighborhood->set_income(median);

}

/**
 * Makes this neighborhood patch's associated neighborhood with the specified Place_Type.
 *
 * @param type the place type ID
 */
void Neighborhood_Patch::make_neighborhood(int type) {
  char label[80];
  snprintf(label, 80, "N-%04d-%04d", this->row, this->col);
  fred::geo lat = Geo::get_latitude(this->center_y);
  fred::geo lon = Geo::get_longitude(this->center_x);
  this->neighborhood = Place::add_place(label, type, Place::SUBTYPE_NONE, lon, lat, 0.0, this->admin_code);
}

/**
 * Adds the specified Place to the places in the patch.
 *
 * @param place the place
 */
void Neighborhood_Patch::add_place(Place* place) {
  int type_id = place->get_type_id();
  this->places[type_id].push_back(place);
  if(this->admin_code == 0) {
    this->admin_code = place->get_admin_code();
  }
  Neighborhood_Patch::neighborhood_patch_logger->info(
      "NEIGHBORHOOD_PATCH: add place {:d} {:s} type_id {:d} lat {:.8f} lon {:.8f}  row {:d}  col {:d}  place_number {:d}",
      place->get_id(), place->get_label(), type_id, place->get_longitude(), place->get_latitude(),
      row, col, static_cast<int>(this->places[type_id].size()));
}

/**
 * Records the activity groups in the patch and sets the population size.
 */
void Neighborhood_Patch::record_activity_groups() {
  Household* house;
  Person* per;
  Place* p;
  Place* s;

  // create lists of persons, workplaces, schools (by age)
  this->person.clear();
  this->schools_attended_by_neighborhood_residents.clear();
  this->workplaces_attended_by_neighborhood_residents.clear();
  for(int age = 0; age < Global::ADULT_AGE; ++age) {
    this->schools_attended_by_neighborhood_residents_by_age[age].clear();
  }

  int houses = this->get_number_of_households();
  for(int i = 0; i < houses; ++i) {
    house = static_cast<Household*>(this->get_household(i));
    int hsize = house->get_size();
    // fprintf(fp, "%d ", hsize);
    for(int j = 0; j < hsize; ++j) {
      per = house->get_member(j);
      person.push_back(per);
      p = per->get_workplace();
      if(p != nullptr) {
        insert_if_unique(&workplaces_attended_by_neighborhood_residents,p);
      }
      s = per->get_school();
      if(s != nullptr) {
        insert_if_unique(&this->schools_attended_by_neighborhood_residents,s);
        for(int age = 0; age < Global::ADULT_AGE; ++age) {
          if(s->get_original_size_by_age(age) > 0) {
            insert_if_unique(&schools_attended_by_neighborhood_residents_by_age[age],s);
          }
        }
      }
    }
    // fprintf(fp, "\n");
  }
  // fclose(fp);
  this->popsize = static_cast<int>(this->person.size());
}

/**
 * Selects a random Household in the patch.
 *
 * @return the household
 */
Place* Neighborhood_Patch::select_random_household() {
  int n = this->get_number_of_households();
  if(n == 0) {
    return nullptr;
  }
  int i = Random::draw_random_int(0, n - 1);
  return this->get_household(i);
}

/**
 * Performs quality control on the patch. Outputs data to the global status file.
 */
void Neighborhood_Patch::quality_control() {
  if(Global::Quality_control > 1 && this->person.size() > 0) {
    std::stringstream ss;
    ss << fmt::format(
        "PATCH row = {:d} col = {:d}  pop = {:d}  houses = {:d} work = {:d} schools = {:d} by_age ",
        this->row, this->col, static_cast<int>(this->person.size()), this->get_number_of_households(),
        this->get_number_of_workplaces(), this->get_number_of_schools());
    for(int age = 0; age < 20; ++age) {
      ss << fmt::format("{:d}", static_cast<int>(this->schools_attended_by_neighborhood_residents_by_age[age].size()));
    }
    Neighborhood_Patch::neighborhood_patch_logger->info("{:s}", ss.str());

    for(int i = 0; i < static_cast<int>(this->schools_attended_by_neighborhood_residents.size()); ++i) {
      Place* s = this->schools_attended_by_neighborhood_residents[i];
      ss.str(std::string());
      ss << fmt::format("School {:d}: {:s} by_age: ", i, s->get_label());
      for(int a = 0; a < 19; ++a) {
        ss << fmt::format("{:d}:{:d},{:d} ", a, s->get_size_by_age(a), s->get_original_size_by_age(a));
      }
      Neighborhood_Patch::neighborhood_patch_logger->info(ss.str());
    }
  }
}

/**
 * Gets the elevation at a given latitude and longitude.
 *
 * @param lat the latitude
 * @param lon the longitude
 * @return the elevation
 */
double Neighborhood_Patch::get_elevation(double lat, double lon) {

  int size = static_cast<int>(this->elevation_sites.size());
  if(size > 0) {

    // use FCC interpolation from
    // http://www.softwright.com/faq/support/Terrain%20Elevation%20Interpolation.html

    // find the four elevation points closest to the target point
    elevation_t* A = nullptr;
    elevation_t* B = nullptr;
    elevation_t* C = nullptr;
    elevation_t* D = nullptr;
    for(int i = 1; i < size; ++i) {
      elevation_t* e = this->elevation_sites[i];
      if((lat <= e->lat && e->lon <= lon) && (A == nullptr || (e->lat <= A->lat && A->lon <= e->lon))) {
        A = e;
      }
      if((lat <= e->lat && lon < e->lon) && (B == nullptr || (e->lat <= B->lat && e->lon <= B->lon))) {
        B = e;
      }
      if((e->lat <= lat && e->lon <= lon) && (C == nullptr || (C->lat <= e->lat && C->lon <= e->lon))) {
        C = e;
      }
      if((e->lat <= lat && lon < e->lon) && (D == nullptr || (D->lat <= e->lat && e->lon <= D->lon))) {
        D = e;
      }
    }  
    assert(A != nullptr && B != nullptr && C != nullptr && D != nullptr);

    // interpolate on the lines AB and CD
    double elev_E = -9999;
    double lat_E = -999;
    if(A == nullptr) {
      if(B != nullptr) {
        elev_E = B->elevation;
        lat_E = B->lat;
      }
    } else if(B == nullptr) {
      if(A != nullptr) {
        elev_E = A->elevation;
        lat_E = A->lat;
      }
    } else {
      elev_E = ((B->lon - lon) * A->elevation + (lon - A->lon) * B->elevation) / (B->lon - A->lon);
      lat_E = A->lat;
    }

    double elev_F = -9999;
    double lat_F = -999;
    if(C == nullptr) {
      if(D != nullptr) {
        elev_F = D->elevation;
        lat_F = D->lat;
      }
    } else if(D == nullptr) {
      if(C!=nullptr) {
        elev_F = C->elevation;
        lat_F = C->lat;
      }
    } else {
      elev_F = ((D->lon - lon) * C->elevation + (lon - C->lon) * D->elevation) / (D->lon - C->lon);
      lat_F = C->lat;
    }

    // interpolate on the line EF
    double elev_G;
    if(elev_E < 0) {
      elev_G = elev_F;
    } else if(elev_F < 0) {
      elev_G = elev_E;
    } else {
      if(lat_E > lat_F) {
        elev_G = ((lat_E - lat) * elev_F + (lat - lat_F) * elev_E) / (lat_E - lat_F);
      } else {
        elev_G = elev_F;
      }
    }
    if(elev_G < -9000) {
      Neighborhood_Patch::neighborhood_patch_logger->critical(
          "HELP! lat_E {:f} lat_F {:f} elev_E {:f} elev_F {:f} elev_G {:f}", lat_E, lat_F, elev_E, elev_F, elev_G);
      assert(elev_G >= -9000);
    }
    return elev_G;

    // use closest elevation site
    double min_dist = Geo::xy_distance(lat, lon, this->elevation_sites[0]->lat, this->elevation_sites[0]->lon);
    int min_dist_index = 0;
    for(int i = 1; i < size; ++i) {
      double dist = Geo::xy_distance(lat, lon, this->elevation_sites[i]->lat, this->elevation_sites[i]->lon);
      if(dist < min_dist) {
        min_dist = dist;
        min_dist_index = i;
      }
    }
    return this->elevation_sites[min_dist_index]->elevation;
  } else {
    return 0.0;
  }
}

/**
 * Gets Place objects of the specified Place_Type at the specified distance from this patch. 
 * Places will be fetched from neighborhood patches that are a combination of x adjacent cells 
 * away from the cell of this neighborhood patch, where x is the distance.
 *
 * @param type_id the type ID
 * @param dist the distance
 * @return the places
 */
place_vector_t Neighborhood_Patch::get_places_at_distance(int type_id, int dist) {
  place_vector_t results;
  place_vector_t tmp;
  results.clear();
  tmp.clear();
  Neighborhood_Patch* next_patch;

  if(dist == 0) {
    int r = this->row;
    int c = this->col;
    Neighborhood_Patch::neighborhood_patch_logger->debug("get_patch X {:d} Y {:d} | dist = {:d} | x {:d} y {:d}", 
        this->col, this->row, dist, c, r);
    next_patch = Global::Neighborhoods->get_patch(r, c);
    if(next_patch != nullptr) {
      tmp = next_patch->get_places(type_id);
      // append results from one patch to overall results
      results.insert(std::end(results), std::begin(tmp), std::end(tmp));
    }
    return results;
  }

  for(int c = this->col - dist; c <= this->col + dist; ++c) {
    int r = this->row - (dist - abs(c - this->col));;
    Neighborhood_Patch::neighborhood_patch_logger->debug("get_patch X {:d} Y {:d} | dist = {:d} | x {:d} y {:d}", 
        this->col, this->row, dist, c, r);
    next_patch = Global::Neighborhoods->get_patch(r, c);
    if(next_patch != nullptr) {
      tmp = next_patch->get_places(type_id);
      // append results from one patch to overall results
      results.insert(std::end(results), std::begin(tmp), std::end(tmp));
    }

    if(dist > abs(c - this->col)) {
      r = this->row + (dist - abs(c - this->col));;
      Neighborhood_Patch::neighborhood_patch_logger->debug("get_patch X {:d} Y {:d} | dist = {:d} | x {:d} y {:d}", 
          this->col, this->row, dist, c, r);
      next_patch = Global::Neighborhoods->get_patch(r, c);
      if(next_patch != nullptr) {
        tmp = next_patch->get_places(type_id);
        // append results from one patch to overall results
        results.insert(std::end(results), std::begin(tmp), std::end(tmp));
      }
    }
  }
  return results;
}

/**
 * Initialize the class-level logging
 * Initializes the static logger if it has not been created yet
 */
void Neighborhood_Patch::setup_logging() {
  if(Neighborhood_Patch::is_log_initialized) {
    return;
  }

  Parser::get_property("neighborhood_patch_log_level", &Neighborhood_Patch::neighborhood_patch_log_level);

  try {
    spdlog::sinks_init_list sink_list = {Global::StdoutSink, Global::ErrorFileSink, 
        Global::DebugFileSink, Global::TraceFileSink};
    Neighborhood_Patch::neighborhood_patch_logger = std::make_unique<spdlog::logger>("neighborhood_patch_logger", 
        sink_list.begin(), sink_list.end());
    Neighborhood_Patch::neighborhood_patch_logger->set_level(
        Utils::get_log_level_from_string(Neighborhood_Patch::neighborhood_patch_log_level));
  } catch(const spdlog::spdlog_ex& ex) {
    Utils::fred_abort("ERROR --- Log initialization failed:  %s\n", ex.what());
  }

  Neighborhood_Patch::neighborhood_patch_logger->trace("<{:s}, {:d}>: Neighborhood_Patch logger initialized", 
      __FILE__, __LINE__  );
  Neighborhood_Patch::is_log_initialized = true;
}
