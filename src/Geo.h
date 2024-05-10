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
// File: Geo.h
//

#ifndef FRED_GEO_H_
#define FRED_GEO_H_

#include <cmath>

#include "Global.h"

typedef struct {
  fred::geo lat;
  fred::geo lon;
  double elevation;
} elevation_t;

/**
 * This class contains static methods that complete various geological calculations, such as conversions 
 * between latitude / longitude and global x and y values, and distances between points on the Earth's surface.
 */
class Geo {
 public:

  static const double DEG_TO_RAD;		// PI/180
  static double km_per_deg_longitude;
  static double km_per_deg_latitude;

  static void set_km_per_degree(fred::geo lat);
  static double haversine_distance(fred::geo lon1, fred::geo lat1, fred::geo lon2, fred::geo lat2);
  static double spherical_cosine_distance(fred::geo lon1, fred::geo lat1, fred::geo lon2, fred::geo lat2);
  static double spherical_projection_distance(fred::geo lon1, fred::geo lat1, fred::geo lon2, fred::geo lat2);

  /**
   * Gets the x value of a specified longitude using the conversion: 
   * (longitude + 180) * kilometers per degree of longitude
   *
   * @param lon the longitude
   * @return the x value
   */
  static double get_x(fred::geo lon) {
    return (lon + 180.0) * Geo::km_per_deg_longitude;
  }

  /**
   * Gets the y value of a specified latitude using the conversion: 
   * (latitude + 180) * kilometers per degree of latitude
   *
   * @param lat the latitude
   * @return the y value
   */
  static double get_y(fred::geo lat) {
    return (lat + 90.0) * Geo::km_per_deg_latitude;
  }

  /**
   * Gets the longitude of a specified x value using the conversion: 
   * (x value / kilometers per degree of longitude) - 180
   *
   * @param x the x value
   * @return the longitude
   */
  static double get_longitude(double x) {
    return static_cast<double>(x / Geo::km_per_deg_longitude - 180.0);
  }

  /**
   * Gets the latitude of a specified y value using the conversion: 
   * (y value / kilometers per degree of latitude) - 90
   *
   * @param y the y value
   * @return the latitude
   */
  static double get_latitude(double y) {
    return static_cast<double>(y / Geo::km_per_deg_latitude - 90.0);
  }

  /**
   * Calculates the distance between two points on the Earth's surface using the distance formula. 
   * This does not account for a spherical earth.
   *
   * @param lat1 first latitude
   * @param lon1 first longitude
   * @param lat2 second latitude
   * @param lon2 second longitude
   * @return the distance
   */
  static double xy_distance(fred::geo lat1, fred::geo lon1, fred::geo lat2, fred::geo lon2) {
    double x1 = Geo::get_x(lon1);
    double y1 = Geo::get_y(lat1);
    double x2 = Geo::get_x(lon2);
    double y2 = Geo::get_y(lat2);
    return sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
  }

  /**
   * Converts an x distance in kilometers to degrees of longitude.
   *
   * @param xsize the x distance
   * @return the degrees of longitude
   */
  static double xsize_to_degree_longitude(double xsize) {
    return (xsize / Geo::km_per_deg_longitude);
  }

  /**
   * Converts a y distance in kilometers to degrees of latitude.
   *
   * @param ysize the y distance
   * @return the degrees of latitude
   */
  static double ysize_to_degree_latitude(double ysize) {
    return (ysize / Geo::km_per_deg_latitude);
  }


private:
  // see http://andrew.hedges.name/experiments/haversine/
  static const double EARTH_RADIUS;	 // earth's radius in kilometers
  static const double KM_PER_DEG_LAT;	     // assuming spherical earth

  // US Mean latitude-longitude (http://www.travelmath.com/country/United+States)
//  static const double MEAN_US_LON;		// near Wichita, KS
//  static const double MEAN_US_LAT;		// near Wichita, KS

  // from http://www.ariesmar.com/degree-latitude.php
  static const double MEAN_US_KM_PER_DEG_LON;		// at 38 deg N
  static const double MEAN_US_KM_PER_DEG_LAT; //

};

#endif /* FRED_GEO_H_ */
