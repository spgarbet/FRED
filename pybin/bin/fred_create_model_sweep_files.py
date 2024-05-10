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
import json
import logging
import math
import os
import re
from typing import Literal, List, Dict
import uuid

from fredpy.util import fredutil
from fredpy.util import constants
from fredpy import frederr

try:
    import numpy as np
except ImportError:
    print('This script requires the numpy module.')
    print('Installation instructions can be found at https://numpy.org/install/')
    sys.exit(constants.EXT_CD_ERR)
    
try:
    import pandas as pd
except ImportError:
    print('This script requires the pandas module.')
    print('Installation instructions can be found at https://pandas.pydata.org/getting_started.html')
    sys.exit(constants.EXT_CD_ERR)

THRESHOLD = 100
MAX_REDRAW = 10
MAX_LOOPS = 20
TEMP_MODEL_FILE = 'temp.fred'

def main():

    usage = '''
    Typical usage example:
    
    $ fred_create_model_sweep_files.py basefile.fred sweep_info.json -s JSON
    '''

    # Create and set up the parser
    parser = argparse.ArgumentParser(
        description='Given a FRED parameter (model) file, a list of parameters to sweep, and a list of vales for each parameter to sweep over, create a new model file for each combination.',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('base_param_file',
                        metavar='base_param_file',
                        help='the .fred parameter file to use as the basis for all sweep files')
    parser.add_argument('sweep_file',
                        metavar='sweep_file',
                        help='a file that has the list of parameters in the model file that will be swept over as well as the values for those parameters')
    parser.add_argument('model_file_dir',
                        metavar='model_file_dir',
                        nargs='?',
                        default='MODELS',
                        help='the directory where the created model files will be written')
    parser.add_argument('-s',
                        '--sweep_file_format',
                        default='json',
                        choices=['json', 'JSON', 'CSV', 'csv'],
                        help='the format for the sweep file')
    parser.add_argument('-f',
                        '--force',
                        metavar='True, False, T, F, 1, 0, Y, N',
                        type=fredutil.str2bool,
                        nargs='?',
                        const=True,
                        default=False,
                        help='force the program to ignore the threshold for total combinations')
    parser.add_argument('--loglevel',
                        default='ERROR',
                        choices=constants.LOGGING_LEVELS,
                        help='set the logging level of this script')

    # Parse the arguments
    args = parser.parse_args()
    base_param_file_str = args.base_param_file
    sweep_file_str = args.sweep_file
    model_file_dir_str = args.model_file_dir
    sweep_file_format = args.sweep_file_format
    force = args.force
    loglevel = args.loglevel

    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)

    result = fred_create_model_sweep_files(base_param_file_str, sweep_file_str, model_file_dir_str, sweep_file_format, force)
    sys.exit(result)

def fred_create_model_sweep_files(base_param_file_str: str, sweep_file_str: str, model_file_dir_str: str, sweep_file_format: str, force: bool) -> Literal[0, 2]:
    '''
    Creates a set of model files from parameters and values for those parameters. Expects one of two formats for the sweep files:
    
    * **CSV file format**
    
        | Each column name matches a parameter and the values are the fields in a given row.
        | Each row will be used to create a single model file.
        | The following .csv file would create 5 .fred files with the various values set for each parameter
        
        .. csv-table:: Example .csv
            :header: "PARAM1", "PARAM2", "PARAM3"
            :widths: 10, 10, 10

            "A", 2.99, 1.0
            "A", 1.99, 1.0
            "A", 1.99, 1.5
            "B", 2.99, 1.5
            "B", 1.99, 1.5
        
    * **JSON file format**
    
        | Given a specific .json file format will create a combination of all the parameter values selected using the method requested
        | The "param_name" key will be used to name each parameter (similar to the column headers in .csv files)
        | There are three options for describing how to fill the values: "value_list", "value_dist", or "value_range"
        
        * **value_list**
        
            | This is simply a list of values that will be used for the parameter.
            | The list values must all be the same type, either string or numeric
        
        * **value_dist**
        
            | Pick the values from a distribution.
            | The "count" key is required and tells the program how many times to pick.
            | The "type" is one of "uniform", "discrete_uniform", "normal", or "lognormal"
            | Each distribution type has its own structure as well
   
        * **value_range**
        
            | This will create a range from the required values "min" and "max" (inclusive)
            | by the required "step" value. In the example below, it would use the values
            | [ 2.0, 2.5, 3.0, 3.5 ]
          
        | There will be some validation checking
        | Required keys are submitted in
        | The values for those keys are the correct types
        | The values make sense, e.g. "max" is > "min"
        
        .. code:: javascript

            {
              "sweep_params": [
                {
                  "param_name": "PARAM1",
                  "value_list": [
                    "str1",
                    "str2",
                    "str3"
                  ]
                },
                {
                  "param_name": "PARAM2",
                  "value_dist": {
                    "count": 2,
                    "type": "uniform",
                    "min": 4,
                    "max": 8
                  }
                },
                {
                  "param_name": "PARAM3",
                  "value_dist": {
                    "count": 2,
                    "type": "discrete_uniform",
                    "pick_list": [
                      8,
                      10,
                      22,
                      9.9,
                      55.5
                    ]
                  }
                },
                {
                  "param_name": "PARAM4",
                  "value_range": {
                    "min": 2,
                    "max": 3.5,
                    "step": 0.5
                  }
                },
                {
                  "param_name": "PARAM5",
                  "value_dist": {
                    "count": 3,
                    "type": "normal",
                    "mean": 22,
                    "stddev": 3
                  }
                }
              ]
            }
            
    Once the combinations are created, it will validate a couple of things:
    
        * The total number of files created will not be greater than a threshold.
        
            | *The user can FORCE the program to create all of the files by setting the -f option*
            
        * Each parameter requested to be set actually exists in the base parameter file.
        
    The generated model files will be created in the directory requested (the third argument to the main program call). By default it will be in a directory
    called "MODELS" created in the current directory. Each file will have a filename generated from the combination of the parameter names and the values set.
    
    Parameters
    ----------
    base_param_file_str : str
        The .fred parameter file to use as the basis for all sweep files
        
    sweep_file_str : str
        A file that has the list of parameters in the model file that will be swept over as well as the values for those parameters
        
    model_file_dir_str : str
        The directory where the created model files will be written
        
    sweep_file_format : str
        The format for the sweep file (either .csv or .json)
        
    force : bool
        Force the program to ignore the threshold for total files written
        
    Returns
    -------
    Literal[0, 2]
        The exit status
    '''
    
    try:
        if(sweep_file_format.lower() == 'json'):
            # Opening JSON file
            f = open(sweep_file_str)
  
            # returns JSON object as
            # a dictionary
            sweep_data = json.load(f)
            f.close()
        
            # Validate the JSON
            sweep_data = _validate_sweep_json(sweep_file_str)
            sweep_df = _create_dataframe_from_validated_sweep_json(sweep_data, force)
        
        elif(sweep_file_format.lower() == 'csv'):
            # Opening CSV file
            sweep_df = pd.read_csv(sweep_file_str)
            rowcount = sweep_df.shape[0]
            if(rowcount >= THRESHOLD and not force):
                logging.error(frederr.FredSweepCartesianProductError(THRESHOLD))
                
    except frederr.FredError as e:
        logging.error(e)
        return constants.EXT_CD_ERR
    
    # Sweep dataframe is populated, now validate that the parameters to be swept-over actually exist in the base parameter file
    validate_sweep_param_dict = _validate_sweep_parameters(base_param_file_str, sweep_df)
    if validate_sweep_param_dict['is_error']:
        logging.error(frederr.FredSweepParamError(validate_sweep_param_dict['error_message']))
        return constants.EXT_CD_ERR
    
    try:
        # The parameters are correct, so now we write all of the model files using the altered parameters
        _create_model_files_from_sweep_parameters(base_param_file_str, model_file_dir_str, sweep_df, validate_sweep_param_dict['param_pos_dict'])
    except BaseException as e:
        logging.error(e)
        return constants.EXT_CD_ERR
        
    return constants.EXT_CD_NRM


def _validate_sweep_json(sweep_file: str) -> Dict:
    '''
    Make sure that the JSON description for the sweep is valid
    
    Parameters
    ----------
    sweep_file: str
        The filename that includes the json description of the parameter sweep
        
        
    Returns
    -------
    Dict[str]
        The json data as a Python Dictionary
    
    Raises
    ------
    frederr.FredSweepJsonError
        If the JSON sweep file does not pass validation
    '''

    # Opening JSON file
    f = open(sweep_file)
  
    # get JSON object as a dictionary
    sweep_json_data = json.load(f)
  
    # Begin validation
    # Validate that sweep_params exists and that it is an array of at least size 1
    if 'sweep_params' not in sweep_json_data.keys():
        raise frederr.FredSweepJsonError(sweep_file, f'Missing key(s) [sweep_params]')

    if(isinstance(sweep_json_data['sweep_params'], list)):
        # Make sure that the the parameter array is at least size 1
        if len(sweep_json_data['sweep_params']) == 0:
            raise frederr.FredSweepJsonError(sweep_file, 'sweep_params must be <class \'list\'> (JSON array) of at least one item')
    else:
        raise frederr.FredSweepJsonError(sweep_file, 'sweep_params must be <class \'list\'> (JSON array). The datatype is: \'' + str(type(sweep_json_data['sweep_params'])) + '\'')
    
    # Now validate the sweep_params
    error_str = ''
    sweep_params_list = sweep_json_data['sweep_params']
    error_dict = _validate_sweep_params(sweep_params_list)
    if error_dict['is_error']:
        error_list = []
        for key, value in sorted(error_dict['errored_items'].items(), key=lambda item: int(item[0])):
            error_str = 'Item ' + str(key) + ': ' + str(value)
            error_list.append(error_str)
        raise frederr.FredSweepJsonError(sweep_file, str(error_list))
    else:
        # So far so good ...
        logging.debug('Validation Successful')
        
    return sweep_json_data
    

def _validate_sweep_params(sweep_params_list: List) -> Dict:
    '''
    Checks that the list of sweep_param dictionaries are valid
    
    Parameters
    ----------
    sweep_params_list : List
        A list of sweep_param dictionary objects
    
    Returns
    -------
    Dict
        A dictionary with a minimum of key 'is_error' = False and any errors otherwise
    '''
    
    param_idx = 0
    error_dict = {'is_error': False}
    for sweep_param_dict in sweep_params_list:
        # Check for required key 'param_name'
        if 'param_name' not in sweep_param_dict.keys():
            if not error_dict['is_error']:
                error_dict['is_error'] = True
                
            if error_dict.get('errored_items') == None:
                error_dict['errored_items'] = {}
                
            if error_dict['errored_items'].get(param_idx) == None:
                error_dict['errored_items'][param_idx] = {}
                error_dict['errored_items'][param_idx]['REQUIRED'] = ['param_name']
            else:
                if error_dict['errored_items'][param_idx].get('REQUIRED') == None:
                    error_dict['errored_items'][param_idx]['REQUIRED'] = ['param_name']
                else:
                    error_dict['errored_items'][param_idx]['REQUIRED'].append('param_name')
                    
        # Check for required key being one of ['value_list', 'value_range', 'value_dist']
        # Note: we are not checking for exclusivity, meaning 'value_list' > 'value_range' > 'value_dist' in terms of precedence
        if 'value_list' not in sweep_param_dict.keys() and 'value_range' not in sweep_param_dict.keys() and 'value_dist' not in sweep_param_dict.keys():
            if not error_dict['is_error']:
                error_dict['is_error'] = True
                
            if error_dict.get('errored_items') == None:
                error_dict['errored_items'] = {}
                
            if error_dict['errored_items'].get(param_idx) == None:
                error_dict['errored_items'][param_idx] = {}
                error_dict['errored_items'][param_idx]['REQUIRED'] = ['One of [value_list, value_range, value_dist]']
            else:
                if error_dict['errored_items'][param_idx].get('REQUIRED') == None:
                    error_dict['errored_items'][param_idx]['REQUIRED'] = ['One of [value_list, value_range, value_dist]']
                else:
                    error_dict['errored_items'][param_idx]['REQUIRED'].append('One of [value_list, value_range, value_dist]')
        else:
            if not sweep_param_dict.get('value_list') == None:
                inner_error_dict = _validate_value_list(sweep_param_dict.get('value_list'), param_idx)
                # Either is_error means that both need to be is_error = True for merge
                if error_dict['is_error'] or inner_error_dict['is_error']:
                    error_dict['is_error'] = True
                    inner_error_dict['is_error'] = True
                fredutil.deep_merge(error_dict, inner_error_dict)
            elif not sweep_param_dict.get('value_range') == None:
                inner_error_dict = _validate_value_range(sweep_param_dict.get('value_range'), param_idx)
                # Either is_error means that both need to be is_error = True for merge
                if error_dict['is_error'] or inner_error_dict['is_error']:
                    error_dict['is_error'] = True
                    inner_error_dict['is_error'] = True
                fredutil.deep_merge(error_dict, inner_error_dict)
            elif not sweep_param_dict.get('value_dist') == None:
                inner_error_dict = _validate_value_dist(sweep_param_dict.get('value_dist'), param_idx)
                # Either is_error means that both need to be is_error = True for merge
                if error_dict['is_error'] or inner_error_dict['is_error']:
                    error_dict['is_error'] = True
                    inner_error_dict['is_error'] = True
                fredutil.deep_merge(error_dict, inner_error_dict)
            
        # Check for unrecognized keys
        expected_keys = ['param_name', 'value_list', 'value_range', 'value_dist']
        for sweep_param_dict_key in sweep_param_dict.keys():
            if sweep_param_dict_key not in expected_keys:
                if not error_dict['is_error']:
                    error_dict['is_error'] = True
    
                if error_dict.get('errored_items') == None:
                    error_dict['errored_items'] = {}
                
                if error_dict['errored_items'].get(param_idx) == None:
                    error_dict['errored_items'][param_idx] = {}
                    error_dict['errored_items'][param_idx]['UNRECOGNIZED'] = [sweep_param_dict_key]
                else:
                    if error_dict['errored_items'][param_idx].get('UNRECOGNIZED') == None:
                        error_dict['errored_items'][param_idx]['UNRECOGNIZED'] =  [sweep_param_dict_key]
                    else:
                        error_dict['errored_items'][param_idx]['UNRECOGNIZED'].append(sweep_param_dict_key)
        param_idx += 1
        
    return error_dict
    
def _validate_value_list(value_list: List, param_idx: int) -> Dict:
    '''
    Checks that value list contains either all numeric values or all string values
    
    Parameters
    ----------
    value_list : List
        A list of values for a particular parameter
    param_idx: int
        The index of the value_list in the sweep_params_list
        
    Returns
    -------
    Dict
        A dictionary with a minimum of key 'is_error' = False and any errors otherwise
    '''
    
    error_dict = {'is_error': False}
    if value_list == None or (isinstance(value_list, list) and len(value_list) == 0) or not isinstance(value_list, list):
        error_dict['is_error'] = True
        error_dict['errored_items'] = {}
        error_dict['errored_items'][param_idx] = {}
        if value_list == None:
            error_dict['errored_items'][param_idx]['value_list'] = 'NULL'
        elif isinstance(value_list, list) and len(value_list) == 0:
            error_dict['errored_items'][param_idx]['value_list'] = 'Zero Length List'
        else:
            error_dict['errored_items'][param_idx]['value_list'] = 'Not a List'
    else:
        value_list_idx = 0
        list_type = None
        for val in value_list:
            # Set the overall list type to be the type of the first item
            if value_list_idx == 0 and type(val) == str:
                list_type = 'string'
            elif value_list_idx == 0 and (type(val) == int or type(val) == float):
                list_type = 'numeric'
            elif value_list_idx == 0:
                error_dict['is_error'] = True
                error_dict['errored_items'] = {}
                error_dict['errored_items'][param_idx] = {}
                error_dict['errored_items'][param_idx]['value_list'] = {}
                error_dict['errored_items'][param_idx]['value_list']['value_list[0]'] = 'Must be String or Numeric (int or float) type'
                # Can't validate any of the rest of the list items since we don't know what type to expect
                break
            elif not(type(val) == str or type(val) == int or type(val) == float):
                if not error_dict['is_error']:
                    error_dict['is_error'] = True
                if error_dict.get('errored_items') == None:
                    error_dict['errored_items'] = {}
                if error_dict['errored_items'].get(param_idx) == None:
                    error_dict['errored_items'][param_idx] = {}
                if error_dict['errored_items'][param_idx].get('value_list') == None:
                    error_dict['errored_items'][param_idx]['value_list'] = {}
                    error_dict['errored_items'][param_idx]['value_list']['value_list[' + str(value_list_idx) + ']'] = 'Must be String or Numeric (int or float) type'
                else:
                    error_dict['errored_items'][param_idx]['value_list']['value_list[' + str(value_list_idx) + ']'] = 'Must be String or Numeric (int or float) type'
            elif type(val) == str and not list_type == 'string':
                if not error_dict['is_error']:
                    error_dict['is_error'] = True
                if error_dict.get('errored_items') == None:
                    error_dict['errored_items'] = {}
                if error_dict['errored_items'].get(param_idx) == None:
                    error_dict['errored_items'][param_idx] = {}
                if error_dict['errored_items'][param_idx].get('value_list') == None:
                    error_dict['errored_items'][param_idx]['value_list'] = {}
                    error_dict['errored_items'][param_idx]['value_list']['value_list[' + str(value_list_idx) + ']'] = 'String type but list items should all be Numeric'
                else:
                    error_dict['errored_items'][param_idx]['value_list']['value_list[' + str(value_list_idx) + ']'] = 'String type but list items should all be Numeric'
            elif (type(val) == int or type(val) == float) and not list_type == 'numeric':
                if not error_dict['is_error']:
                    error_dict['is_error'] = True
                if error_dict.get('errored_items') == None:
                    error_dict['errored_items'] = {}
                if error_dict['errored_items'].get(param_idx) == None:
                    error_dict['errored_items'][param_idx] = {}
                if error_dict['errored_items'][param_idx].get('value_list') == None:
                    error_dict['errored_items'][param_idx]['value_list'] = {}
                    error_dict['errored_items'][param_idx]['value_list']['value_list[' + str(value_list_idx) + ']'] = 'Numeric type but list items should all be String'
                else:
                    error_dict['errored_items'][param_idx]['value_list']['value_list[' + str(value_list_idx) + ']'] = 'Numeric type but list items should all be String'
            
            value_list_idx += 1
            
    return error_dict
    
    
def _validate_value_range(value_range: Dict, param_idx: int) -> Dict:
    '''
    Checks that value_range is a dictionary that meets all of the requirements of a data range
    
    Parameters
    ----------
    value_range : Dict
        A dictionary of items required for a range sweep
    param_idx: int
        The index of the value_range in the sweep_params_list
        
    Returns
    -------
    Dict
        A dictionary with a minimum of key 'is_error' = False and any errors otherwise
    '''
    
    error_dict = {'is_error': False}
    if value_range == None or not isinstance(value_range, dict):
        error_dict['is_error'] = True
        error_dict['errored_items'] = {}
        error_dict['errored_items'][param_idx] = {}
        if value_range == None:
            error_dict['errored_items'][param_idx]['value_range'] = 'NULL'
        else:
            error_dict['errored_items'][param_idx]['value_range'] = 'Not a Dictionary (JSON Object)'
    else:
        for required_key in ['min', 'max', 'step']:
            if required_key not in value_range.keys():
                # Check for required keys 'min', 'max', and 'step'
                if not error_dict['is_error']:
                    error_dict['is_error'] = True
                
                if error_dict.get('errored_items') == None:
                    error_dict['errored_items'] = {}
                
                if error_dict['errored_items'].get(param_idx) == None:
                    error_dict['errored_items'][param_idx] = {}
                    error_dict['errored_items'][param_idx]['value_range'] = {}
                    error_dict['errored_items'][param_idx]['value_range']['REQUIRED'] = [required_key]
                elif error_dict['errored_items'][param_idx].get('value_range') == None:
                    error_dict['errored_items'][param_idx]['value_range'] = {}
                    error_dict['errored_items'][param_idx]['value_range']['REQUIRED'] = [required_key]
                elif error_dict['errored_items'][param_idx]['value_range'].get('REQUIRED') == None:
                    error_dict['errored_items'][param_idx]['value_range']['REQUIRED'] = [required_key]
                else:
                    error_dict['errored_items'][param_idx]['value_range']['REQUIRED'].append(required_key)
            elif type(value_range[required_key]) != int and type(value_range[required_key]) != float:
                # Check for numeric values for keys 'min', 'max', and 'step'
                if not error_dict['is_error']:
                    error_dict['is_error'] = True
                
                if error_dict.get('errored_items') == None:
                    error_dict['errored_items'] = {}
                
                if error_dict['errored_items'].get(param_idx) == None:
                    error_dict['errored_items'][param_idx] = {}
                    error_dict['errored_items'][param_idx]['value_range'] = {}
                    error_dict['errored_items'][param_idx]['value_range']['NONNUMERIC_VALUE'] = [required_key]
                elif error_dict['errored_items'][param_idx].get('value_range') == None:
                    error_dict['errored_items'][param_idx]['value_range'] = {}
                    error_dict['errored_items'][param_idx]['value_range']['NONNUMERIC_VALUE'] = [required_key]
                elif error_dict['errored_items'][param_idx]['value_range'].get('NONNUMERIC_VALUE') == None:
                    error_dict['errored_items'][param_idx]['value_range']['NONNUMERIC_VALUE'] = [required_key]
                else:
                    error_dict['errored_items'][param_idx]['value_range']['NONNUMERIC_VALUE'].append(required_key)

        # Check for unrecognized keys
        expected_keys = ['min', 'max', 'step']
        for value_range_key in value_range.keys():
            if value_range_key not in expected_keys:
                if not error_dict['is_error']:
                    error_dict['is_error'] = True
    
                if error_dict.get('errored_items') == None:
                    error_dict['errored_items'] = {}
   
                if error_dict['errored_items'].get(param_idx) == None:
                    error_dict['errored_items'][param_idx] = {}
                    error_dict['errored_items'][param_idx]['value_range'] = {}
                    error_dict['errored_items'][param_idx]['value_range']['UNRECOGNIZED'] = [value_range_key]
                elif error_dict['errored_items'][param_idx].get('value_range') == None:
                    error_dict['errored_items'][param_idx]['value_range'] = {}
                    error_dict['errored_items'][param_idx]['value_range']['UNRECOGNIZED'] = [value_range_key]
                elif error_dict['errored_items'][param_idx]['value_range'].get('UNRECOGNIZED') == None:
                    error_dict['errored_items'][param_idx]['value_range']['UNRECOGNIZED'] = [value_range_key]
                else:
                    error_dict['errored_items'][param_idx]['value_range']['UNRECOGNIZED'].append(value_range_key)
                    
    # If we still have no errors, check validity of min, max, and step values
    if not error_dict['is_error']:
        if value_range['min'] >= value_range['max']:
            error_dict['is_error'] = True
            error_dict['errored_items'] = {}
            error_dict['errored_items'][param_idx] = {}
            error_dict['errored_items'][param_idx]['value_range'] = {}
            error_dict['errored_items'][param_idx]['value_range']['MIN|MAX'] = 'Min value [{}] is >= Max value [{}].'.format(value_range['min'], value_range['max'])
        if value_range['step'] <= 0:
            if not error_dict['is_error']:
                error_dict['is_error'] = True
                error_dict['errored_items'] = {}
                error_dict['errored_items'][param_idx] = {}
                error_dict['errored_items'][param_idx]['value_range'] = {}
                error_dict['errored_items'][param_idx]['value_range']['STEP'] = 'Step value [{}] must be positive.'.format(value_range['step'])
            else:
                error_dict['errored_items'][param_idx]['value_range']['STEP'] = 'Step value [{}] must be positive.'.format(value_range['step'])
    return error_dict
    
    
def _validate_value_dist(value_dist: Dict, param_idx: int) -> Dict:
    '''
    Checks that value_dist is a dictionary that meets all of the requirements of a data distribution
    
    Parameters
    ----------
    value_dist : Dict
        A dictionary of items required for a distribution sweep
    param_idx: int
        The index of the value_dist in the sweep_params_list
        
    Returns
    -------
    Dict
        A dictionary with a minimum of key 'is_error' = False and any errors otherwise
    '''
    
    error_dict = {'is_error': False}
    if value_dist == None or not isinstance(value_dist, dict):
        error_dict['is_error'] = True
        error_dict['errored_items'] = {}
        error_dict['errored_items'][param_idx] = {}
        if value_range == None:
            error_dict['errored_items'][param_idx]['value_dist'] = 'NULL'
        else:
            error_dict['errored_items'][param_idx]['value_dist'] = 'Not a Dictionary (JSON Object)'
    else:
        if 'count' not in value_dist.keys():
            error_dict['is_error'] = True
            error_dict['errored_items'] = {}
            error_dict['errored_items'][param_idx] = {}
            error_dict['errored_items'][param_idx]['value_dist'] = {}
            error_dict['errored_items'][param_idx]['value_dist']['REQUIRED'] = ['count']
        elif type(value_dist['count']) != int and type(value_dist['count']) != float:
            error_dict['is_error'] = True
            error_dict['errored_items'] = {}
            error_dict['errored_items'][param_idx] = {}
            error_dict['errored_items'][param_idx]['value_dist'] = {}
            error_dict['errored_items'][param_idx]['value_dist']['NONNUMERIC_VALUE'] = ['count']
        
        distribution_type_list = ['normal', 'lognormal', 'uniform', 'discrete_uniform']
        if 'type' not in value_dist.keys():
            if not error_dict['is_error']:
                error_dict['is_error'] = True
                
            if error_dict.get('errored_items') == None:
                error_dict['errored_items'] = {}
              
            if error_dict['errored_items'].get(param_idx) == None:
                error_dict['errored_items'][param_idx] = {}
                error_dict['errored_items'][param_idx]['value_dist'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['REQUIRED'] = ['type']
            elif error_dict['errored_items'][param_idx].get('value_dist') == None:
                error_dict['errored_items'][param_idx]['value_dist'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['REQUIRED'] = ['type']
            elif error_dict['errored_items'][param_idx]['value_dist'].get('REQUIRED') == None:
                error_dict['errored_items'][param_idx]['value_dist']['REQUIRED'] = ['type']
            else:
                error_dict['errored_items'][param_idx]['value_dist']['REQUIRED'].append('type')
        elif type(value_dist['type']) != str:
            if not error_dict['is_error']:
                error_dict['is_error'] = True
            
            if error_dict.get('errored_items') == None:
                error_dict['errored_items'] = {}
        
            if error_dict['errored_items'].get(param_idx) == None:
                error_dict['errored_items'][param_idx] = {}
                error_dict['errored_items'][param_idx]['value_dist'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['NONSTRING_VALUE'] = ['type']
            elif error_dict['errored_items'][param_idx].get('value_dist') == None:
                error_dict['errored_items'][param_idx]['value_dist'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['NONSTRING_VALUE'] = ['type']
            elif error_dict['errored_items'][param_idx]['value_dist'].get('NONSTRING_VALUE') == None:
                error_dict['errored_items'][param_idx]['value_dist']['NONSTRING_VALUE'] = ['type']
            else:
                error_dict['errored_items'][param_idx]['value_dist']['NONSTRING_VALUE'].append('type')
        elif type(value_dist['type']) == str and value_dist['type'] not in distribution_type_list:
            if not error_dict['is_error']:
                error_dict['is_error'] = True
            
            if error_dict.get('errored_items') == None:
                error_dict['errored_items'] = {}
        
            if error_dict['errored_items'].get(param_idx) == None:
                error_dict['errored_items'][param_idx] = {}
                error_dict['errored_items'][param_idx]['value_dist'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['type'] = 'Must be in [' + ','.join(distribution_type_list) + ']'
            elif error_dict['errored_items'][param_idx].get('value_dist') == None:
                error_dict['errored_items'][param_idx]['value_dist'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['type'] = 'Must be in [' + ','.join(distribution_type_list) + ']'
        elif type(value_dist['type']) == str and value_dist['type'] in distribution_type_list:
            inner_error_dict = SWEEP_VALUE_DIST_VALIDATION_FUNCTIONS[value_dist['type']](value_dist, param_idx)
            # Either is_error means that both need to be is_error = True for merge
            if error_dict['is_error'] or inner_error_dict['is_error']:
                error_dict['is_error'] = True
                inner_error_dict['is_error'] = True
            fredutil.deep_merge(error_dict, inner_error_dict)

    return error_dict
    
def _validate_normal_dist(value_dist: Dict, param_idx: int) -> Dict:
    '''
    Checks that value_dist is a dictionary that meets all of the requirements of a normal data distribution
    
    Parameters
    ----------
    value_dist : Dict
        A dictionary of items required for a normal distribution sweep
    param_idx: int
        The index of the value_dist in the sweep_params_list
        
    Returns
    -------
    Dict
        A dictionary with a minimum of key 'is_error' = False and any errors otherwise
    '''
    error_dict = {'is_error': False}
    for required_key in ['mean', 'stddev']:
        if required_key not in value_dist.keys():
            # Check for required keys 'mean' and 'stddev'
            if not error_dict['is_error']:
                error_dict['is_error'] = True
                
            if error_dict.get('errored_items') == None:
                error_dict['errored_items'] = {}
                
            if error_dict['errored_items'].get(param_idx) == None:
                error_dict['errored_items'][param_idx] = {}
                error_dict['errored_items'][param_idx]['value_dist'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['normal'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['normal']['REQUIRED'] = [required_key]
            elif error_dict['errored_items'][param_idx].get('value_dist') == None:
                error_dict['errored_items'][param_idx]['value_dist'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['normal'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['normal']['REQUIRED'] = [required_key]
            elif error_dict['errored_items'][param_idx]['value_dist'].get('normal') == None:
                error_dict['errored_items'][param_idx]['value_dist']['normal'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['normal']['REQUIRED'] = [required_key]
            else:
                error_dict['errored_items'][param_idx]['value_dist']['normal']['REQUIRED'].append(required_key)
        elif type(value_dist[required_key]) != int and type(value_dist[required_key]) != float:
                # Check for numeric values for keys 'mean' and 'stddev'
                if not error_dict['is_error']:
                    error_dict['is_error'] = True
                
                if error_dict.get('errored_items') == None:
                    error_dict['errored_items'] = {}
                
                if error_dict['errored_items'].get(param_idx) == None:
                    error_dict['errored_items'][param_idx] = {}
                    error_dict['errored_items'][param_idx]['value_dist'] = {}
                    error_dict['errored_items'][param_idx]['value_dist']['normal'] = {}
                    error_dict['errored_items'][param_idx]['value_dist']['normal']['NONNUMERIC_VALUE'] = [required_key]
                elif error_dict['errored_items'][param_idx].get('value_dist') == None:
                    error_dict['errored_items'][param_idx]['value_dist'] = {}
                    error_dict['errored_items'][param_idx]['value_dist']['normal'] = {}
                    error_dict['errored_items'][param_idx]['value_dist']['normal']['NONNUMERIC_VALUE'] = [required_key]
                elif error_dict['errored_items'][param_idx]['value_dist'].get('normal') == None:
                    error_dict['errored_items'][param_idx]['value_dist']['normal'] = {}
                    error_dict['errored_items'][param_idx]['value_dist']['normal']['NONNUMERIC_VALUE'] = [required_key]
                elif error_dict['errored_items'][param_idx]['value_dist']['normal'].get('NONNUMERIC_VALUE') == None:
                    error_dict['errored_items'][param_idx]['value_dist']['normal']['NONNUMERIC_VALUE'] = [required_key]
                else:
                    error_dict['errored_items'][param_idx]['value_dist']['normal']['NONNUMERIC_VALUE'].append(required_key)

    # Check for unrecognized keys
    expected_keys = ['count', 'type', 'min', 'max', 'mean', 'stddev']
    for value_dist_key in value_dist.keys():
        if value_dist_key not in expected_keys:
            if not error_dict['is_error']:
                error_dict['is_error'] = True
    
            if error_dict.get('errored_items') == None:
                error_dict['errored_items'] = {}
   
            if error_dict['errored_items'].get(param_idx) == None:
                error_dict['errored_items'][param_idx] = {}
                error_dict['errored_items'][param_idx]['value_dist'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['normal'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['normal']['UNRECOGNIZED'] = [value_dist_key]
            elif error_dict['errored_items'][param_idx].get('value_dist') == None:
                error_dict['errored_items'][param_idx]['value_dist'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['normal'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['normal']['UNRECOGNIZED'] = [value_dist_key]
            elif error_dict['errored_items'][param_idx]['value_dist'].get('normal') == None:
                error_dict['errored_items'][param_idx]['value_dist']['normal'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['normal']['UNRECOGNIZED'] = [value_dist_key]
            elif error_dict['errored_items'][param_idx]['value_dist']['normal'].get('UNRECOGNIZED') == None:
                error_dict['errored_items'][param_idx]['value_dist']['normal']['UNRECOGNIZED'] = [value_dist_key]
            else:
                error_dict['errored_items'][param_idx]['value_dist']['normal']['UNRECOGNIZED'].append(value_dist_key)
                    
    # If we still have no errors, check validity of min, max, and step values
    if not error_dict['is_error']:
        if not value_dist.get('min') == None:
            if not (type(value_dist['min']) == int or type(value_dist['min']) == float):
                error_dict['is_error'] = True
                error_dict['errored_items'] = {}
                error_dict['errored_items'][param_idx] = {}
                error_dict['errored_items'][param_idx]['value_dist'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['normal'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['normal']['NONNUMERIC_VALUE'] = ['min']
        if not value_dist.get('max') == None:
            if not (type(value_dist['max']) == int or type(value_dist['max']) == float):
                if not error_dict['is_error']:
                    error_dict['is_error'] = True
                    error_dict['errored_items'] = {}
                    error_dict['errored_items'][param_idx] = {}
                    error_dict['errored_items'][param_idx]['value_dist'] = {}
                    error_dict['errored_items'][param_idx]['value_dist']['normal'] = {}
                    error_dict['errored_items'][param_idx]['value_dist']['normal']['NONNUMERIC_VALUE'] = ['max']
                else:
                    error_dict['errored_items'][param_idx]['value_dist']['normal']['NONNUMERIC_VALUE'].append('max')
        if not (value_dist.get('min') == None and value_dist.get('max') == None):
            if value_dist['min'] >= value_dist['max']:
                if not error_dict['is_error']:
                    error_dict['is_error'] = True
                    error_dict['errored_items'] = {}
                    error_dict['errored_items'][param_idx] = {}
                    error_dict['errored_items'][param_idx]['value_dist'] = {}
                    error_dict['errored_items'][param_idx]['value_dist']['normal'] = {}
                    error_dict['errored_items'][param_idx]['value_dist']['normal']['MIN|MAX'] = 'Min value [{}] is >= Max value [{}].'.format(value_dist['min'], value_dist['max'])
                else:
                    error_dict['errored_items'][param_idx]['value_dist']['normal']['MIN|MAX'] = 'Min value [{}] is >= Max value [{}].'.format(value_dist['min'], value_dist['max'])
                    
    return error_dict
    
    
def _validate_lognormal_dist(value_dist: Dict, param_idx: int) -> Dict:
    '''
    Checks that value_dist is a dictionary that meets all of the requirements of a lognormal data distribution
    
    Parameters
    ----------
    value_dist : Dict
        A dictionary of items required for a lognormal distribution sweep
    param_idx: int
        The index of the value_dist in the sweep_params_list
        
    Returns
    -------
    Dict
        A dictionary with a minimum of key 'is_error' = False and any errors otherwise
    '''
    error_dict = {'is_error': False}
    for required_key in ['median', 'dispersion']:
        if required_key not in value_dist.keys():
            # Check for required keys 'mean' and 'stddev'
            if not error_dict['is_error']:
                error_dict['is_error'] = True
                
            if error_dict.get('errored_items') == None:
                error_dict['errored_items'] = {}
                
            if error_dict['errored_items'].get(param_idx) == None:
                error_dict['errored_items'][param_idx] = {}
                error_dict['errored_items'][param_idx]['value_dist'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['lognormal'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['lognormal']['REQUIRED'] = [required_key]
            elif error_dict['errored_items'][param_idx].get('value_dist') == None:
                error_dict['errored_items'][param_idx]['value_dist'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['lognormal'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['lognormal']['REQUIRED'] = [required_key]
            elif error_dict['errored_items'][param_idx]['value_dist'].get('lognormal') == None:
                error_dict['errored_items'][param_idx]['value_dist']['lognormal'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['lognormal']['REQUIRED'] = [required_key]
            else:
                error_dict['errored_items'][param_idx]['value_dist']['lognormal']['REQUIRED'].append(required_key)
        elif type(value_dist[required_key]) != int and type(value_dist[required_key]) != float:
                # Check for numeric values for keys 'mean' and 'stddev'
                if not error_dict['is_error']:
                    error_dict['is_error'] = True
                
                if error_dict.get('errored_items') == None:
                    error_dict['errored_items'] = {}
                
                if error_dict['errored_items'].get(param_idx) == None:
                    error_dict['errored_items'][param_idx] = {}
                    error_dict['errored_items'][param_idx]['value_dist'] = {}
                    error_dict['errored_items'][param_idx]['value_dist']['lognormal'] = {}
                    error_dict['errored_items'][param_idx]['value_dist']['lognormal']['NONNUMERIC_VALUE'] = [required_key]
                elif error_dict['errored_items'][param_idx].get('value_dist') == None:
                    error_dict['errored_items'][param_idx]['value_dist'] = {}
                    error_dict['errored_items'][param_idx]['value_dist']['lognormal'] = {}
                    error_dict['errored_items'][param_idx]['value_dist']['lognormal']['NONNUMERIC_VALUE'] = [required_key]
                elif error_dict['errored_items'][param_idx]['value_dist'].get('lognormal') == None:
                    error_dict['errored_items'][param_idx]['value_dist']['lognormal'] = {}
                    error_dict['errored_items'][param_idx]['value_dist']['lognormal']['NONNUMERIC_VALUE'] = [required_key]
                elif error_dict['errored_items'][param_idx]['value_dist']['lognormal'].get('NONNUMERIC_VALUE') == None:
                    error_dict['errored_items'][param_idx]['value_dist']['lognormal']['NONNUMERIC_VALUE'] = [required_key]
                else:
                    error_dict['errored_items'][param_idx]['value_dist']['lognormal']['NONNUMERIC_VALUE'].append(required_key)

    # Check for unrecognized keys
    expected_keys = ['count', 'type', 'min', 'max', 'median', 'dispersion']
    for value_dist_key in value_dist.keys():
        if value_dist_key not in expected_keys:
            if not error_dict['is_error']:
                error_dict['is_error'] = True
    
            if error_dict.get('errored_items') == None:
                error_dict['errored_items'] = {}
   
            if error_dict['errored_items'].get(param_idx) == None:
                error_dict['errored_items'][param_idx] = {}
                error_dict['errored_items'][param_idx]['value_dist'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['lognormal'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['lognormal']['UNRECOGNIZED'] = [value_dist_key]
            elif error_dict['errored_items'][param_idx].get('value_dist') == None:
                error_dict['errored_items'][param_idx]['value_dist'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['lognormal'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['lognormal']['UNRECOGNIZED'] = [value_dist_key]
            elif error_dict['errored_items'][param_idx]['value_dist'].get('lognormal') == None:
                error_dict['errored_items'][param_idx]['value_dist']['lognormal'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['lognormal']['UNRECOGNIZED'] = [value_dist_key]
            elif error_dict['errored_items'][param_idx]['value_dist']['lognormal'].get('UNRECOGNIZED') == None:
                error_dict['errored_items'][param_idx]['value_dist']['lognormal']['UNRECOGNIZED'] = [value_dist_key]
            else:
                error_dict['errored_items'][param_idx]['value_dist']['lognormal']['UNRECOGNIZED'].append(value_dist_key)
                    
    # If we still have no errors, check validity of min, max, and step values
    if not error_dict['is_error']:
        if not value_dist.get('min') == None:
            if not (type(value_dist['min']) == int or type(value_dist['min']) == float):
                error_dict['is_error'] = True
                error_dict['errored_items'] = {}
                error_dict['errored_items'][param_idx] = {}
                error_dict['errored_items'][param_idx]['value_dist'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['lognormal'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['lognormal']['NONNUMERIC_VALUE'] = ['min']
        if not value_dist.get('max') == None:
            if not (type(value_dist['max']) == int or type(value_dist['max']) == float):
                if not error_dict['is_error']:
                    error_dict['is_error'] = True
                    error_dict['errored_items'] = {}
                    error_dict['errored_items'][param_idx] = {}
                    error_dict['errored_items'][param_idx]['value_dist'] = {}
                    error_dict['errored_items'][param_idx]['value_dist']['lognormal'] = {}
                    error_dict['errored_items'][param_idx]['value_dist']['lognormal']['NONNUMERIC_VALUE'] = ['max']
                else:
                    error_dict['errored_items'][param_idx]['value_dist']['lognormal']['NONNUMERIC_VALUE'].append('max')
                    
        if not (value_dist.get('min') == None and value_dist.get('max') == None):
            if value_dist['min'] >= value_dist['max']:
                if not error_dict['is_error']:
                    error_dict['is_error'] = True
                    error_dict['errored_items'] = {}
                    error_dict['errored_items'][param_idx] = {}
                    error_dict['errored_items'][param_idx]['value_dist'] = {}
                    error_dict['errored_items'][param_idx]['value_dist']['lognormal'] = {}
                    error_dict['errored_items'][param_idx]['value_dist']['lognormal']['MIN|MAX'] = 'Min value [{}] is >= Max value [{}].'.format(value_dist['min'], value_dist['max'])
                else:
                    error_dict['errored_items'][param_idx]['value_dist']['lognormal']['MIN|MAX'] = 'Min value [{}] is >= Max value [{}].'.format(value_dist['min'], value_dist['max'])
                    
    return error_dict
    
    
def _validate_uniform_dist(value_dist: Dict, param_idx: int) -> Dict:
    '''
    Checks that value_dist is a dictionary that meets all of the requirements of a uniform data distribution
    
    Parameters
    ----------
    value_dist : Dict
        A dictionary of items required for a uniform distribution sweep
    param_idx: int
        The index of the value_dist in the sweep_params_list
        
    Returns
    -------
    Dict
        A dictionary with a minimum of key 'is_error' = False and any errors otherwise
    '''
    error_dict = {'is_error': False}
    for required_key in ['min', 'max']:
        if required_key not in value_dist.keys():
            # Check for required keys 'mean' and 'stddev'
            if not error_dict['is_error']:
                error_dict['is_error'] = True
                
            if error_dict.get('errored_items') == None:
                error_dict['errored_items'] = {}
                
            if error_dict['errored_items'].get(param_idx) == None:
                error_dict['errored_items'][param_idx] = {}
                error_dict['errored_items'][param_idx]['value_dist'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['uniform'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['uniform']['REQUIRED'] = [required_key]
            elif error_dict['errored_items'][param_idx].get('value_dist') == None:
                error_dict['errored_items'][param_idx]['value_dist'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['uniform'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['uniform']['REQUIRED'] = [required_key]
            elif error_dict['errored_items'][param_idx]['value_dist'].get('uniform') == None:
                error_dict['errored_items'][param_idx]['value_dist']['uniform'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['uniform']['REQUIRED'] = [required_key]
            else:
                error_dict['errored_items'][param_idx]['value_dist']['uniform']['REQUIRED'].append(required_key)
        elif type(value_dist[required_key]) != int and type(value_dist[required_key]) != float:
                # Check for numeric values for keys 'mean' and 'stddev'
                if not error_dict['is_error']:
                    error_dict['is_error'] = True
                
                if error_dict.get('errored_items') == None:
                    error_dict['errored_items'] = {}
                
                if error_dict['errored_items'].get(param_idx) == None:
                    error_dict['errored_items'][param_idx] = {}
                    error_dict['errored_items'][param_idx]['value_dist'] = {}
                    error_dict['errored_items'][param_idx]['value_dist']['uniform'] = {}
                    error_dict['errored_items'][param_idx]['value_dist']['uniform']['NONNUMERIC_VALUE'] = [required_key]
                elif error_dict['errored_items'][param_idx].get('value_dist') == None:
                    error_dict['errored_items'][param_idx]['value_dist'] = {}
                    error_dict['errored_items'][param_idx]['value_dist']['uniform'] = {}
                    error_dict['errored_items'][param_idx]['value_dist']['uniform']['NONNUMERIC_VALUE'] = [required_key]
                elif error_dict['errored_items'][param_idx]['value_dist'].get('uniform') == None:
                    error_dict['errored_items'][param_idx]['value_dist']['uniform'] = {}
                    error_dict['errored_items'][param_idx]['value_dist']['uniform']['NONNUMERIC_VALUE'] = [required_key]
                elif error_dict['errored_items'][param_idx]['value_dist']['uniform'].get('NONNUMERIC_VALUE') == None:
                    error_dict['errored_items'][param_idx]['value_dist']['uniform']['NONNUMERIC_VALUE'] = [required_key]
                else:
                    error_dict['errored_items'][param_idx]['value_dist']['uniform']['NONNUMERIC_VALUE'].append(required_key)

    # Check for unrecognized keys
    expected_keys = ['count', 'type', 'min', 'max']
    for value_dist_key in value_dist.keys():
        if value_dist_key not in expected_keys:
            if not error_dict['is_error']:
                error_dict['is_error'] = True
    
            if error_dict.get('errored_items') == None:
                error_dict['errored_items'] = {}
   
            if error_dict['errored_items'].get(param_idx) == None:
                error_dict['errored_items'][param_idx] = {}
                error_dict['errored_items'][param_idx]['value_dist'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['uniform'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['uniform']['UNRECOGNIZED'] = [value_dist_key]
            elif error_dict['errored_items'][param_idx].get('value_dist') == None:
                error_dict['errored_items'][param_idx]['value_dist'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['uniform'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['uniform']['UNRECOGNIZED'] = [value_dist_key]
            elif error_dict['errored_items'][param_idx]['value_dist'].get('uniform') == None:
                error_dict['errored_items'][param_idx]['value_dist']['uniform'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['uniform']['UNRECOGNIZED'] = [value_dist_key]
            elif error_dict['errored_items'][param_idx]['value_dist']['uniform'].get('UNRECOGNIZED') == None:
                error_dict['errored_items'][param_idx]['value_dist']['uniform']['UNRECOGNIZED'] = [value_dist_key]
            else:
                error_dict['errored_items'][param_idx]['value_dist']['uniform']['UNRECOGNIZED'].append(value_dist_key)
                    
    # If we still have no errors, check validity of min and max
    if not error_dict['is_error']:
        if value_dist['min'] >= value_dist['max']:
            error_dict['is_error'] = True
            error_dict['errored_items'] = {}
            error_dict['errored_items'][param_idx] = {}
            error_dict['errored_items'][param_idx]['value_dist'] = {}
            error_dict['errored_items'][param_idx]['value_dist']['uniform'] = {}
            error_dict['errored_items'][param_idx]['value_dist']['uniform']['MIN|MAX'] = 'Min value [{}] is >= Max value [{}].'.format(value_dist['min'], value_dist['max'])
                    
    return error_dict
    
    
def _validate_discrete_uniform_dist(value_dist: Dict, param_idx: int) -> Dict:
    '''
    Checks that value_dist is a dictionary that meets all of the requirements of a discrete uniform distribution
    
    Parameters
    ----------
    value_dist : Dict
        A dictionary of items required for a discrete uniform distribution sweep
    param_idx: int
        The index of the value_dist in the sweep_params_list
        
    Returns
    -------
    Dict
        A dictionary with a minimum of key 'is_error' = False and any errors otherwise
    '''
    error_dict = {'is_error': False}
    for required_key in ['pick_list']:
        if required_key not in value_dist.keys():
            # Check for required keys 'mean' and 'stddev'
            if not error_dict['is_error']:
                error_dict['is_error'] = True
                
            if error_dict.get('errored_items') == None:
                error_dict['errored_items'] = {}
                
            if error_dict['errored_items'].get(param_idx) == None:
                error_dict['errored_items'][param_idx] = {}
                error_dict['errored_items'][param_idx]['value_dist'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['uniform'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['uniform']['REQUIRED'] = [required_key]
            elif error_dict['errored_items'][param_idx].get('value_dist') == None:
                error_dict['errored_items'][param_idx]['value_dist'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['uniform'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['uniform']['REQUIRED'] = [required_key]
            elif error_dict['errored_items'][param_idx]['value_dist'].get('uniform') == None:
                error_dict['errored_items'][param_idx]['value_dist']['uniform'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['uniform']['REQUIRED'] = [required_key]
            else:
                error_dict['errored_items'][param_idx]['value_dist']['uniform']['REQUIRED'].append(required_key)
                
    # Now make sure that pick_list is a list and that it is either string or numeric values (all should be the same type)
    if not error_dict['is_error']:
        pick_list = value_dist['pick_list']
        if pick_list == None or (isinstance(pick_list, list) and len(pick_list) == 0) or not isinstance(pick_list, list):
            error_dict['is_error'] = True
            error_dict['errored_items'] = {}
            error_dict['errored_items'][param_idx] = {}
            if value_list == None:
                error_dict['errored_items'][param_idx]['pick_list'] = 'NULL'
            elif isinstance(value_list, list) and len(value_list) == 0:
                error_dict['errored_items'][param_idx]['pick_list'] = 'Zero Length List'
            else:
                error_dict['errored_items'][param_idx]['pick_list'] = 'Not a List'
        else:
            pick_list_idx = 0
            list_type = None
            for val in pick_list:
                # Set the overall list type to be the type of the first item
                if pick_list_idx == 0 and type(val) == str:
                    list_type = 'string'
                elif pick_list_idx == 0 and (type(val) == int or type(val) == float):
                    list_type = 'numeric'
                elif pick_list_idx == 0:
                    error_dict['is_error'] = True
                    error_dict['errored_items'] = {}
                    error_dict['errored_items'][param_idx] = {}
                    error_dict['errored_items'][param_idx]['pick_list'] = {}
                    error_dict['errored_items'][param_idx]['pick_list']['pick_list[0]'] = 'Must be String or Numeric (int or float) type'
                    # Can't validate any of the rest of the list items since we don't know what type to expect
                    break
                elif not(type(val) == str or type(val) == int or type(val) == float):
                    if not error_dict['is_error']:
                        error_dict['is_error'] = True
                    if error_dict.get('errored_items') == None:
                        error_dict['errored_items'] = {}
                    if error_dict['errored_items'].get(param_idx) == None:
                        error_dict['errored_items'][param_idx] = {}
                    if error_dict['errored_items'][param_idx].get('pick_list') == None:
                        error_dict['errored_items'][param_idx]['pick_list'] = {}
                        error_dict['errored_items'][param_idx]['pick_list']['pick_list[' + str(pick_list_idx) + ']'] = 'Must be String or Numeric (int or float) type'
                    else:
                        error_dict['errored_items'][param_idx]['pick_list']['pick_list[' + str(pick_list_idx) + ']'] = 'Must be String or Numeric (int or float) type'
                elif type(val) == str and not list_type == 'string':
                    if not error_dict['is_error']:
                        error_dict['is_error'] = True
                    if error_dict.get('errored_items') == None:
                        error_dict['errored_items'] = {}
                    if error_dict['errored_items'].get(param_idx) == None:
                        error_dict['errored_items'][param_idx] = {}
                    if error_dict['errored_items'][param_idx].get('pick_list') == None:
                        error_dict['errored_items'][param_idx]['pick_list'] = {}
                        error_dict['errored_items'][param_idx]['pick_list']['pick_list[' + str(pick_list_idx) + ']'] = 'String type but list items should all be Numeric'
                    else:
                        error_dict['errored_items'][param_idx]['pick_list']['pick_list[' + str(pick_list_idx) + ']'] = 'String type but list items should all be Numeric'
                elif (type(val) == int or type(val) == float) and not list_type == 'numeric':
                    if not error_dict['is_error']:
                        error_dict['is_error'] = True
                    if error_dict.get('errored_items') == None:
                        error_dict['errored_items'] = {}
                    if error_dict['errored_items'].get(param_idx) == None:
                        error_dict['errored_items'][param_idx] = {}
                    if error_dict['errored_items'][param_idx].get('pick_list') == None:
                        error_dict['errored_items'][param_idx]['pick_list'] = {}
                        error_dict['errored_items'][param_idx]['pick_list']['pick_list[' + str(pick_list_idx) + ']'] = 'Numeric type but list items should all be String'
                    else:
                        error_dict['errored_items'][param_idx]['pick_list']['pick_list[' + str(pick_list_idx) + ']'] = 'Numeric type but list items should all be String'
            
                pick_list_idx += 1

    # Check for unrecognized keys
    expected_keys = ['count', 'type', 'pick_list']
    for value_dist_key in value_dist.keys():
        if value_dist_key not in expected_keys:
            if not error_dict['is_error']:
                error_dict['is_error'] = True
    
            if error_dict.get('errored_items') == None:
                error_dict['errored_items'] = {}
   
            if error_dict['errored_items'].get(param_idx) == None:
                error_dict['errored_items'][param_idx] = {}
                error_dict['errored_items'][param_idx]['value_dist'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['uniform'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['uniform']['UNRECOGNIZED'] = [value_dist_key]
            elif error_dict['errored_items'][param_idx].get('value_dist') == None:
                error_dict['errored_items'][param_idx]['value_dist'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['uniform'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['uniform']['UNRECOGNIZED'] = [value_dist_key]
            elif error_dict['errored_items'][param_idx]['value_dist'].get('uniform') == None:
                error_dict['errored_items'][param_idx]['value_dist']['uniform'] = {}
                error_dict['errored_items'][param_idx]['value_dist']['uniform']['UNRECOGNIZED'] = [value_dist_key]
            elif error_dict['errored_items'][param_idx]['value_dist']['uniform'].get('UNRECOGNIZED') == None:
                error_dict['errored_items'][param_idx]['value_dist']['uniform']['UNRECOGNIZED'] = [value_dist_key]
            else:
                error_dict['errored_items'][param_idx]['value_dist']['uniform']['UNRECOGNIZED'].append(value_dist_key)
                    
    return error_dict


def _validate_sweep_parameters(base_param_file: str, sweep_df: pd.DataFrame) -> Dict:
    '''
    Make sure that each parameter name (the dataframe header) is actually a parameter in the
    base parameter file
    
    Parameters
    ----------
    base_param_file: str
        The filename that is the base parameter file that will be altered for each realization
    sweep_df: pd.DataFrame
        The dataframe that has a header with each parameter name
        
    Returns
    -------
    Dict
        A dictionary with either 'is_error' = False and a dictionary of the lines to be replaced, 'param_pos_dict',
        or a dictionary with 'is_error' = True and 'error_message' = {whatever caused the error}
    '''
    result_dict = {'is_error': False}
    
    with open(base_param_file) as my_file:
        base_param_file_line_arr = my_file.readlines()
    # Go through the list in reverse to find the LAST occurence of each parameter
    base_param_file_line_arr.reverse()
    
    param_position = {}
    missing_params = []
    for column_header_str in sweep_df.columns:
        regex_str = r'^\s*' + column_header_str + r'\s*=\s*(\w*|(?!-0?(\.0+)?$)-?(0|[1-9]\d*)?(\.\d+)?(?<=\d))\s*$'
        matcher = re.compile(regex_str)
        logging.debug('Regex = [' + regex_str + ']')
        found = False
        pos = 0
        for base_param_file_line in base_param_file_line_arr:
            if matcher.match(base_param_file_line):
                found = True
                param_position[column_header_str] = pos
                break
            pos += 1
        if not found:
            missing_params.append(column_header_str)

    if len(missing_params) == 1:
        result_dict['is_error'] = True
        result_dict['error_message'] = 'The following parameter does not exist in the base_param_file, ' + base_param_file + ': ' + str(missing_params)
    elif len(missing_params) > 1:
        result_dict['is_error'] = True
        result_dict['error_message'] = 'The following parameters do not exist in the base_param_file, ' + base_param_file + ': ' + str(missing_params)
    else:
        result_dict['param_pos_dict'] = param_position
    return result_dict


def _create_dataframe_from_validated_sweep_json(sweep_json: Dict, force: bool = False) -> pd.DataFrame:
    '''
    Using the validated sweep_json (in Python Dictionary format) create a dataframe that combines all of the parameter sweeps in a cartesian product
    If the total combination count is greater than the THRESHOLD constant and not forced, raise an exception
    
    Parameters
    ----------
    sweep_json : Dict
        A dictionary of the validated sweep information
    force: bool
        A flag that can be used to force the dataframe to be larger than the THRESHOLD
        
    Returns
    -------
    pd.DataFrame
        A dataframe of all combinations of the various parameters
        
    Raises
    ------
    frederr.FredSweepCartesianProductError
        If the Count of the cartesian product exceeds the THRESHOLD and it is not forced to exceed that limit
    '''
    column_header_list = []
    column_data_list = []
    for sweep_param_dict in sweep_json['sweep_params']:
        column_header_list.append(sweep_param_dict['param_name'])
        if sweep_param_dict.get('value_list') != None:
            column_data_list.append(sweep_param_dict['value_list'])
        elif sweep_param_dict.get('value_range') != None:
            column_data_list.append(_create_list_from_value_range(sweep_param_dict['value_range']))
        elif sweep_param_dict.get('value_dist') != None:
            column_data_list.append(_create_list_from_value_dist(sweep_param_dict['value_dist']))
            
    idx = 0
    cross_count = 0
    return_dataframe = None
    for column_data in column_data_list:
        df1 = pd.DataFrame({column_header_list[idx]: column_data})
        if idx == 0:
            cross_count = len(column_data)
            if cross_count > THRESHOLD and not force:
                raise frederr.FredSweepCartesianProductError(THRESHOLD)
            return_dataframe = df1
        else:
            cross_count = cross_count * len(column_data)
            if cross_count > THRESHOLD and not force:
                raise frederr.FredSweepCartesianProductError(THRESHOLD)
            return_dataframe = return_dataframe.merge(df1, how='cross')
        idx += 1
        
    return return_dataframe


def _create_list_from_value_range(value_range_json: Dict) -> List:
    '''
    Using the validated value_range_json (in Python Dictionary format) create a List of all the values from the range
    
    Parameters
    ----------
    value_range_json : Dict
        A dictionary of the validated value range information
        
    Returns
    -------
    List
        A List of all of the values created from the value range
        
    '''
    return_list = []
    current_value = value_range_json['min']
    max = value_range_json['max']
    step = value_range_json['step']
    loop_counter = 0 # We are guarding against the step size being too small to finish the while loop
    while current_value <= max and loop_counter < MAX_LOOPS:
        return_list.append(current_value)
        current_value += step
        loop_counter += 1
    
    return return_list


def _create_list_from_value_dist(value_dist_json: Dict) -> List:
    '''
    Using the validated value_dist_json (in Python Dictionary format) create a List of all the values from the approriate distribution
        
    Parameters
    ----------
    value_dist_json : Dict
        A dictionary of the validated value distribution information
        
    Returns
    -------
    List
        A List of all of the values created from the appropriate value distribution
        
    Notes
    -----
        The actual distribution functions are mapped in the dictionary SWEEP_VALUE_DIST_VALIDATION_FUNCTIONS
    '''
    return_list = []
    count = value_dist_json['count']
    min = None
    if value_dist_json.get('min') != None:
        min = value_dist_json['min']
        
    max = None
    if value_dist_json.get('max') != None:
        max = value_dist_json['max']
        
    if value_dist_json['type'] == 'normal':
        mean = value_dist_json['mean']
        stddev = value_dist_json['stddev']
        return_list = _create_list_from_normal_dist(count, mean, stddev, min, max)
    elif value_dist_json['type'] == 'lognormal':
        median = value_dist_json['median']
        dispersion = value_dist_json['dispersion']
        return_list = _create_list_from_lognormal_dist(count, median, dispersion, min, max)
    elif value_dist_json['type'] == 'uniform':
        return_list = _create_list_from_uniform_dist(count, min, max)
    elif value_dist_json['type'] == 'discrete_uniform':
        return_list = _create_list_from_discrete_uniform_dist(count, value_dist_json['pick_list'])
    
    return return_list
    
    
def _create_list_from_normal_dist(count: int, mean: float, stddev: float, min: float = None, max: float = None) -> List:
    '''
    Create a normal distribtion from the mean and standard deviation. Return a list of (at most) count items selected from that distribution.
    If min and/or max are set, then if the selected value is outside of the bounds, pick again.
    
    Parameters
    ----------
    count : int
        The max size of the returned list
    mean : float
        The mean of the Normal distribution
    stddev : float
        The standard deviation of the Normal distribution
    min : float
        The minimum value that can be selected without redraw
    max : float
        The maximum value that can be selected without redraw
        
    Returns
    -------
    List
        A List of (at most) count values selected from the Normal distribution
        
    Notes
    -----
        The constant, MAX_REDRAW, is used to assure that we don't pick endlessly
    '''
    return_list = []
    redraw_count = 0
    while redraw_count < MAX_REDRAW and len(return_list) < count:
        x = np.random.default_rng().normal(loc=mean, scale=stddev, size=count)
        redraw_count += 1
        for test_x in x:
            if min == None and max == None:
                return_list.append(test_x)
            elif min == None and test_x <= max:
                return_list.append(test_x)
            elif max == None and test_x >= min:
                return_list.append(test_x)
            elif test_x >= min and test_x <= max:
                return_list.append(test_x)
                
    return return_list


def _create_list_from_lognormal_dist(count: int, median: float, dispersion: float, min: float = None, max: float = None) -> List:
    '''
    Create a lognormal distribtion from the median and dispersion. Return a list of (at most) count items selected from that distribution.
    If min and/or max are set, then if the selected value is outside of the bounds, pick again.
    
    Parameters
    ----------
    count : int
        The max size of the returned list
    median : float
        The median of the Lognormal distribution
    dispersion : float
        The dispersion of the Lognormal distribution
    min : float
        The minimum value that can be selected without redraw
    max : float
        The maximum value that can be selected without redraw
        
    Returns
    -------
    List
        A List of (at most) count values selected from the Logormal distribution
        
    Notes
    -----
        The constant, MAX_REDRAW, is used to assure that we don't pick endlessly
        
    '''
    if median <= 0.0 or dispersion <= 0.0:
        return []
        
    return_list = []
    redraw_count = 0
    mu = math.log(median)
    sigma = math.log(dispersion)
    while redraw_count < MAX_REDRAW and len(return_list) < count:
        z_norm = np.random.default_rng().normal(loc=0.0, scale=1.0, size=count)
        x_lognorm = []
        for z in z_norm:
            x_lognorm.append(math.exp(mu + sigma * z))
        redraw_count += 1
        for test_x in x_lognorm:
            if min == None and max == None:
                if len(return_list) < count:
                    return_list.append(test_x)
            elif min == None and test_x <= max:
                if len(return_list) < count:
                    return_list.append(test_x)
            elif max == None and test_x >= min:
                if len(return_list) < count:
                    return_list.append(test_x)
            elif test_x >= min and test_x <= max:
                if len(return_list) < count:
                    return_list.append(test_x)
                
    return return_list
    
    
def _create_list_from_uniform_dist(count: int, min: float, max: float) -> List:
    '''
    Use numpy to create a Uniform distribution from [min, max]
    Return a list of (at most) count items selected from that distribution.
    
    Parameters
    ----------
    count : int
        The size of the returned list
    min : float
        The minimum value that can be selected
    max : float
        The maximum value that can be selected
        
    Returns
    -------
    List
        A List of count values selected from Uniform distribution
    '''
    return np.random.default_rng().uniform(min, max, size=count).tolist()
        
        
def _create_list_from_discrete_uniform_dist(count: int, pick_list: list) -> list:
    '''
    Return a list of count randomly selected items from pick_list
    
    Parameters
    ----------
    count : int
        The max size of the returned list
    pick_list:
        The list of items to be selected from
        
    Returns
    -------
    List
        A List of (at most) count values randomly selected from the pick_list
        
    Notes
    -----
        Since, we will be using the returned list in a merge join (Cartesian Product) it makes no sense to return duplicates.
        Therefore, we immediately create unique_list from the unique values of pick_list
        
        If the count is greater than or equal to the size of unique_list, then just return unique_list.
        
        When we randomly select from unique_list, we pop() the values, so that we can select without replacement.
    '''
    return_list = []
    # insert the list to the set
    list_set = set(pick_list)
    # convert the set to the list
    unique_list = (list(list_set))
    
    if len(unique_list) <= count:
        return_list = unique_list
    else:
        for i in range(count):
            list_idx = np.random.default_rng().integers(len(unique_list))
            return_list.append(unique_list.pop(list_idx))
            
    return return_list
   
def _create_model_files_from_sweep_parameters(base_param_file: str, model_file_dir: str, sweep_df: pd.DataFrame, param_pos_dict: Dict) -> dict:
    '''
    Sets up the model file directory if it is not already created.
    Loop over the dataframe and create a .fred file for each combination in the dataframe
    
    Parameters
    ----------
    base_param_file : str
        The filename of the base parameter file
    model_file_dir : str
        The directory where the files will be created
    sweep_df: pd.DataFrame
        The dataframe that has a header with each parameter name and rows with all of the combinations for the values for each parameter
    param_pos_dict: Dict
        A dictionary with each parameter name as a key and the position (in the reverse list of file lines) of the parameter to be replaced
    
    Raises
    ------
    OSError
        If an error occurs in the creation of the directories folders
    '''
    # Set up model file directory
    model_file_dir_path = Path(model_file_dir)
    fredutil.create_dir(model_file_dir_path)
    with open(base_param_file) as my_file:
        base_param_file_line_list = my_file.readlines()
    
    # Go through the list in reverse so that we change the LAST value of each parameter
    base_param_file_line_list.reverse()
    replacement_file_line_list = []
    pos = 0
    for base_param_file_line in base_param_file_line_list:
        # Check to see if the current line is one that was flagged to be replaced
        if pos in param_pos_dict.values():
            key_str = list(param_pos_dict.keys())[list(param_pos_dict.values()).index(pos)]
            # Figure out which column needs to be replaced
            field_counter = 0
            for column_header_str in sweep_df.columns:
                field_counter += 1
                if column_header_str == key_str:
                    replacement_file_line_list.append(column_header_str + ' = _REPLACE_FIELD_' + str(field_counter) + '_')
                    break
                
        else:
            replacement_file_line_list.append(base_param_file_line.rstrip())
        pos += 1
    # Put the replacement file list back in the correct order
    replacement_file_line_list.reverse()
    
    # Write a temp file in the directory with the replacement text
    tempfile = model_file_dir + '/' + TEMP_MODEL_FILE
    with open(tempfile, 'w') as fp:
        fp.write('\n'.join(replacement_file_line_list) + '\n')
        
    # For each row in the dataframe, create a model file where we replace the _REPLACE_FIELD_[n]_ with the corresponding column data
    tempfile_str = ''
    # Read in the tempfile
    with open(tempfile, 'r') as file:
        tempfile_str = file.read()
    
    for index, row in sweep_df.iterrows():
        replace_file_str = tempfile_str
        # Create a new file with each of the parameter values replaced by the appropriate column
        field_counter = 0
        new_filename = model_file_dir + '/'
        for column_header_str in sweep_df.columns:
            field_counter += 1
            if type(row[column_header_str]) == np.float64 or type(row[column_header_str]) == float:
                field_str = '%.3f' % row[column_header_str]
            else:
                field_str = str(row[column_header_str])
                
            if field_counter == len(sweep_df.columns):
                new_filename += column_header_str + '-' + field_str
            else:
                new_filename += column_header_str + '-' + field_str + '_'
            replace_file_str = replace_file_str.replace('_REPLACE_FIELD_' + str(field_counter) + '_', field_str)
        new_filename += '.fred'
        # Write the new file model file
        with open(new_filename, 'w') as file:
            file.write(replace_file_str)
        
    # Delete the temp file
    if os.path.isfile(tempfile):
        os.remove(tempfile)

SWEEP_VALUE_DIST_VALIDATION_FUNCTIONS = {
    'normal': _validate_normal_dist,
    'lognormal': _validate_lognormal_dist,
    'uniform': _validate_uniform_dist,
    'discrete_uniform': _validate_discrete_uniform_dist,
}

            
if __name__ == '__main__':
    main()
