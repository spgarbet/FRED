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
import time
from typing import Literal

from fredpy.util import fredutil
from fredpy.util import constants
from fredpy import frederr


def main():

    usage = '''
    Typical usage examples:

    $ fred_status.py mykey
    FINISHED Thu Jul 22 18:26:13 2021

    $ fred_status.py mykey -s 2
    RUNNING RUN 1/3 Thu Jul 22 18:33:06 2021
    RUNNING RUN 2/3 Thu Jul 22 18:33:08 2021
    RUNNING RUN 3/3 Thu Jul 22 18:33:10 2021
    FINISHED RUN 3/3 Thu Jul 22 18:33:12 2021

    FINISHED Thu Jul 22 18:33:12 2021
    '''

    # Creates and sets up the parser
    parser = argparse.ArgumentParser(
        description='Prints the status of the given FRED Job.',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('key', help='the FRED key to look up')
    parser.add_argument(
        '-s',
        '--seconds',
        type=float,
        default=None,
        help=
        'repeatedly print status every n seconds until status is FINISHED (n > 0)'
    )
    parser.add_argument('--loglevel',
                        default='ERROR',
                        choices=constants.LOGGING_LEVELS,
                        help='set the logging level of this script')

    # Parses the arguments
    args = parser.parse_args()
    fred_key = args.key
    seconds = args.seconds
    loglevel = args.loglevel

    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)

    # Seconds must not be zero
    if seconds == 0:
        logging.error('Seconds argument must have a value greater than 0.')
        sys.exit(constants.EXT_CD_ERR)

    result = fred_status(fred_key, seconds)
    sys.exit(result)


def fred_status(fred_key: str, seconds: float) -> Literal[0, 2]:
    '''
    Prints the status of the FRED Job that is associated with the key. If seconds 
    are specified, it will continue to print the status until the Job is finished.
    
    Parameters
    ----------
    fred_key : str
        The FRED Job key to look up
    seconds : float
        If specified, prints status every n seconds until the Job is finished
    
    Returns
    -------
    Literal[0, 2]
        The exit status
    '''
    # Find the META directory
    fred_job_meta_dir_str = None
    try:
        fred_job_meta_dir_str = fredutil.get_fred_job_meta_dir_str(fred_key)
    except (frederr.FredHomeUnsetError, FileNotFoundError, IsADirectoryError,
            NotADirectoryError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    if fred_job_meta_dir_str == None:
        print(f'Key [{fred_key}] was not found.')
        return constants.EXT_CD_NRM

    # Get status from STATUS file
    try:
        status = fredutil.get_meta_file_data(fred_job_meta_dir_str, 'STATUS')
    except (FileNotFoundError, IsADirectoryError,
            frederr.FredFileSizeError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Print status

    if status == 'RUNNING':
        total_runs = fredutil.get_meta_file_data(fred_job_meta_dir_str, 'RUNS')

        while status != 'FINISHED':
            status = fredutil.get_meta_file_data(fred_job_meta_dir_str,
                                                 'STATUS')
            timestamp = fredutil.get_local_timestamp()
            cur_run = len(
                fredutil.get_run_dirs(
                    fredutil.get_fred_job_out_dir_str(fred_key)))

            print(f'{status} RUN {cur_run}/{total_runs} {timestamp}')

            if not seconds:  # exit while loop if not repeating
                break

            if status != 'FINISHED':
                time.sleep(seconds)

        print()

    elif status == 'FINISHED':
        try:
            timestamp = fredutil.get_meta_file_data(fred_job_meta_dir_str,
                                                    'FINISHED')
        except (FileNotFoundError, IsADirectoryError,
                frederr.FredFileSizeError) as e:
            logging.error(e)
            return constants.EXT_CD_ERR

        print(f'FINISHED {timestamp}')

    else:
        print(status)

    return constants.EXT_CD_NRM


if __name__ == '__main__':
    main()