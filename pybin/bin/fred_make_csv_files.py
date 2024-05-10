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
import csv
import logging
import os
from typing import Literal

from fredpy.util import fredutil
from fredpy.util import constants

try:
    import numpy as np
except ImportError:
    print('This script requires the numpy module.')
    print('Installation instructions can be found at https://pypi.org/project/numpy/.')
    sys.exit(constants.EXT_CD_ERR)


def main():

    usage = '''
    Typical usage example:
    
    $ fred_make_csv_files.py FRED_HOME/RESULTS/JOB/{id}/OUT
    '''

    # Create and setup the parser
    parser = argparse.ArgumentParser(
        description='Makes CSV files for a given output directory.',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('directory',
                        help='the output directory to make csv files in')
    parser.add_argument('--loglevel',
                        default='ERROR',
                        choices=constants.LOGGING_LEVELS,
                        help='set the logging level of this script')

    # Parse the arguments
    args = parser.parse_args()
    directory = args.directory
    loglevel = args.loglevel

    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)

    result = fred_make_csv_files(directory)
    sys.exit(result)


def fred_make_csv_files(directory: str) -> Literal[0, 2]:
    '''
    Makes CSV files in the given output directory. It will first compute 
    the WEEKLY data for each run given the files in the DAILY directories, then use 
    both the DAILY and WEEKLY files to create corresponding CSV files in the PLOT subdirectory.
    
    Parameters
    ----------
    directory : str
        A string representation of the path to the output directory
    
    Returns
    -------
    Literal[0, 2]
        The exit status
    '''

    print('Setting up output files ...')

    # Check the directory
    output_dir_path = Path(directory).resolve()
    try:
        fredutil.validate_dir(output_dir_path)
    except (FileNotFoundError, NotADirectoryError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Setup PLOT directories
    plot_dir_path = Path(output_dir_path, 'PLOT')
    _setup_plot_dir(plot_dir_path)

    run_dirs = fredutil.get_run_dirs(output_dir_path)

    if len(run_dirs) == 0:
        logging.error(f'No RUN subdirectories in [{output_dir_path}].')
        return constants.EXT_CD_ERR

    # Use first run for getting persistent data
    run1 = run_dirs[0]

    # Get text files in run1/DAILY excluding certain files
    excluded_files = ['Day.txt', 'Date.txt', 'EpiWeek.txt', 'Popsize.txt']
    var_names = []  # will store variable file names
    for file in os.listdir(Path(output_dir_path, run1, 'DAILY')):
        if file.endswith('.txt') and file not in excluded_files:
            # Add the file name to var_names without the .txt file extension
            # This will split the file name at the right most '.',
            # leaving the filename indexable at 0
            filename = file.rsplit('.', 1)[0]
            var_names.append(filename)

    # We want Popsize at the beginning of var_names
    var_names.insert(0, 'Popsize')

    # Write var_names to VARS file
    with open(Path(plot_dir_path, 'VARS'), 'w') as VARS:
        for filename in var_names:
            VARS.write(f'{filename}\n')

    # Get EpiWeeks.txt file
    epi_weeks_file_path = Path(output_dir_path, run1, 'DAILY', 'EpiWeek.txt')
    try:
        epi_weeks_file_str = fredutil.get_file_str_from_path(
            epi_weeks_file_path)
    except (FileNotFoundError, IsADirectoryError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Get day offset from epi weeks file
    day_offset = _get_day_offset(epi_weeks_file_str)

    # Make weekly files
    for filename in var_names:
        _make_weekly_file(output_dir_path, filename, day_offset, run_dirs)

    frequency_dirs = ['DAILY', 'WEEKLY']

    # Compute the popsize averages for daily and weekly files for use in csv files
    # This will store the average popsize across all runs for each day / week
    popsize_averages = {}
    popsize_filename = var_names.pop(0)

    print('Computing CSV files ...')

    for frequency_dir in frequency_dirs:
        popsize_data = _get_runs_data(output_dir_path, popsize_filename,
                                      frequency_dir, run_dirs)

        popsize_average = [np.mean(row) for row in popsize_data]
        popsize_averages[frequency_dir] = popsize_average

    # Create PLOT csv files
    for filename in var_names:
        for frequency_dir in frequency_dirs:
            data = _get_runs_data(output_dir_path, filename, frequency_dir,
                                  run_dirs)

            _create_csv_file(plot_dir_path, filename, frequency_dir, data,
                             popsize_averages[frequency_dir], run_dirs)

    print('CSV files completed.')
    return constants.EXT_CD_NRM

def _setup_plot_dir(plot_dir_path: Path) -> None:
    '''
    Sets up the PLOT directory given the PLOT directory path. This will create the 
    PLOT directory along with the necessary subdirectories: a DAILY and WEEKLY 
    subdirectory, if any have not yet been created.
    
    Parameters
    ----------
    plot_dir_path : pathlib.Path
        A Path object representing the full path to the PLOT directory
    
    Raises
    ------
    OSError
        If an error occurs in the creation of any of the directories folders
    '''
    # Create PLOT directory
    if plot_dir_path.exists() and not plot_dir_path.is_dir():
        raise OSError(
            f'PLOT directory could not be created; [{plot_dir_path}] is a file.'
        )
    elif not plot_dir_path.exists():
        os.mkdir(plot_dir_path)

    # Create DAILY subdirectory
    daily_path = Path(plot_dir_path, 'DAILY')
    if daily_path.exists() and not daily_path.is_dir():
        raise OSError(
            f'DAILY subdirectory could not be created; [{daily_path}] is a file.'
        )
    elif not daily_path.exists():
        os.mkdir((daily_path))

    # Create WEEKLY subdirectory
    weekly_path = Path(plot_dir_path, 'WEEKLY')
    if weekly_path.exists() and not weekly_path.is_dir():
        raise OSError(
            f'WEEKLY subdirectory could not be created; [{weekly_path}] is a file.'
        )
    elif not weekly_path.exists():
        os.mkdir((weekly_path))


def _get_day_offset(epi_weeks_file: str) -> int:
    '''
    Given the Epi Weeks file, get the day offset of the first week. This 
    will be how many days into the week the first day in the EpiWeek file is. 
    An offset of 0, for example, would be the first day of the week. An offset
    of 6 would be the last day of the week, and an offset of 7 would represent 
    the following week (although the returned offset must be less than 7).
    
    The EpiWeek file is used to pair simulation day indexes to their corresponding 
    EPI Week, which is used for making statistics. The first EPI week present won't 
    always be a complete week, and this offset must be tracked in order to properly 
    align statistics to their proper EPI week.
    
    Parameters
    ----------
    epi_weeks_file : str
        A string representation of the full path to the EpiWeeks.txt file
    
    Return
    ------
    int
        The day offset
    '''

    lines = []
    with open(epi_weeks_file, 'r') as file:
        lines = file.readlines()

    initial_week = lines[0].split()[1]  # epi week is second entry in line
    days_counted = 0

    while lines[days_counted].split()[1] == initial_week:
        days_counted += 1

    return 7 - days_counted  # 7 - days_counted will represent starting offset


def _make_weekly_file(output_dir_path: Path, filename: str, day_offset: int,
                      run_dirs: list) -> None:
    '''
    Creates a weekly file for the corresponding daily file with the specified filename
    for each RUN subdirectory of the specified output directory.
    
    Parameters
    ----------
    output_dir_path: Path
        A Path object representing the full path to the output directory
    filename: str
        The name of the WEEKLY file to create from the DAILY file with the same name, with 
        no file extension
    day_offset: int
        The day offset, see _get_day_offset for more detail
    run_dirs : list
        A list of all RUN subdirectories of the output directory
    
    Raises
    ------
    AssertionError
        If the the length of the DAILY/filename.txt file is not equal to the length of 
        the DAILY/EpiWeeks.txt file; this is essential in order for the code to work
        as intended
    '''

    for run in run_dirs:

        daily_dir = Path(output_dir_path, run, 'DAILY')
        weekly_dir = Path(output_dir_path, run, 'WEEKLY')

        if weekly_dir.exists() and not weekly_dir.is_dir():
            raise OSError(
                f'WEEKLY RUN subdirectory could not be created; [{weekly_dir}] is a file.'
            )
        elif not weekly_dir.exists():
            os.mkdir(weekly_dir)

        daily_lines = []
        with open(Path(daily_dir, f'{filename}.txt'), 'r') as daily_file:
            daily_lines = daily_file.readlines()

        epiweek_lines = []
        with open(Path(daily_dir, 'EpiWeek.txt'), 'r') as epiweek_file:
            epiweek_lines = epiweek_file.readlines()

        assert len(daily_lines) == len(epiweek_lines)

        num_days = len(daily_lines)

        # Weekly stats will be outputted to a weekly file with the same name
        weekly_file = open(Path(weekly_dir, f'{filename}.txt'), 'w')

        weekly_stats = []  # hold the stats for each day; output at end of week

        for day in range(num_days):

            # Get daily line from each file
            daily_line = daily_lines[day]
            epiweek_line = epiweek_lines[day]

            # Get data from lines; will be in second field
            daily_stat = daily_line.split()[1]
            epiweek = epiweek_line.split()[1]

            weekly_stats.append(float(daily_stat))

            # Add one to day of week so the range is [1, 7] rather than [0, 6]
            # This allows day_of_week % 7 == 0 to be true at end of week
            day_of_week = day + day_offset + 1

            # Output stats if it is end of week, or final day
            if day_of_week % 7 == 0 or day == num_days - 1:
                # If file is a total file, we want the end of week stat
                if 'tot' in filename:
                    cur_total = int(weekly_stats[-1])
                    weekly_file.write(f'{epiweek} {cur_total}\n')

                # If file is a new file, we want the sum of the daily stats
                elif 'new' in filename:
                    weekly_sum = int(sum(weekly_stats))
                    weekly_file.write(f'{epiweek} {weekly_sum}\n')

                # If file is standard file, we want the weekly average, rounded up
                else:
                    weekly_ave = np.mean(weekly_stats)
                    weekly_ave = int(weekly_ave + 0.5)
                    weekly_file.write(f'{epiweek} {weekly_ave}\n')

                weekly_stats.clear()

        weekly_file.close()


def _get_runs_data(output_dir_path: Path, filename: str,
                   frequency_dir: Literal['DAILY', 'WEEKLY'],
                   run_dirs: list) -> np.ndarray:
    '''
    Gets data from the file with the specified filename from all run
    subdirectories of the given output directory. These files will either be 
    located in the DAILY or WEEKLY output subdirectory, which is given by the 
    frequency directory.
    
    A numpy array will be created with a row for each line in the file (which 
    is consistent across the separate runs), and a column for each run that data will 
    be retrieved from. This data is being collected for the creation of the PLOT csv files,
    which use data from all runs.
    
    Parameters
    ----------
    output_dir_path : pathlib.Path
        A Path object representing the full path to the output directory
    filename : str
        The name of the file to get data from in each run, without the .txt 
        file extension
    frequency_dir : Literal['DAILY', 'WEEKLY']
        The frequency directory to find the file in (DAILY or WEEKLY)
    run_dirs : list 
        A list of all RUN subdirectories of the output directory, each of which 
        contain a file with the filename in either frequency directory
    
    Returns
    -------
    numpy.ndarray
        A numpy array containing a row for each line in the files, and a column
        for each run directory's file's data
    '''

    # Setup a numpy array with a row for each file line and a col for each run
    cols = len(run_dirs)
    rows = 0
    with open(
            Path(output_dir_path, run_dirs[0], frequency_dir,
                 f'{filename}.txt'), 'r') as file:
        # Line count will be the same in any RUN's file with the filename
        rows = len(file.readlines())

    # Create the numpy array with zeros as place holders
    data = np.zeros((rows, cols))

    for col, run in enumerate(run_dirs):
        file_path = Path(output_dir_path, run, frequency_dir,
                         f'{filename}.txt')

        file_lines = []
        with open(file_path, 'r') as file:
            file_lines = file.readlines()

        for row, line in enumerate(file_lines):
            # Get the data from the second field in line
            line_data = line.split()[1]

            # Insert data into data array at correct row and column
            data[row][col] = line_data

    return data


def _create_csv_file(plot_dir_path: Path, filename: str,
                     frequency_dir: Literal['DAILY',
                                            'WEEKLY'], data: np.ndarray,
                     popsize_data: list, run_dirs: list) -> None:
    '''
    Creates csv files in the given PLOT directory and frequency subdirectory.
    The CSV files will be created using the corresponding data retrieved in _get_runs_data,
    as well as population data that is indpendent of filename or run. The CSV file will 
    correspond to the data from .txt files with the specified filename, and will be created
    with the same filename, but with a .csv file extension.
    
    Parameters
    ----------
    plot_dir_path : pathlib.Path
        A Path object representing the full path to the PLOT directory
    filename : str
        The name of the file to be created, without the .csv file extension
    frequency_dir : Literal['DAILY', 'WEEKLY']
        The frequency subdirectory of PLOT to create the csv file in (DAILY or WEEKLY)
    data : numpy.ndarray
        A numpy array containing a row for each line in the files, and a column
        for each run directory's file's data
    '''

    csv_file = open(Path(plot_dir_path, frequency_dir, f'{filename}.csv'), 'w')
    writer = csv.writer(csv_file)

    header = [
        "INDEX", "N", "POPSIZE", "MIN", "QUART1", "MED", "QUART3", "MAX",
        "MEAN", "STD"
    ] + run_dirs  # add run directories for a column of each run's data
    writer.writerow(header)

    for i, row in enumerate(data):
        row_data = [
            i,  # index
            len(run_dirs),  # number of runs
            popsize_data[i],  # average popsize
            min(row),  # minimum
            np.percentile(row, 25),  # first quartile
            np.percentile(row, 50),  # median
            np.percentile(row, 75),  # third quartile
            max(row),  # max
            round(np.mean(row), 5),  # mean
            round(np.std(row, ddof=1), 5) if len(run_dirs) > 1 else
            0,  # standard deviation (will be 0 if there is only one run entry)
        ] + list(row)  # add run data to end of row

        writer.writerow(row_data)

    csv_file.close()


if __name__ == '__main__':
    main()
