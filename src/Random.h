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
// File: Random.h
//

#ifndef _FRED_RANDOM_H
#define _FRED_RANDOM_H

#include <random>
#include <sstream>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "Global.h"

class RNG {

public:
  void set_seed(unsigned long seed);
  double random() {
    return this->unif_dist(mt_engine);
  }

  int random_int(int low, int high) {
    return low + static_cast<int>((high - low + 1) * random());
  }

  double exponential(double lambda);

  int draw_from_distribution(int n, double* dist);

  double normal(double mu, double sigma);

  double lognormal(double mu, double sigma);

  int geometric(double p) {
    std::geometric_distribution<int> geometric_dist(p);
    return geometric_dist(this->mt_engine);
  }

  int binomial(int t, double p) {
    std::binomial_distribution<int> binomial_dist(t, p);
    return binomial_dist(this->mt_engine);
  }

  int negative_binomial(int k, double p) {
    std::negative_binomial_distribution<int> negative_binomial_dist(k, p);
    return negative_binomial_dist(this->mt_engine);
  }

  int poisson(double mean) {
    std::poisson_distribution<int> poisson_dist(mean);
    return poisson_dist(this->mt_engine);
  }

  int draw_from_cdf(double* v, int size);
  int draw_from_cdf_vector(const std::vector <double>& v);
  void sample_range_without_replacement(int N, int s, int* result);

  static void setup_logging();
  
private:
  std::mt19937_64 mt_engine;
  std::uniform_real_distribution<double> unif_dist;
  std::normal_distribution<double> normal_dist;

  static bool is_log_initialized;
  static std::string rng_log_level;
  static std::unique_ptr<spdlog::logger> rng_logger;
};


class Thread_RNG {
public:
  Thread_RNG();

  void set_seed(unsigned long seed);

  double get_random() {
    return this->thread_rng[fred::omp_get_thread_num()].random();
  }

  double get_random(double low, double high) {
    return low + (high - low) * this->thread_rng[fred::omp_get_thread_num()].random();
  }

  int get_random_int(int low, int high) {
    return this->thread_rng[fred::omp_get_thread_num()].random_int(low,high);
  }

  int draw_from_cdf(double *v, int size) {
    return this->thread_rng[fred::omp_get_thread_num()].draw_from_cdf(v, size);
  }

  int draw_from_cdf_vector(const std::vector <double>& v) {
    return this->thread_rng[fred::omp_get_thread_num()].draw_from_cdf_vector(v);
  }

  int draw_from_distribution(int n, double *dist) {
    return this->thread_rng[fred::omp_get_thread_num()].draw_from_distribution(n, dist);
  }

  double exponential(double lambda) {
    return this->thread_rng[fred::omp_get_thread_num()].exponential(lambda);
  }

  double normal(double mu, double sigma) {
    return this->thread_rng[fred::omp_get_thread_num()].normal(mu, sigma);
  }

  double lognormal(double mu, double sigma) {
    return this->thread_rng[fred::omp_get_thread_num()].lognormal(mu, sigma);
  }

  int geometric(double p) {
    return this->thread_rng[fred::omp_get_thread_num()].geometric(p);
  }

  int binomial(int t, double p) {
    return this->thread_rng[fred::omp_get_thread_num()].binomial(t, p);
  }

  int negative_binomial(int k, double p) {
    return this->thread_rng[fred::omp_get_thread_num()].negative_binomial(k, p);
  }

  int poisson(double mean) {
    return this->thread_rng[fred::omp_get_thread_num()].poisson(mean);
  }

  void sample_range_without_replacement(int N, int s, int* result) {
	  this->thread_rng[fred::omp_get_thread_num()].sample_range_without_replacement(N, s, result);
  }

private:
  RNG* thread_rng;
};

class Random {
  
public:
  static void set_seed(unsigned long seed) { 
    Random::Random_Number_Generator.set_seed(seed);
  }

  static double draw_random() { 
    return Random::Random_Number_Generator.get_random();
  }

  static double draw_random(double low, double high) { 
    return Random::Random_Number_Generator.get_random(low,high);
  }

  static int draw_random_int(int low, int high) { 
    return Random::Random_Number_Generator.get_random_int(low,high);
  }

  static double draw_exponential(double lambda) { 
    return Random::Random_Number_Generator.exponential(lambda);
  }

  static double draw_normal(double mu, double sigma) { 
    return Random::Random_Number_Generator.normal(mu, sigma);
  }

  static double draw_lognormal(double mu, double sigma) { 
    return Random::Random_Number_Generator.lognormal(mu, sigma);
  }

  static int draw_geometric(double p) { 
    return Random::Random_Number_Generator.geometric(p);
  }

  static int draw_from_cdf(double *v, int size) { 
    return Random::Random_Number_Generator.draw_from_cdf(v, size);
  }

  static int draw_binomial(int t, double p) {
    return Random::Random_Number_Generator.binomial(t, p);
  }

  static int draw_negative_binomial(int k, double p) {
    return Random::Random_Number_Generator.negative_binomial(k, p);
  }

  static int draw_poisson(double mean) {
    return Random::Random_Number_Generator.poisson(mean);
  }

  static int draw_from_cdf_vector(const std::vector <double>& vec) { 
    return Random::Random_Number_Generator.draw_from_cdf_vector(vec);
  }

  static int draw_from_distribution(int n, double *dist) { 
    return Random::Random_Number_Generator.draw_from_distribution(n, dist);
  }

  static void sample_range_without_replacement(int N, int s, int* result) {
	  Random::Random_Number_Generator.sample_range_without_replacement(N, s, result);
  }


  /**
   * Generate a random character
   *
   * @return a random character
   *
   * @see https://lowrey.me/guid-generation-in-c-11/
   */
  static unsigned int random_char() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    return dis(gen);
  }

  /**
   * Generate a random string of a requested number of hex character
   *
   * @param len the length of the string requested
   * @return a string of random hex characters
   *
   * @see https://lowrey.me/guid-generation-in-c-11/
   */
  static std::string generate_hex(const unsigned int len) {
    std::stringstream ss;
    for(auto i = 0; i < len; ++i) {
      const auto rc = Random::random_char();
      std::stringstream hexstream;
      hexstream << std::hex << rc;
      auto hex = hexstream.str();
      ss << (hex.length() < 2 ? '0' + hex : hex);
    }
    return ss.str();
  }

  /**
   * Generate a random Global Unique Identifier (GUID)
   *
   * A GUID is of the form:
   * 30dd879c-ee2f-11db-8314-0800200c9a66
   *
   * @return the GUID generated
   */
  static std::string generate_GUID() {
    std::stringstream ss;
    ss << Random::generate_hex(8) << "-" << Random::generate_hex(4) <<
        "-" << Random::generate_hex(4) << "-" << Random::generate_hex(4) <<
        "-" << Random::generate_hex(12);

    return ss.str();
  }

private:
  static Thread_RNG Random_Number_Generator;
};


template <typename T> 
void FYShuffle(std::vector <T> &array) {
  int m;
  int randIndx;
  T tmp;
  unsigned int n = array.size();
  m = n;
  while(m > 0) {
    randIndx = (int)(Random::draw_random() * n);
    --m;
    tmp = array[m];
    array[m] = array[randIndx];
    array[randIndx] = tmp;
  }
}


#endif // _FRED_RANDOM_H
