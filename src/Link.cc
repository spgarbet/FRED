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

#include "Link.h"
#include "Group.h"
#include "Network.h"
#include "Person.h"
#include "Place.h"

/**
 * Creates a Link with default properties.
 */
Link::Link() {
  this->group = nullptr;
  this->member_index = -1;
  this->inward_edge.clear();
  this->outward_edge.clear();
  this->inward_timestamp.clear();
  this->outward_timestamp.clear();
  this->inward_weight.clear();
  this->outward_weight.clear();
}

/**
 * Adds the given Person as a member of the specified Group. This link will store the group 
 * as well as the index of the person in the group's members.
 *
 * @param person the person
 * @param new_group the group
 */
void Link::begin_membership(Person* person, Group* new_group) {
  if (is_member()) {
    return;
  }
  this->group = new_group;
  this->member_index = this->group->begin_membership(person);
}

/**
 * Removes all outward and inward edges that point from or to the given Person in this link's 
 * associated Network. After all edges have been removed, that person's membership in the network 
 * will be terminated.
 *
 * @param person the person
 */
void Link::remove_from_network(Person* person) {
  // remove edges to other people
  int size = this->outward_edge.size();
  for(int i = 0; i < size; ++i) {
    this->outward_edge[i]->delete_edge_from(person, get_network());
  }

  // remove edges from other people
  size = this->inward_edge.size();
  for(int i = 0; i < size; ++i) {
    this->inward_edge[i]->delete_edge_to(person, get_network());
  }

  // end_membership in this network
  this->end_membership(person);
}

/**
 * Ends the membership of this link's associated member in this link's associated Group. 
 * The link's tracked group and member index will be reset.
 *
 * @param person the person _UNUSED_
 */
void Link::end_membership(Person* person) {
  if (this->group) {
    this->group->end_membership(this->member_index);
    this->group = nullptr;
  }
  this->member_index = -1;
}

/**
 * Gets this link's associated Group as a Network.
 *
 * @return the network
 */
Network* Link::get_network() {
  return static_cast<Network*>(this->group);
}

/**
 * Gets this link's associated Group as a Place.
 *
 * @return the place
 */ 
Place* Link::get_place() {
  return static_cast<Place*>(this->group);
}

// these methods should be used to add or delete edges

/**
 * Adds an outward edge from this link's associated member to the specified Person, if that outward edge 
 * does not already exist. This edge will have a timestamp of the current simulation step, and an initial 
 * weight of 1.0.
 *
 * @param other_person the person
 */
void Link::add_edge_to(Person* other_person) {
  int size = this->outward_edge.size();
  for(int i = 0; i < size; ++i) {
    if(other_person == this->outward_edge[i]) {
      return;
    }
  }

  // add other_person to my outward_edge list.
  this->outward_edge.push_back(other_person);
  this->outward_timestamp.push_back(Global::Simulation_Step);
  this->outward_weight.push_back(1.0);
}

/**
 * Adds an inward edge to this link's associated member from the specified Person, if that inward edge 
 * does not already exist. This edge will have a timestamp of the current simulation step, and an initial 
 * weight of 1.0.
 *
 * @param other_person the person
 */
void Link::add_edge_from(Person* other_person) {
  int size =  this->inward_edge.size();
  for(int i = 0; i < size; ++i) {
    if(other_person == this->inward_edge[i]) {
      return;
    }
  }

  // add other_person to my inward_edge list.
  this->inward_edge.push_back(other_person);
  this->inward_timestamp.push_back(Global::Simulation_Step);
  this->inward_weight.push_back(1.0);
}

/**
 * Deletes the outward edge from this link's associated member to the specified Person, if that outward edge exists.
 *
 * @param other_person the person
 */
void Link::delete_edge_to(Person* other_person) {
  // delete other_person from my outward_edge list.
  int size =  this->outward_edge.size();
  for(int i = 0; i < size; ++i) {
    if(other_person == this->outward_edge[i]) {
      this->outward_edge[i] =  this->outward_edge.back();
      this->outward_edge.pop_back();
      this->outward_timestamp[i] =  this->outward_timestamp.back();
      this->outward_timestamp.pop_back();
      this->outward_weight[i] =  this->outward_weight.back();
      this->outward_weight.pop_back();
    }
  }
}

/**
 * Deletes the inward edge from this link's associated member to the specified Person, if that inward edge exists.
 *
 * @param other_person the person
 */
void Link::delete_edge_from(Person* other_person) {
  // delete other_person from my inward_edge list.
  int size =  this->inward_edge.size();
  for(int i = 0; i < size; i++) {
    if(other_person == this->inward_edge[i]) {
      this->inward_edge[i] =  this->inward_edge.back();
      this->inward_edge.pop_back();
      this->inward_timestamp[i] =  this->inward_timestamp.back();
      this->inward_timestamp.pop_back();
      this->inward_weight[i] =  this->inward_weight.back();
      this->inward_weight.pop_back();
    }
  }
}

/**
 * Prints details about the link to the specified file. _UNUSED_
 *
 * @param fp the file
 */
void Link::print(FILE *fp) {
  /*
  fprintf(fp,"%d ->", this->myself->get_id());
  int size = this->outward_edge.size();
  for (int i = 0; i < size; ++i) {
    fprintf(fp," %d", this->outward_edge[i]->get_id());
  }
  fprintf(fp,"\n");
  return;

  size = inward_edge.size();
  for(int i = 0; i < size; ++i) {
    fprintf(fp,"%d ", this->inward_edge[i]->get_id());
  }
  fprintf(fp,"-> %d\n\n", this->myself->get_id());
  */
}

/**
 * Checks if there exists an outward edge connecting this link's associated member to the specified Person.
 *
 * @param other_person the person
 * @return if there is a connection to
 */
bool Link::is_connected_to(Person* other_person) {
  int size = this->outward_edge.size();
  for(int i = 0; i < size; ++i) {
    if(this->outward_edge[i] == other_person) {
      return true;
    }
  }
  return false;
}

/**
 * Checks if there exists an inward edge connecting this link's associated member from the specified Person.
 *
 * @param other_person the person
 * @return if there is a connection from
 */
bool Link::is_connected_from(Person* other_person) {
  int size = this->inward_edge.size();
  for(int i = 0; i < size; ++i) {
    if(this->inward_edge[i] == other_person) {
      return true;
    }
  }
  return false;
}

/**
 * Updates the member index of this link's associated member that is stored by the link.
 *
 * @param new_index the index
 */
void Link::update_member_index(int new_index) {
  assert(this->member_index != -1);
  assert(new_index != -1);
  this->member_index = new_index;
}

/**
 * Sets the weight of the outward edge to the given Person to the specified value.
 *
 * @param other_person the person
 * @param value the weight
 */
void Link::set_weight_to(Person* other_person, double value) {
  int size = this->outward_edge.size();
  for(int i = 0; i < size; ++i) {
    if(this->outward_edge[i] == other_person) {
      this->outward_weight[i] = value;
      return;
    }
  }
  return;
}

/**
 * Gets the weight of the outward edge to the given Person.
 *
 * @param other_person the person
 * @return the weight
 */
double Link::get_weight_to(Person* other_person) {
  int size = this->outward_edge.size();
  for(int i = 0; i < size; ++i) {
    if(this->outward_edge[i] == other_person) {
      return this->outward_weight[i];
    }
  }
  return 0.0;
}

/**
 * Sets the weight of the inward edge from the given Person to the specified value.
 *
 * @param other_person the person
 * @param value the weight
 */
void Link::set_weight_from(Person* other_person, double value) {
  int size = this->inward_edge.size();
  for(int i = 0; i < size; ++i) {
    if(this->inward_edge[i] == other_person) {
      this->inward_weight[i] = value;
      return;
    }
  }
  return;
}

/**
 * Gets the weight of the inward edge from the given Person.
 *
 * @param other_person the person
 * @return the weight
 */
double Link::get_weight_from(Person* other_person) {
  int size = this->inward_edge.size();
  for(int i = 0; i < size; ++i) {
    if(this->inward_edge[i] == other_person) {
      return this->inward_weight[i];
    }
  }
  return 0.0;
}

/**
 * Gets the timestamp of the outward edge to the given Person. This will be the global simulation step 
 * that the edge was created at.
 *
 * @param person the person
 * @return the timestamp
 */ 
int Link::get_timestamp_to(Person* other_person) {
  int size = this->outward_edge.size();
  for(int i = 0; i < size; ++i) {
    if(this->outward_edge[i] == other_person) {
      return this->outward_timestamp[i];
    }
  }
  return -1;
}

/**
 * Gets the timestamp of the inward edge from the given Person. This will be the global simulation step 
 * that the edge was created at.
 *
 * @param person the person
 * @return the timestamp
 */ 
int Link::get_timestamp_from(Person* other_person) {
  int size = this->inward_edge.size();
  for(int i = 0; i < size; ++i) {
    if(this->inward_edge[i] == other_person) {
      return this->inward_timestamp[i];
    }
  }
  return -1;
}

/**
 * Gets the ID of the last person in the outward edge person vector. This will be the person most recently added.
 *
 * @return the ID
 */
int Link::get_id_of_last_outward_edge() {
  int size = this->outward_edge.size();
  int max_time = -9999999;
  int pos = -1;
  for(int i = 0; i < size; ++i) {
    if (max_time < this->outward_timestamp[i]) {
      max_time = this->outward_timestamp[i];
      pos = i;
    }
  }
  if (0 <= pos) {
    return this->outward_edge[pos]->get_id();
  }
  else {
    return -99999999;
  }
}

/**
 * Gets the ID of the last person in the inward edge person vector. This will be the person most recently added.
 *
 * @return the ID
 */
int Link::get_id_of_last_inward_edge() {
  int size = this->inward_edge.size();
  int max_time = -9999999;
  int pos = -1;
  for(int i = 0; i < size; ++i) {
    if (max_time < this->inward_timestamp[i]) {
      max_time = this->inward_timestamp[i];
      pos = i;
    }
  }
  if (0 <= pos) {
    return this->inward_edge[pos]->get_id();
  }
  else {
    return -99999999;
  }
}

/**
 * Gets the ID of the person that the outward edge with the largest weight points to.
 *
 * @return the ID
 */
int Link::get_id_of_max_weight_outward_edge() {
  int size = this->outward_edge.size();
  double max_weight;
  int pos = -1;
  for(int i = 0; i < size; ++i) {
    if (i==0) {
      max_weight = this->outward_weight[i];
      pos = i;
    }
    else {
      if (max_weight < this->outward_weight[i]) {
        max_weight = this->outward_weight[i];
        pos = i;
      }
    }
  }
  if (0 <= pos) {
    return this->outward_edge[pos]->get_id();
  }
  else {
    return -99999999;
  }
}

/**
 * Gets the ID of the person that the inward edge with the largest weight points from.
 *
 * @return the ID
 */
int Link::get_id_of_max_weight_inward_edge() {
  int size = this->inward_edge.size();
  double max_weight;
  int pos = -1;
  for(int i = 0; i < size; ++i) {
    if (i==0) {
      max_weight = this->inward_weight[i];
      pos = i;
    }
    else {
      if (max_weight < this->inward_weight[i]) {
	max_weight = this->inward_weight[i];
	pos = i;
      }
    }
  }
  if (0 <= pos) {
    return this->inward_edge[pos]->get_id();
  }
  else {
    return -99999999;
  }
}

/**
 * Gets the ID of the person that the outward edge with the smallest weight points to.
 *
 * @return the ID
 */
int Link::get_id_of_min_weight_outward_edge() {
  int size = this->outward_edge.size();
  double min_weight;
  int pos = -1;
  for(int i = 0; i < size; ++i) {
    if (i==0) {
      min_weight = this->outward_weight[i];
      pos = i;
    }
    else {
      if (min_weight > this->outward_weight[i]) {
	min_weight = this->outward_weight[i];
	pos = i;
      }
    }
  }
  if (0 <= pos) {
    return this->outward_edge[pos]->get_id();
  }
  else {
    return -99999999;
  }
}

/**
 * Gets the ID of the person that the inward edge with the smallest weight points to.
 *
 * @return the ID
 */
int Link::get_id_of_min_weight_inward_edge() {
  int size = this->inward_edge.size();
  double min_weight;
  int pos = -1;
  for(int i = 0; i < size; ++i) {
    if (i==0) {
      min_weight = this->inward_weight[i];
      pos = i;
    }
    else {
      if (min_weight > this->inward_weight[i]) {
	min_weight = this->inward_weight[i];
	pos = i;
      }
    }
  }
  if (0 <= pos) {
    return this->inward_edge[pos]->get_id();
  }
  else {
    return -99999999;
  }
}

/**
 * Gets the timestamp of the last inward edge in the inward edge person vector. This will be the 
 * timestamp of the edge most recently added.
 *
 * @return the timestamp
 */
int Link::get_timestamp_of_last_inward_edge() {
  int size = this->inward_edge.size();
  int max_time = -1;
  for(int i = 0; i < size; ++i) {
    if (max_time < this->inward_timestamp[i]) {
      max_time = this->inward_timestamp[i];
    }
  }
  return max_time;
}

/**
 * Gets the timestamp of the last outward edge in the outward edge person vector. This will be the 
 * timestamp of the edge most recently added.
 *
 * @return the timestamp
 */
int Link::get_timestamp_of_last_outward_edge() {
  int size = this->outward_edge.size();
  int max_time = -1;
  for(int i = 0; i < size; ++i) {
    if (max_time < this->outward_timestamp[i]) {
      max_time = this->outward_timestamp[i];
    }
  }
  return max_time;
}


////////////////////////////////////////////

/**
 * Adds the specified Group as this link's associated Group.
 *
 * @param person the person _UNUSED_
 * @param new_group the group
 */
void Link::link(Person* person, Group* new_group) {
  this->group = new_group;
}

/**
 * Unlinks this link's associated member and group.
 *
 * @param person the person _UNUSED_
 */
void Link::unlink(Person* person) {
  this->member_index = -1;
  this->group = nullptr;
}
