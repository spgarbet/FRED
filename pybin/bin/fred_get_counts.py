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


def main():

    usage = '''
    Typical usage example:

    $ fred_get_counts.py mykey INF.E
    0 10
    1 5
    2 1
    3 3
    ...
    '''

    # Creates and sets up the parser
    parser = argparse.ArgumentParser(
        description='Reads the specified variable file for the given FRED Job.',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('key', help='the FRED key to look up')
    parser.add_argument('var', help='the variable to get counts for')
    parser.add_argument('-r',
                        '--run',
                        default=1,
                        type=int,
                        help='the run number to get counts from (default=1)')
    parser.add_argument(
        '-w',
        '--weekly',
        action='store_true',
        help=('sets the script to use the WEEKLY text file for the variable '
              '(uses the DAILY file by default)'))
    parser.add_argument('--loglevel',
                        default='ERROR',
                        choices=constants.LOGGING_LEVELS,
                        help='set the logging level of this script')

    # Parses the arguments
    args = parser.parse_args()
    fred_key = args.key
    fred_var = args.var
    run_num = args.run
    frequency_dir = 'WEEKLY' if args.weekly else 'DAILY'
    loglevel = args.loglevel

    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)

    result = fred_get_counts(fred_key, fred_var, run_num, frequency_dir)
    sys.exit(result)


def fred_get_counts(
        fred_key: str, fred_var: str, run_num: str,
        frequency_dir: Literal['DAILY', 'WEEKLY']) -> Literal[0, 2]:
    '''
    Prints the contents of a specified variable file for the FRED Job that is associated 
    with the key for the specified run number.

    Variable files are stored in FRED_RESULTS/JOB/{id}/OUT/RUN{run_number}/{frequency_dir}.
    The frequency directory is DAILY by default, but can be set to WEEKLY through the 
    arguments. The variable name should be a name of a text file stored in that location,
    without the .txt file extension
    
    Parameters
    ----------
    fred_key : str
        The FRED Job key to look up
    fred_var : str
        The name of the variable file to get counts from, without the .txt file extension
    run_num : str
        The number of the RUN subdirectory to get counts from
    frequency_dir : Literal['DAILY', 'WEEKLY']
        The frequency subdirectory to get counts from (DAILY or WEEKLY)
    
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

    # Find the variable file
    fred_var_file_path = Path(fred_job_out_dir_str, f'RUN{run_num}',
                              frequency_dir, f'{fred_var}.txt')
    try:
        fred_var_file_str = fredutil.get_file_str_from_path(fred_var_file_path)
    except (FileNotFoundError, IsADirectoryError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Reads the variables file
    fredutil.read_file(fred_var_file_str)
    return constants.EXT_CD_NRM


if __name__ == '__main__':
    main()