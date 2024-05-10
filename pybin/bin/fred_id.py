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

    $ fred_id.py mykey
    df19c9f1-ea7b-4350-bb46-0ba308a5ccc8
    '''

    # Creates and sets up the parser
    parser = argparse.ArgumentParser(
        description='Prints the ID of the Job associated with the key.',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('key', help='the FRED key to look up')
    parser.add_argument('--loglevel',
                        default='ERROR',
                        choices=constants.LOGGING_LEVELS,
                        help='set the logging level of this script')

    # Parses the arguments
    args = parser.parse_args()
    fred_key = args.key
    loglevel = args.loglevel

    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)

    result = fred_id(fred_key)
    sys.exit(result)


def fred_id(fred_key: str) -> Literal[0, 2]:
    '''
    Prints the ID of the FRED Job associated with the key.
    
    Parameters
    ----------
    fred_key : str
        The FRED Job key to look up
    
    Returns
    -------
    Literal[0, 2]
        The exit status
    '''
    # Find the ID
    fred_job_id = None
    try:
        fred_job_id = fredutil.get_fred_id(fred_key)
    except (frederr.FredHomeUnsetError, FileNotFoundError, IsADirectoryError,
            NotADirectoryError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Print the ID if the key was found
    if fred_job_id == None:
        print(f'Key [{fred_key}] was not found.')
    else:
        print(fred_job_id)
    return constants.EXT_CD_NRM


if __name__ == '__main__':
    main()
