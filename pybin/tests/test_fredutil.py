"""Test module for fredpy.util.fredutil module

This pytest module should test the methods in the fredutil.py module

  Typical usage example:

  python3 -m pytest test_fredutil.py
  
"""

###################################################################################################
##
##  This file is part of the FRED system.
##
## Copyright (c) 2021, University of Pittsburgh, David Galloway, Mary Krauland, Matthew Dembiczak,
## and Mark Roberts
## All rights reserved.
##
## FRED is distributed on the condition that users fully understand and agree to all terms of the
## End User License Agreement.
##
## FRED is intended FOR NON-COMMERCIAL, EDUCATIONAL OR RESEARCH PURPOSES ONLY.
##
## See the file "LICENSE" for more information.
##
###################################################################################################

import sys
# Update the Python search Path
from pathlib import Path

file = Path(__file__).resolve()
package_root_directory_path = Path(file.parents[1], 'src')
for foo in file.parents:
    print(foo)
sys.path.append(str(package_root_directory_path.resolve()))

import pytest
import os
import shutil

from fredpy.util import fredutil
from fredpy.util import constants
from fredpy import frederr


def test_get_fred_home_dir(tmp_path):
    """ Test the get_fred_home_dir function
    
    Check for FRED_HOME not being set
    Check to make sure that the value returned from the function matches the value in the environment variables
    
    Parameters
    ----------
    tmp_path : pathlib.Path
        Built in @pytest.fixture decoration that is a path to a temporary directory which is unique to each test function.
    """
    tmp_fred_home_dir_path = Path(str(tmp_path.resolve()), 'HOME')
    tmp_fred_home_dir_str = str(tmp_fred_home_dir_path.resolve())

    # Check FRED_HOME is not set
    old_environ = dict(os.environ)
    for env_var in ['FRED_HOME', 'FRED_RESULTS']:
        os.environ.pop(env_var, default=None)

    # Check the case where FRED_HOME environment variable is not set
    with pytest.raises(frederr.FredHomeUnsetError):
        fred_home_dir_str = fredutil.get_fred_home_dir_str()

    # Now set the FRED_HOME environment variable
    os.environ.update({
        'FRED_HOME': tmp_fred_home_dir_str,
    })

    fred_home_dir_str = fredutil.get_fred_home_dir_str()
    assert fred_home_dir_str == tmp_fred_home_dir_str

    os.environ.clear()
    os.environ.update(old_environ)

    # Now make sure that the user has actually set his FRED_HOME
    user_fred_home_dir = os.environ.get('FRED_HOME')

    if user_fred_home_dir == None:
        print(
            'Please set environmental variable FRED_HOME to location of FRED home directory'
        )
        assert False

    fred_home_dir_str = fredutil.get_fred_home_dir_str()

    assert fred_home_dir_str == user_fred_home_dir


def test_get_fred_results_dir(tmp_path):
    """ Test the get_fred_results_dir function
    
    We should be able to set either the environment variable FRED_HOME and use the default of results
    is in FRED_HOME/RESULTS
    Or, we should be able to set the FRED_RESULTS environment variable to get the correct directory
    
    Parameters
    ----------
    tmp_path : pathlib.Path
        Built in @pytest.fixture decoration that is a path to a temporary directory which is unique to each test function.
    """
    tmp_fred_home_dir_path = Path(str(tmp_path.resolve()), 'HOME')
    tmp_fred_home_dir_str = str(tmp_fred_home_dir_path.resolve())
    tmp_fred_results_dir_path = Path(str(tmp_path), 'RESULTS')
    tmp_fred_results_dir_str = str(tmp_fred_results_dir_path.resolve())
    old_environ = dict(os.environ)

    # Check the case where FRED_RESULTS environment variable is set
    for env_var in ['FRED_HOME', 'FRED_RESULTS']:
        os.environ.pop(env_var, default=None)

    # Now set the FRED_RESULTS environment variable
    os.environ.update({
        'FRED_HOME':
        tmp_fred_home_dir_str,  # Note: this should be superceded by FRED_RESULTS being set
        'FRED_RESULTS': tmp_fred_results_dir_str,
    })
    fred_results_dir_str = fredutil.get_fred_results_dir_str()
    assert fred_results_dir_str == tmp_fred_results_dir_str

    # Now check the case where FRED_RESULTS environment variable is NOT set
    for env_var in ['FRED_HOME', 'FRED_RESULTS']:
        os.environ.pop(env_var, default=None)

    # Set just the FRED_HOME environment variable
    os.environ.update({
        'FRED_HOME': tmp_fred_home_dir_str,
    })
    fred_results_dir_str = fredutil.get_fred_results_dir_str()
    tmp_fred_results_dir_under_home_path = Path(tmp_fred_home_dir_str,
                                                'RESULTS')
    assert fred_results_dir_str == str(
        tmp_fred_results_dir_under_home_path.resolve())


def test_get_fred_id_environment(tmp_path):
    """ Test the get_fred_id function environmental issues
    Cases where the FRED_HOME environment variable is not set
    Cases where the RESULTS directory can't be found, is not a directory, etc
    
    Parameters
    ----------
    tmp_path : pathlib.Path
        Built in @pytest.fixture decoration that is a path to a temporary directory which is unique to each test function.
    """
    tmp_fred_home_dir_path = Path(str(tmp_path.resolve()), 'HOME')
    tmp_fred_home_dir_str = str(tmp_fred_home_dir_path.resolve())
    test_key = 'ABCDefg'
    old_environ = dict(os.environ)

    # Check the case where FRED_HOME environment variable is not set

    # Unset both FRED_HOME and FRED_RESULTS environment variables
    for env_var in ['FRED_HOME', 'FRED_RESULTS']:
        os.environ.pop(env_var, default=None)

    with pytest.raises(frederr.FredHomeUnsetError):
        fred_id = fredutil.get_fred_id(test_key)

    # Now set the FRED_HOME environment variable
    os.environ.update({
        'FRED_HOME': tmp_fred_home_dir_str,
    })

    tmp_fred_home_dir_path.resolve().mkdir()

    # There is no RESULTS directory, so expect ERR_CODE_FILE_NOT_FOUND
    with pytest.raises(FileNotFoundError):
        fred_id = fredutil.get_fred_id(test_key)

    # Now create the RESULTS directory as a FILE
    tmp_fred_results_dir_path = Path(str(tmp_fred_home_dir_path), 'RESULTS')
    tmp_fred_results_dir_path.resolve().touch()

    with pytest.raises(NotADirectoryError):
        fred_id = fredutil.get_fred_id(test_key)

    # Set the environment back to the way it was
    os.environ.clear()
    os.environ.update(old_environ)


def test_get_fred_id_keyfile(tmp_path):
    """ Test the get_fred_id function KEY file
    Cases where the KEY file can't be found or is not a proper file
    Case where the KEY file is empty
    Case where the KEY file contains the fred_key
    
    Parameters
    ----------
    tmp_path : pathlib.Path
        Built in @pytest.fixture decoration that is a path to a temporary directory which is unique to each test function.
    """
    test_key = 'keyABCDefg'

    old_environ = dict(os.environ)

    _createTestingFredResultsEnvironment(tmp_path)

    # Check the case where KEY file is missing
    fred_results_dir_str = fredutil.get_fred_results_dir_str()

    # Remove the KEY file if it exists
    fred_key_file_path = Path(fred_results_dir_str, 'KEY')
    if fred_key_file_path.exists():
        fred_key_file_path.unlink()

    with pytest.raises(FileNotFoundError):
        fred_id = fredutil.get_fred_id(test_key)

    # Check the case where KEY file is a directory
    fred_key_file_path.mkdir()
    with pytest.raises(IsADirectoryError):
        fred_id = fredutil.get_fred_id(test_key)

    # Remove the KEY directory and create a blank file
    fred_key_file_path.rmdir()
    fred_key_file_path.touch()
    fred_key_file_str = str(fred_key_file_path.resolve())

    # Should get None back
    fred_id = fredutil.get_fred_id(test_key)
    assert fred_id == None

    # Put some key|id pairs into the KEY file
    key_id_dict = {
        'key1': 'id1',
        'model': 'Mustang',
        'keyABCDefg': 'testId',
        'keyZ': '1964',
    }
    with open(fred_key_file_str, 'a') as f:
        for temp_key in key_id_dict.keys():
            f.write(temp_key + ' ' + key_id_dict[temp_key] + '\n')

    # Now, make sure we get back the value we think we will get
    fred_id = fredutil.get_fred_id(test_key)
    assert fred_id == 'testId'

    # Set the environment back to the way it was
    os.environ.clear()
    os.environ.update(old_environ)


def test_get_fred_job_meta_dir(tmp_path):
    """ Test the get_fred_meta_out_dir function
    
    Create a blank testing environment and a KEY file with a key|id pair
    Make sure that the META directory returned is the one we expect
    
    Parameters
    ----------
    tmp_path : pathlib.Path
        Built in @pytest.fixture decoration that is a path to a temporary directory which is unique to each test function.
    """
    test_key = 'keyABCDefg'

    old_environ = dict(os.environ)

    _createTestingFredResultsEnvironment(tmp_path, {'keyABCDefg': 'testId'})

    # Now, make sure we get back the value we think we will get
    fred_id = fredutil.get_fred_id(test_key)
    assert fred_id == 'testId'

    expected_fred_job_meta_dir_path = Path(str(tmp_path.resolve()), 'HOME',
                                           'RESULTS', 'JOB', fred_id, 'META')
    fred_job_meta_dir = fredutil.get_fred_job_meta_dir_str('keyABCDefg')
    assert fred_job_meta_dir == str(expected_fred_job_meta_dir_path.resolve())

    # Set the environment back to the way it was
    os.environ.clear()
    os.environ.update(old_environ)


def test_get_fred_job_out_dir(tmp_path):
    """ Test the get_fred_job_out_dir function
    
    Create a blank testing environment and a KEY file with a key|id pair
    Make sure that the OUT directory returned is the one we expect
    
    Parameters
    ----------
    tmp_path : pathlib.Path
        Built in @pytest.fixture decoration that is a path to a temporary directory which is unique to each test function.
    """
    test_key = 'keyABCDefg'

    old_environ = dict(os.environ)

    _createTestingFredResultsEnvironment(tmp_path, {'keyABCDefg': 'testId'})

    # Now, make sure we get back the value we think we will get
    fred_id = fredutil.get_fred_id(test_key)
    assert fred_id == 'testId'

    expected_fred_job_out_dir_path = Path(str(tmp_path.resolve()), 'HOME',
                                          'RESULTS', 'JOB', fred_id, 'OUT')
    fred_job_out_dir = fredutil.get_fred_job_out_dir_str('keyABCDefg')
    assert fred_job_out_dir == str(expected_fred_job_out_dir_path.resolve())

    # Set the environment back to the way it was
    os.environ.clear()
    os.environ.update(old_environ)


def _createTestingFredResultsEnvironment(tmp_path, key_id_dict={}):
    """ A utility function to set up a clean RESULTS environment
    For a lot of the simple utilities, we just need to know that they are getting the proper information from
    the environment variables and the KEY file in the RESULTS directory
    
    Parameters
    ----------
    tmp_path : pathlib.Path
        This should be the same tmp_path passed to each of the test functions
    key_id_dict : dict
        A dictionary of fred_key to fred_id pairs. These will be written to the KEY file. The fred_id will each be used to
        create a directory under the RESULTS/JOBS folder
    """
    # Unset both FRED_HOME and FRED_RESULTS environment variables
    for env_var in ['FRED_HOME', 'FRED_RESULTS']:
        os.environ.pop(env_var, default=None)

    fred_home_dir_path = Path(str(tmp_path.resolve()), 'HOME')
    fred_home_dir_str = str(fred_home_dir_path.resolve())
    fred_results_dir_path = Path(fred_home_dir_str, 'RESULTS')
    fred_results_dir_str = str(fred_results_dir_path.resolve())
    fred_key_file_path = Path(fred_results_dir_str, 'KEY')
    fred_key_file_str = str(fred_key_file_path.resolve())

    os.environ.update({
        'FRED_HOME': fred_home_dir_str,
    })

    if fred_home_dir_path.exists() and fred_home_dir_path.is_dir():
        shutil.rmtree(fred_home_dir_path)
    elif fred_home_dir_path.exists():
        fred_home_dir_path.unlink()

    fred_home_dir_path.mkdir()
    fred_results_dir_path.mkdir()
    fred_key_file_path.touch()

    print(key_id_dict)
    # Add the key|id pairs to the KEY file
    with open(fred_key_file_str, 'a') as f:
        for temp_key in key_id_dict.keys():
            f.write(temp_key + ' ' + key_id_dict[temp_key] + '\n')

    # Create the JOBS folder and all of the ID folders under it
    fred_results_jobs_dir_path = Path(fred_results_dir_str, 'JOB')
    fred_results_jobs_dir_str = str(fred_results_jobs_dir_path.resolve())
    fred_results_jobs_dir_path.mkdir()

    for temp_key in key_id_dict:
        tmp_job_dir_path = Path(fred_results_jobs_dir_str,
                                key_id_dict[temp_key])
        tmp_job_dir_path.mkdir()
        tmp_job_dir_str = str(tmp_job_dir_path.resolve())
        Path(tmp_job_dir_str, 'OUT').mkdir()
        Path(tmp_job_dir_str, 'WORK').mkdir()
        Path(tmp_job_dir_str, 'META').mkdir()
