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

    $ fred_get_fips.py Pitts
    Pittsfield_MA 25003
    Pittsburg_County_OK 40121
    Pittsburgh_PA 42003 42005 42007 42019 42051 42125 42129
    Pittsylvania_County_VA 51143

    $ fred_get_fips.py Pitts -s
    Pittsfield_MA 25003
    Pittsburg_County_OK 40121
    Pittsylvania_County_VA 51143

    $ fred_get_fips.py 'Pitts.*County'
    Pittsburg_County_OK 40121
    Pittsylvania_County_VA 51143

    $ fred_get_fips.py 'Pitts.*[MO]'
    Pittsfield_MA 25003
    Pittsburg_County_OK 40121

    $ fred_get_fips.py ^PA
    PA 42001 42003 42005 42007 42009 42011 ...
    '''

    # Creates and sets up the parser
    parser = argparse.ArgumentParser(
        description=
        'Prints locations matching the regex pattern as well as their associated fips code.',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('pattern', help='the location pattern to search')
    parser.add_argument('-s',
                        '--single',
                        action='store_true',
                        help='return only locations with a single fips code')
    parser.add_argument('--loglevel',
                        default='ERROR',
                        choices=constants.LOGGING_LEVELS,
                        help='set the logging level of this script')

    # Parses the arguments
    args = parser.parse_args()
    pattern = args.pattern
    single = args.single
    loglevel = args.loglevel

    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)

    result = fred_get_fips(pattern, single)
    sys.exit(result)


def fred_get_fips(pattern: str, single: bool) -> Literal[0, 2]:
    '''
    Prints locations matching the regex pattern as well as their associated fips code.
    Fips data is stored in FRED_HOME/data/country/usa/locations.txt.
    
    Parameters
    ----------
    pattern : str
        The regex pattern to search for in the fips file
    single : bool
        If true, only searches locations with a single fips code entry
    
    Returns
    -------
    Literal[0, 2]
        The exit status
    '''
    # Find the FRED HOME directory
    try:
        fred_home_dir_str = fredutil.get_fred_home_dir_str()
    except frederr.FredHomeUnsetError as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Find the fips file
    fred_fips_file_path = Path(fred_home_dir_str, 'data', 'country', 'usa',
                               'locations.txt')
    try:
        fred_fips_file_str = fredutil.get_file_str_from_path(
            fred_fips_file_path)
    except (FileNotFoundError, IsADirectoryError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Search the fips file and print occurrences
    with open(fred_fips_file_str, 'r') as file:
        for line in file:
            # Entries will be the location name, followed by one or more fips codes
            entries = line.split(' ')
            if single and len(entries) > 2:
                continue
            if re.search(pattern, entries[0]):
                print(line.rstrip())

    return constants.EXT_CD_NRM


if __name__ == '__main__':
    main()