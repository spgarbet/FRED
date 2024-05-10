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
import multiprocessing as mp
import os
import shutil
import socket
import uuid
from typing import Literal, List

from fredpy.util import fredutil
from fredpy.util import constants
from fredpy import frederr

from fred_compile import fred_compile
from fred_make_csv_files import fred_make_csv_files
from fred_run import fred_run


def main():

    usage = '''
    Typical usage example:
    
    $ fred_job.py paramfile.fred -k mykey
    Compiling /home/mojadem/PHDL/FRED/inf_baseline.fred ...
    No errors found.
    
    Key: mykey, ID: 1
    ...
    '''

    # Create and set up the parser
    parser = argparse.ArgumentParser(
        description='Runs a FRED simulation as a Job.',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('file',
                        help='the .fred parameter file to use for the Job',
                        metavar='params.fred')
    parser.add_argument('-k',
                        '--key',
                        help='the name to use as the key for this FRED Job')
    parser.add_argument('-s',
                        '--start_run_number',
                        default=1,
                        type=int,
                        help='the first run number to complete (default=1)')
    parser.add_argument('-n',
                        '--end_run_number',
                        default=1,
                        type=int,
                        help='the last run number to complete (default=1)')
    parser.add_argument(
        '-m',
        '--multi_process_count',
        default=1,
        type=int,
        help='the number of runs to execute in parallel (default=1)')
    parser.add_argument('--nolog',
                        action='store_true',
                        help='do not store a LOG output of the run')
    parser.add_argument('--loglevel',
                        default='ERROR',
                        choices=constants.LOGGING_LEVELS,
                        help='set the logging level of this script')

    # Parse the arguments
    args = parser.parse_args()
    param_file = args.file
    key = args.key
    start_run = args.start_run_number
    end_run = args.end_run_number
    multi_process_count = args.multi_process_count
    no_log = args.nolog
    loglevel = args.loglevel

    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)

    # End run number must be greater than or equal to start run number
    if end_run < start_run:
        end_run = start_run

    result = fred_job(param_file, start_run, end_run, multi_process_count, key,
                      no_log)
    sys.exit(result)


def fred_job(param_file: str, start_run: int, end_run: int,
             multi_process_count: int, key: str,
             no_log: bool) -> Literal[0, 2]:
    '''
    Completes a FRED Job run given a parameter file. This will setup the 
    RESULTS directory if needed, create and setup a Job directory for the Job, run
    the FRED simulation, and make files that detail the output of the simulation.
    
    The starting run and ending run can be specified through the start_run and end_run
    parameters. Because the FRED simulation's random seed is rooted in the run number,
    you can rerun a specific run to receive the same results, assuming the other factors 
    are kept constant.
    
    Parameters
    ----------
    param_file : str
        The path to the .fred parameter file to use for the simulation
    start_run : int
        The starting run number to simulate
    end_run : int
        The ending run number to simulate
    multi_process_count : int
        The number of runs to execute in parallel
    key : str
        If supplied, the custom key name for the FRED Job; by default, the key 
        will be equal to the Job ID
    no_log : bool
        If true, will write the output to /dev/null rather than a LOG file
    Returns
    -------
    Literal[0, 2]
        The exit status
    '''

    # Get the FRED HOME directory
    try:
        fred_home_dir_str = fredutil.get_fred_home_dir_str()
    except frederr.FredHomeUnsetError as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Get the param file
    param_file_path = Path(param_file).resolve()
    try:
        param_file_str = fredutil.get_file_str_from_path(param_file_path)
    except (FileNotFoundError, IsADirectoryError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Compile the parameter file
    result = _compile_param_file(param_file)
    if result == constants.EXT_CD_ERR:
        return result

    # Change to param files directory to find included files
    cwd = Path.cwd()
    os.chdir(param_file_path.parent)

    # Find included files
    try:
        included_files = _get_included_files(param_file_str)
    except (FileNotFoundError, IsADirectoryError) as e:
        logging.error(f'Included file not found; {e}')
        return constants.EXT_CD_ERR

    os.chdir(cwd)

    # TODO: add parallel mode / threads support

    # We will be working in the FRED HOME directory from now on
    home_dir = Path(fred_home_dir_str)

    # Lock the FRED HOME directory
    sem_file_name = fredutil.get_fred_semaphore_file_name()
    sem_file = open(Path(home_dir, sem_file_name), 'w')

    try:
        fcntl.flock(sem_file, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError as e:
        logging.error('Semaphore file is currently locked by another process.')
        return constants.EXT_CD_ERR

    # Get RESULTS directory as Path
    results_dir = Path(fredutil.get_fred_results_dir_str())

    # Setup RESULTS directory
    try:
        _setup_results(results_dir)
    except OSError as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Check if key is already used if a key has been specified
    if key:
        if not _valid_key(key, results_dir):
            logging.error(f'Key [{key}] already in use.')
            return constants.EXT_CD_ERR

    # TODO: add force

    # TODO: add cache

    # Get ID for Job, which will be a random UUID
    id = uuid.uuid4()

    # Key is set to ID if there has not been a custom key specified
    if not key:
        key = id

    # Add key id pair to KEY file
    with open(Path(results_dir, 'KEY'), 'a') as key_file:
        key_file.write(f'{key} {id}\n')

    # Unlock the FRED HOME directory
    fcntl.flock(sem_file, fcntl.LOCK_UN)
    sem_file.close()

    print(f'\nKEY: {key}, ID: {id}\n')

    # Set up Job directory
    job_dir = Path(results_dir, 'JOB', f'{id}')
    try:
        _setup_job_dir(job_dir)
    except OSError as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Set up META files
    meta_dir = Path(job_dir, 'META')
    num_runs = end_run - start_run + 1
    _setup_meta_files(meta_dir, key, num_runs)

    # Copy param, version, and config file into META directory
    try:
        shutil.copy(param_file_str, Path(meta_dir, 'PARAMS'))
        shutil.copy(Path(fred_home_dir_str, 'data', 'config.fred'),
                    Path(meta_dir, 'CONFIG.FRED'))
        shutil.copy(Path(fred_home_dir_str, 'VERSION'),
                    Path(meta_dir, 'VERSION'))
    except FileNotFoundError as e:
        logging.errror(e)
        return constants.EXT_CD_ERR

    # Copy param and config file into WORK directory
    work_dir = Path(job_dir, 'WORK')
    shutil.copy(param_file_str, Path(work_dir, param_file_path.name))
    shutil.copy(Path(meta_dir, 'CONFIG.FRED'), Path(work_dir, 'config.fred'))

    # Copy included files into WORK directory
    for file in included_files:
        shutil.copy(file, Path(work_dir, Path(file).name))

    # Update META STATUS
    with open(Path(meta_dir, 'STATUS'), 'w') as status_file:
        status_file.write('RUNNING\n')

    # Run FRED
    out_dir = Path(job_dir, 'OUT')
    failure = False

    if multi_process_count > 1:
        # subtract 1 for this process
        max_process = mp.cpu_count() - 1
        print(f'FRED Job is multiprocess with {multi_process_count} cores')
        pool = mp.Pool(processes=multi_process_count)
        results = pool.starmap(fred_run,
                               ((str(Path(work_dir, param_file_path.name)),
                                 str(out_dir), r, r, False, no_log)
                                for r in range(start_run, end_run + 1)))
        pool.terminate()

        if constants.EXT_CD_ERR in results:
            failure = True

    else:

        result = fred_run(param_file=str(Path(work_dir, param_file_path.name)),
                          directory=str(out_dir),
                          start_run=start_run,
                          end_run=end_run,
                          compile=False,
                          no_log=no_log)

        if result == constants.EXT_CD_ERR:
            failure = True

    if failure:
        # Mark META status as failed
        with open(Path(meta_dir, 'STATUS'), 'w') as status_file:
            status_file.write('FAILED\n')
        return constants.EXT_CD_ERR

    print('FRED runs completed.\n')

    # Record META data from first LOG file
    log_file_path = Path(out_dir, f'RUN{start_run}', 'LOG')
    _record_meta_data(log_file_path, meta_dir)

    # Remove the one existing LOG if no_log was specified
    # fred_run will always create at least one LOG file for the first RUN
    # so that we can record the meta data from it
    if no_log:
        os.remove(log_file_path)

    # Make csv files
    result = fred_make_csv_files(str(out_dir))
    if result == constants.EXT_CD_ERR:
        return result

    # Update META files to finished
    with open(Path(meta_dir, 'STATUS'), 'w') as status_file:
        status_file.write('FINISHED\n')

    with open(Path(meta_dir, 'FINISHED'), 'w') as finished_file:
        finished_file.write(fredutil.get_local_timestamp())

    print(f'FRED Job completed. View results at {job_dir}.')


def _compile_param_file(param_file: str):
    '''
    Compile the given parameter file before continuing with the FRED Job. The 
    compiler directory is removed after compiling.
    
    Parameters
    ----------
    param_file : str
        The path to the .fred parameter file to compile
    
    Returns
    -------
    Literal[0, 2]
        The exit status of fred_compile
    '''
    # Make a temporary directory for compiler using process ID
    temp_dir = f'.temp_dir-{os.getpid()}'
    os.mkdir(temp_dir)

    # Compile the .fred file and remove the compiler directory
    result = fred_compile(param_file=param_file, directory=temp_dir)
    shutil.rmtree(temp_dir)
    return result


def _get_included_files(param_file: str) -> List[str]:
    '''
    Gets a list of strings representing the full paths to the files included 
    by the given parameter file. These will be specified in lines of the parameter
    file starting with 'include ' and followed by the path to the included file, 
    relative to the original parameter file.
    
    Parameters
    ----------
    param_file : str
        The string representation of the full path to the parameter file
    
    Returns
    -------
    List[str]
        A list of strings representing the full path to each included file
    
    Raises
    ------
    FileNotFoundError
        If one of the included files can not be found relative to the param file
    IsADirectoryError
        If one of the included files is actually a directory
    '''
    included_files = []

    lines = []
    with open(param_file, 'r') as file:
        lines = file.readlines()

    # The space is so lines like 'include_condition = ...' aren't counted
    include_lines = [line for line in lines if line.startswith('include ')]

    for line in include_lines:
        # The included file name will be the second entry in the line, after 'include'
        include_file = line.split()[1]

        # Get full path, then convert back to string, validating the file
        include_file_path = Path(include_file).resolve()
        include_file_str = fredutil.get_file_str_from_path(include_file_path)
        included_files.append(include_file_str)

    return included_files


def _setup_results(results_path: Path) -> None:
    '''
    Sets up the RESULTS directory given the RESULTS path. This will create the 
    results directory along with a JOB subdirectory and a KEY file, if any have 
    not yet been created.
    
    Parameters
    ----------
    results_path : pathlib.Path
        A Path object representing the full path to the RESULTS directory
    
    Raises
    ------
    OSError
        If an error occurs in the creation of any of the files or folders
    '''

    fredutil.create_dir(results_path)

    job_path = Path(results_path, 'JOB')
    fredutil.create_dir(job_path)

    key_path = Path(results_path, 'KEY')
    if key_path.exists() and key_path.is_dir():
        raise OSError(
            f'RESULTS/KEY file could not be created; [{key_path}] is a directory.'
        )
    elif not key_path.exists():
        with open(key_path, 'x'):
            pass


def _valid_key(key: str, results_dir: Path) -> bool:
    '''
    Checks that the given key is not already in use by searching the existing KEY
    file in the specified results directory.
    
    Parameters
    ----------
    key : str
        The key to check for
    results_dir : pathlib.Path
        A Path object representing the full path to the results directory in which
        the KEY file will be searched
    
    Returns
    -------
    bool
        If the key is valid (False if the key is already in use)
    '''

    with open(Path(results_dir, 'KEY'), 'r') as key_file:
        for line in key_file:
            # The key entry will be the first field in the line
            key_entry = line.split()[0]

            # TODO: add force

            if key_entry == key:
                return False

    return True


def _setup_job_dir(job_dir_path: Path) -> None:
    '''
    Sets up the Job directory given the Job directory path. This will create the 
    Job directory along with the necessary subdirectories: a WORK, OUT, and META 
    subdirectory, if any have not yet been created.
    
    Parameters
    ----------
    job_dir_path : pathlib.Path
        A Path object representing the full path to the Job directory
    
    Raises
    ------
    OSError
        If an error occurs in the creation of any of the directories folders
    '''

    fredutil.create_dir(job_dir_path)

    work_path = Path(job_dir_path, 'WORK')
    fredutil.create_dir(work_path)

    out_path = Path(job_dir_path, 'OUT')
    fredutil.create_dir(out_path)

    meta_path = Path(job_dir_path, 'META')
    fredutil.create_dir(meta_path)


def _setup_meta_files(meta_path: Path, key: str, runs: int) -> None:
    '''
    Sets up initial contents of META files given the META directory path. This will 
    create a STATUS, START, RUNS, KEY, USER, WHERE, and LOG file, overwriting any that 
    already exist.
    
    Parameters
    ----------
    meta_path : pathlib.Path
        A Path object representing the full path to the META directory
    key : str
        The key used for the Job; will be recorded in META/KEY
    runs : int
        The number of runs to record into META/RUNS
    '''

    with open(Path(meta_path, 'STATUS'), 'w') as status_file:
        status_file.write('SETUP\n')

    timestamp = fredutil.get_local_timestamp()
    with open(Path(meta_path, 'START'), 'w') as start_file:
        start_file.write(f'{timestamp}\n')

    with open(Path(meta_path, 'RUNS'), 'w') as runs_file:
        runs_file.write(f'{runs}\n')

    with open(Path(meta_path, 'KEY'), 'w') as key_file:
        key_file.write(f'{key}\n')

    user = os.environ.get('USER')
    with open(Path(meta_path, 'USER'), 'w') as user_file:
        user_file.write(f'{user}\n')

    hostname = socket.gethostname()
    with open(Path(meta_path, 'WHERE'), 'w') as where_file:
        where_file.write(f'{hostname}\n')

    with open(Path(meta_path, 'LOG'), 'w'):
        pass


def _record_meta_data(log_file: Path, meta_dir: Path) -> None:
    '''
    Record data from the given log file into corresponding META files in
    the given META directory. This will create a POPULATION, FIPS, POPSIZE, 
    and DENSITY file with data from the log file.
    
    Parameters
    ----------
    log_file : pathlib.Path
        A Path object representing the full path to the LOG file
    meta_dir : pathlib.Path
        A Path object representing the full path to the META directory
    '''

    lines = []
    with open(log_file, 'r') as log:
        lines = log.readlines()

    for line in lines:
        line = line.rstrip()

        if 'POPULATION_FILE' in line:
            # Population will be a path on the second entry on this line
            population = line.split()[1]
            with open(Path(meta_dir, 'POPULATION'), 'a') as population_file:
                population_file.write(f'{population}\n')

            # Fips code will be the population path's last directory
            fips = Path(population).name
            with open(Path(meta_dir, 'FIPS'), 'a') as fips_file:
                fips_file.write(f'{fips}\n')

        if 'convex_density' in line:
            # Popsize will be the fourth entry of this line
            popsize = line.split()[3]
            with open(Path(meta_dir, 'POPSIZE'), 'w') as popsize_file:
                popsize_file.write(f'{popsize}\n')

            # Density will be the eleventh entry of this line
            density = line.split()[10]
            with open(Path(meta_dir, 'DENSITY'), 'w') as density_file:
                density_file.write(f'{density}\n')


if __name__ == '__main__':
    main()
