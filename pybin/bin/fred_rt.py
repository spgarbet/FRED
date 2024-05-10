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
import difflib
import filecmp
import logging
import os
import shutil
from typing import Literal

from fredpy.util import fredutil
from fredpy.util import constants
from fredpy import frederr

from fred_run import fred_run


def main():

    usage = '''
    Typical usage examples:
    
    $ fred_rt.py
    Running FRED regression test: base.
    Please wait ...

    ...
    
    $ fred_rt.py -d vaccines
    Running FRED regression test: vaccines.
    Please wait ...

    ...
    '''

    # Create and set up the parser
    parser = argparse.ArgumentParser(
        description='Runs a regression test.',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('test',
                        nargs='?',
                        default='base',
                        help='the regression test to create',
                        choices=['antivirals', 'base', 'vaccine'])
    parser.add_argument('--loglevel',
                        default='ERROR',
                        choices=constants.LOGGING_LEVELS,
                        help='set the logging level of this script')

    # Parse the arguments
    args = parser.parse_args()
    test = args.test
    loglevel = args.loglevel

    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)

    result = fred_rt(test)
    sys.exit(result)


def fred_rt(test: str) -> Literal[0, 2]:
    '''
    Runs a regression test by first ensuring the necessary files are present, 
    then running FRED and comparing the output to the expected output stored 
    by fred_make_rt. Because the parameter file is constant, the outputs should 
    be exactly the same.
    
    See fred_make_rt for more detail on regression tests.
    
    Parameters
    ----------
    test : str
        The regression test to run
    
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

    # Find and validate the tests subdirectory
    test_dir = Path(fred_home_dir_str, 'tests', test)
    try:
        fredutil.validate_dir(test_dir)
    except (FileNotFoundError, NotADirectoryError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Find param file in test directory
    # This will be used to run the regression test
    param_file_path = Path(test_dir, 'test.fred')
    try:
        param_file_str = fredutil.get_file_str_from_path(param_file_path)
    except (FileNotFoundError, IsADirectoryError) as e:
        logging.error(
            f'Missing test.fred parameter file in directory [{test_dir}].')
        return constants.EXT_CD_ERR

    # Find and validate OUT.RT directory
    # This holds the expected output of the FRED run
    out_rt_dir = Path(test_dir, 'OUT.RT')
    try:
        fredutil.validate_dir(out_rt_dir)
    except (FileNotFoundError, IsADirectoryError) as e:
        logging.error(f'Missing regression test in directory [{test_dir}]. '
                      f'Create one with: \n\'fred_make_rt.py {test}\'.')
        return constants.EXT_CD_ERR

    print(f'Running FRED regression test: {test}.')
    print('Please wait ...\n')

    # Remove existing OUT.TEST directory and recreate for this regression test
    out_test_dir = Path(test_dir, 'OUT.TEST')

    if out_test_dir.exists() and out_test_dir.is_dir():
        shutil.rmtree(out_test_dir)

    try:
        fredutil.create_dir(out_test_dir)
    except OSError as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Change system directory to test_dir to use files included in the
    # test directory when calling the FRED binary in fred_run
    os.chdir(test_dir)

    # Get output for regression test
    result = fred_run(param_file=param_file_str,
                      directory=str(out_test_dir),
                      start_run=1,
                      end_run=2,
                      compile=False,
                      no_log=True)
    if result == constants.EXT_CD_ERR:
        return result

    print('Comparing results ...\n')

    # We will compare the following files between OUT.RT and OUT.TEST
    files = [
        Path('RUN1', 'health_records.txt'),
        Path('RUN2', 'health_records.txt'),
        Path('RUN1', 'out.csv'),
        Path('RUN2', 'out.csv')
    ]

    failed = False

    # Compare each set of files, and if there are differences, print them
    for file in files:
        rt_file = Path(out_rt_dir, file)
        test_file = Path(out_test_dir, file)
        if not filecmp.cmp(rt_file, test_file):
            failed = True
            print(f'{file} differs in OUT.RT and OUT.TEST.')
            _print_differences(rt_file, test_file, file)

    if failed:
        print('Regression test failed.')
        return constants.EXT_CD_ERR
    else:
        print('Regression test passed.')
        return constants.EXT_CD_NRM


def _print_differences(rt_file: Path, test_file: Path,
                       file_suffix: Path) -> None:
    '''
    Prints the differences between the files using difflib.unified_diff().
    
    Parameters
    ----------
    rt_file : pathlib.Path
        A Path object representing the full path to the file in the OUT.RT 
        directory
    test_file : pathlib.Path
        A Path object representing the full path to the file in the OUT.TEST
        directory
    file_suffix : pathlib.Path
        The file suffix, which will consist of the OUT directory, RUN subdirectory,
        and file name; this is used for labelling the files in difflib.unified_diff()
    '''
    rt_lines = []
    with open(rt_file, 'r') as rt:
        rt_lines = rt.readlines()

    test_lines = []
    with open(test_file, 'r') as test:
        test_lines = test.readlines()

    # Will contain a list of differences between the files
    result = list(
        difflib.unified_diff(rt_lines,
                             test_lines,
                             fromfile=f'OUT.RT/{file_suffix}',
                             tofile=f'OUT.TEST/{file_suffix}'))

    # Output can be very large (tens of thousands of lines)
    if len(result) > 25:
        print(f'First 25 lines of output: ')
        sys.stdout.writelines(result[:25])
    else:
        print('Output: ')
        sys.stdout.writelines(result)
    print()


if __name__ == '__main__':
    main()