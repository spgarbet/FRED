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
import shutil
from typing import Literal

from fredpy.util import fredutil
from fredpy.util import constants
from fredpy import frederr


def main():

    usage = '''
    Typical usage example:
    
    $ fred_clear_all_results.py
    You are about to delete FRED_RESULTS. This cannot be undone.
    Proceed? y/n [n] y
    FRED_RESULTS deleted
    '''

    # Creates and sets up the parser
    parser = argparse.ArgumentParser(
        description='Clears the FRED RESULTS directory.',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('-y',
                        '--yes',
                        action='store_true',
                        help='assumes yes to prompt')
    parser.add_argument('--loglevel',
                        default='ERROR',
                        choices=constants.LOGGING_LEVELS,
                        help='set the logging level of this script')

    # Parses the arguments
    args = parser.parse_args()
    assume_yes = args.yes
    loglevel = args.loglevel

    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)

    result = fred_clear_all_results(assume_yes)
    sys.exit(result)


def fred_clear_all_results(assume_yes: bool) -> Literal[0, 2]:
    '''
    Deletes the FRED RESULTS directory.
    
    Parameters
    ----------
    assume_yes : bool
        Assumes yes to the confirmation prompt
    
    Returns
    -------
    Literal[0, 2]
        The exit status
    '''
    # Find the RESULTS directory
    fred_results_dir_str = None
    try:
        fred_results_dir_str = fredutil.get_fred_results_dir_str()
    except frederr.FredHomeUnsetError as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Confirms decision
    if not assume_yes:
        print(
            f'You are about to delete {fred_results_dir_str}. This cannot be undone.'
        )
        response = input('Proceed? y/n [n] ').lower()

        if response != 'y' and response != 'yes':
            print('cancelled')
            return constants.EXT_CD_NRM

    # Remove the RESULTS directory
    try:
        shutil.rmtree(fred_results_dir_str)
        print(f'{fred_results_dir_str} deleted.')
        return constants.EXT_CD_NRM
    except (FileNotFoundError, NotADirectoryError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR


if __name__ == '__main__':
    main()