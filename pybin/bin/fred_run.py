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
import os
import shutil
import subprocess
from typing import Literal

from fredpy.util import fredutil
from fredpy.util import constants
from fredpy import frederr

from fred_compile import fred_compile


def main():

    usage = '''
    Note: if you are attempting to run a FRED simulation as a job, use fred_job.
    
    Typical usage example:
    
    $ fred_run.py paramfile.fred
    '''

    # Create and setup the parser
    parser = argparse.ArgumentParser(
        description='Runs a FRED simulation.',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('file',
                        help='the .fred parameter file to use for the run',
                        metavar='params.fred')
    parser.add_argument('directory',
                        help='the directory to store the output of the run')
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
    parser.add_argument('-c',
                        '--compile',
                        action='store_true',
                        help='compile the parameter file before running')
    parser.add_argument('--nolog',
                        action='store_true',
                        help='do not store a LOG output of the run')
    parser.add_argument('--loglevel',
                        default='ERROR',
                        choices=constants.LOGGING_LEVELS,
                        help='set the logging level of this script')

    # TODO: add threads

    # Parse the arguments
    args = parser.parse_args()
    param_file = args.file
    directory = args.directory
    start_run = args.start_run_number
    end_run = args.start_run_number
    compile = args.compile
    no_log = args.nolog
    loglevel = args.loglevel

    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)

    # End run number must be greater than or equal to start run number
    if end_run < start_run:
        end_run = start_run

    result = fred_run(param_file, directory, start_run, end_run, compile,
                      no_log)
    sys.exit(result)


def fred_run(param_file: str, directory: str, start_run: int, end_run: int,
             compile: bool, no_log: bool) -> Literal[0, 2]:
    '''
    Runs the provided .fred parameter file, storing results in the 
    specified directory. If compile is specified, the parameter is first compiled, 
    throwing an error if the compile is not successful.
    
    This script will issue a system call to the FRED binary for each run; 
    the system call will include the parameter file, the run number (used 
    for storing run results in the correct RUN subdirectory), and the output 
    directory to store results in.
    
    The run number passed to the FRED binary will range from the specified 
    starting run number and ending run number, inclusive.
    
    Parameters
    ----------
    param_file : str
        A string representation of the path to the .fred parameter file to run
    directory : str
        A string representation of the path to the output directory
    start_run : int
        The starting run number to simulate
    end_run : int
        The ending run number to simulate
    compile : bool
        If the parameter file should be compiled before running
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

    # TODO: add optional directory (with outdir param getter)

    # Check the directory
    output_dir_path = Path(directory).resolve()
    try:
        fredutil.validate_dir(output_dir_path)
    except (FileNotFoundError, NotADirectoryError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Find the FRED binary
    fred_binary_path = Path(fred_home_dir_str, 'bin', 'FRED')
    try:
        fred_binary_str = fredutil.get_file_str_from_path(fred_binary_path)
    except (FileNotFoundError, IsADirectoryError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Only compile if the compile parameter was set to true
    if compile:
        result = _compile_param_file(param_file)
        if result == constants.EXT_CD_ERR:
            return result

    # Copy param file into output directory
    shutil.copy(param_file_str, output_dir_path)

    # Set fred_run command
    fred_run_cmd = f'fred_run.py {param_file_str} {output_dir_path} -s {start_run} -n {end_run} -c {compile}'
    if no_log:
        fred_run_cmd += ' --nolog'

    # Save command to file in output directory
    with open(Path(output_dir_path, 'COMMAND_LINE'), 'w') as file:
        file.write(f'{fred_run_cmd}\n')

    # Execute runs
    try:
        _execute_runs(fred_binary_str, output_dir_path, param_file_str,
                      start_run, end_run, no_log)
        return constants.EXT_CD_NRM
    except (OSError, frederr.FredRunError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR


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


def _execute_runs(fred_binary_str: str, output_dir_path: Path, param_file: str,
                  start_run: int, end_run: int, no_log: bool):
    '''
    Execute the specified number of runs using the given FRED binary, 
    storing results in the given output directory.
    
    Parameters
    ----------
    fred_binary_str : str
        The string representation of the full path to the FRED binary file
    output_dir_path : pathlib.Path
        A Path object representing the fulll path to the chosen output directory
    param_file : str
        The string representation of the full path to the param file
    start_run : int
        The starting run number to simulate
    end_run : int
        The ending run number to simulate
    no_log : bool
        If true, will write the output to /dev/null rather than a LOG file
    
    Raises
    ------
    OSError
        If an error occurs in the creation of a RUN subdirectory
    frederr.FredRunError
        If an error occurs during a FRED run
    '''

    for run in range(start_run, end_run + 1):
        # Create RUN subdirectory
        run_dir_path = Path(output_dir_path, f'RUN{run}')
        fredutil.create_dir(run_dir_path)

        # Set run command, and append output location
        cmd = f'{fred_binary_str} -p {param_file} -r {run} -d {output_dir_path}'

        # A human readable alternative to print
        param_filename = Path(param_file).name
        print(
            f'Running FRED: RUN{run} with parameter file [{param_filename}].')

        # Execute run command
        result = subprocess.run(cmd,
                                shell=True,
                                stderr=subprocess.STDOUT,
                                stdout=subprocess.PIPE,
                                text=True)

        # Write output to LOG file
        # TODO: reevaluate if nolog is needed after logging refactor is complete
        log_file_path = Path(run_dir_path, 'LOG')
        with open(log_file_path, 'w') as log_file:
            log_file.write(result.stdout)

        # Raise error if run was unsuccessful
        if result.returncode != constants.EXT_CD_NRM:
            raise frederr.FredRunError(log_file_path)


if __name__ == '__main__':
    main()
