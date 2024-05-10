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

    $ fred_log.py mykey
    STARTED: Thu Jul 15 15:13:33 2021
    FINISHED: Thu Jul 15 15:13:39 2021

    Last 10 lines in LOG file [{log file path}]:
    day 152 report population took 0.000002 seconds
    day 152 maxrss 140296
    ...
    '''

    # Creates and sets up the parser
    parser = argparse.ArgumentParser(
        description='Prints details about the given FRED Job.',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('key', help='the FRED key to look up')
    parser.add_argument('-r',
                        '--run',
                        default=1,
                        type=int,
                        help='the run number get the log from (default=1)')
    parser.add_argument(
        '-n',
        '--number',
        default=10,
        type=int,
        help='display the last n lines of the LOG file (default=10)')
    parser.add_argument('--loglevel',
                        default='ERROR',
                        choices=constants.LOGGING_LEVELS,
                        help='set the logging level of this script')

    # Parses the arguments
    args = parser.parse_args()
    fred_key = args.key
    run_num = args.run
    num_lines = args.number
    loglevel = args.loglevel

    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)

    result = fred_log(fred_key, run_num, num_lines)
    sys.exit(result)


def fred_log(fred_key: str, run_num: str, num_lines: int) -> Literal[0, 2]:
    '''
    Prints details about the given FRED Job. Will print the start and finish time 
    from the META directory, as well as lines from the end of the LOG file in the 
    specified RUN subdirectory.
    
    Parameters
    ----------
    fred_key : str
        The FRED Job key to look up
    run_num : str
        The number of the RUN subdirectory to get the LOG file from
    num_lines : int
        The number of lines to read from the end of the LOG file
    
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

    # Print the start and finish data from META directory
    try:
        start = fredutil.get_meta_file_data(fred_job_meta_dir_str, 'START')
        finish = fredutil.get_meta_file_data(fred_job_meta_dir_str, 'FINISHED')
    except (FileNotFoundError, IsADirectoryError,
            frederr.FredFileSizeError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    print(f'STARTED: {start}')
    print(f'FINISHED: {finish}\n')

    # Find the OUT directory
    fred_job_out_dir_str = None
    try:
        fred_job_out_dir_str = fredutil.get_fred_job_out_dir_str(fred_key)
    except (FileNotFoundError, NotADirectoryError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Find the LOG file if one was created
    fred_log_file_path = Path(fred_job_out_dir_str, f'RUN{run_num}', 'LOG')
    try:
        fred_log_file_str = fredutil.get_file_str_from_path(fred_log_file_path)
    except (FileNotFoundError, IsADirectoryError) as e:
        print(f'No LOG file found in [{fred_log_file_path.parent}].')
        return constants.EXT_CD_NRM

    read_log_lines(fred_log_file_str, num_lines)
    return constants.EXT_CD_NRM


def read_log_lines(log_file_str: str, num_lines: int) -> None:
    '''
    Prints the last n lines of the specified log file.
    
    Parameters
    ----------
    log_file_str : str
        The string representation of the full path to the LOG file
    num_lines : int
        The number of lines to read off the end of the LOG file
    '''

    lines = []
    with open(log_file_str) as file:
        lines = file.readlines()

    if num_lines > len(lines):
        num_lines = len(lines)

    lines_to_read = lines[-num_lines:]
    print(f'Last {num_lines} lines in LOG file [{log_file_str}]:')
    for line in lines_to_read:
        print(line.rstrip())


if __name__ == '__main__':
    main()