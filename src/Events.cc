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
// File: Events.cc
//

#include "Events.h"
#include "Global.h"
#include "Utils.h"

/**
 * Creates an Events object with an events vector size equal to the number of 
 * steps of the simulation. This will be 24 * Global::Simulation_Days.
 */
Events::Events() {
  this->event_queue_size = 24 * Global::Simulation_Days;
  this->events = new events_t [ event_queue_size ];
  for(int step = 0; step < this->event_queue_size; ++step) {
    this->clear_events(step);
  }
}

/**
 * Adds a specified event item to the events vector at a specified step. If the events 
 * vector is at its capacity, it's capacity will be increased.
 *
 * @param step the step
 * @param item the event item
 */
void Events::add_event(int step, event_t item) {

  if (step < 0 || this->event_queue_size <= step) {
    // won't happen during this simulation
    return;
  }
  if(this->events[step].size() == this->events[step].capacity()) {
    if(this->events[step].capacity() < 4) {
      this->events[step].reserve(4);
    }
    this->events[step].reserve(2 * this->events[step].capacity());
  }
  this->events[step].push_back(item);
}

/**
 * Deletes a specified event item from the events vector at a specified step.
 *
 * @param step the step
 * @param the event item
 */
void Events::delete_event(int step, event_t item) {

  if(step < 0 || this->event_queue_size <= step) {
    // won't happen during this simulation
    return;
  }
  // find item in the list
  int size = this->get_size(step);
  for(int pos = 0; pos < size; ++pos) {
    if(this->events[step][pos] == item) {
      // copy last item in list into this slot
      this->events[step][pos] = this->events[step].back();
      // delete last slot
      this->events[step].pop_back();
      return;
    }
  }
  // item not found
  Utils::fred_abort("delete_events: item not found\n");
}

/**
 * Clears all event items from the events vector at a specified step.
 *
 * @param step the step
 */
void Events::clear_events(int step) {
  assert(0 <= step && step < this->event_queue_size);
  this->events[step] = events_t();
}

/**
 * Gets the size of the events vector at a specified step.
 *
 * @param step the step
 */
int Events::get_size(int step) {
  assert(0 <= step && step < this->event_queue_size);
  return static_cast<int>(this->events[step].size());
}

/**
 * Gets the event item in the events vector at a given step and index.
 *
 * @param step the step
 * @param i the index
 * @return the event item
 */
event_t Events::get_event(int step, int i) {
  assert(0 <= step && step < this->event_queue_size);
  if(0 <= i && i < static_cast<int>(this->events[step].size())) {
    return this->events[step][i];
  } else {
    Utils::fred_abort("get_event: i = %d size = %d\n", i, static_cast<int>(this->events[step].size()));
    return nullptr;
  }
}

/**
 * Outputs all event items in the events vector at a specified step to a specified 
 * output location.
 *
 * @param fp the output location
 * @param step the step
 */
void Events::print_events(FILE* fp, int step) {
  assert(0 <= step && step < this->event_queue_size);
  events_itr_t itr_end = this->events[step].end();
  fprintf(fp, "events[%d] = %d : ", step, this->get_size(step));
  for(events_itr_t itr = this->events[step].begin(); itr != itr_end; ++itr) {
    // fprintf(fp, "id %d age %d ", (*itr)->get_id(), (*itr)->get_age());
  }
  fprintf(fp,"\n");
  fflush(fp);
}

/**
 * Outputs all event items in the evens vector at a specified step to stdout.
 *
 * @param step the step
 */
void Events::print_events(int step) {
  this->print_events(stdout, step);
}
