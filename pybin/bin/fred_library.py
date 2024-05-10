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
from typing import Literal

from fredpy.util import fredutil
from fredpy.util import constants
from fredpy import frederr


def main():

    usage = '''
    Typical usage examples:

    $ fred_library.py -l
    Aging
    Asthma
    Influenza
    ...

    $ fred_library.py -p Influenza
    #####################################################
    # MODULE FRED::Influenza
    # Author: John Grefenstette
    # Date: 22 Jul 2019

    condition INF {
    states = S E Is Ia R Import
    ...

    $ fred_library.py -d Mortality
    Module FRED::Mortality
    Author: John Grefenstette
    Created: 14 Apr 2019
    Condition: MORTALITY
    States: Wait Screening Female Male Survival Death

    Rules:

    1. Each individual begins in a Wait state that lasts randomly between 1
    ...
    '''

    # Creates and sets up the parser
    parser = argparse.ArgumentParser(
        description='Print varying information on FRED libraries.',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    options = parser.add_mutually_exclusive_group(required=True)
    options.add_argument('-l',
                         '--list',
                         action='store_true',
                         help='list available FRED libraries')
    options.add_argument(
        '-p',
        '--program',
        help='read the fred program file in the given FRED library')
    options.add_argument(
        '-d',
        '--documentation',
        help='read the documentation file in the given FRED library')
    parser.add_argument('--loglevel',
                        default='ERROR',
                        choices=constants.LOGGING_LEVELS,
                        help='set the logging level of this script')

    # Parses the arguments
    args = parser.parse_args()
    list_contents = args.list
    program = args.program
    documentation = args.documentation
    loglevel = args.loglevel

    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)

    result = fred_library(list_contents, program, documentation)
    sys.exit(result)


def fred_library(list_contents: bool, program: str,
                 documentation: str) -> Literal[0, 2]:
    '''
    Prints varying information on FRED libraries. This script includes three main functionalities:
    1. listing the available FRED libraries, which are directories in FRED_HOME/library;
    2. reading the program file in a given FRED library, which is located at 
    FRED_HOME/library/{FRED library}/{FRED library}.fred;
    3. reading the documentation file in a given FRED library, which is located at
    FRED_HOME/library/{FRED library}/README.txt
    
    Note that because the arguments are mutually exclusive, only one of these 
    parameters will be specified at a time.
    
    Parameters
    ----------
    list_contents : bool
        If true, list the available FRED libraries
    program : str
        If specified, read the .fred program file in the given FRED library
    documentation : str
        If specified, read the documentation file in the given FRED library
    
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
        constants.EXT_CD_ERR

    # List the library contents
    if list_contents:
        try:
            list_fred_libraries(fred_home_dir_str)
            return constants.EXT_CD_NRM
        except (FileNotFoundError, NotADirectoryError) as e:
            logging.error(e)
            constants.EXT_CD_ERR

    # Print the program file
    elif program:
        program_file_path = Path(fred_home_dir_str, 'library', program,
                                 f'{program}.fred')

        try:
            program_file_str = fredutil.get_file_str_from_path(
                program_file_path)
        except (FileNotFoundError, IsADirectoryError) as e:
            logging.error(e)
            constants.EXT_CD_ERR

        with open(program_file_str, 'r') as file:
            for line in file:
                print(line.rstrip())

        return constants.EXT_CD_NRM

    # Print the documentation file
    elif documentation:
        documentation_file_path = Path(fred_home_dir_str, 'library',
                                       documentation, 'README.txt')

        try:
            documentation_file_str = fredutil.get_file_str_from_path(
                documentation_file_path)
        except (FileNotFoundError, IsADirectoryError) as e:
            logging.error(e)
            constants.EXT_CD_ERR

        with open(documentation_file_str, 'r') as file:
            for line in file:
                print(line.rstrip())

        return constants.EXT_CD_NRM


def list_fred_libraries(fred_home_dir_str: str) -> None:
    '''
    Given the FRED_HOME directory, ensure that there exists a library directory, 
    and print out the libraries within that directory.
    
    Parameters
    ----------
    fred_home_dir_str : str
        The string representation of the full path to the FRED HOME directory
    
    Raises
    ------
    FileNotFoundError
        If the library directory cannot be found in the FRED HOME directory
    NotADirectoryError
        If the library is a file, not a directory
    '''

    library_dir_path = Path(fred_home_dir_str, 'library')
    library_dir_str = str(library_dir_path.resolve())

    subdirs = os.listdir(library_dir_str)
    for dir in subdirs:

        # Do not list these directories
        if dir[0] == '.':
            continue
        if dir == 'user':
            continue
        if dir == 'contrib':
            continue

        print(dir)


if __name__ == '__main__':
    main()