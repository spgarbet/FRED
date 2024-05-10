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

from pathlib import Path

import os
import random
import re
import sys
import uuid


# Update the Python search Path
file = Path(__file__).resolve()
package_root_directory_path = Path(file.parents[1], 'src')
sys.path.append(str(package_root_directory_path.resolve()))

import argparse
import logging
from typing import Literal

from fred_job import fred_job
from fred_delete import fred_delete
from fred_create_model_sweep_files import fred_create_model_sweep_files
from fredpy import fredcsv
from fredpy.util import fredutil
from fredpy.util import constants
from fredpy.genetic_algorithm_calibration import parameter, genetic_calibration_param_group, genetic_calibration

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
    print('Installation instructions can be found at https://pypi.org/project/pandas/.')
    sys.exit(constants.EXT_CD_ERR)

TEMP_MODEL_FILE = 'temp.fred'
THRESHOLD = 0.5

def main():

#    usage = '''
#    Typical usage example:
#
#    $ fred_log.py mykey
#    STARTED: Thu Jul 15 15:13:33 2021
#    FINISHED: Thu Jul 15 15:13:39 2021
#
#    Last 10 lines in LOG file [{log file path}]:
#    day 152 report population took 0.000002 seconds
#    day 152 maxrss 140296
#    ...
#    '''
#
#    # Creates and sets up the parser
#    parser = argparse.ArgumentParser(
#        description='Prints details about the given FRED Job.',
#        epilog=usage,
#        formatter_class=argparse.RawDescriptionHelpFormatter)
#    parser.add_argument('key', help='the FRED key to look up')
#    parser.add_argument('-r',
#                        '--run',
#                        default=1,
#                        type=int,
#                        help='the run number get the log from (default=1)')
#    parser.add_argument(
#        '-n',
#        '--number',
#        default=10,
#        type=int,
#        help='display the last n lines of the LOG file (default=10)')
#    parser.add_argument('--loglevel',
#                        default='ERROR',
#                        choices=constants.LOGGING_LEVELS,
#                        help='set the logging level of this script')
#
#    # Parses the arguments
#    args = parser.parse_args()
#    fred_key = args.key
#    run_num = args.run
#    num_lines = args.number
#    loglevel = args.loglevel
#
#    # Set up the logger
#    logging.basicConfig(format='%(levelname)s: %(message)s')
#    logging.getLogger().setLevel(loglevel)
#
#    result = fred_log(fred_key, run_num, num_lines)




# Initialization variables
#Global pStart_to_PrescriptionUser
#pStart_to_PrescriptionUser = 0.009
    pStart_to_PrescriptionUser_1 = parameter.BreedableParameter('pStart_to_PrescriptionUser', 0.009, 0.00000001, 0.01, 0.2)
    pStart_to_PrescriptionUser_2 = parameter.BreedableParameter('pStart_to_PrescriptionUser', 0.0079915800, 0.00000001, 0.01, 0.2)

#Global pStart_to_MisUser
#pStart_to_MisUser = 0.083
    pStart_to_MisUser_1 = parameter.BreedableParameter('pStart_to_MisUser', 0.083, 0.00000001, 0.1, 0.2)
    pStart_to_MisUser_2 = parameter.BreedableParameter('pStart_to_MisUser', 0.0445629450, 0.00000001, 0.1, 0.2)

#Global pStart_to_OUD
#pStart_to_OUD = 0.002
    pStart_to_OUD_1 = parameter.BreedableParameter('pStart_to_OUD', 0.002, 0.00000001, 0.06, 0.2)
    pStart_to_OUD_2 = parameter.BreedableParameter('pStart_to_OUD', 0.0325408170, 0.00000001, 0.06, 0.2)

#Global pStart_to_RX
#pStart_to_RX = 0.083
    pStart_to_RX_1 = parameter.BreedableParameter('pStart_to_RX', 0.083, 0.00000001, 0.1, 0.2)
    pStart_to_RX_2 = parameter.BreedableParameter('pStart_to_RX', 0.0354355520, 0.00000001, 0.1, 0.2)

# State Transition Values
#Global pNonUser_to_PrescriptionUser
#pNonUser_to_PrescriptionUser = 0.010
    pNonUser_to_PrescriptionUser_1 = parameter.BreedableParameter('pNonUser_to_PrescriptionUser', 0.009211400, 0.00000001, 0.01, 0.2)
    pNonUser_to_PrescriptionUser_2 = parameter.BreedableParameter('pNonUser_to_PrescriptionUser', 0.0079211400, 0.00000001, 0.01, 0.2)

#Global pNonUser_to_MisUser
#pNonUser_to_MisUser = 0.004
    pNonUser_to_MisUser_1 = parameter.BreedableParameter('pNonUser_to_MisUser', 0.004, 0.00000001, 0.006, 0.2)
    pNonUser_to_MisUser_2 = parameter.BreedableParameter('pNonUser_to_MisUser', 0.002, 0.00000001, 0.006, 0.2)

#Global pNonUser_to_OUD
#pNonUser_to_OUD = 0.0008
    pNonUser_to_OUD_1 = parameter.BreedableParameter('pNonUser_to_OUD', 0.0008, 0.00000001, 0.001, 0.2)
    pNonUser_to_OUDr_2 = parameter.BreedableParameter('pNonUser_to_OUD', 0.00045, 0.00000001, 0.001, 0.2)

#Global pPrescriptionUser_to_NonUser
#pPrescriptionUser_to_NonUser = 0.846
    pPrescriptionUser_to_NonUser_1 = parameter.BreedableParameter('pPrescriptionUser_to_NonUser', 0.846, 0.00000001, 1.0, 0.2)
    pPrescriptionUser_to_NonUser_2 = parameter.BreedableParameter('pPrescriptionUser_to_NonUser', 0.915, 0.00000001, 1.0, 0.2)

#Global pPrescriptionUser_to_MisUser
#pPrescriptionUser_to_MisUser = 0.085
    pPrescriptionUser_to_MisUser_1 = parameter.BreedableParameter('pPrescriptionUser_to_MisUser', 0.085, 0.00000001, 0.1, 0.2)
    pPrescriptionUser_to_MisUser_2 = parameter.BreedableParameter('pPrescriptionUser_to_MisUser', 0.06, 0.00000001, 0.1, 0.2)

#Global pPrescriptionUser_to_OUD
#pPrescriptionUser_to_OUD = 0.004
    pPrescriptionUser_to_OUD_1 = parameter.BreedableParameter('pPrescriptionUser_to_OUD', 0.004, 0.00000001, 0.025, 0.2)
    pPrescriptionUser_to_OUD_2 = parameter.BreedableParameter('pPrescriptionUser_to_OUD', 0.0227, 0.00000001, 0.03, 0.2)

#Global p_Death
#p_Death = 0.000238
    p_Death_1 = parameter.BreedableParameter('p_Death', 0.000238, 0.00000001, 0.0005, 0.2)
    p_Death_2 = parameter.BreedableParameter('p_Death', 0.000338, 0.00000001, 0.0005, 0.2)

#Global pMisUser_NonUser
#pMisUser_NonUser = 0.010
    pMisUser_NonUser_1 = parameter.BreedableParameter('pMisUser_NonUser', 0.01, 0.00000001, 0.05, 0.2)
    pMisUser_NonUser_2 = parameter.BreedableParameter('pMisUser_NonUser', 0.049, 0.00000001, 0.05, 0.2)
    
#Global pMisUser_OUD
#pMisUser_OUD = 0.004
    pMisUser_OUD_1 = parameter.BreedableParameter('pMisUser_OUD', 0.004, 0.00000001, 0.015, 0.2)
    pMisUser_OUD_2 = parameter.BreedableParameter('pMisUser_OUD', 0.014, 0.00000001, 0.015, 0.2)
    
#Global pMisUser_DeathOD
#pMisUser_DeathOD = 0.0000001305
    pMisUser_DeathOD_1 = parameter.BreedableParameter('pMisUser_DeathOD', 0.0000001305, 0.00000001, 0.000002, 0.2)
    pMisUser_DeathOD_2 = parameter.BreedableParameter('pMisUser_DeathOD', 0.0000002305, 0.00000001, 0.000002, 0.2)
    
#Global pOUD_to_NonUser
#pOUD_to_NonUser = 0.011
    pOUD_to_NonUser_1 = parameter.BreedableParameter('pOUD_to_NonUser', 0.011, 0.00000001, 0.08, 0.2)
    pOUD_to_NonUser_2 = parameter.BreedableParameter('pOUD_to_NonUser', 0.0262, 0.00000001, 0.08, 0.2)
    
#Global pOUD_to_MisUser
#pOUD_to_MisUser = 0.029
    pOUD_to_MisUser_1 = parameter.BreedableParameter('pOUD_to_MisUser', 0.029, 0.00000001, 0.1, 0.2)
    pOUD_to_MisUser_2 = parameter.BreedableParameter('pOUD_to_MisUser', 0.023, 0.00000001, 0.1, 0.2)

#Global pOUD_to_RX
#pOUD_to_RX = 0.088
    pOUD_to_RX_1 = parameter.BreedableParameter('pOUD_to_RX', 0.088, 0.00000001, 0.1, 0.2)
    pOUD_to_RX_2 = parameter.BreedableParameter('pOUD_to_RX', 0.0249107290, 0.00000001, 0.1, 0.2)
    
#Global pOUD_to_DeathOD
#pOUD_to_DeathOD = 0.001
    pOUD_to_DeathOD_1 = parameter.BreedableParameter('pOUD_to_DeathOD', 0.001, 0.00000001, 0.002, 0.2)
    pOUD_to_DeathOD_2 = parameter.BreedableParameter('pOUD_to_DeathOD', 0.0004952390, 0.00000001, 0.002, 0.2)
    
#Global pOUD_to_Death
#pOUD_to_Death = 0.013
    pOUD_to_Death_1 = parameter.BreedableParameter('pOUD_to_Death', 0.013, 0.00000001, 0.02, 0.2)
    pOUD_to_Death_2 = parameter.BreedableParameter('pOUD_to_Death', 0.003818, 0.00000001, 0.02, 0.2)
    
#Global pRX_to_OUD
#pRX_to_OUD = 0.0049
    pRX_to_OUD_1 = parameter.BreedableParameter('pRX_to_OUD', 0.0049, 0.00000001, 0.007, 0.2)
    pRX_to_OUD_2 = parameter.BreedableParameter('pRX_to_OUD', 0.0069, 0.00000001, 0.007, 0.2)
    
#Global pRX_to_NonUser
#pRX_to_NonUser = 0.030
    pRX_to_NonUser_1 = parameter.BreedableParameter('pRX_to_NonUser', 0.030, 0.00000001, 0.1, 0.2)
    pRX_to_NonUser_2 = parameter.BreedableParameter('pRX_to_NonUser', 0.01, 0.00000001, 0.1, 0.2)
    
    
    calibration_dir = 'CALIBRATION_MODELS'
    base_param_file = 'OpioidBaseline.fred'
    max_iterations = 5
    trials_per_iteration = 10
    genetic_calibrations = []
    
    original_gc = genetic_calibration.GeneticCalibration()
    
    original_gc.param_group.append(pStart_to_PrescriptionUser_1)
    original_gc.param_group.append(pStart_to_MisUser_1)
    original_gc.param_group.append(pStart_to_OUD_1)
    original_gc.param_group.append(pStart_to_RX_1)
    original_gc.param_group.append(pNonUser_to_PrescriptionUser_1)
    original_gc.param_group.append(pNonUser_to_MisUser_1)
    original_gc.param_group.append(pPrescriptionUser_to_NonUser_1)
    original_gc.param_group.append(pPrescriptionUser_to_MisUser_1)
    original_gc.param_group.append(pPrescriptionUser_to_OUD_1)
    original_gc.param_group.append(p_Death_1)
    original_gc.param_group.append(pMisUser_NonUser_1)
    original_gc.param_group.append(pMisUser_OUD_1)
    original_gc.param_group.append(pMisUser_DeathOD_1)
    original_gc.param_group.append(pOUD_to_NonUser_1)
    original_gc.param_group.append(pOUD_to_MisUser_1)
    original_gc.param_group.append(pOUD_to_RX_1)
    original_gc.param_group.append(pOUD_to_DeathOD_1)
    original_gc.param_group.append(pOUD_to_Death_1)
    original_gc.param_group.append(pRX_to_OUD_1)
    original_gc.param_group.append(pRX_to_NonUser_1)
    
    # Create original parameter sets
    for i in range(trials_per_iteration):
        gc = genetic_calibration.GeneticCalibration()
        gc.param_group = original_gc.param_group.randomize()
        gc.targets = {
          'opioid_deaths': 53, #tot_opioid_deaths,
          'opioid_use_disorder': 237#tot_opioid_use_disorder
        }

        gc.target_to_parameters_weights = {
          'opioid_deaths': {'pOUD_to_DeathOD': 0.5,
                            'pMisUser_DeathOD': 0.5},
          'opioid_use_disorder': {'pStart_to_OUD': 0.4,
                                  'pNonUser_to_OUD': 0.1,
                                  'pPrescriptionUser_to_OUD': 0.1,
                                  'pMisUser_OUD': 0.2,
                                  'pRX_to_OUD': 0.2}
        }
        
        genetic_calibrations.append(gc)
    
    
    # Run until either error less that threshhold or reached max_iterations
    min_error = sys.float_info.max
    
    iteration_counter = 1
    for i in range(max_iterations):

        sweep_df = _get_sweep_dataframe_from_genetic_calibrations(genetic_calibrations)
        param_pos_dict = _validate_sweep_parameters(base_param_file, sweep_df)['param_pos_dict']
        # Setup the sweep models from the calibration parameters
        genetic_calibrations = _create_model_files_from_genetic_calibrations(base_param_file, calibration_dir, sweep_df, param_pos_dict, genetic_calibrations)

        # Execute the FRED jobs
        _execute_fred_jobs(genetic_calibrations, calibration_dir)
        
        for genetic_cal in genetic_calibrations:
            genetic_cal.evaluate(_get_target_values_from_genetic_calibration(genetic_cal.fred_key))

        print('--------------------------------------------------------------------------------')
        genetic_calibrations.sort()
        print('Targets: ' + str(genetic_calibrations[0].targets))
        print('Outcomes: ' + str(genetic_calibrations[0].outcomes))
        print('FRED key: ' + genetic_calibrations[0].fred_key)
        print('individual_error_contributions: ' + str(genetic_calibrations[0].evaluate_individual_error_contributions()))
        min_error = genetic_calibrations[0].evaluated_error
        for genetic_cal in genetic_calibrations:
            print(genetic_cal.evaluated_error)
        print('--------------------------------------------------------------------------------')
        

        fred_key_list = []
        for genetic_cal in genetic_calibrations:
            fred_key_list.append(genetic_cal.fred_key)
        
        if min_error < THRESHOLD:
            print(genetic_calibrations[0])
            break
            
        # Every third iteration adjust the parameters
        if iteration_counter % 2 == 0:
            genetic_calibrations = _perform_weighted_parameter_adjustments(genetic_calibrations)
        else:
            genetic_calibrations = _perform_genetic_mixing(genetic_calibrations)
        iteration_counter += 1
        
        if i + 1 < max_iterations:
            _delete_fred_jobs(fred_key_list)
        

    sys.exit(1)


def _execute_fred_jobs(genetic_calibrations: list, calibration_dir: str):
    for item_gc in genetic_calibrations:
        fred_filename = calibration_dir + '/' + item_gc.fred_key + '.fred'
        fred_job(fred_filename, 0, 3, 4, item_gc.fred_key, True)
        
def _delete_fred_jobs(fred_key_list: list):
    for fred_key in fred_key_list:
        fred_delete(fred_key, True)

def _get_sweep_dataframe_from_genetic_calibrations(genetic_calibrations: list) -> pd.DataFrame():
    ret_df = pd.DataFrame()
    for genetic_cal in genetic_calibrations:
        df_dictionary = pd.DataFrame(genetic_cal.param_group.get_as_dictionary(), index=[0])
        ret_df = pd.concat([ret_df, df_dictionary], ignore_index=True)
    return ret_df
    
def _perform_genetic_mixing(genetic_calibrations: list) -> list:
    ret_genetic_calibrations = []
    return_list_size = len(genetic_calibrations)
    if not return_list_size == 10:
        raise IndexError('Genetic Calibration List must be exactly ten items')
        
    genetic_calibrations.sort()
    ret_genetic_calibrations.append(genetic_calibrations[0])
    ret_genetic_calibrations.append(genetic_calibrations[0].breed(genetic_calibrations[1]))
    ret_genetic_calibrations.append(genetic_calibrations[0].breed(genetic_calibrations[2]))
    ret_genetic_calibrations.append(genetic_calibrations[0].breed(genetic_calibrations[3]))
    ret_genetic_calibrations.append(genetic_calibrations[1].breed(genetic_calibrations[2]))
    ret_genetic_calibrations.append(genetic_calibrations[1].breed(genetic_calibrations[3]))
    ret_genetic_calibrations.append(genetic_calibrations[1].breed(genetic_calibrations[4]))
    ret_genetic_calibrations.append(genetic_calibrations[2].breed(genetic_calibrations[3]))
    ret_genetic_calibrations.append(genetic_calibrations[2].breed(genetic_calibrations[4]))
    ret_genetic_calibrations.append(genetic_calibrations[3].breed(genetic_calibrations[4]))
    return ret_genetic_calibrations


def _perform_weighted_parameter_adjustments(genetic_calibrations: list) -> list:
    ret_genetic_calibrations = []
    return_list_size = len(genetic_calibrations)
    if not return_list_size == 10:
        raise IndexError('Genetic Calibration List must be exactly ten items')

    genetic_calibrations.sort()
    ret_genetic_calibrations.append(genetic_calibrations[0])
    ret_genetic_calibrations.append(genetic_calibrations[1])
    ret_genetic_calibrations.append(genetic_calibrations[0].gradient_update_to_target())
    ret_genetic_calibrations.append(genetic_calibrations[1].gradient_update_to_target())
    ret_genetic_calibrations.append(genetic_calibrations[2].gradient_update_to_target())
    ret_genetic_calibrations.append(genetic_calibrations[3].gradient_update_to_target())
    ret_genetic_calibrations.append(genetic_calibrations[4].gradient_update_to_target())
    ret_genetic_calibrations.append(genetic_calibrations[5].gradient_update_to_target())
    ret_genetic_calibrations.append(genetic_calibrations[6].gradient_update_to_target())
    ret_genetic_calibrations.append(genetic_calibrations[7].gradient_update_to_target())
 
    return ret_genetic_calibrations
        

def _validate_sweep_parameters(base_param_file: str, sweep_df: pd.DataFrame) -> dict:
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


    
    
def _create_model_files_from_genetic_calibrations(base_param_file: str, model_file_dir: str, sweep_df: pd.DataFrame, param_pos_dict: dict, genetic_calibrations: list) -> list:
    '''
#    Sets up the model file directory if it is not already created.
#    Loop over the dataframe and create a .fred file for each combination in the dataframe
#    
#    Parameters
#    ----------
#    base_param_file : str
#        The filename of the base parameter file
#    model_file_dir : str
#        The directory where the files will be created
#    sweep_df: pd.DataFrame
#        The dataframe that has a header with each parameter name and rows with all of the combinations for the values for each parameter
#    param_pos_dict: Dict
#        A dictionary with each parameter name as a key and the position (in the reverse list of file lines) of the parameter to be replaced
#    
#    Raises
#    ------
#    OSError
#        If an error occurs in the creation of the directories folders
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

    print(param_pos_dict.values())
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
                    found = True
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
    
    ret_genetic_calibrations = []
    genetic_calibrations_idx = 0 # the index of the genetic_calibration that we are working on
    for index, row in sweep_df.iterrows():
        replace_file_str = tempfile_str
        # Create a new file with each of the parameter values replaced by the appropriate column
        field_counter = 0
        fred_key = str(uuid.uuid4())
        new_filename = model_file_dir + '/' + fred_key + '.fred'
        
        parameter_str = ''
        for column_header_str in sweep_df.columns:
            field_counter += 1
            if type(row[column_header_str]) == np.float64 or type(row[column_header_str]) == float:
                field_str = '%.3f' % row[column_header_str]
            else:
                field_str = str(row[column_header_str])
                
            replace_file_str = replace_file_str.replace('_REPLACE_FIELD_' + str(field_counter) + '_', field_str)
        
        ret_genetic_calibrations.append(genetic_calibrations[genetic_calibrations_idx])
        ret_genetic_calibrations[-1].fred_key = fred_key
        genetic_calibrations_idx += 1
        
        # Write the new file model file
        with open(new_filename, 'w') as file:
            file.write(replace_file_str)
        
    # Delete the temp file
    if os.path.isfile(tempfile):
        os.remove(tempfile)
        
    return ret_genetic_calibrations
    
def _get_target_values_from_genetic_calibration(fred_key: str) -> dict:
    fred_output = fredcsv.FredOuputCsvData(fred_key)
    df_tot_opioid_deaths = fred_output.get_csv_as_dataframe('ORU.totDeath')
    tot_opioid_deaths = df_tot_opioid_deaths['MEAN'].iat[-1]

    df_tot_opioid_use_disorder = fred_output.get_csv_as_dataframe('ORU.totOUD')
    tot_opioid_use_disorder = df_tot_opioid_use_disorder['MEAN'].iat[-1]
    
    return {
      "opioid_deaths": tot_opioid_deaths,
      "opioid_use_disorder": tot_opioid_use_disorder
    }

if __name__ == '__main__':
    main()
