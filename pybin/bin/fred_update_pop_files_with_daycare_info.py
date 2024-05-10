#!/usr/bin/env python3

###################################################################################################
##
## This file is part of the FRED system.
##
## Copyright (c) 2010-2012, University of Pittsburgh, John Grefenstette, Shawn Brown,
## Roni Rosenfield, Alona Fyshe, David Galloway, Nathan Stone, Jay DePasse,
## Anuroop Sriram, and Donald Burke
## All rights reserved.
##
## Copyright (c) 2013-2020, University of Pittsburgh, John Grefenstette, Robert Frankeny,
## David Galloway, Mary Krauland, Michael Lann, David Sinclair, and Donald Burke
## All rights reserved.
##
## Copyright (c) 2021, University of Pittsburgh, David Galloway, Mary Krauland, Matthew Dembiczak,
## Johnson Paul, Natalie Sumetsky, and Mark Roberts
## All rights reserved.
##
## FRED is distributed on the condition that users fully understand and agree to all terms of the
## End User License Agreement.
##
## FRED is intended FOR NON-COMMERCIAL, EDUCATIONAL OR RESEARCH PURPOSES ONLY.
##
## See the file "LICENSE" for more information.
##
##################################################################################################

import sys
from pathlib import Path

# Update the Python search Path
file = Path(__file__).resolve()
package_root_directory_path = Path(file.parents[1], 'src')
sys.path.append(str(package_root_directory_path.resolve()))

import argparse
import csv
import logging
import os
import random
import re
from typing import List, Literal, Tuple

from fredpy import location
from fredpy import fredpop
from fredpy.util import fredutil
from fredpy.util import constants

try:
    import numpy as np
except ImportError:
    print('This script requires the numpy module.')
    print('Installation instructions can be found at https://pypi.org/project/numpy/.')
    sys.exit(constants.EXT_CD_ERR)
    
try:
    import pandas as pd
except ImportError:
    print('This script requires the pandas module.')
    print('Installation instructions can be found at https://pypi.org/project/pandas/.')
    sys.exit(constants.EXT_CD_ERR)


def main():

    usage = '''
    Typical usage example:
    
    $ fred_update_pop_files_with_daycare_info.py daycares-42065.csv new_pop_dir
    '''

    # Create and setup the parser
    parser = argparse.ArgumentParser(
        description='Creates updated people.txt and schools.txt files using a daycare center file in the provided output directory.',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('infile',
                        help='the input file with the daycare information')
    parser.add_argument('directory',
                        help='the output directory for the updated population files')
    parser.add_argument('-i',
                        '--infant_attend_daycare_prob',
                        metavar='infant_prob',
                        default=0.13,
                        type=float,
                        help='the probability that an infant [0, 1) will attend daycare')
    parser.add_argument('-t',
                        '--toddler_attend_daycare_prob',
                        metavar='toddler_prob',
                        default=0.26,
                        type=float,
                        help='the probability that a toddler [1, 2] will attend daycare')
    parser.add_argument('-p',
                        '--preschooler_attend_daycare_prob',
                        metavar='preschooler_prob',
                        default=0.61,
                        type=float,
                        help='the probability that a preschool aged [3, 5] child will attend daycare')
    parser.add_argument('--loglevel',
                        default='ERROR',
                        choices=constants.LOGGING_LEVELS,
                        help='set the logging level of this script')
                        
    # Parse the arguments
    args = parser.parse_args()
    infile = args.infile
    directory = args.directory
    loglevel = args.loglevel
    
    infant_attend_daycare_prob = args.infant_attend_daycare_prob
    if infant_attend_daycare_prob < 0.0 or infant_attend_daycare_prob > 1.0:
        infant_attend_daycare_prob = 0.13
    toddler_attend_daycare_prob = args.toddler_attend_daycare_prob
    if toddler_attend_daycare_prob < 0.0 or toddler_attend_daycare_prob > 1.0:
        toddler_attend_daycare_prob = 0.26
    preschooler_attend_daycare_prob = args.preschooler_attend_daycare_prob
    if preschooler_attend_daycare_prob < 0.0 or preschooler_attend_daycare_prob > 1.0:
        preschooler_attend_daycare_prob = 0.61

    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)
    
    logging.debug('infant_attend_daycare_prob: {0}'.format(infant_attend_daycare_prob))
    logging.debug('toddler_attend_daycare_prob: {0}'.format(toddler_attend_daycare_prob))
    logging.debug('preschooler_attend_daycare_prob: {0}'.format(preschooler_attend_daycare_prob))

    result = fred_update_pop_files_with_daycare_info(infile, directory, infant_attend_daycare_prob, toddler_attend_daycare_prob, preschooler_attend_daycare_prob)
    sys.exit(result)


def fred_update_pop_files_with_daycare_info(infile: str, directory: str, infant_attend_daycare_prob: float,
      toddler_attend_daycare_prob: float, preschooler_attend_daycare_prob: float) -> Literal[0, 2]:
    '''
    Makes Tab-delimited population files in the given output directory.
    The new files will be an updated people.txt file and an updated schools.txt file that include
    the new daycare centers as schools and the assignment of preschool children (0-5 years old) to the
    daycare centers
    
    Parameters
    ----------
    infile : str
        A string representation of the path to the input file
    directory : str
        A string representation of the path to the output directory
    infant_attend_daycare_prob : float
        The probability that a child aged [0, 1) will attend daycare
    toddler_attend_daycare_prob: float
        The probability that a child aged [1, 2] will attend daycare
    preschooler_attend_daycare_prob: float
        The probability that a child aged [3, 5] will attend daycare
    
    Returns
    -------
    Literal[0, 2]
        The exit status
    '''
    
    logging.debug('Make sure that the output directory exists')

    # Check the directory
    output_dir_path = Path(directory).resolve()
    try:
        fredutil.validate_dir(output_dir_path)
    except (FileNotFoundError, NotADirectoryError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR
    
#    Based on the stats I have, 13% of kids <1 go to “center-based care” 26% of those 1-2, and 61% of those 3-5.
#    In FRED, about ~64% of 3-5-year-olds are assigned to schools. “Center-based care” includes daycare, Head Start,
#    preschools, and pre-K. Should some of these 3-5-yo kids ALSO go to daycare? CCing Mary and Mark
#    infant_attend_daycare_prob = 0.13
#    toddler_attend_daycare_prob = 0.26
#    preschooler_attend_daycare_prob = 0.61
    
    # Read the daycare file
    logging.debug('Read the input file')
    daycare_df = pd.read_csv(infile)
    
    # Create DaycareCenter objects for each one in the dataframe
    daycare_center_list = pd.Series(
        map(_create_new_daycare_center,
            daycare_df['id'],
            daycare_df['lat'],
            daycare_df['lon']))
    
    county_fips = re.split(r'daycares-', infile)[1].split('.')[0]
    
    people = fredpop.FredPeopleData(county_fips)
    schools = fredpop.FredSchoolData(county_fips)
    households = fredpop.FredHouseholdData(county_fips)
    
    # Add the new daycare centers to the schools DataFrame
    # -> Make sure the 'elevation' field is in the Schools data
    if 'elevation' not in schools.schools_df.columns:
        schools.schools_df['elevation'] = -1.00
        
    # -> Get the mean of the elevation
    elevation_mean = schools.schools_df.loc[:, 'elevation'].mean()
    
    # -> The stco (state / county FIPS) is county_fips for the new daycare centers
    for daycare_center in daycare_center_list:
        new_row = {'sp_id': daycare_center.id, 'stco': county_fips, 'latitude': daycare_center.latitude, 'longitude': daycare_center.longitude, 'elevation': elevation_mean}
        schools.schools_df.loc[len(schools.schools_df)] = new_row

    # Create a temporary dataframe that is a join of people and households
    temp_df = pd.merge(people.people_df, households.households_df, how='inner', on=['sp_hh_id'])
    
    people_info_list_temp = pd.Series(
        map(_get_person_info_as_tuple,
            temp_df['sp_id'],
            temp_df['sp_hh_id'],
            temp_df['school_id'],
            temp_df['age'],
            temp_df['latitude'],
            temp_df['longitude']))
    people_info_list = [x for x in people_info_list_temp if x[2] is not None]
    
    unassigned_due_to_full_daycare_agent_count = 0
    for person_info in people_info_list:
        # Note: person_info is a tuple of (id, age, household)
        if person_info[1] >= 0 and person_info[1] < 1:
            if random.uniform(0, 1) >= infant_attend_daycare_prob:
                continue
                
            # Loop over the Daycare Centers closest to the agent's household
            is_daycare_assigned = False
            for possible_daycare_center in _order_daycare_center_list_using_gravity_model(person_info[2], person_info[2].get_place_list_sorted_by_distance_to_here(daycare_center_list, 5)):
                for daycare_center in daycare_center_list:
                    if possible_daycare_center == daycare_center:
                        try:
                            daycare_center.infant_count += 1
                            is_daycare_assigned = True
                            # Update the agent's school in the people dataframe
                            people.people_df.loc[people.people_df['sp_id'] == person_info[0], 'school_id'] = daycare_center.id
                            break
                        except location.MaxAssignedOccupancyExceededError:
                            continue
                if is_daycare_assigned:
                     break
            if not is_daycare_assigned:
                logging.info('All DaycareCenters are full')
                unassigned_due_to_full_daycare_agent_count += 1
                continue
        elif person_info[1] >= 1 and person_info[1] < 3:
            if random.uniform(0, 1) >= toddler_attend_daycare_prob:
                continue
                
            # Loop over the Daycare Centers closest to the agent's household
            is_daycare_assigned = False
            for possible_daycare_center in _order_daycare_center_list_using_gravity_model(person_info[2], person_info[2].get_place_list_sorted_by_distance_to_here(daycare_center_list, 5)):
                for daycare_center in daycare_center_list:
                    if possible_daycare_center == daycare_center:
                        try:
                            daycare_center.toddler_count += 1
                            is_daycare_assigned = True
                            # Update the agent's school in the people dataframe
                            people.people_df.loc[people.people_df['sp_id'] == person_info[0], 'school_id'] = daycare_center.id
                            break
                        except location.MaxAssignedOccupancyExceededError:
                            continue
                if is_daycare_assigned:
                     break
            if not is_daycare_assigned:
                logging.info('All DaycareCenters are full')
                unassigned_due_to_full_daycare_agent_count += 1
                continue
        elif person_info[1] >= 3 and person_info[1] < 6:
            if random.uniform(0, 1) >= preschooler_attend_daycare_prob:
                continue
                
            # Loop over the Daycare Centers closest to the agent's household
            is_daycare_assigned = False
            for possible_daycare_center in _order_daycare_center_list_using_gravity_model(person_info[2], person_info[2].get_place_list_sorted_by_distance_to_here(daycare_center_list, 5)):
                for daycare_center in daycare_center_list:
                    if possible_daycare_center == daycare_center:
                        try:
                            daycare_center.preschooler_count += 1
                            is_daycare_assigned = True
                            # Update the agent's school in the people dataframe
                            people.people_df.loc[people.people_df['sp_id'] == person_info[0], 'school_id'] = daycare_center.id
                            break
                        except location.MaxAssignedOccupancyExceededError:
                            continue
                if is_daycare_assigned:
                     break
            if not is_daycare_assigned:
                logging.info('All DaycareCenters are full')
                unassigned_due_to_full_daycare_agent_count += 1
                continue
        else:
            logging.error('Age out of valid range for daycare centers [0 - 5]')
            return constants.EXT_CD_ERR
     
    # Print out the new files
    # Want to preserve the school float formats in the original files
    schools.schools_df['latitude'] = schools.schools_df['latitude'].map('{:.7f}'.format)
    schools.schools_df['longitude'] = schools.schools_df['longitude'].map('{:.7f}'.format)
    schools.schools_df['elevation'] = schools.schools_df['elevation'].map('{:.6f}'.format)
    school_file_path = Path(output_dir_path, 'schools.txt')
    schools.schools_df.to_csv(school_file_path, index=False, sep='\t')
    
    people_file_path = Path(output_dir_path, 'people.txt')
    people.people_df.to_csv(people_file_path, index=False, float_format='%.7f', sep='\t')
    print('The count of children who were unassigned due to daycares being filled: {0}'.format(unassigned_due_to_full_daycare_agent_count))
    return constants.EXT_CD_NRM


def _create_new_daycare_center(daycare_id: int, latitude: float, longitude: float) -> location.DaycareCenter:
    '''
    Given a daycare_id, latitude, and longitude, create a new DaycareCenter object and return it

    Parameters
    ----------
    daycare_id : int
        The integer ID of the daycare
        
    latitude : float
        A float holding latitude of the Daycare Center
        
    longitude : float
        A float holding longitude of the Daycare Center

    Return
    ------
    location.DaycareCenter
        The the new DaycareCenter object
    '''

    # NOTE: The min and max school ids are found by looking through all of the schools.txt files
    #       in all of the US county folders
    #
    # % cat */schools.txt | awk '{print $1;}' | sort -n  | uniq > sorted_school_id.txt
    #
    # % head -4 sorted_school_id.txt
    # sp_id
    # 450048418
    # 450048420
    # 450048421
    #
    # % tail -3 sorted_school_id.txt
    # 450155055
    # 450155056
    # 450155057
    #
    # So, let's start the new school ids for the daycares at 451000000
    new_school_id_start = 451000000
    daycare_max_size = 500 + int(np.random.normal(250.0, 50.0))
    if daycare_max_size < 0:
        daycare_max_size = 500
    return location.DaycareCenter(str(daycare_id + new_school_id_start), latitude, longitude, 0, 0, 0, daycare_max_size)
    
    
def _get_person_info_as_tuple(id: str, hh_id: str, school_id: str, age: int, latitude: float, longitude: float) -> List[Tuple]:
    
    if age < 6 and school_id == 'X':
        household = location.Place(hh_id, latitude, longitude)
        return (id, age, household)
    else:
        return (id, age, None)
        
def _order_daycare_center_list_using_gravity_model(check_location: location.Place, daycare_center_list: List[location.DaycareCenter]) -> List[location.DaycareCenter]:
    
    index_list = list(range(0, len(daycare_center_list)))
    return_index_list = []
    return_list = []
    
    done = False
    while not done:
        probability_list = []

        total_prob = 0.0
        for index in index_list:
            distance = daycare_center_list[index].get_distance_to_here(check_location)
            openings = daycare_center_list[index].max_size - (daycare_center_list[index].infant_count +
                                                              daycare_center_list[index].toddler_count +
                                                              daycare_center_list[index].preschooler_count)
            if distance > 0 and openings > 0:
                prob = openings / distance ** 2
                total_prob += prob
                probability_list.append(prob)
            else:
                probability_list.append(0.0)
    
        # Normalize the list
        if total_prob > 0:
            probability_list = list(map(lambda x: x / total_prob, probability_list))
            temp_list = []
            cum_prob = 0.0
            for prob in probability_list:
                cum_prob += prob
                temp_list.append(cum_prob)
            probability_list = temp_list
        else:
            done = True
            
        # Try to find a DaycareCenter based on probability
        if not done:
            is_index_found = False
            while not is_index_found:
                j = 0
                random_draw = random.uniform(0, 1)
                for prob in probability_list:
                    if random_draw < prob:
                        return_index_list.append(index_list.pop(j))
                        is_index_found = True
                        break
                    j += 1
                if not is_index_found:
                    print('Error in finding the probability')
                    sys.exit(1)
            done = len(index_list) == 0

    for index in return_index_list:
        return_list.append(daycare_center_list[index])
      
    return return_list


if __name__ == '__main__':
    main()
