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
import re
from typing import Literal

from fredpy.util import fredutil
from fredpy.util import constants
from fredpy import frederr

try:
    import pandas as pd
except ImportError:
    print('This script requires the pandas module.')
    print('Installation instructions can be found at https://pypi.org/project/pandas/.')
    sys.exit(constants.EXT_CD_ERR)

def main():

    usage = '''
    Typical usage examples:
    
    $ fred_convert_vna_to_model.py infile outfile
    ---> Reads infile and writes the outfile as a FRED model
    
    $ fred_convert_vna_to_model.py infile
    ---> Reads infile and writes an outfile as a FRED model with the same name as infile but with .fred as the extension
    
    '''

    # Creates and sets up the parser
    parser = argparse.ArgumentParser(
        description='Reads infile and writes the outfile as a FRED model',
        epilog=usage,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('infile', help='the .vna file to read')
    parser.add_argument('outfile',
                        default=None,
                        nargs='?',
                        help='the file to write')
    parser.add_argument('-n',
                        '--network_name',
                        default=None,
                        type=str,
                        help='the name of the created network (by default, the network will be named using infile)')
    parser.add_argument('-N',
                        '--normalize',
                        action='store_true',
                        help='normalize weights of edges such that the weights are all proportional to the max weight (i.e. 0.0 to 1.0)')
    parser.add_argument('--loglevel',
                        default='ERROR',
                        choices=constants.LOGGING_LEVELS,
                        help='set the logging level of this script')

    # Parses the arguments
    args = parser.parse_args()
    infile = args.infile
    outfile = args.outfile
    network_name = args.network_name
    normalize_weights = args.normalize
    loglevel = args.loglevel

    # Set up the logger
    logging.basicConfig(format='%(levelname)s: %(message)s')
    logging.getLogger().setLevel(loglevel)

    if not os.path.splitext(infile)[1] == '.vna':
        print('The file, {0}, must be have extension .vna'.format(infile))
        sys.exit(constants.EXT_CD_ERR)
        
    if outfile == None:
        infile_basename = Path(infile).stem
        outfile = infile_basename + ".fred"

    if network_name == None:
        infile_basename = Path(infile).stem
        network_name = infile_basename
        
    result = fred_convert_vna_to_model(infile, outfile, network_name, normalize_weights)
    sys.exit(result)


def fred_convert_vna_to_model(infile_str: str, outfile_str: str, network_name: str, normalize_weights: bool) -> Literal[0, 2]:
    '''
    Reads infile (a .vna network file), finds the information that is needed to create a FRED network model,
    and writes the outfile as a FRED model
    
    The .vna file will be searched line by line to find the line "from to weight". This is the beginning
    of the information pertinent to FRED
    
    Parameters
    ----------
    infile_str : str
        The file name to read
    outfile_str : str
        The file name to write
    network_name : str
        The name of the network created in FRED
    normalize_weights : bool
        Whether of not the edge weights should be normalized
    
    Returns
    -------
    Literal[0, 2]
        The exit status
    '''
    
    header_line_found = False
    network_dictionary = {
      'from': [],
      'to': [],
      'weight': []
    }
    # Find the "from to weight" header then capture everything after that
    with open(infile_str) as file:
        while line := file.readline():
            #print(line.strip().split())
            if header_line_found:
                fields = line.strip().split()
                network_dictionary['from'].append(fields[0])
                network_dictionary['to'].append(fields[1])
                network_dictionary['weight'].append(float(fields[2]))
            elif re.search('from\s+to\s+weight', line):
                header_line_found = True

    df = pd.DataFrame.from_dict(network_dictionary)
    
    if normalize_weights:
        # Normalize the weights
        max_weight = df['weight'].max()
        if not max_weight == 0.0:
            df['weight'] = df['weight'] / max_weight
    
    logging.debug('Network as dataframe:\n' + str(df))
    
    full_text = ''
    full_text += 'network {0} {{\n'.format(network_name)
    full_text += '  is_undirected = 1\n'
    full_text += '__ADD_EDGE_REPLACE__\n'
    full_text += '}\n'

    add_edge_text = ''
    for i in range(len(df)):
        add_edge_text += '  add_edge = {0} {1} {2}\n'.format(df.loc[i, 'to'], df.loc[i, 'from'], df.loc[i, 'weight'])
        
    # Replace the appropriate parts of the full_text
    full_text = full_text.replace('__ADD_EDGE_REPLACE__', add_edge_text)
    
    # write out the .fred file
    with open(outfile_str, 'w') as file:
        file.write(full_text)
        
        
if __name__ == '__main__':
    main()

