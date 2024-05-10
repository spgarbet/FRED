'''Utility methods used by classes in the FredPythonProject

Common utility functions that will be useful to executable FRED Python scripts;
e.g. things like finding the ID of a FRED job given a fred_key, or getting a 
full path string to a JOB's META or OUT directories.
  
'''

###################################################################################################
##
##  This file is part of the FRED system.
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

import argparse
import os
import re
import time
from collections import namedtuple
from functools import reduce
from pathlib import Path
from typing import List, Dict

# from fredpy.util import constants
from fredpy import frederr

# Custom data type that will represent a latitude longitude point
GeoPoint = namedtuple('GeoPoint', ['lat', 'lon'])
 
 
def create_dir(directory: Path) -> None:
    '''Creates the given directory if it is not yet created.
    
    Parameters
    ----------
    directory : pathlib.Path
        A Path object representing the full path to the directory that will be created
    
    Raises
    ------
    OSError
        If the directory path is occupied by a file
    '''

    if directory.exists() and not directory.is_dir():
        raise OSError(f'Directory could not be created; [{directory}] is a file.')
    elif not directory.exists():
        os.mkdir(directory)
        

def deep_merge(a: Dict, b: Dict, path=None) -> Dict:
    '''Performs a deep merge of two dictionaries and throws exception if there is a conflict
    
    Parameters
    ----------
    a : Dict
        Dictionary to be merged into
    b : Dict
        The Dictionary to be merged
        
    Returns
    ------
    Dict
        A dictionary that is a merge of dictionary a and dictionary b
        
    Raises
    ------
    Exception
        If there are conflicting values for the same key
        
    Notes
    -----
    This comes from https://stackoverflow.com/questions/7204805/how-to-merge-dictionaries-of-dictionaries
        
    '''
    if path is None: path = []
    for key in b:
        if key in a:
            if isinstance(a[key], dict) and isinstance(b[key], dict):
                deep_merge(a[key], b[key], path + [str(key)])
            elif a[key] == b[key]:
                pass # same leaf value
            else:
                raise Exception('Conflict at {}'.format('.'.join(path + [str(key)])))
        else:
            a[key] = b[key]
    return a
    

def find_all_indexes_of_substring_in_string(substring: str, fullstring: str) -> List[int]:
    '''Finds all indices of substring in fullstring and returns those as a List of integers
    If not found, will return None
    
    Parameters
    ----------
    substring : str
        The string to be searched for
    fullstring : str
        The string that will be searched

    Returns
    -------
    List[int]
        The integer indices of substring in fullstring
    
    '''
    return_list = [m.start() for m in re.finditer(f'(?={substring})', fullstring)]
    if len(return_list) == 0:
      return None
    else:
      return return_list
      

def get_file_str_from_path(file_path: Path) -> str:
    '''Checks that a file path is a valid file (not a directory), and returns the string representation
    of that path.
    
    Parameters
    ----------
    file_path : pathlib.Path
        A Path object containing the path to the file
    
    Returns
    -------
    str
        The string representation of the given Path
    
    Raises
    ------
    FileNotFoundError
        If the file at the path cannot be found
    IsADirectoryError
        If the file at the path is a directory
    '''

    file_str = str(file_path.resolve())

    if not file_path.exists():
        raise FileNotFoundError(f'Can\'t find file [{file_str}].')
    elif file_path.is_dir():
        raise IsADirectoryError(f'File [{file_str}] is a directory.')

    return file_path
    

def get_fred_home_dir_str() -> str:
    '''Returns the string value of the FRED_HOME environment variable or exits
    if FRED_HOME is not set.

    Returns
    -------
    str
        a string that contains the full path to the FRED home directory or
        exits if FRED_HOME is not set
    
    Raises
    ------
    frederr.FredHomeUnsetError
        If the the FRED_HOME environment variable is unset
    '''

    fred_home_dir_str = os.environ.get('FRED_HOME')

    if fred_home_dir_str == None:
        raise frederr.FredHomeUnsetError()

    return fred_home_dir_str
    

def get_fred_id(fred_key: str) -> str:
    '''Get the ID of the job that FRED associated with the supplied key.
    
    Parameters
    ----------
    fred_key : str
        The key to lookup
    
    Returns
    -------
    str
        A string that is the ID of the job that FRED associated with fred_key, 
        or None if the key is not found.
        
    Raises
    ------
    frederr.FredHomeUnsetError
        If the the FRED_RESULTS environment variable is not set and the 
        FRED_HOME environment variable is unset
    FileNotFoundError
        If either the RESULTS directory can't be found or the KEY file can't 
        be found in the RESULTS directory
    IsADirectoryError
        If the KEY file is actually a directory
    NotADirectoryError
        If the RESULTS directory is not actually a directory
    
    Examples
    --------
    >>> from fredpy.util import fredutil
    >>> fred_id = fredutil.get_fred_id('abcd123')
    >>> print(fred_id)
    2

    If the key does not exist, return None
    
    >>> fred_id = fredutil.get_fred_id('badkey')
    >>> print(fred_id)
    None
    '''

    fred_id = _scan_key_file_for_id(fred_key)

    return fred_id


def get_fred_job_dir_str(fred_key: str) -> str:
    '''Get the string value of the directory path to the Job with the ID that 
    FRED associated with the supplied key.
    
    Parameters
    ----------
    fred_key : str
        The key to lookup
    
    Returns
    -------
    str
        The string value of the directory path to the Job with the ID that FRED 
        associated with fred_key or None if the key is not found
    
    Raises
    ------
    frederr.FredHomeUnsetError
        If the FRED_HOME environment variable is unset
    FileNotFoundError
        If the Job's directory can't be found
    NotADirectoryError
        If the Job's directory is not actually a directory
    '''

    fred_results_dir_str = get_fred_results_dir_str()
    fred_id = get_fred_id(fred_key)
    if fred_id == None:
        return None

    fred_job_dir_path = Path(fred_results_dir_str, 'JOB', fred_id)
    fred_job_dir_str = str(fred_job_dir_path.resolve())

    validate_dir(fred_job_dir_path)

    return fred_job_dir_str


def get_fred_job_meta_dir_str(fred_key: str) -> str:
    '''Get the string value of the directory path of the META directory of the 
    job that FRED associated with the supplied key.
    
    Parameters
    ----------
    fred_key : str
        The key to lookup
    
    Returns
    -------
    str
        The string value of the directory path of the META directory of the job 
        that FRED associated with fred_key or None if the key is not found.
        
    Raises
    ------
    frederr.FredHomeUnsetError
        If the the FRED_HOME environment variable is unset
    FileNotFoundError
        If the META directory can't be found
    NotADirectoryError
        If the META directory is not actually a directory
    '''

    fred_job_dir_str = get_fred_job_dir_str(fred_key)
    if fred_job_dir_str == None:
        return None

    fred_job_meta_dir_path = Path(fred_job_dir_str, 'META')
    fred_job_meta_dir_str = str(fred_job_meta_dir_path.resolve())

    validate_dir(fred_job_meta_dir_path)

    return fred_job_meta_dir_str


def get_fred_job_out_dir_str(fred_key: str) -> str:
    '''Get the string value of the directory path of the OUT directory of the 
    job that FRED associated with the supplied key.
    
    Parameters
    ----------
    fred_key : str
        The key to lookup
    
    Returns
    -------
    str
        The string value of the directory path of the OUT directory of the job 
        that FRED associated with fred_key or None if the key is not found.
        
    Raises
    ------
    frederr.FredHomeUnsetError
        If the the FRED_HOME environment variable is unsetS
    FileNotFoundError
        If the OUT directory can't be found
    NotADirectoryError
        If the OUT directory is not actually a directory
    '''

    fred_job_dir_str = get_fred_job_dir_str(fred_key)
    if fred_job_dir_str == None:
        return None

    fred_job_out_dir_path = Path(fred_job_dir_str, 'OUT')
    fred_job_out_dir_str = str(fred_job_out_dir_path.resolve())

    validate_dir(fred_job_out_dir_path)

    return fred_job_out_dir_str


def get_fred_results_dir_str() -> str:
    '''Return a string representation of the path to the directory where FRED 
    stores its results. 
    If the optional FRED_RESULTS environment variable is set, then return that value.
    Otherwise, if the FRED_HOME environment variable is set, the result directory 
    is defaulted to use that with /RESULTS appended to it.
    Lastly, if FRED_HOME is not set, then return None.

    Returns
    -------
    str
        A string that contains the full path to the FRED results directory or 
        None if neither FRED_HOME nor FRED_RESULTS is set.
    
    Raises
    ------
    frederr.FredHomeUnsetError
        If the the FRED_RESULTS environment variable is not set and the 
        FRED_HOME environment variable is unset
    '''
    fred_results_dir_str = os.environ.get('FRED_RESULTS')

    if fred_results_dir_str == None:
        fred_home_dir_str = get_fred_home_dir_str()  # Note this will raise FredHomeUnsetError if FRED_HOME is not set
        fred_results_dir_path = Path(fred_home_dir_str, 'RESULTS')
        fred_results_dir_str = str(fred_results_dir_path.resolve())

    return fred_results_dir_str


def get_fred_semaphore_file_name() -> str:
    '''Gets the FRED_SEMAPHORE file name set in the user's environment.
    
    If the optional FRED_SEMAPHORE environment variable is set, then return that value. 
    Otherwise, the SEMAPHORE file will just be called .results.lck.

    The FRED semaphore file is used to lock a FRED directory while undergoing operation.
    This ensures that other processes that are running cannot modify that directory 
    during the operation. Examples of where this is used are in fred_job, locking the 
    FRED_HOME directory while the Job is being completed, and in fred_delete, locing the 
    FRED_RESULTS directory while the Job is being deleted.

    Returns
    -------
    str
        A string that contains the file name of the FRED semaphore file.
    '''
    fred_semaphore_file_str = os.environ.get('FRED_SEMAPHORE')

    if fred_semaphore_file_str == None:
        fred_semaphore_file_str = '.results.lck'

    return fred_semaphore_file_str


def get_local_timestamp() -> str:
    '''Gets a local timestamp in the format: DayName Mon DayNum H:M:S Year,
    Example: Thu Jul 22 16:29:41 2021.
    This is the timestamp format used by the FRED program.

    Returns
    -------
    str
        The local timestamp as a string
    '''

    timestamp = time.localtime()
    timestamp = time.strftime('%a %b %d %H:%M:%S %Y', timestamp)
    return timestamp
    

def get_meta_file_data(meta_dir_str: str, file_name: str, type=str):
    '''Gets the data in the specified file in the given META directory.
    Ensures the data is of the correct data type. This functioned is used for
    one-line META files.
    
    Parameters
    ----------
    meta_dir_str : str
        The string representation of the full path to the META directory
    file_name : str
        The name of the file to get data from
    type : type
        The type of the expected return value (default = str)

    Returns
    -------
    type
        The data stored in the meta file
    
    Raises
    ------
    FileNotFoundError
        If the specified file could not be found
    IsADirectoryError
        If the specified file is a directory
    frederr.FredFileSizeError
        If the specified file doesn't have exactly one line
    ValueError
        If the data from the file is not of the specified type
    '''

    meta_file_dir_path = Path(meta_dir_str, file_name)
    meta_file_dir_str = get_file_str_from_path(meta_file_dir_path)

    lines = []
    with open(meta_file_dir_str) as file:
        lines = file.readlines()

    # There should only be one line in this file
    if len(lines) != 1:
        raise frederr.FredFileSizeError(meta_file_dir_str)

    data = lines[0].rstrip()

    try:
        return type(data)
    except ValueError:
        raise ValueError(f'{file_name} value [{data}] is not a valid {type.__name__}.')


def get_run_dirs(out_dir_str: str) -> List[str]:
    '''Find all valid RUN subdirectories in a given OUT directory, and returns
    their local names in a list. Example return value: ['RUN1', 'RUN2', 'RUN3']

    Parameters
    ----------
    out_dir_str : str
        The string representation of the full path to the OUT directory
    
    Returns
    -------
    List[str]
        A list of valid RUN subdirectories
    '''

    subdirs: List[str] = os.listdir(out_dir_str)
    run_dirs = [subdir for subdir in subdirs if subdir.startswith('RUN')]

    valid_run_dirs = []
    for dir in run_dirs:
        if Path(out_dir_str, dir).is_dir():
            valid_run_dirs.append(dir)

    return valid_run_dirs
   

def read_file(file) -> None:
    '''Prints the contents of a file. Assumes that the file is valid.
    
    Parameters
    ----------
    file : pathlib.Path or str
        The file can either be a Path object or string representing the full path to the file
    '''

    with open(file, 'r') as f:
        for line in f:
            print(line.rstrip())


def str2bool(v: str) -> bool:
    '''Converts various string representations of a boolean value to an actual boolean
    
    Parameters
    ----------
    v : str
        The string value to be converted to a boolean value
        
    Returns
    -------
    bool
        The boolean representation of of v

    Raises
    ------
    argparse.ArgumentTypeError
        If v can't be converted to a boolean value
    '''
    
    if isinstance(v, bool):
        return v
    if v.lower() in ('yes', 'true', 't', 'y', '1'):
        return True
    elif v.lower() in ('no', 'false', 'f', 'n', '0'):
        return False
    else:
        raise argparse.ArgumentTypeError('Boolean value expected.')
        

def validate_dir(directory: Path) -> None:
    '''Ensures the specified directory Path is a valid directory.
    
    Parameters
    ----------
    directory : pathlib.Path
        A Path object representing the full path to the directory
    
    Raises
    ------
    FileNotFoundError
        If the directory does not exist
    NotADirectoryError
        If the Path exists, but is not a directory
    '''

    if not directory.exists():
        raise FileNotFoundError(f'Can\'t find directory [{directory}].')
    elif not directory.is_dir():
        raise NotADirectoryError(f'File [{directory}] is not a directory.')


def _scan_key_file_for_id(fred_key: str) -> str:
    '''Search FRED's KEY file for the string, fred_key, and return the internal 
    ID that FRED used (directory name) to store the job's results.

    Parameters
    ----------
    fred_key : str
        The key value that FRED used for the job.

    Returns
    -------
    str
        A string ID that FRED associated with the key or None if the key is not found.
        
    Raises
    ------
    frederr.FredHomeUnsetError
        If the the FRED_RESULTS environment variable is not set and the FRED_HOME 
        environment variable is unset
    FileNotFoundError
        If either the RESULTS directory can't be found or the KEY file can't be 
        found in the RESULTS directory
    IsADirectoryError
        If the KEY file is actually a directory
    NotADirectoryError
        If the RESULTS directory is not actually a directory
    '''

    fred_results_dir_str = get_fred_results_dir_str()  # raises FredHomeUnsetError
    fred_results_dir_path = Path(fred_results_dir_str)
    fred_results_dir_str = str(fred_results_dir_path.resolve())
    fred_key_file_path = Path(str(fred_results_dir_path), 'KEY')
    fred_key_file_str = str(fred_key_file_path.resolve())

    fred_id = None

    if not fred_results_dir_path.exists():
        raise FileNotFoundError(f'Can\'t find directory [{fred_results_dir_str}].')
    if fred_results_dir_path.is_dir():
        if not fred_key_file_path.exists():
            raise FileNotFoundError(f'KEY file [{fred_key_file_str}] not found.')
        elif fred_key_file_path.is_dir():
            raise IsADirectoryError(f'KEY file [{fred_key_file_str}] is a directory.')
    else:
        raise NotADirectoryError(
            f'File [{fred_results_dir_str}] is not a directory.')

    with open(fred_key_file_str) as keyfile:
        for line in keyfile:
            fields = line.split()
            if fields[0] == fred_key:
                fred_id = fields[1]
                break
    return fred_id
