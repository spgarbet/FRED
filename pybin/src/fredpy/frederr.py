'''Module containing the FRED specific errors.

This module contains classes for errors that are specific to FRED.

Examples
--------

>>> from fredpy import frederr
>>> import os
>>> fred_home_dir = os.environ.get('FRED_HOME')
>>> if fred_home_dir == None:
...     raise frederr.FredHomeUnsetError()
...
Traceback (most recent call last):
  File "<stdin>", line 2, in <module>
fredpy.frederr.FredHomeUnsetError: Environment Variable, FRED_HOME, is not set. Please set it to the location of FRED home directory.
'''

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

class FredError(Exception):
    '''Base Exception class for all FRED specific Exceptions'''
    pass
    
class FredSweepCartesianProductError(FredError):
    '''Exception raised when the sweep will create more combinations than the allowable threshold
    
    '''
    
    def __init__(self, allowable_threshold, msg=''):
        if msg == '':
            msg = f'The row count of the parameter combinations is greater than the allowable threshold: [{allowable_threshold}]'
        super(FredError, self).__init__(msg)
        

class FredSweepJsonError(FredError):
    '''Exception raised when the JSON sweep file is invalid
    
    '''
    
    def __init__(self, filename, reason, msg=''):
        if msg == '':
            msg = f'The .json file [{filename}] did not pass validation: {reason}.'
        super(FredError, self).__init__(msg)

class FredSweepParamError(FredError):
    '''Exception raised when the sweep is trying to change values for parameters that don't exist in the base file
    
    '''
    
    def __init__(self, msg):
        super(FredError, self).__init__(msg)

class FredFileSizeError(FredError):
    '''Exception raised when the a FRED file does not have the number of lines that we would expect.
    
    Commonly FRED writes files with a single line which is then able to be read as `cat filename` into a variable.
    '''
    
    def __init__(self, filename, msg=''):
        if msg == '':
            msg = f'The file [{filename}] should have exactly one line.'
        super(FredError, self).__init__(msg)
        
class FredHomeUnsetError(FredError):
    '''Exception raised when the user has not set their FRED_HOME environment variable.'''
    
    def __init__(self, msg=None):
        if msg is None:
            msg = 'Environment Variable, FRED_HOME, is not set. Please set it to the location of FRED home directory.'
        super(FredError, self).__init__(msg)
    
class FredRunError(FredError):
    '''Exception raised when an error occurs during a FRED run.'''
    
    def __init__(self, log_file: str, msg: str = ''):
        if msg == '':
            msg = 'FRED run unsuccessful. See the LOG file at [{0}] for more detail.'.format(log_file)
        super(FredError, self).__init__(msg)
    
class ReadOnlyPropertyError(FredError):
    '''Exception raised when trying to set a property that is read-only.'''
    
    def __init__(self, property_name: str, is_constructable: bool = True, msg: str = ''):
        if msg == '' and is_constructable:
            msg = '{0} is read-only and must be set in the Constructor'.format(property_name)
        elif msg == '' and not is_constructable:
            msg = '{0} is read-only'.format(property_name)
            
        super(FredError, self).__init__(msg)
        
