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

from fredpy import frederr
from fredpy.util import fredutil
from fredpy.util import constants

from fred_rt import fred_rt
from fred_run import fred_run


def main():

    usage = '''
    Note: this will overwrite the existing regression test
    
    Typical usage examples:
    
    $ fred_make_rt.py
    Making files for FRED regression test: base.
    Please wait ...

    ...
    
    $ fred_make_rt.py antivirals
    Making files for FRED regression test: antivirals.
    Please wait ...

    ...
    '''

    # Create and set up the parser
    parser = argparse.ArgumentParser(
        description='Creates a regression test.',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('test',
                        nargs='?',
                        default='base',
                        help='the regression test to create (default=base)',
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

    result = fred_make_rt(test)
    sys.exit(result)


def fred_make_rt(test: str) -> Literal[0, 2]:
    '''
    Creates a regression test by running a constant parameter file and storing
    the output. This allows regression tests to be run by running the same 
    parameter file and comparing the results to the stored results.
    
    Regression tests are contained within a subdirectory of the FRED_HOME/tests 
    directory. The specific regression test is specified through the arguments,
    with the base regression test being the default.
    
    Expected results for a regression test are stored in the OUT.RT subdirectory
    of their corresponding test directory. This directory is tracked by git, 
    and should only be overwritten if changes to the output are expected.
    
    Parameters
    ----------
    test : str
        The regression test to create
    
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
    param_file_path = Path(test_dir, 'test.fred')
    try:
        param_file_str = fredutil.get_file_str_from_path(param_file_path)
    except (FileNotFoundError, IsADirectoryError) as e:
        logging.error(f'Missing test.fred parameter file in directory [{test_dir}].')
        return constants.EXT_CD_ERR

    print(f'Making files for FRED regression test: {test}.')
    print('Please wait ...\n')

    # Overwrite exisiting OUT.RT; this will store the expected output
    # of a regression test
    out_rt_dir = Path(test_dir, 'OUT.RT')

    if out_rt_dir.exists() and out_rt_dir.is_dir():
        shutil.rmtree(out_rt_dir)

    try:
        fredutil.create_dir(out_rt_dir)
    except OSError as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Change system directory to test_dir to use files included in the
    # test directory when calling the FRED binary in fred_run
    os.chdir(test_dir)

    # Create the regression test by storing run output to OUT.RT
    result = fred_run(param_file=param_file_str,
                      directory=str(out_rt_dir),
                      start_run=1,
                      end_run=2,
                      compile=False,
                      no_log=True)
    if result == constants.EXT_CD_ERR:
        return result

    print('Regression test created.\n')

    # Run a regression test
    result = fred_rt(test)
    if result == constants.EXT_CD_ERR:
        return result

    return constants.EXT_CD_NRM


if __name__ == '__main__':
    main()
