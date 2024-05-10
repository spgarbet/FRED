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
from collections import namedtuple
from typing import Literal, List

from fredpy.util import fredutil
from fredpy.util import constants
from fredpy import frederr

IS_EPIWEEKS_INSTALLED = False

try:
    import numpy as np
except ImportError:
    print('This script requires the numpy module.')
    print('Installation instructions can be found at https://pypi.org/project/numpy/.')
    sys.exit(constants.EXT_CD_ERR)

try:
    import matplotlib.pyplot as plt
except ImportError:
    print('This script requires the matplotlib module.')
    print('Installation instructions can be found at https://pypi.org/project/matplotlib/.')
    sys.exit(constants.EXT_CD_ERR)

try:
    import pandas as pd
except ImportError:
    print('This script requires the pandas module.')
    print('Installation instructions can be found at https://pypi.org/project/pandas/.')
    sys.exit(constants.EXT_CD_ERR)
    
try:
    from epiweeks import Week
    IS_EPIWEEKS_INSTALLED = True
except ImportError:
    print('Warning: If you are trying to get the date from the EpiWeek you will need to have the epiweeks package installed.')
    print('Installation instructions can be found at https://pypi.org/project/epiweeks/.')
    


def main():

    usage = '''
    Typical usage examples:
    
    # Plot the median data from the INF.E file for the key
    $ fred_plot.py -k mykey -v INF.E
    
    # Plot the median data from both the INF.E and INF.totE files for both keys
    $ fred_plot.py -k mykey -k mykey2 -v INF.E -v INF.totE
    
    # Plot the weekly data of all runs from the INF.newE file for the three keys
    $ fred_plot.py -k mykey -k mykey2 -k mykey3 -v INF.newE -w
    '''

    # Create and setup the parser
    parser = argparse.ArgumentParser(
        description=
        'Plot data corresponding to the given FRED keys and variables.',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)

    data = parser.add_argument_group(
        title='data (required)',
        description='plot data for each key and variable combination')
    data.add_argument('-k',
                      '--key',
                      required=True,
                      action='append',
                      help='the FRED key(s) to look up')
    data.add_argument('-v',
                      '--var',
                      required=True,
                      action='append',
                      help='the FRED variable file name(s) to plot')

    config = parser.add_argument_group(
        title='configuration',
        description='configure the type of data that is plotted')
    config.add_argument(
        '-d',
        '--use_data',
        default=['MED'],
        nargs='+',
        help=
        ('the csv data column(s) to plot (POPSIZE, MIN, QUART1, MED (default), '
         'QUART3, MAX, MEAN, STD, RUNS); '
         'RUNS will plot data from all available RUN columns'),
        choices=[
            'POPSIZE', 'MIN', 'QUART1', 'MED', 'QUART3', 'MAX', 'MEAN', 'STD',
            'RUNS'
        ],
        metavar='column')
    config.add_argument('-w',
                        '--weekly',
                        action='store_true',
                        help='plot WEEKLY data (plots DAILY data by default)')
    config.add_argument('-D',
                        '--use_dates',
                        action='store_true',
                        help='display dates (rather than simulation day / week) on x-axis')
    config.add_argument('-t',
                        '--title',
                        default='FRED SIMULATION',
                        action='store',
                        help='sets the title of the plot')
    config.add_argument('-x',
                        '--xaxis',
                        action='store',
                        help='sets the label of the x-axis')
    config.add_argument('-y',
                        '--yaxis',
                        default='Individuals',
                        action='store',
                        help='sets the label of the y-axis')

    parser.add_argument('--loglevel',
                        default='ERROR',
                        choices=constants.LOGGING_LEVELS,
                        help='set the logging level of this script')

    # Parse the arguments
    args = parser.parse_args()
    fred_keys = args.key
    fred_vars = args.var
    data_cols = args.use_data
    frequency_dir = 'WEEKLY' if args.weekly else 'DAILY'
    title = args.title
    if args.xaxis != None:
        x_axis = args.xaxis
    elif args.use_dates and args.weekly:
        x_axis = 'EpiWeek'
    elif args.use_dates:
        x_axis = ''
    elif args.weekly:
        x_axis = 'Simulation Week'
    else:
        x_axis = 'Simulation Day'
        
    y_axis = args.yaxis
    loglevel = args.loglevel
    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)

    result = fred_plot(fred_keys, fred_vars, data_cols, frequency_dir, title,
                       x_axis, y_axis, args.use_dates)
    sys.exit(result)


def fred_plot(fred_keys: list[str], fred_vars: list[str], data_cols: list[str],
              frequency_dir: Literal['DAILY', 'WEEKLY'], title: str,
              x_axis: str, y_axis: str, use_dates: bool) -> Literal[0, 2]:
    '''
    Plots data using the PLOT csv files. A plot is created for each specified key,
    each specified variable file corresponding to that key, and each specified data 
    column corresponding to that variable file, or simply the median column (MED) if no 
    data columns are specified.
    
    The csv files are located in FRED_RESULTS/JOB/{id}/OUT/PLOT/{frequency_dir}, 
    where frequency_dir will either be DAILY or WEEKLY. A list of valid variable names
    for that Job can be viewed in the VARS file located in the PLOT directory. If 
    one of the provided variable names does not match a variable file for a key, 
    that file will be skipped, allowing the plotting of variables that may exist for 
    one key but not another.
    
    Customization of the graphing window can be applied through the interactive window
    that matplotlib generates. Aspects like title, axis labels, legend, margins, etc. 
    can all be configured.
    
    Parameters
    ----------
    fred_keys : List[str]
        The FRED Job keys to locate plot data
    fred_vars : List[str]
        The FRED variable file names to use plot data from, without the .csv 
        file extension
    data_cols : List[str]
        The data columns in the PLOT csv files to plot
    frequency_dir : Literal['DAILY', 'WEEKLY']
        The data frequency to plot (DAILY or WEEKLY)
    title : str
        The title of the plot
    x_axis : str
        The label of the x-axis
    y_axis : str
        The label of the y-axis
    use_dates : bool
        A flag to decide whether to use dates / epiweek along the x-axis, rather than simulation day / week
    
    Returns
    -------
    Literal[0, 2]
        The exit status
    '''
    # Create a custom data type that will hold information related to a plot
    # This will group the plot data with necessary metadeta for labeling
    PlotDataConfig = namedtuple('PlotDataConfig',
                                ['key', 'var', 'col', 'data'])

    # Will hold a list of all plots that will be plotted to the graph window
    plot_configs: list[PlotDataConfig] = []

    for key in fred_keys:
        # Find the OUT directory
        fred_job_out_dir_str = None
        try:
            fred_job_out_dir_str = fredutil.get_fred_job_out_dir_str(key)
        except (frederr.FredHomeUnsetError, FileNotFoundError,
                IsADirectoryError, NotADirectoryError) as e:
            logging.error(e)
            return constants.EXT_CD_ERR

        if fred_job_out_dir_str == None:
            print(f'Key [{key}] was not found.')
            return constants.EXT_CD_NRM
        
        x_axis_dict = {}
        if use_dates:
            if frequency_dir == 'DAILY':
                date_file_path = Path(fred_job_out_dir_str, 'RUN1', 'DAILY', 'Date.txt')
                try:
                    date_file_str = fredutil.get_file_str_from_path(date_file_path)
                
                    with open(date_file_str) as f:
                        for line in f:
                            (u, v) = line.split()
                            x_axis_dict[int(u)] = v
                
                except (FileNotFoundError, IsADirectoryError) as e:
                    logging.error(f'No Date.txt file found for Job [{key}]')
                    return constants.EXT_CD_ERR
            else:
                # Need to get a file to get mapping for labels of Epiweek. The one file that will always be available is Popsize.csv
                popsize_file_path = Path(fred_job_out_dir_str, 'PLOT', 'WEEKLY', 'Popsize.csv')
                try:
                    popsize_file_str = fredutil.get_file_str_from_path(popsize_file_path)
                    skip_row = True
                    row_num = 0
                    with open(popsize_file_str) as f:
                        for line in f:
                            # Skip header row
                            if skip_row:
                                skip_row = False
                                continue
                            if IS_EPIWEEKS_INSTALLED:
                                # Create an EpiWeek object from the week string in the csv file E.g. '2022.11' becomes Week(2022, 11)
                                week = Week(int(line.split(',')[0].split('.')[0]), int(line.split(',')[0].split('.')[1]))
                                x_axis_dict[row_num] = week.startdate()
                            else:
                                
                                x_axis_dict[row_num] = line.split(',')[0].replace('.', '|')
                            row_num += 1
                
                except (FileNotFoundError, IsADirectoryError) as e:
                    logging.error(f'No Popsize.csv file found for Job [{key}]')
                    return constants.EXT_CD_ERR

        for var in fred_vars:
            # Get the csv file corresponding the the variable name
            csv_file_path = Path(fred_job_out_dir_str, 'PLOT', frequency_dir,
                                 f'{var}.csv')
            try:
                csv_file_str = fredutil.get_file_str_from_path(csv_file_path)
            except (FileNotFoundError, IsADirectoryError) as e:
                logging.error(f'No variable file [{var}] found for Job [{key}]'
                              ' ... Skipping file ...')
                continue

            # Get dataframe from csv file
            df = pd.read_csv(csv_file_str)

            # Find which cols will be needed from data frame
            df_cols = _expand_data_cols(df, data_cols)
            
            for df_col in df_cols:
                df_plot = pd.DataFrame()
                if frequency_dir == 'DAILY':
                    df_plot = df[['INDEX', df_col]].copy()
                else:
                    df_plot = df[[df_col]].copy()
                    
                # Create a PlotDataConfig that stores:
                #   the key used to find the data
                #   the var file that the data is located in
                #   the column that the data will be retrieved from
                #   the data itself
                config = PlotDataConfig(key, var, df_col, df_plot)
                plot_configs.append(config)

    # If no valid variables were found, exit
    if len(plot_configs) == 0:
        logging.error(
            f'Could not find any variable files matching {fred_vars} '
            f'in the Jobs {fred_keys}.')
        return constants.EXT_CD_ERR

    # Setup graph window
    plt.title(title)
    plt.xlabel(x_axis)
    plt.ylabel(y_axis)
    
    # Plot each config
    for config in plot_configs:
        if frequency_dir == 'DAILY':
            plt.plot(config.data.iloc[:, 0], config.data.iloc[:, 1], label=f'{config.key} {config.var} {config.col}')
        else:
            plt.plot(config.data, label=f'{config.key} {config.var} {config.col}')
    
    # If using dates instead of simulation day / week, need to set the labels
    if use_dates:
        # Get the current xtick values
        cur_tick_lst = plt.xticks()[0]
        new_tick_lst = []
        new_tick_label_lst = []
        # Put the values into a new list and map each to a label
        for tick_val in cur_tick_lst:
            if int(tick_val) in x_axis_dict.keys():
                new_tick_lst.append(int(tick_val))
                new_tick_label_lst.append(x_axis_dict[int(tick_val)])
        plt.xticks(new_tick_lst, new_tick_label_lst)
        # Rotate the xtick labels and make the text a little smaller
        ax = plt.gca()
        ax.tick_params(axis='x', labelrotation=30, labelsize='small')

    # Legend must be set to top left for labels to show (for some reason)
    plt.legend(loc='upper left')

    # Show the graph window
    plt.show()
    return constants.EXT_CD_NRM


def _expand_data_cols(df: pd.DataFrame, data_cols: List[str]) -> List[str]:
    '''
    Expand the data columns for the given dataframe. This will simply 
    return the given data columns, but with RUNS replaced with each RUN 
    column in the dataframe.
    
    Parameters
    ----------
    df : pandas.DataFrame
        The dataframe that's columns will be checked
    data_cols : List[str]
        The user specified data columns, which will be expanded
    
    Returns
    -------
    List[str]
        A copy of data_cols, but with RUNS expanded to each RUN column    
    '''
    expanded_cols = []
    
    for data_col in data_cols:
        if data_col == 'RUNS':
            # Get each RUN column in the dataframe
            run_cols = [
                df_col for df_col in df.columns if df_col.startswith('RUN')
            ]

            # Concatentate the list of RUN columns to the expanded columns list
            expanded_cols += run_cols
        else:
            expanded_cols.append(data_col)

    return expanded_cols


if __name__ == '__main__':
    main()
