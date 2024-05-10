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
import fcntl
import logging
import os
import shutil
from typing import Literal

from fredpy.util import fredutil
from fredpy.util import constants
from fredpy import frederr


def main():

    usage = '''
    Typical usage example:

    $ fred_delete.py mykey
    You are about to delete FRED_RESULTS/JOB/{id}. This cannot be undone.
    Proceed? y/n [n] y
    FRED_RESULTS/JOB/{id} deleted.
    '''

    # Creates and sets up the parser
    parser = argparse.ArgumentParser(
        description='Deletes the given FRED Job directory and key.',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('key', help='the FRED key to look up')
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
    fred_key = args.key
    assume_yes = args.yes
    loglevel = args.loglevel

    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)

    result = fred_delete(fred_key, assume_yes)
    sys.exit(result)


def fred_delete(fred_key: str, assume_yes: bool) -> Literal[0, 2]:
    '''
    Deletes the FRED Job that is associated with the key. Both the Job's directory 
    and entry in the KEY file will be removed.
    
    Parameters
    ----------
    fred_key : str
        The key of the FRED Job to delete
    assume_yes : str
        Assume yes to the confirmation prompt
    
    Returns
    -------
    Literal[0, 2]
        The exit status
    '''

    # Find the Job directory
    fred_job_dir_str = None
    try:
        fred_job_dir_str = fredutil.get_fred_job_dir_str(fred_key)
    except (frederr.FredHomeUnsetError, FileNotFoundError, IsADirectoryError,
            NotADirectoryError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    if fred_job_dir_str == None:
        print(f'Key [{fred_key}] was not found.')
        return constants.EXT_CD_NRM

    # Confirms decision
    if not assume_yes:
        print(f'You are about to delete {fred_job_dir_str}. This cannot be undone.')
        response = input('Proceed? y/n [n] ').lower()

        if response != 'y' and response != 'yes':
            print('cancelled')
            return constants.EXT_CD_NRM

    # Remove the key from the KEY file
    try:
        delete_key(fred_key)
    except BlockingIOError:
        logging.error('Results are currently locked by another process.')
        return constants.EXT_CD_ERR
    except ValueError as e:
        logging.error(f'Bad data in KEY file; {e}')
        return constants.EXT_CD_ERR

    # Remove the Job directory
    try:
        shutil.rmtree(fred_job_dir_str)
        print(f'{fred_job_dir_str} deleted.')
        return constants.EXT_CD_NRM
    except (FileNotFoundError, NotADirectoryError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR


def delete_key(key):
    ''' 
    Deletes the line corresponding to the specified key from the KEY file 
    in FRED_RESULTS.
    
    Parameters
    ----------
    key : str
        The FRED key to delete from the KEY file.
    
    Raises
    ------
    BlockingIOError
        If the semaphore file is currently locked by another process
    ValueError
        If the values in a line of the KEY file are invalid
    '''

    fred_results_dir_str = fredutil.get_fred_results_dir_str()
    fred_key_file_path = Path(fred_results_dir_str, 'KEY')
    fred_key_file_str = str(fred_key_file_path.resolve())

    # Locks the semaphore file until operation is complete
    sem_file_name = fredutil.get_fred_semaphore_file_name()
    fred_sem_file_path = Path(fred_results_dir_str, sem_file_name)
    sem_file = open(fred_sem_file_path,
                    'w')  # w flag allows file to be created if not found
    fcntl.flock(sem_file, fcntl.LOCK_EX | fcntl.LOCK_NB)

    # Use pid in tmp file name to ensure it is not a file used by another process
    tmp_file_str = f'{fred_results_dir_str}/tmp{os.getpid()}'

    with open(fred_key_file_str, 'r') as old_key_file:
        with open(tmp_file_str, 'w') as new_key_file:
            for line in old_key_file:
                # Key entry will be first the first field
                key_entry = line.split()[0]
                if key_entry != key:
                    new_key_file.write(line)

    os.replace(tmp_file_str, fred_key_file_str)

    fcntl.flock(sem_file, fcntl.LOCK_UN)
    sem_file.close()


if __name__ == '__main__':
    main()
