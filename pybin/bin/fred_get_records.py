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
import logging
import re
from typing import Literal

from fredpy.util import fredutil
from fredpy.util import constants
from fredpy import frederr


def main():

    usage = '''
    Typical usage examples:
    
    $ fred_get_records.py mykey -i 643871
    HEALTH RECORD: 2019-12-17 6am day 46 person 643871 age 44 is EXPOSED to INF ... 
    HEALTH RECORD: 2019-12-19 8am day 48 person 643871 ENTERING state INF.Is ...

    $ fred_get_records.py mykey -i 643871 -s INF
    HEALTH RECORD: 2019-12-17 6am day 46 person 643871 age 44 is EXPOSED to INF ...

    $ fred_get_records.py mykey -p 'age 49$'
    ... age 78 is EXPOSED to INF at W-513950750-002 from person 1173687 age 49
    ... age 25 is EXPOSED to INF at W-513990956-055 from person 1064628 age 49
    ... age 44 is EXPOSED to INF at W-513988701-017 from person 248490 age 49
    '''

    # Creates and sets up the parser
    parser = argparse.ArgumentParser(
        description='Prints health records adhering to the specified filters.',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('key', help='the FRED key to look up')
    parser.add_argument('-r',
                        '--run',
                        default=1,
                        type=int,
                        help='the run number to get counts from (default=1)')
    parser.add_argument('-i',
                        '--id',
                        help='the person ID to filter results for')
    parser.add_argument('-s',
                        '--state',
                        help='the condition state to filter results for')
    parser.add_argument('-p',
                        '--pattern',
                        help='the regex pattern to filter results by')
    parser.add_argument('--loglevel',
                        default='ERROR',
                        choices=constants.LOGGING_LEVELS,
                        help='set the logging level of this script')

    # Parses the arguments
    args = parser.parse_args()
    fred_key = args.key
    run_num = args.run
    person_id = args.id
    state = args.state
    pattern = args.pattern
    loglevel = args.loglevel

    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)

    result = fred_get_records(fred_key, run_num, person_id, state, pattern)
    sys.exit(result)


def fred_get_records(fred_key: str, run_num: str, person_id: str, state: str,
                     pattern: str) -> Literal[0, 2]:
    '''
    Prints lines in the health records file for the FRED Job that is associated with 
    the key for the specified run number that adhere to the specified filters. Users 
    can filter results by a specific person ID, condition state, or general regex pattern.

    The health record file will be located at FRED_RESULTS/JOB/{id}/OUT/RUN{run_number}/health_records.txt,
    if health records are enabled in the Job's input file.
    
    Parameters
    ----------
    fred_key : str
        The FRED Job key to look up
    run_num : str
        The number of the RUN subdirectory to get health records from
    person_id : str
        If set, filter the records by a specific person ID
    state : str
        If set, filter the records by a specific condition state
    pattern : str
        If set, filter the records by a specific regex pattern
    
    Returns
    -------
    Literal[0, 2]
        The exit status
    '''
    # Find the OUT directory
    fred_job_out_dir_str = None
    try:
        fred_job_out_dir_str = fredutil.get_fred_job_out_dir_str(fred_key)
    except (frederr.FredHomeUnsetError, FileNotFoundError, IsADirectoryError,
            NotADirectoryError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    if fred_job_out_dir_str == None:
        print(f'Key [{fred_key}] was not found.')
        return constants.EXT_CD_NRM

    # Find the health records file
    fred_records_file_path = Path(fred_job_out_dir_str, f'RUN{run_num}',
                                  'health_records.txt')
    try:
        fred_records_file_str = fredutil.get_file_str_from_path(
            fred_records_file_path)
    except (FileNotFoundError, IsADirectoryError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Read and filter results from health records file
    with open(fred_records_file_str, 'r') as file:
        for line in file:

            if person_id and not re.search(f'person {person_id}', line):
                continue
            if state and not re.search(f'to {state}', line):
                continue
            if pattern and not re.search(pattern, line):
                continue

            print(line.rstrip())

    return constants.EXT_CD_NRM


if __name__ == '__main__':
    main()