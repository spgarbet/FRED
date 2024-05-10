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

    $ fred_csv.py mykey
    RUN1 - {out.csv file path}
               Date  EpiWeek  Popsize  ... 
    Day                                ... 
    0    2019-11-01  2019.44    45318  ... 
    1    2019-11-02  2019.44    45318  ...
    ...

    RUN2 - {out.csv file path}
               Date  EpiWeek  Popsize  ... 
    Day                                ... 
    0    2019-11-01  2019.44    45318  ... 
    1    2019-11-02  2019.44    45318  ... 
    ...
    '''

    # Creates and sets up the parser
    parser = argparse.ArgumentParser(
        description=
        'Print the contents of the out.csv file(s) for the given FRED Job.',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('key', help='the FRED key to look up')
    parser.add_argument(
        '-r',
        '--run',
        type=int,
        help=('the run directory to get the out.csv from '
              '(gets out.csv from all run directories by default)'))
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
    run_num = args.run
    verbose = args.verbose
    loglevel = args.loglevel

    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)

    result = fred_csv(fred_key, run_num, verbose)
    sys.exit(result)


def fred_csv(fred_key: str, run_num: str, verbose: bool) -> Literal[0, 2]:
    '''
    Prints the contents of the out.csv file for each RUN in the FRED Job that is 
    associated with the key. If a specific run number is specified, only the 
    contents of the out.csv for the corresponding RUN directory are printed.

    The out.csv file for a RUN will be stored at 
    FRED_RESULTS/JOB/{id}/OUT/RUN{run_number}/out.csv.
    
    Parameters
    ----------
    fred_key : str
        The FRED Job key to look up
    run_num : str
        The number of the RUN subdirectory to find the out.csv from (if None, 
        will use the out.csv file from all RUN subdirectories)
    verbose : bool
        If true, prints the full contents of the csv file, without truncating
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

    # Get run subdirectorie(s)
    run_dirs = fredutil.get_run_dirs(
        fred_job_out_dir_str) if not run_num else [f'RUN{run_num}']

    for dir in run_dirs:
        # Find the out.csv file
        run_out_file_path = Path(fred_job_out_dir_str, dir, 'out.csv')
        try:
            run_out_file_str = fredutil.get_file_str_from_path(
                run_out_file_path)
        except (FileNotFoundError, IsADirectoryError) as e:
            logging.error(e)
            return constants.EXT_CD_ERR

        # Read the out.csv file
        print(f'\n{dir} - {run_out_file_str}')
        df = pd.read_csv(run_out_file_str, index_col=0)
        print(df) if not verbose else print(df.to_string())

    return constants.EXT_CD_NRM


if __name__ == '__main__':
    main()