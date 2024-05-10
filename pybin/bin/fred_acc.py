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
    from pandas.api.types import is_numeric_dtype
except ImportError:
    print('This script requires the pandas module.')
    print(
        'Installation instructions can be found at https://pypi.org/project/pandas/.'
    )
    sys.exit(constants.EXT_CD_ERR)


def main():

    usage = '''
    Typical usage examples:

    $ fred_acc.py mykey INF.E
        RUN1    ACC
    INDEX             
    0        10     10
    1         5     15
    2         5     20
    ...

    $ fred_acc.py mykey INF.E -r 2 -w
            RUN2   ACC
    INDEX              
    2019.44     7     7
    2019.45     8    15
    2019.46    24    39
    ...
    '''

    # Creates and sets up the parser
    parser = argparse.ArgumentParser(
        description=
        'Accumulates the specified csv file for the given FRED Job.',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('key', help='the FRED key to look up')
    parser.add_argument('var', help='the variable file name to accumulate')
    parser.add_argument(
        '-r',
        '--run',
        default=1,
        type=int,
        help='the run number field to get data from (default=1)')
    parser.add_argument(
        '-w',
        '--weekly',
        action='store_true',
        help=('sets the script to use the WEEKLY csv file for the variable '
              '(uses the DAILY file by default)'))
    parser.add_argument(
        '-v',
        '--verbose',
        action='store_true',
        help='does not truncate the data to fit the terminal window')
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
    verbose = args.verbose
    loglevel = args.loglevel

    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)

    result = fred_acc(fred_key, fred_var, run_num, frequency_dir, verbose)
    sys.exit(result)


def fred_acc(fred_key: str, fred_var: str, run_num: str,
             frequency_dir: Literal['DAILY',
                                    'WEEKLY'], verbose: bool) -> Literal[0, 2]:
    '''
    Finds the csv file for a FRED Job that is associated with the key and 
    accumulates the values in the field corresponding to the given run number, 
    which is 1 by default.

    The csv files are stored in FRED_RESULTS/JOB/{id}/OUT/PLOT/{frequency_dir}. 
    The frequency directory is DAILY by default, but can be set to WEEKLY through 
    the arguments. The variable name should be a prefix of a csv file stored in 
    that location.

    Parameters
    ----------
    fred_key : str
        The FRED Job key to look up
    fred_var : str
        The csv file to accumulate, without the .csv file extension
    run_num : str
        The RUN column to accumulate in the csv file
    frequency_dir : Literal['DAILY', 'WEEKLY']
        The frequency directory to find the csv file in (DAILY or WEEKLY)
    verbose : bool
        If true, prints the full contents of the accumulated csv file, without truncating
        
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
    fred_csv_file_path = Path(fred_job_out_dir_str, 'PLOT', frequency_dir,
                              f'{fred_var}.csv')
    try:
        fred_csv_file_str = fredutil.get_file_str_from_path(fred_csv_file_path)
    except (FileNotFoundError, IsADirectoryError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Accumulate and read csv file
    try:
        result = accumulate_csv_file(fred_csv_file_str, run_num)
        print(result) if not verbose else print(result.to_string())
        return constants.EXT_CD_NRM
    except ValueError:
        logging.error(f'Could not find data for RUN{run_num}.')
        return constants.EXT_CD_ERR
    except TypeError as e:
        logging.error(e)
        return constants.EXT_CD_ERR


def accumulate_csv_file(csv_file_str: str, run_num: str) -> pd.DataFrame:
    ''' Given a csv file, accumulates the values in the corresponding run number
    column. This will create a DataFrame with a row for each line in the csv file,
    each row containing a column for the corresponding line's index, run value for 
    the specified run number, and the accumulated value.
    
    Parameters
    ----------
    csv_file_str : str
        The string representation of the full path to the POPSIZE file
    run_num : str
        The RUN column to accumulate
    
    Returns
    -------
    DataFrame
        The dataframe corresponding to the file
    
    Raises
    ------
    ValueError
        If a column for the specified run number cannot be found in the file
    TypeError
        If an entry in the column has an invalid data type (not a float)
    '''

    run_col = f'RUN{run_num}'
    df = pd.read_csv(csv_file_str,
                     usecols=['INDEX', run_col],
                     index_col='INDEX')

    # Checks that entire column is valid numbers
    if not is_numeric_dtype(df[run_col]):
        # Reports the location of invalid rows
        invalid_rows = []

        for index, data in df.iterrows():
            try:
                float(data)
            except ValueError:
                invalid_rows.append(index)

        raise TypeError(
            f'Invalid data type at index {invalid_rows} in file [{csv_file_str}].'
        )

    # Accumulates the dataframe, then appends the accumulation column to the dataframe
    acc_df = df.cumsum()
    df['ACC'] = acc_df[run_col].tolist()
    return df


if __name__ == '__main__':
    main()
