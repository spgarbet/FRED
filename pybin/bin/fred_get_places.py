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
from typing import Literal

from fredpy.util import fredutil
from fredpy.util import constants
from fredpy import frederr

try:
    import pandas as pd
except ImportError:
    print('This script requires the pandas module.')
    print(
        'Installation instructions can be found at https://pypi.org/project/pandas/.'
    )
    sys.exit(constants.EXT_CD_ERR)


def main():

    usage = '''
    Typical usage example:

    $ fred_get_places.py mykey
       Place count  Percent of total
    X            0              0.00
    H        20584             31.88
    N        21212             32.85
    S         5775              8.94
    C        10329             15.99
    W         2210              3.42
    O         4467              6.92
    '''

    # Creates and sets up the parser
    parser = argparse.ArgumentParser(
        description='Gets data from WHERE place files.',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('key', help='the FRED key to look up')
    parser.add_argument(
        '-r',
        '--run',
        type=int,
        help=('the run directory to get the place data from '
              '(gets place data from all run directories by default)'))
    parser.add_argument(
        '-w',
        '--weekly',
        action='store_true',
        help=('sets the script to use the WEEKLY csv file for the variable '
              '(uses the DAILY file by default)'))
    parser.add_argument('--loglevel',
                        default='ERROR',
                        choices=constants.LOGGING_LEVELS,
                        help='set the logging level of this script')

    # Parses the argumennts
    args = parser.parse_args()
    fred_key = args.key
    run_num = args.run
    frequency_dir = 'WEEKLY' if args.weekly else 'DAILY'
    loglevel = args.loglevel

    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)

    result = fred_get_places(fred_key, run_num, frequency_dir)
    sys.exit(result)


def fred_get_places(
        fred_key: str, run_num: str,
        frequency_dir: Literal['DAILY', 'WEEKLY']) -> Literal[0, 2]:
    '''
    Gets data from place files from the OUT directory in the FRED Job that is 
    associated with the key. These place files detail the number of agents who 
    entered the exposed state for the tracked condition in a specific location 
    each day / week. An example place file would be found at:
    FRED_HOME/RESULTS/JOB/{id}/OUT/RUN{run_number}/{frequency_dir}/WHERE.H.txt, 
    where the H represents the household location.

    Note: WHERE files will only be created if a WHERE condition is defined in the 
    Job input file. An example WHERE condition can be found at 
    FRED_HOME/calibration/influenza/WHERE.fred.
    
    Parameters
    ----------
    fred_key : str
        The FRED Job key to look up
    run_num : str
        The number of the RUN subdirectory to find place files in (if None, will 
        use place files in all RUN subdirectories)
    frequency_dir : Literal['DAILY', 'WEEKLY']
        The frequency directory to find the place files in (DAILY or WEEKLY)
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
        return constants.EXT_CD_NRMs

    # Get run subdirectories
    run_dirs = fredutil.get_run_dirs(
        fred_job_out_dir_str) if not run_num else [f'RUN{run_num}']

    # Get place data

    place_vars = ['X', 'H', 'N', 'S', 'C', 'W',
                  'O']  # place variables used in file names
    place_counts = []  # will store place counts for each place file
    total_counts = 0  # will track the total place counts regardless of place file
    place_aves = [
    ]  # will store the percent of total counts each place count makes up

    for var in place_vars:
        place_count = 0

        for dir in run_dirs:
            place_var_file_path = Path(fred_job_out_dir_str, dir,
                                       frequency_dir, f'WHERE.{var}.txt')

            try:
                place_count += get_count_from_place_file(place_var_file_path)
            except (FileNotFoundError, IsADirectoryError, ValueError) as e:
                logging.error(e)
                return constants.EXT_CD_ERR

        place_counts.append(place_count)
        total_counts += place_count

    for i in range(len(place_vars)):
        if total_counts > 0:
            place_aves.append(round(100 * place_counts[i] / total_counts, 2))
        else:
            place_aves.append(0)

    # Construct and print dataframe
    data = {'Place count': place_counts, 'Percent of total': place_aves}
    df = pd.DataFrame(data,
                      columns=['Place count', 'Percent of total'],
                      index=place_vars)
    print(df)
    return constants.EXT_CD_NRM


def get_count_from_place_file(place_file_path: Path) -> int:
    '''
    Given a Path to a place file, ensure that that place file exists, and return the count from it.
    The count we are looking for is the last entry in the place file.
    
    Parameters
    ----------
    place_file_path : pathlib.Path
        A Path object representing the path to the place file
    
    Returns
    -------
    int
        The place file count
    
    Raises
    ------
    FileNotFoundError
        If the place file cannot be found
    IsADirectoryError
        If the place file is a directory
    ValueError
        If the count entry retrieved is not an int
    '''

    place_file_str = fredutil.get_file_str_from_path(place_file_path)

    lines = []
    with open(place_file_str) as file:
        lines = file.readlines()

    # Count will be the last entry in the last line
    count = lines[-1].split()[-1]

    try:
        return int(count)
    except ValueError:
        raise ValueError(
            f'Count value [{count}] retrieved from place file [{place_file_str}] is not an int.'
        )


if __name__ == '__main__':
    main()
