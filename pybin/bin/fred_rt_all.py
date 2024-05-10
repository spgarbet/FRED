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

from fredpy.util import constants

from fred_rt import fred_rt


def main():

    usage = '''
    Typical usage example:
    
    $ fred_rt_all.py
    Running FRED regression test: base.
    Please wait ...
    
    ...
    
    All regression tests passed.
    '''

    # Create and setup the parser
    parser = argparse.ArgumentParser(
        description='Runs all regression tests.',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--loglevel',
                        default='ERROR',
                        choices=constants.LOGGING_LEVELS,
                        help='set the logging level of this script')

    # Parse the arguments
    args = parser.parse_args()
    loglevel = args.loglevel

    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)

    result = fred_rt_all()
    sys.exit(result)


def fred_rt_all() -> Literal[0, 2]:
    '''
    Runs all regression tests by simply calling fred_rt on each of the tests:
    base, antivirals, and vaccine. An overview of the tests is printed at the end.
    
    Returns
    -------
    Literal[0, 2]
        The exit status
    '''
    # Run regression tests
    base_result = fred_rt('base')
    print()
    antivirals_result = fred_rt('antivirals')
    print()
    vaccine_result = fred_rt('vaccine')
    print()

    # Get failed tests by filtering for an error result
    results = [base_result, antivirals_result, vaccine_result]
    failed_results = [
        result for result in results if result == constants.EXT_CD_ERR
    ]

    # If elements exist in the failed results list, then detail how many
    if failed_results:
        print(f'{len(failed_results)} regression test(s) failed.')
        return constants.EXT_CD_ERR
    else:
        print('All regression tests passed.')
        return constants.EXT_CD_NRM


if __name__ == '__main__':
    main()
