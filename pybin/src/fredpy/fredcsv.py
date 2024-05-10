"""Module containing the FRED specific errors.

This module contains classes for handling the csv data produced by FRED

Examples
--------

>>> from fredpy import fredcsv
>>> import os
>>> fred_home_dir = os.environ.get('FRED_HOME')
>>> if fred_home_dir == None:
...     raise frederr.FredHomeUnsetError()
...
Traceback (most recent call last):
  File "<stdin>", line 2, in <module>
fredpy.frederr.FredHomeUnsetError: Environment Variable, FRED_HOME, is not set. Please set it to the location of FRED home directory.
"""

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
import logging
from pathlib import Path
from typing import Literal, Self

from fredpy.util import fredutil
from fredpy import frederr

try:
    import pandas as pd
except ImportError:
    print('This script requires the pandas module.')
    print('Installation instructions can be found at https://pypi.org/project/pandas/.')
    sys.exit(constants.EXT_CD_ERR)

class FredCsvError(Exception):
    pass

class FredOuputCsvData:
    '''
    FredOuputCsvData class

    This is a class to hold the information needed to gather CSV data for a FRED job specified by a given fred_key
    '''
    def __init__(self: Self, fred_key: str, frequency: Literal['DAILY', 'WEEKLY'] = 'DAILY'):
        '''
        Default Constructor
        
        Parameters
        ----------
        fred_key : str
            The FRED Job key to locate plot data
        frequency : Literal['DAILY', 'WEEKLY']
            The data frequency for the csv file (DAILY or WEEKLY)
            
        '''
        self.__fred_key = fred_key
        self.__frequency = frequency
        self.__available_vars = self.find_available_variables(fred_key)
        
    @property
    def fred_key(self: Self):
        return self.__fred_key

    @fred_key.setter
    def fred_key(self: Self, value):
        raise frederr.ReadOnlyPropertyError("fred_key")

    @property
    def frequency(self: Self):
        return self.__frequency

    @frequency.setter
    def frequency(self: Self, value):
        raise frederr.ReadOnlyPropertyError("frequency")
        
    @property
    def available_vars(self: Self):
        return self.__available_vars

    @available_vars.setter
    def available_vars(self: Self, value):
        raise frederr.ReadOnlyPropertyError("available_vars", False)
        
    def find_available_variables(self: Self, fred_key: str) -> list:
        '''
        Find the available variables for the FRED job described by this object's fred_key
        
        Returns
        -------
        List
            A List of all of the variables that are available for the FRED job specified by this object's fred_key
        '''
        
        fred_job_out_dir_str = None
        var_file_str = None
        
        ret_list = []
        try:
            fred_job_out_dir_str = fredutil.get_fred_job_out_dir_str(fred_key)
            var_file_path = Path(fred_job_out_dir_str, 'PLOT', 'VARS')
            var_file_str = str(var_file_path.resolve())
            
            with open(var_file_str, 'r') as f:
                for line in f:
                    ret_list.append(line.strip())
                
            
        except (frederr.FredHomeUnsetError, FileNotFoundError,
                IsADirectoryError, NotADirectoryError) as e:
            logging.error(e)
            raise e
            
        return ret_list
        
        
    def get_csv_as_dataframe(self: Self, fred_variable: str) -> pd.DataFrame:
        fred_job_out_dir_str = None
        var_file_str = None
        all_csv_dir_str = None
        try:
            fred_job_out_dir_str = fredutil.get_fred_job_out_dir_str(self.fred_key)
            var_file_path = Path(fred_job_out_dir_str, 'PLOT', 'VARS')
            var_file_str = str(var_file_path.resolve())
            var_csv_file_path = Path(fred_job_out_dir_str, 'PLOT', self.frequency, fred_variable + '.csv')
            var_csv_file_str = str(var_csv_file_path.resolve())
            # Read CSV file as a dataframe
            df_variable_name = pd.read_csv(var_csv_file_str)
        except (frederr.FredHomeUnsetError, FileNotFoundError,
                IsADirectoryError, NotADirectoryError) as e:
            logging.error(e)
            raise e
            
        return df_variable_name

    def __str__(self: Self):
        return '{{fred_key: {0}, frequency: {1}, available_vars: {2}}}'.format(self.__fred_key, self.__frequency, self.__available_vars)
    
def fred_get_csv_stats_by_var(fred_key: str, frequency_dir: Literal['DAILY', 'WEEKLY'], selected_index: int, filename: str) -> Literal[0, 2]:
    '''
    Creates a summary csv file for all variables in the PLOT/{DAILY  or WEEKLY} directory for a given KEY
    
    The csv files are located in FRED_RESULTS/JOB/{id}/OUT/PLOT/{frequency_dir},
    where frequency_dir will either be DAILY or WEEKLY. A list of valid variable names
    for that Job can be viewed in the VARS file located in the PLOT directory. If
    one of the csv filenames does not match a variable in the VARS file for a key,
    that file will be skipped, allowing the summarizing of other csv files.
    
    Parameters
    ----------
    fred_key : str
        The FRED Job key to locate plot data
    frequency_dir : Literal['DAILY', 'WEEKLY']
        The data frequency for the csv file (DAILY or WEEKLY)
    selected_index : int
        The index of a specific row to summarize (-1 is none)
    filename : str
        The csv file to create
    
    Returns
    -------
    Literal[0, 2]
        The exit status
    '''

    fred_job_out_dir_str = None
    var_file_str = None
    all_csv_dir_str = None
    try:
        fred_job_out_dir_str = fredutil.get_fred_job_out_dir_str(fred_key)
        var_file_path = Path(fred_job_out_dir_str, 'PLOT', 'VARS')
        var_file_str = str(var_file_path.resolve())
        all_csv_dir_path = Path(fred_job_out_dir_str, 'PLOT', frequency_dir)
        all_csv_dir_str = str(all_csv_dir_path.resolve())
    except (frederr.FredHomeUnsetError, FileNotFoundError,
            IsADirectoryError, NotADirectoryError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    if fred_job_out_dir_str == None or all_csv_dir_str == None:
        print('There was a problem finding the directory used for Key: {0}.'.format(key))
        return constants.EXT_CD_NRM
    
    available_vars = []
    # Here are the available variables
    with open(var_file_str) as file:
        while line := file.readline():
            available_vars.append(line.rstrip())

    # get a list of all CSV files in the PLOT/DAILY or PLOT/WEEKLY folder
    file_list = glob.glob('{0}/*.csv'.format(all_csv_dir_str), recursive = False)

    
    # Create data frame to which the summary stats will be written
    if frequency_dir == 'DAILY':
        if selected_index < 0:
            summary_stats = {'Variable Name': [],
                             'PeakMean': [],
                             'PeakStddev': [],
                             'PeakMedian': [],
                             'PeakMin': [],
                             'PeakMax': [],
                             'PeakDay': [],
                             'FinalMean': [],
                             'FinalStddev': [],
                             'FinalMedian': [],
                             'FinalMin': [],
                             'FinalMax': []}
        else:
            summary_stats = {'Variable Name': [],
                             'PeakMean': [],
                             'PeakStddev': [],
                             'PeakMedian': [],
                             'PeakMin': [],
                             'PeakMax': [],
                             'PeakDay': [],
                             'Day{0}Mean'.format(selected_index): [],
                             'Day{0}Stddev'.format(selected_index): [],
                             'Day{0}Median'.format(selected_index): [],
                             'Day{0}Min'.format(selected_index): [],
                             'Day{0}Max'.format(selected_index): [],
                             'FinalMean': [],
                             'FinalStddev': [],
                             'FinalMedian': [],
                             'FinalMin': [],
                             'FinalMax': []}
    else:
        if selected_index < 0:
            summary_stats = {'Variable Name': [],
                             'PeakMean': [],
                             'PeakStddev': [],
                             'PeakMedian': [],
                             'PeakMin': [],
                             'PeakMax': [],
                             'PeakWeek': [],
                             'FinalMean': [],
                             'FinalStddev': [],
                             'FinalMedian': [],
                             'FinalMin': [],
                             'FinalMax': []}
        else:
            summary_stats = {'Variable Name': [],
                             'PeakMean': [],
                             'PeakStddev': [],
                             'PeakMedian': [],
                             'PeakMin': [],
                             'PeakMax': [],
                             'PeakWeek': [],
                             'Week{0}Mean'.format(selected_index): [],
                             'Week{0}Stddev'.format(selected_index): [],
                             'Week{0}Median'.format(selected_index): [],
                             'Week{0}Min'.format(selected_index): [],
                             'Week{0}Max'.format(selected_index): [],
                             'FinalMean': [],
                             'FinalStddev': [],
                             'FinalMedian': [],
                             'FinalMin': [],
                             'FinalMax': []}

                     
    summary_stats = pd.DataFrame(summary_stats)
 
    # Next, loop through the file names
    for file_name in file_list:
    
        # Get df names based on file names (strip the extension)
        file_variable_name = os.path.splitext(os.path.basename(file_name))[0]
        
        if file_variable_name not in available_vars:
            logging.debug('Variable {0} not found'.format(file_variable_name))
            continue
   
        # Read CSV files and store it in the dictionary dfs
        df_variable_name = pd.read_csv(file_name)
        
        # Find the peak index (first occurence of highest value for MEAN)
        idx = 0
        cur_max = -1
        for i in range(len(df_variable_name)):
            if df_variable_name.loc[i, 'MEAN'] > cur_max:
                cur_max = df_variable_name.loc[i, 'MEAN']
                idx = i
                
        peak_mean = df_variable_name.loc[idx, 'MEAN']
        peak_std = df_variable_name.loc[idx, 'STD']
        peak_median = df_variable_name.loc[idx, 'MED']
        peak_min = df_variable_name.loc[idx, 'MIN']
        peak_max = df_variable_name.loc[idx, 'MAX']
        peak_index = df_variable_name.loc[idx, 'INDEX']
        
        final_mean = df_variable_name['MEAN'].iloc[-1]
        final_std = df_variable_name['STD'].iloc[-1]
        final_median = df_variable_name['MED'].iloc[-1]
        final_min = df_variable_name['MIN'].iloc[-1]
        final_max = df_variable_name['MAX'].iloc[-1]
            
        if frequency_dir == 'DAILY':
            if selected_index < 0:
                summary_stats_row = {'Variable Name': [file_variable_name],
                                     'PeakMean': [peak_mean],
                                     'PeakStddev': [peak_std],
                                     'PeakMedian': [peak_median],
                                     'PeakMin': [peak_min],
                                     'PeakMax': [peak_max],
                                     'PeakDay': [peak_index],
                                     'FinalMean': [final_mean],
                                     'FinalStddev': [final_std],
                                     'FinalMedian': [final_median],
                                     'FinalMin': [final_min],
                                     'FinalMax': [final_max]}
            else:
                try:
                    idx_mean = df_variable_name.loc[selected_index, 'MEAN']
                    idx_std = df_variable_name.loc[selected_index, 'STD']
                    idx_median = df_variable_name.loc[selected_index, 'MED']
                    idx_min = df_variable_name.loc[selected_index, 'MIN']
                    idx_max = df_variable_name.loc[selected_index, 'MAX']
                
                except (KeyError) as e:
                    logging.debug("Index is out of range (Max index is {0})".format(df_variable_name.shape[0] - 1))
                    idx_mean = np.nan
                    idx_std = np.nan
                    idx_median = np.nan
                    idx_min = np.nan
                    idx_max = np.nan
                
                summary_stats_row = {'Variable Name': [file_variable_name],
                                     'PeakMean': [peak_mean],
                                     'PeakStddev': [peak_std],
                                     'PeakMedian': [peak_median],
                                     'PeakMin': [peak_min],
                                     'PeakMax': [peak_max],
                                     'PeakDay': [peak_index],
                                     'Day{0}Mean'.format(selected_index): [idx_mean],
                                     'Day{0}Stddev'.format(selected_index): [idx_std],
                                     'Day{0}Median'.format(selected_index): [idx_median],
                                     'Day{0}Min'.format(selected_index): [idx_min],
                                     'Day{0}Max'.format(selected_index): [idx_max],
                                     'FinalMean': [final_mean],
                                     'FinalStddev': [final_std],
                                     'FinalMedian': [final_median],
                                     'FinalMin': [final_min],
                                     'FinalMax': [final_max]}
            
        else:
            if selected_index < 0:
                summary_stats_row = {'Variable Name': [file_variable_name],
                                     'PeakMean': [peak_mean],
                                     'PeakStddev': [peak_std],
                                     'PeakMedian': [peak_median],
                                     'PeakMin': [peak_min],
                                     'PeakMax': [peak_max],
                                     'PeakDay': [peak_index],
                                     'FinalMean': [final_mean],
                                     'FinalStddev': [final_std],
                                     'FinalMedian': [final_median],
                                     'FinalMin': [final_min],
                                     'FinalMax': [final_max]}
            else:
                try:
                    idx_mean = df_variable_name.loc[selected_index, 'MEAN']
                    idx_std = df_variable_name.loc[selected_index, 'STD']
                    idx_median = df_variable_name.loc[selected_index, 'MED']
                    idx_min = df_variable_name.loc[selected_index, 'MIN']
                    idx_max = df_variable_name.loc[selected_index, 'MAX']
                
                except (KeyError) as e:
                    logging.debug("Index is out of range (Max index is {0})".format(df_variable_name.shape[0] - 1))
                    idx_mean = np.nan
                    idx_std = np.nan
                    idx_median = np.nan
                    idx_min = np.nan
                    idx_max = np.nan
                
                summary_stats_row = {'Variable Name': [file_variable_name],
                                     'PeakMean': [peak_mean],
                                     'PeakStddev': [peak_std],
                                     'PeakMedian': [peak_median],
                                     'PeakMin': [peak_min],
                                     'PeakMax': [peak_max],
                                     'PeakWeek': [str(peak_index).replace(".", "|")],
                                     'Week{0}Mean'.format(selected_index): [idx_mean],
                                     'Week{0}Stddev'.format(selected_index): [idx_std],
                                     'Week{0}Median'.format(selected_index): [idx_median],
                                     'Week{0}Min'.format(selected_index): [idx_min],
                                     'Week{0}Max'.format(selected_index): [idx_max],
                                     'FinalMean': [final_mean],
                                     'FinalStddev': [final_std],
                                     'FinalMedian': [final_median],
                                     'FinalMin': [final_min],
                                     'FinalMax': [final_max]}
                                     
        summary_stats = pd.concat([summary_stats, pd.DataFrame(summary_stats_row)], ignore_index=True)
   
    # save summary stats as a csv file
    summary_stats.to_csv(filename, na_rep='NaN', index=False)
    
#    def __init__(self, allowable_threshold, msg=''):
#        if msg == '':
#            msg = f'The row count of the parameter combinations is greater than the allowable threshold: [{allowable_threshold}]'
#        super(FredError, self).__init__(msg)
        

#class FredSweepJsonError(FredError):
#    """Exception raised when the JSON sweep file is invalid
#    
#    """
#    
#    def __init__(self, filename, reason, msg=''):
#        if msg == '':
#            msg = f'The .json file [{filename}] did not pass validation: {reason}.'
#        super(FredError, self).__init__(msg)
#
#class FredSweepParamError(FredError):
#    """Exception raised when the sweep is trying to change values for parameters that don't exist in the base file
#    
#    """
#    
#    def __init__(self, msg):
#        super(FredError, self).__init__(msg)
#
#class FredFileSizeError(FredError):
#    """Exception raised when the a FRED file does not have the number of lines that we would expect.
#    
#    Commonly FRED writes files with a single line which is then able to be read as `cat filename` into a variable.
#    """
#    
#    def __init__(self, filename, msg=''):
#        if msg == '':
#            msg = f'The file [{filename}] should have exactly one line.'
#        super(FredError, self).__init__(msg)
#        
#class FredHomeUnsetError(FredError):
#    """Exception raised when the user has not set their FRED_HOME environment variable."""
#    
#    def __init__(self, msg=None):
#        if msg is None:
#            msg = 'Environment Variable, FRED_HOME, is not set. Please set it to the location of FRED home directory.'
#        super(FredError, self).__init__(msg)
#    
#class FredRunError(FredError):
#    '''Exception raised when an error occurs during a FRED run.'''
#    
#    def __init__(self, log_file, msg=''):
#        if msg == '':
#            msg = f'FRED run unsuccessful. See the LOG file at [{log_file}] for more detail.'
#        super(FredError, self).__init__(msg)
