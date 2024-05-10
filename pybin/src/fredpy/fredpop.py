"""Module containing classes for handling the population files used by the FRED system

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
import abc
from typing import Self

from fredpy.util import fredutil
from fredpy import frederr

try:
    import pandas as pd
except ImportError:
    print('This script requires the pandas module.')
    print('Installation instructions can be found at https://pypi.org/project/pandas/.')
    sys.exit(constants.EXT_CD_ERR)

class FredPopulationData(metaclass=abc.ABCMeta):
    '''
    FredPopulationData class

    This is the base class for all classes that hold the information from the FRED population files
    '''
    def __init__(self: Self, fips: str, pop_dir_str: str = None):
        '''
        Default Constructor
        
        Parameters
        ----------
        fips : str
            The FIPS code that identifies the county in FRED
        pop_dir_str : str
            The full string of the directory where the population files reside
            
        '''
        
        if type(fips) == str:
            if len(fips) != 5:
                raise ValueError('FIPS must either be must be a five digit string or a positive integer <= 99999 (fips = {0})'.format(fips))
        elif type(fips) == int:
            if fips < 0 or fips > 99999:
                raise ValueError('FIPS must either be must be a five digit string or a positive integer <= 99999 (fips = {0})'.format(fips))
            fips = '{:05d}'.format(fips)
        else:
            raise ValueError('FIPS must either be must be a five digit string or a positive integer <= 99999 (fips = {0})'.format(fips))
        self.__fips = fips
        
        if pop_dir_str is None:
            self.__pop_dir_str = '{0}/{1}/{2}/{3}/{4}'.format(fredutil.get_fred_home_dir_str(), 'data', 'country', 'usa', 'US_2010.v3')
        else:
            self.__pop_dir_str = pop_dir_str
            
    @property
    def fips(self: Self):
        return self.__fips

    @fips.setter
    def fips(self: Self, value):
        raise frederr.ReadOnlyPropertyError("fips")

    @property
    def pop_dir_str(self: Self):
        return self.__pop_dir_str

    @pop_dir_str.setter
    def pop_dir_str(self: Self, value):
        raise frederr.ReadOnlyPropertyError("pop_dir_str")
        
    @abc.abstractmethod
    def find(self: Self, id: str):
        pass
    
        
class FredPeopleData(FredPopulationData):
    '''
    FredPeopleData class

    The class that holds the specific information from a people.txt file from the FRED populations
    
    '''
    def __init__(self: Self, fips: str, pop_dir_str: str = None):
        '''
        Default Constructor
        
        Parameters
        ----------
        fips : str
            The FIPS code that identifies the county in FRED
        pop_dir_str : str
            The full string of the directory where the population files reside
            
        '''
        
        super().__init__(fips, pop_dir_str)
        self.__people_df = self._read_people_file()
        
    @property
    def people_df(self):
        return self.__people_df

    @people_df.setter
    def people_df(self, value):
        raise frederr.ReadOnlyPropertyError('people_df')
        
    def find(self: Self, id: str):
        return self.__people_df[self.__people_df['sp_id'] == id]
        
    def _read_people_file(self: Self) -> pd.DataFrame:
        '''
        The private function used to read the people.txt file given this object's pop_dir_str and fips
        
        Parameters
        ----------
        self : Self
            This FredPeopleData
        
        Returns
        -------
        pd.DataFrame
            A dataframe representation of the data in the people.txt file
      
        '''
        
        filename = '{0}/{1}/people.txt'.format(self.pop_dir_str, self.fips)
        df = pd.read_csv(filename, dtype={'sp_id': str, 'sp_hh_id': str, 'school_id': str, 'work_id': str}, sep='\t')
        return df
        
class FredHouseholdData(FredPopulationData):
    '''
    FredHouseholdData class

    The class that holds the specific information from a households.txt file from the FRED populations
    
    '''
    def __init__(self: Self, fips: str, pop_dir_str: str = None):
        '''
        Default Constructor
        
        Parameters
        ----------
        fips : str
            The FIPS code that identifies the county in FRED
        pop_dir_str : str
            The full string of the directory where the population files reside
            
        '''
        
        super().__init__(fips, pop_dir_str)
        self.__households_df = self._read_households_file()
        
    @property
    def households_df(self):
        return self.__households_df

    @households_df.setter
    def households_df(self, value):
        raise frederr.ReadOnlyPropertyError('households_df')
        
    def find(self: Self, id: str):
        return self.__households_df[self.__households_df['sp_hh_id'] == id]
        
    def _read_households_file(self: Self) -> pd.DataFrame:
        '''
        The private function used to read the households.txt file given this object's pop_dir_str and fips

        Parameters
        ----------
        self : Self
            This FredHouseholdData
        
        Returns
        -------
        pd.DataFrame
            A dataframe representation of the data in the households.txt file
            
        '''
        
        filename = '{0}/{1}/households.txt'.format(self.pop_dir_str, self.fips)
        df = pd.read_csv(filename, dtype={'sp_id': str, 'stcotrbg': str}, sep='\t')
        df = df.rename(errors='raise', columns={'sp_id': 'sp_hh_id'})
        return df
        
class FredSchoolData(FredPopulationData):
    '''
    FredSchoolData class

    The class that holds the specific information from a schools.txt file from the FRED populations
    
    '''
    
    def __init__(self: Self, fips: str, pop_dir_str: str = None):
        '''
        Default Constructor
        
        Parameters
        ----------
        fips : str
            The FIPS code that identifies the county in FRED
        pop_dir_str : str
            The full string of the directory where the population files reside
            
        '''
        
        super().__init__(fips, pop_dir_str)
        self.__schools_df = self._read_schools_file()
        
    @property
    def schools_df(self):
        return self.__schools_df

    @schools_df.setter
    def schools_df(self, value):
        raise frederr.ReadOnlyPropertyError('schools_df')
        
    def find(self: Self, id: str):
        return self.schools_df[self.schools_df['sp_id'] == id]
        
    def _read_schools_file(self: Self) -> pd.DataFrame:
        '''
        The private function used to read the schools.txt file given this object's pop_dir_str and fips

        Parameters
        ----------
        self : Self
            This FredSchoolData
        
        Returns
        -------
        pd.DataFrame
            A dataframe representation of the data in the schools.txt file
            
        '''
        
        filename = '{0}/{1}/schools.txt'.format(self.pop_dir_str, self.fips)
        df = pd.read_csv(filename, dtype={'sp_id': str, 'stco': str}, sep='\t')
        return df
    
