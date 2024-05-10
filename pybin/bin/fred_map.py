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
from typing import Literal, Optional, List

from fredpy.util import fredutil
from fredpy.util import constants
from fredpy import frederr

try:
    import pandas as pd
except ImportError:
    print('This script requires the pandas module.')
    print('Installation instructions can be found at https://pandas.pydata.org/getting_started.html')
    sys.exit(constants.EXT_CD_ERR)

try:
    import plotly.express as px
except ImportError:
    print('This script requires the plotly module.')
    print('Installation instructions can be found at https://pypi.org/project/plotly/.')
    sys.exit(constants.EXT_CD_ERR)


def main():

    usage = '''
    Typical usage examples:
    
    $ fred_map.py mykey INF.E
    
    # Saves to an HTML file
    $ fred_map.py mykey INF.E --save
    
    # Setting the API Key for an API key specific style
    $ export MAPBOX_API_KEY=...
    $ fred_map.py mykey INF.E --style 'light'
    '''

    # Create and setup the parser
    parser = argparse.ArgumentParser(
        description='Creates a visualization of the FRED variable for a FRED Job using the Mapbox API.',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)

    data = parser.add_argument_group(title='data', description='define the data to visualize')
    data.add_argument('key', help='the FRED key to look up')
    data.add_argument('var',
                      help=('the FRED variable to visualize; if set to '
                            '\'list\', visualized FRED variables for the key '
                            'are listed'))
    data.add_argument('-r',
                      '--run',
                      default=1,
                      type=int,
                      help='the run number to visualize (default=1)')

    config = parser.add_argument_group(
        title='configuration',
        description='configure the output of the visualization')
    config.add_argument(
        '--save',
        action='store_true',
        help=('saves the visualization as an html file rather '
              'than showing it; the name will be \'{key}.html\''))
    config.add_argument(
        '--style',
        choices=[
            'white-bg', 'open-street-map', 'carto_positron',
            'carto-darkmatter', 'stamen-terrain', 'stamen-toner',
            'stamen-watercolor', 'basic', 'streets', 'outdoors', 'light',
            'dark', 'satellite', 'satellite-streets'
        ],
        metavar='map-style',
        help=
        ('sets the style of map to use for the visualization; '
         'with no API key, the maps "white-bg", "open-street-map", "carto-positron", '
         '"carto-darkmatter", "stamen-terrain", "stamen-toner" or '
         '"stamen-watercolor" can be used; with an API key, the maps '
         '"basic", "streets", "outdoors", "light", "dark", "satellite", '
         'or "satellite-streets" can also be used (default="open-street-map" '
         'if no API key is found, "basic" if an API key is found)'))
    config.add_argument(
        '--speed',
        type=int,
        default=750,
        help=('sets the duration of each frame in the animation in milliseconds; '
              'a slower speed helps larger datasets animate (default=750)'))
    config.add_argument(
        '--title',
        default='FRED Simulation',
        help='sets the title of the HTML window (default=\'FRED Simulation\')')

    parser.add_argument('--loglevel',
                        default='ERROR',
                        choices=constants.LOGGING_LEVELS,
                        help='set the logging level of this script')

    # Parse the arguments
    args = parser.parse_args()
    fred_key = args.key
    fred_var = args.var
    run_num = args.run
    save_file = args.save
    map_style = args.style
    anim_speed = args.speed
    fig_title = args.title
    loglevel = args.loglevel

    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)

    result = fred_map(fred_key, fred_var, run_num, save_file, map_style,
                      anim_speed, fig_title)
    sys.exit(result)


def fred_map(fred_key: str, fred_var: str, run_num: str, save_file: bool,
             map_style: Optional[str], anim_speed: int,
             fig_title: str) -> Literal[0, 2]:
    '''
    Creates a visualization of a FRED Job by plotting the VIS data of the specified 
    variable over a map in an interactive HTML window. VIS data is stored in 
    FRED_RESULTS/JOB/{id}/OUT/RUN{run_num}/VIS, with a subdirectory for each 
    visualized variable. Visualization must have been enabled in the Job's 
    parameter file in order for VIS data to be generated.
    
    Parameters
    ----------
    fred_key : str
        The FRED Job key to visualize
    fred_var : str
        The FRED variable to visualize
    run_num : str
        The RUN subdirectory to visualize (uses RUN1 by default)
    save_file : bool
        If true, save the HTML file rather than openining it in a browser
    map_style : Optional[str]
        If specified, uses a map-style available in the Mapbox API, some 
        of which require an API key. See the help --style for more detail
    anim_speed : int
        The duration of each frame of the animation in milliseconds
    fig_title : str
        The title of the HTML window
    
    Returns
    -------
    Literal[0, 2]
        The exit status
    '''
    # Get API key from environment
    api_key = os.environ.get('MAPBOX_API_KEY')

    # Assign default map style based on wheter or not an API key was found
    # If an API key required style was specified and no key was found, the user is notified
    api_key_styles = [
        'basic', 'streets', 'outdoors', 'light', 'dark', 'satellite',
        'satellite-streets'
    ]
    if api_key and not map_style:
        map_style = 'basic'
    elif not api_key and not map_style:
        map_style = 'open-street-map'
    elif not api_key and map_style in api_key_styles:
        logging.error(
            'Could not find Mapbox API key. In order to use the map-style '
            f'"{map_style}", an API key is required. Please choose a map-style '
            'that doesn\'t require an API key, or obtain a key from https://www.mapbox.com/, '
            'then store the API key in an environment variable called '
            'MAPBOX_API_KEY.')
        return constants.EXT_CD_ERR

    px.set_mapbox_access_token(api_key)

    # Find the OUT directory
    fred_job_out_dir_str = None
    try:
        fred_job_out_dir_str = fredutil.get_fred_job_out_dir_str(fred_key)
    except (frederr.FredHomeUnsetError, FileNotFoundError, IsADirectoryError, NotADirectoryError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    if fred_job_out_dir_str == None:
        print(f'Key [{fred_key}] was not found.')
        return constants.EXT_CD_NRM

    # Find the RUN dir
    run_dir = Path(fred_job_out_dir_str, f'RUN{run_num}')
    try:
        fredutil.validate_dir(run_dir)
    except (FileNotFoundError, NotADirectoryError) as e:
        logging.error(f'RUN{run_num} subdirectory not found; {e}')
        return constants.EXT_CD_ERR

    # Find the VIS directory
    vis_dir = Path(fred_job_out_dir_str, f'RUN{run_num}', 'VIS')
    try:
        fredutil.validate_dir(vis_dir)
    except (FileNotFoundError, NotADirectoryError) as e:
        logging.error(f'No visualization data found; {e}')
        return constants.EXT_CD_ERR

    if fred_var == 'list':
        # Valid visualization variables are listed in VARS file
        print(f'Visualized variables for Job [{fred_key}]:')
        fredutil.read_file(Path(vis_dir, 'VARS'))
        return constants.EXT_CD_NRM

    # Find the variable directory
    var_dir = Path(vis_dir, fred_var)
    try:
        fredutil.validate_dir(var_dir)
    except (FileNotFoundError, NotADirectoryError) as e:
        logging.error(f'Visualization data for {fred_var} not found; {e}\n'
                      'To see visualized variables for this Job, try: '
                      f'fred_map.py {fred_key} list')
        return constants.EXT_CD_ERR

    # Find the dates file for use in dataframe
    date_file_path = Path(fred_job_out_dir_str, f'RUN{run_num}', 'DAILY',
                          'Date.txt')
    try:
        date_file_str = fredutil.get_file_str_from_path(date_file_path)
    except (FileNotFoundError, IsADirectoryError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    df = _assemble_dataframe(var_dir, date_file_str)

    # Find the BBOX file for visualized area
    bbox_file_path = Path(vis_dir, 'BBOX')
    try:
        bbox_file_str = fredutil.get_file_str_from_path(bbox_file_path)
    except (FileNotFoundError, IsADirectoryError) as e:
        logging.error(e)
        return constants.EXT_CD_ERR

    # Get center of BBOX
    center = _get_bbox_center(bbox_file_str)

    fig = px.scatter_mapbox(df,
                            lat='lat',
                            lon='lon',
                            animation_frame='date',
                            title=fig_title,
                            center=center,
                            mapbox_style=map_style)

    # Get fips codes from meta file
    fips_codes = []
    with open(Path(vis_dir, 'COUNTIES'), 'r') as fips_file:
        fips_codes = fips_file.readlines()

    # Get the outline file for each fips code if it exists
    for fips_code in fips_codes:
        try:
            outline_file = fredutil.get_file_str_from_path(
                Path(fredutil.get_fred_home_dir_str(), 'data', 'country',
                     'usa', 'SHAPES', f'{fips_code.rstrip()}.txt'))
        except (FileNotFoundError, IsADirectoryError):
            # If an error is raised, we will not use an outline
            continue

        _add_outline(fig, outline_file)

    # Sets the duration of each frame
    fig.layout.updatemenus[0].buttons[0].args[1]['frame'][
        'duration'] = anim_speed

    fig.write_html(f'{fred_key}.html') if save_file else fig.show()


def _get_geopoints(file: str) -> List[fredutil.GeoPoint]:
    '''
    Gathers all latitude longitude points in the given file into a list of 
    GeoPoints, a custom named tuple that has a lat and lon field. The format 
    of each line is assumed to be 'lat lon', which is the format FRED uses for 
    storing latitude longitude coordinates.
    
    Parameters
    ----------
    file : str
        A string representation of the full path to the coordinate file
    
    Returns
    -------
    List[fredutil.GeoPoint]
        A list of GeoPoints containing all coordinates listed in the file
    '''
    lines = []
    with open(file, 'r') as f:
        lines = f.readlines()

    geopoints = []
    for line in lines:
        # The line is assumed to be in the format 'lat lon'
        lat = float(line.split()[0])
        lon = float(line.split()[1])
        geopoints.append(fredutil.GeoPoint(lat, lon))

    return geopoints


def _assemble_dataframe(var_dir: Path, date_file: str) -> pd.DataFrame:
    '''
    Constructs a data frame containing each latitude longitude point stored
    in the text files of the variable directory, along with each point's
    corresponding date from the date_file.
    
    The points are stored in text files named loc-{day_index}.txt in the 
    variable directory, where the day index is the index of the simulation day. 
    These day indexes lead to real world dates by reading the date file, which 
    contains a line for each simulation day, which contain the day index followed 
    by the real world date.
    
    Parameters
    ----------
    var_dir : Path
        A Path object representing the full path to the variable directory
    date_file : str
        A string representation of the full path to the date file
    
    Returns
    -------
    pandas.DataFrame
        A dataframe with columns for the latitude, longitude, and date of each
        visualization point
    '''
    # Get lines from date file, which will be 'day_index date'
    date_lines = []
    with open(date_file, 'r') as f:
        date_lines = f.readlines()

    data = {'lat': [], 'lon': [], 'date': []}

    for date_line in date_lines:
        day_index, date = date_line.split()

        loc_file = Path(var_dir, f'loc-{day_index}.txt')

        geopoints = _get_geopoints(loc_file)

        # Add each lat / lon pair and corresponding date to data
        for geopoint in geopoints:
            data['lat'].append(geopoint.lat)
            data['lon'].append(geopoint.lon)
            data['date'].append(date)

    return pd.DataFrame(data=data)


def _get_bbox_center(bbox_file: str) -> dict:
    '''
    Gets the center of the bounding box of the visualized area from the 
    BBOX file, which is stored in the VIS directory. The BBOX file 
    contains the minimum and maximum latitude and longitude coordinates,
    and should look like this:
    ymin = ...
    xmin = ...
    ymax = ...
    xmax = ...
    
    The center will be stored in a dictionary with keys 'lat' and 'lon', as
    required by plotly.express.scatter_mapbox, where the center will be used to 
    set the center of the display window.
    
    Parameters
    ----------
    bbox_file : str
        A string representation of the full path to the BBOX file
    
    Returns
    -------
    dict
        A dictionary containing the center of the BBOX
    '''
    lat_min = 0.0
    lon_min = 0.0
    lat_max = 0.0
    lon_max = 0.0

    with open(bbox_file, 'r') as f:
        for line in f:
            if 'xmin = ' in line:
                lon_min = float(line.split()[2])
            elif 'ymin = ' in line:
                lat_min = float(line.split()[2])
            elif 'xmax = ' in line:
                lon_max = float(line.split()[2])
            elif 'ymax = ' in line:
                lat_max = float(line.split()[2])

    return {'lat': (lat_min + lat_max) / 2, 'lon': (lon_min + lon_max) / 2}


def _add_outline(fig, outline_file: str) -> None:
    '''
    Adds an outline to the given figure by using the latitude longitude points
    listed in the outline file. This will be overlayed on top of the existing 
    figure.
    
    Parameters
    ----------
    fig : plotly FigureWidget
        The figure that is being used to plot on
    outline_file : str
        A string representation of the full path to the outline file
    '''
    geopoints = _get_geopoints(outline_file)

    data = {'lat': [], 'lon': []}
    for geopoint in geopoints:
        data['lat'].append(geopoint.lat)
        data['lon'].append(geopoint.lon)

    df = pd.DataFrame(data=data)

    fig.add_trace(
        px.line_mapbox(df,
                       lat='lat',
                       lon='lon',
                       color_discrete_sequence=['black']).data[0])


if __name__ == '__main__':
    main()
