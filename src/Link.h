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
// File: Link.h
//
#ifndef _FRED_LINK_H
#define _FRED_LINK_H

#include "Global.h"

class Group;
class Network;
class Person;
class Place;

/**
 * This class represents the link between a Person and a Group.
 *
 * A Link stores a Group, and the index at which a Person is at in the members vector of that same Group. 
 * Person objects have an array of links, each of which link them to a different group of which they are 
 * a member in. Links track data on the connection between members of networks, defined as edges. 
 * An inward edge is a connection from someone else to the tracked person. An outward edge is a 
 * connectiong to someone else from the tracked person. This is the case for directed networks; if the 
 * Network is undirected, these edges do not have a specific direction. Edges represent the interaction 
 * real people have with others.
 */
class Link {
 public:

  Link();
  ~Link() {}

  void begin_membership(Person* person, Group* new_group);
  void end_membership(Person* person);
  void remove_from_network(Person* person);
  void add_edge_to(Person* other_person);
  void add_edge_from(Person* other_person);
  void delete_edge_to(Person* other_person);
  void delete_edge_from(Person* other_person);
  void print(FILE* fp);

  /**
   * Gets this link's associated Group.
   *
   * @return the group
   */
  Group* get_group() {
    return this->group;
  }

  Network* get_network();
  Place* get_place();

  /**
   * Gets the index of this link's associated member in this link's associated Group's members.
   *
   * @return the member index
   */
  int get_member_index() {
    return this->member_index;
  }

  /**
   * Checks if this link is associated with a member. This will be true if there is a member index set.
   *
   * @return if this link is associated with a member
   */
  bool is_member() {
    return this->member_index != -1;
  }

  bool is_connected_to(Person* other_person);
  bool is_connected_from(Person* other_person);

  /**
   * Gets the out-degree. This is the number of outward edges in a Network from this link's associated member, 
   * or the number of people this link's associated member is linked to.
   *
   * @return the out-degree
   */
  int get_out_degree() {
    return this->outward_edge.size();
  }

  /**
   * Gets the in-degree. This is the number of inward edges in a Network to this link's associated member, 
   * or the number of people this link's associated member is linked from.
   *
   * @return the in-degree
   */
  int get_in_degree() {
    return this->inward_edge.size();
  }

  /**
   * Clears values regarding links.
   */
  void clear() {
    this->inward_edge.clear();
    this->outward_edge.clear();
    this->inward_timestamp.clear();
    this->outward_timestamp.clear();
    this->inward_weight.clear();
    this->outward_weight.clear();
  }

  /**
   * Gets the inward edge at a specified index in the inward edge person vector. This will return a Person that this link's 
   * associated member is linked from.
   *
   * @param n the index
   * @return the inward edge
   */
  Person * get_inward_edge(int n) {
    return this->inward_edge[n];
  }

  /**
   * Gets the outward edge at a specified index in the outward edge person vector. This will return a Person that this link's 
   * associated member is linked to.
   *
   * @param n the index
   * @return the outward edge
   */
  Person * get_outward_edge(int n) {
    return this->outward_edge[n];
  }

  /**
   * Gets the outward edge person vector. This will contain all persons to whom this link's associated member is linked.
   *
   * @return the outward edges
   */
  person_vector_t get_outward_edges() {
    return this->outward_edge;
  }

  /**
   * Gets the inward edge person vector. This will contain all persons from whom this link's associated member is linked.
   *
   * @return the inward edges
   */
  person_vector_t get_inward_edges() {
    return this->inward_edge;
  }

  void set_weight_to(Person* other_person, double value);
  void set_weight_from(Person* other_person, double value);

  double get_weight_to(Person* other_person);
  double get_weight_from(Person* other_person);

  int get_timestamp_to(Person* other_person);
  int get_timestamp_from(Person* other_person);

  int get_id_of_last_inward_edge();
  int get_id_of_last_outward_edge();

  int get_timestamp_of_last_inward_edge();
  int get_timestamp_of_last_outward_edge();

  int get_id_of_max_weight_inward_edge();
  int get_id_of_max_weight_outward_edge();

  int get_id_of_min_weight_inward_edge();
  int get_id_of_min_weight_outward_edge();

  void update_member_index(int new_index);
  void link(Person* person, Group* new_group);
  void unlink(Person* person);

 private:
  Group* group;
  int member_index;

  person_vector_t inward_edge;
  person_vector_t outward_edge;

  int_vector_t inward_timestamp;
  int_vector_t outward_timestamp;

  double_vector_t inward_weight;
  double_vector_t outward_weight;

};

#endif // _FRED_LINK_H
