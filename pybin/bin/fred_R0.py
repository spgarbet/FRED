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
    print('Installation instructions can be found at https://pandas.pydata.org/getting_started.html')
    sys.exit(constants.EXT_CD_ERR)


def main():

    usage = '''
    Typical usage examples:

    $ fred_R0.py mykey INF
           N  MIN  QUART1  MED  QUART3  MAX     MEAN      STD
    INDEX                                                    
    0      3    2     1.8  1.9     2.7    3  2.13333  0.49329


    $ fred_R0.py mykey StayHome
           N  MIN  QUART1  MED  QUART3  MAX  MEAN  STD
    INDEX                                             
    0      3    0     0.0  0.0     0.0    0   0.0  0.0
    '''

    # Creates and sets up the parser
    parser = argparse.ArgumentParser(
        description=
        'Gets initial fields from the specified csv file for the given FRED Job.',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('key', help='the FRED key to look up')
    parser.add_argument('var',
                        help='the PREFIX of the .RR variable to read from')
    parser.add_argument('--loglevel',
                        default='ERROR',
                        choices=constants.LOGGING_LEVELS,
                        help='set the logging level of this script')

    # Parses the arguments
    args = parser.parse_args()
    fred_key = args.key
    fred_var = args.var + '.RR'
    loglevel = args.loglevel

    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)

    result = fred_R0(fred_key, fred_var)
    sys.exit(result)


def fred_R0(fred_key: str, fred_var: str) -> Literal[0, 2]:
    '''
    Finds the .RR.csv file with the given variable prefix for a FRED Job and
    prints values from the initial fields.

    The csv files are stored in FRED_RESULTS/JOB/{id}/OUT/PLOT/DAILY. The 
    initial fields can be found on line 2. The variable name should be a prefix 
    of a .RR.csv file stored in that location.
    
    Parameters
    ----------
    fred_key : str
        The FRED Job key to look up
    fred_var : str
        The file name of the variable file to get data from. with the .RR appended 
        to the supplied prefix, but without the .csv file extension
    
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

    # Find the csv file
    fred_csv_file_path = Path(fred_job_out_dir_str, 'PLOT', 'DAILY',
                              f'{fred_var}.csv')
    try:
        fred_csv_file_str = fredutil.get_file_str_from_path(fred_csv_file_path)
    except (FileNotFoundError, IsADirectoryError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Get initial fields from csv file
    try:
        df = pd.read_csv(fred_csv_file_str,
                         index_col='INDEX',
                         usecols=[
                             'INDEX', 'N', 'MIN', 'QUART1', 'MED', 'QUART3',
                             'MAX', 'MEAN', 'STD'
                         ])
        print(f'Initial values for variable file [{fred_csv_file_str}]:\n')
        print(df.head(1))
        return constants.EXT_CD_NRM
    except ValueError as e:
        logging.error(f'Bad data in CSV file [{fred_csv_file_str}]; {e}')
        return constants.EXT_CD_ERR


if __name__ == '__main__':
    main()
