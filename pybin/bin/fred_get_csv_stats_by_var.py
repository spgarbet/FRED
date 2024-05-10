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
import glob
import logging
import os
from typing import Literal

from fredpy.util import fredutil
from fredpy.util import constants
from fredpy import frederr

try:
    import numpy as np
except ImportError:
    print('This script requires the numpy module.')
    print('Installation instructions can be found at https://pypi.org/project/numpy/.')
    sys.exit(constants.EXT_CD_ERR)

try:
    import pandas as pd
except ImportError:
    print('This script requires the pandas module.')
    print('Installation instructions can be found at https://pypi.org/project/pandas/.')
    sys.exit(constants.EXT_CD_ERR)

def main():

    usage = '''
    Typical usage examples:
    
    $ fred_get_csv_stats_by_var.py mykey
    ---> Creates file summary.csv for the given key using the DAILY summary stats for each variable tracked by FRED
    
    $ fred_get_csv_stats_by_var.py mykey -f myfile.csv
    ---> Creates file myfile.csv for the given key using the DAILY summary stats for each variable tracked by FRED
    
    $ fred_get_csv_stats_by_var.py mykey -w
    ---> Creates file summary.csv for the given key using the WEEKLY summary stats for each variable tracked by FRED
    
    $ fred_get_csv_stats_by_var.py mykey -i 120
    ---> Creates file summary.csv for the given key using the DAILY summary stats that includes summary stats for the specific indexed day for each variable tracked by FRED
    
    '''

    # Creates and sets up the parser
    parser = argparse.ArgumentParser(
        description='Creates a summary csv file for all variables in the PLOT/{DAILY or WEEKLY} directory for a given FRED key',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('key', help='the FRED key to look up')
    parser.add_argument('-w',
                        '--weekly',
                        action='store_true',
                        help='get WEEKLY csv data (gets DAILY data by default)')
    parser.add_argument('-r',
                        '--run',
                        type=int,
                        default=-1,
                        help='get the specific run column for a given job for the \'RUNS\' option of (-d, --data)')
    parser.add_argument('-i',
                        '--index',
                        action=IndexParserAction,
                        type=int,
                        default=-1,
                        help='set a specific index for a day/week summary stats {-1}')
    parser.add_argument('-f',
                        '--filename',
                        type=str,
                        default='summary.csv',
                        help='the csv file to create {summary.csv}')
    parser.add_argument('-d',
                        '--data',
                        default=['MED'],
                        nargs='+',
                        help=('the csv data column(s) wanted (MIN, QUART1, MED (default), '
                              'QUART3, MAX, MEAN, STD, RUNS, ROW); '
                              'RUNS will get data from all available RUN columns unless a specific run is requested (-r, --run)'
                              'ROW will get a single indexed row of data (-i, --index) or the final row if no index is supplied'
                              'Note: ROW and RUNS are not compatible with other will get a single indexed row of data (-i, --index) or the final row if no index is supplied'),
                        choices=['MIN', 'QUART1', 'MED', 'QUART3', 'MAX', 'MEAN', 'STD', 'RUNS', 'ROW'],
                        metavar='column')
    parser.add_argument('--loglevel',
                        default='ERROR',
                        choices=constants.LOGGING_LEVELS,
                        help='set the logging level of this script')

    # Parses the arguments
    args = parser.parse_args()
    fred_key = args.key
    frequency_dir = 'WEEKLY' if args.weekly else 'DAILY'
    selected_index = args.index
    filename = args.filename
    loglevel = args.loglevel

    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)

    result = fred_get_csv_summary_stats(fred_key, frequency_dir, selected_index, filename)
    sys.exit(result)


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
    
    
class IndexParserAction(argparse.Action):

    def __call__(self, parser, namespace, values, option_string=None):
        if values < -1:
            parser.error("Index must be an integer >= -1 (Note: the default of -1 will or an index greater than the rowcount will be ignored)")

        setattr(namespace, self.dest, values)



if __name__ == '__main__':
    main()

