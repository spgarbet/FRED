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
###################################################################################################

import sys
from pathlib import Path

# Update the Python search Path
file = Path(__file__).resolve()
package_root_directory_path = Path(file.parents[1], 'src')
sys.path.append(str(package_root_directory_path.resolve()))

import argparse
import contextlib
import io
import logging
from typing import Literal

from fredpy.util import fredutil
from fredpy.util import constants
from fredpy import frederr

from fred_status import fred_status


def main():

    usage = '''
    Note: a blank KEY field means the key is identical to the ID
    
    Typical usage example:
    
    $ fred_jobs.py
    KEY: mykey  ID: 7af5d905-bc07-42d8-8cbb-a956688beb78  STATUS: FINISHED Wed Aug 04 15:05:33 2021
    KEY:        ID: c4a59f88-d08e-4f87-add3-db3f0b1d7a23  STATUS: FINISHED Wed Aug 04 15:06:08 2021
    KEY:   key  ID: 339c1254-3493-42ab-ad29-9cf9d20c0740  STATUS: RUNNING RUN 1/10 Wed Aug 04 15:06:45 2021
    '''

    # Creates and sets up the parser
    parser = argparse.ArgumentParser(
        description='Lists details about the Jobs stored in FRED RESULTS.',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--loglevel',
                        default='ERROR',
                        choices=constants.LOGGING_LEVELS,
                        help='set the logging level of this script')

    # Parses the arguments
    args = parser.parse_args()
    loglevel = args.loglevel

    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)

    result = fred_jobs()
    sys.exit(result)


def fred_jobs() -> Literal[0, 2]:
    '''
    List the FRED Jobs stored in the KEY file of FRED RESULTS.
    
    Returns
    -------
    Literal[0, 2]
        The exit status
    '''
    # Get the FRED RESULTS directory
    try:
        fred_results_dir_str = fredutil.get_fred_results_dir_str()
    except frederr.FredHomeUnsetError as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Find the KEY file
    fred_key_file_path = Path(fred_results_dir_str, 'KEY')

    # If KEY file does not exist, there are no Jobs stored
    if not fred_key_file_path.exists():
        print(f'No FRED Jobs stored in the KEY file [{fred_key_file_path}].')
        return constants.EXT_CD_NRM
    elif fred_key_file_path.is_dir():
        logging.error(
            f'Expected a KEY file at [{fred_key_file_path}], but found a directory.'
        )
        return constants.EXT_CD_ERR

    # Get lines from KEY file
    lines = []
    with open(fred_key_file_path, 'r') as key_file:
        lines = key_file.readlines()

    # Find longest unique key in KEY file for formatting
    unique_keys = [
        line for line in lines if line.split()[0] != line.split()[1]
    ]
    longest_key = 0
    if len(unique_keys) > 0:
        longest_key = max([len(line.split()[0]) for line in unique_keys])

    # Print KEY, ID, and STATUS
    for line in lines:
        key_entry, id_entry = line.split()

        # Get status of Job by capturing output of fred_status
        with contextlib.redirect_stdout(io.StringIO()) as fred_status_output:
            result = fred_status(fred_key=key_entry, seconds=None)
            if result == constants.EXT_CD_ERR:
                return result

        status = fred_status_output.getvalue().rstrip()

        # If key entry is the same as ID entry, leave key entry blank to avoid repetition
        if key_entry == id_entry:
            key_entry = ''

        # Format key entries to be right aligned to the longest key entry
        print(
            f'KEY: {key_entry : >{longest_key}}  ID: {id_entry}  STATUS: {status}'
        )

    return constants.EXT_CD_NRM


if __name__ == '__main__':
    main()