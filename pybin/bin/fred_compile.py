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
from typing import Literal

from fredpy.util import fredutil
from fredpy.util import constants
from fredpy import frederr


def main():

    usage = '''
    Typical usage example:
    
    $ fred_compile.py paramfile.fred
    Compiling /home/mojadem/PHDL/FRED/inf_baseline.fred ...
    No errors found.'''

    # Create and set up the parser
    parser = argparse.ArgumentParser(
        description='Compiles the given .fred parameter file.',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('file',
                        help='the .fred parameter file to compile',
                        metavar='params.fred')
    parser.add_argument(
        '-d',
        '--directory',
        default='.FRED_compiler',
        help='the directory to store compile results (by default, stores '
        'results in a .FRED_compiler folder in the current working directory)')
    parser.add_argument('--loglevel',
                        default='ERROR',
                        choices=constants.LOGGING_LEVELS,
                        help='set the logging level of this script')

    # Parse the arguments
    args = parser.parse_args()
    param_file = args.file
    directory = args.directory
    loglevel = args.loglevel

    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)

    result = fred_compile(param_file, directory)
    sys.exit(result)


def fred_compile(param_file: str, directory: str) -> Literal[0, 2]:
    '''
    Compiles the provided .fred parameter file, storing results in the 
    specified directory, or, if no directory is provided, stores them in a 
    .FRED_compiler folder as a subdirectory of the current working directory.

    This script will issue a system call to the FRED binary; the system call 
    will include the parameter file, the run number (which need only be 
    one for compiling), the directory to store results in, and the compile flag.
    
    Parameters
    ----------
    param_file : str
        A string representation of the path to the .fred parameter file to compile
    directory : str
        A string representation of the path to the output directory
    
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

    # Find the FRED binary
    fred_binary_path = Path(fred_home_dir_str, 'bin', 'FRED')
    try:
        fred_binary_str = fredutil.get_file_str_from_path(fred_binary_path)
    except (FileNotFoundError, IsADirectoryError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Get current working directory
    cwd = Path('.')

    # Ensure param file exists
    param_file_path = Path(cwd, param_file)
    try:
        param_file_str = fredutil.get_file_str_from_path(param_file_path)
    except (FileNotFoundError, IsADirectoryError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Create and setup compiler directory and RUN subdirectory
    directory_path = Path(cwd, directory)
    run_directory_path = Path(directory_path, 'RUN1')

    try:
        _setup_dir(directory_path)
        _setup_dir(run_directory_path)
    except OSError as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Copy param file into directory (if directory is not cwd)
    if directory_path != cwd:
        shutil.copy(param_file_path, directory_path)

    # Set compile command
    cmd = f'{fred_binary_str} -p {param_file} -r 1 -d {directory} -c'

    # Save command to file in compiler directory
    with open(Path(directory_path, 'COMMAND_LINE'), 'w') as file:
        file.write(cmd)

    print(f'Compiling {param_file_str} ...')

    # Execute compile command, writing output to null
    os.system(cmd + ' 2>&1 > /dev/null')

    # Check for errors or warnings
    error_file_path = Path(directory_path, 'errors.txt')
    if error_file_path.exists():
        fredutil.read_file(error_file_path)
        print('\nPlease fix these errors before proceeding.')
        return constants.EXT_CD_ERR
    else:
        print('No errors found.')

        warning_file_path = Path(directory_path, 'warnings.txt')
        if warning_file_path.exists():
            fredutil.read_file(warning_file_path)
        else:
            print('No warnings.')

        return constants.EXT_CD_NRM


def _setup_dir(directory: Path) -> None:
    '''
    Creates the given directory if it has not yet been created. If it already
    exists, remove all existing text files in the directory. These text files
    were most likely left over from an old compile; we want a clean slate.
    
    Parameters
    ----------
    directory : pathlib.Path
        A Path object representing the full path to the directory to set up
    
    Raises
    ------
    OSError
        If the directory path is occupied by a file and it cannot be created
    '''

    fredutil.create_dir(directory)

    text_files = [f for f in os.listdir(directory) if f.endswith('.txt')]
    for file in text_files:
        os.remove(Path(directory, file))


if __name__ == '__main__':
    main()