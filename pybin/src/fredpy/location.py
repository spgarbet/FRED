"""Module containing all location classes and errors related to locations for the FRED system

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

from typing import List, Self

from fredpy import frederr
from fredpy.util import constants

try:
    from haversine import haversine
except ImportError:
    print('This script requires the haversine module.')
    print('Installation instructions can be found at https://pypi.org/project/haversine/')
    sys.exit(constants.EXT_CD_ERR)

class Place:
    '''
    Place class

    This is a generic base class for all Places. It has the simplest attributes that a place would need: namely id, lat, lon
    '''
    def __init__(self: Self, id: str, lat: float, lon: float):
        '''
        Default Constructor
        
        Parameters
        ----------
        id : str
            The id of the Place
        lat : float
            The latitude of Place
        lon : float
            The longitude of Place
        '''
        
        self.__id = id
        
        if lat < -90.0 or lat > 90.0:
            raise ValueError('Latitude {0} is out of range [-90.0, 90.0]'.format(lat))
        self.__latitude = lat
            
        if lon < -180.0 or lon > 180.0:
            raise ValueError('Longitude {0} is out of range [-180.0, 180.0]'.format(lon))
        self.__longitude = lon
        
    @property
    def id(self: Self):
        return self.__id

    @id.setter
    def id(self: Self, value):
        raise frederr.ReadOnlyPropertyError("id")

    @property
    def latitude(self: Self):
        return self.__latitude

    @latitude.setter
    def latitude(self: Self, value):
        if value < -90.0 or value > 90.0:
            raise ValueError('Latitude {0} is out of range [-90.0, 90.0]'.format(value))
        self.__latitude = value
        
    @property
    def longitude(self: Self):
        return self.__longitude

    @longitude.setter
    def longitude(self: Self, value):
        if value < -180.0 or value > 180.0:
            raise ValueError('Longitude {0} is out of range [-180.0, 180.0]'.format(value))
        self.__longitude = value
        
    def __repr__(self: Self):
        return 'location.Place(\'{0}\', {1}, {2})'.format(self.__id, self.__latitude, self.__longitude)
            
    def __str__(self: Self):
        return '{{id: \'{0}\', latitude: {1}, longitude: {2}}}'.format(
            self.__id, self.__latitude, self.__longitude)

    def __eq__(self: Self, other):
        return self.id == other.id

    def __ne__(self: Self, other):
        return self.id != other.id
        
    def __gt__(self: Self, other):
        return self.id > other.id
        
    def __ge__(self: Self, other):
        return self.id >= other.id
        
    def __lt__(self: Self, other):
        return self.id < other.id
        
    def __le__(self: Self, other):
        return self.id <= other.id
        
    def get_distance_to_here(self: Self, place) -> float:
        return haversine((place.latitude, place.longitude), (self.latitude, self.longitude))

    def get_place_list_sorted_by_distance_to_here(self: Self, place_list: List, max_list_size: int = 10) -> List:
        '''
        Sort a list of places by increasing distance to this place.
        
        Parameters
        ----------
        self : Self
            This Place
        place_list : List
            List of Places to compare distance to this Place
            
        Returns
        -------
        List
            A List of Places sorted by the distance to this Place
        '''
        
        if len(place_list) == 0:
            return []
        elif len(place_list) == 1:
            return place_list.copy()
        else:
            return_list = []
            id_dictionary = {}
            # Create a dictionary of each id in the list with its distance to here
            for place in place_list:
                id_dictionary[place.id] = self.get_distance_to_here(place)
            
            # Sort the dictionary by the distance
            id_dictionary = dict(sorted(id_dictionary.items(), key=lambda item: item[1]))
            
            # Create the return list by getting the Places from the place_list by the id
            return_count = 0
            for key in id_dictionary:
                found_place = None
                for place in place_list:
                    if place.id == key:
                        found_place = place
                        return_count += 1
                        break
                return_list.append(found_place)
                if return_count >= max_list_size:
                    break
                
            # Return the sorted list
            return return_list
    
            
class DaycareCenter(Place):
    '''
    DaycareCenter class

    The class that represents a DayCare center in the FRED system
    
    Examples
    --------

    >>> from fredpy import location
    >>> center1 = location.DaycareCenter('id101', 10.0, 10.0)
    >>> center2 = location.DaycareCenter('id102', 11.0, 20.0, 10, 11, 12, 100)
    >>> center3 = location.DaycareCenter('aid103', 10.0, 20.0, 10, 11, 12, 100)
    >>> center4 = location.DaycareCenter('id104', 13.0, 20.0, 10, 11, 12, 100)
    >>> center5 = location.DaycareCenter('zid105', 12.0, 20.0, 10, 11, 12, 100)
    >>> center6 = location.DaycareCenter('id106', 10.0, 20.0, 10, 11, 12, 100)
    >>> center7 = location.DaycareCenter('did107', 10.0, 10.0)
    >>> center8 = location.DaycareCenter('id108', 10.0, 10.0)
    >>>
    >>> print(repr(center2))
    >>> location.DaycareCenter('id102', 11.0, 20.0, 10, 11, 12, 100)
    >>>
    >>> print('{0}'.format(center2))
    >>> {id: 'id102', latitude: 11.0, longitude: 20.0, infant_count: 10, toddler_count: 11, preschooler_count: 12, max_size: 100}
    >>>
    >>> print(center8.get_place_list_sorted_by_distance_to_here([center1, center2, center3, center4, center5, center6, center7]))
    >>> [location.DaycareCenter('id101', 10.0, 10.0, 0, 0, 0, 50), location.DaycareCenter('did107', 10.0, 10.0, 0, 0, 0, 50), location.DaycareCenter('aid103', 10.0, 20.0, 10, 11, 12, 100), location.DaycareCenter('id106', 10.0, 20.0, 10, 11, 12, 100), location.DaycareCenter('id102', 11.0, 20.0, 10, 11, 12, 100), location.DaycareCenter('zid105', 12.0, 20.0, 10, 11, 12, 100), location.DaycareCenter('id104', 13.0, 20.0, 10, 11, 12, 100)]
    >>>
    >>> center2.preschooler_count += 106
    >>> Traceback (most recent call last):
        File "/Users/ddg5/FRED/pybin/bin/./testdaycare.py", line 85, in <module>
        main()
        File "/Users/ddg5/FRED/pybin/bin/./testdaycare.py", line 80, in main
        center2.preschooler_count += 106
        ^^^^^^^^^^^^^^^^^^^^^^^^^
        File "/Users/ddg5/FRED/pybin/src/fredpy/location.py", line 279, in preschooler_count
        raise MaxAssignedOccupancyExceededError(constants.PRESCHOOLERS, value, self.__infant_count,
        fredpy.location.MaxAssignedOccupancyExceededError: If you update the Preschooler Count to 118, you will exceed the maximum count of children allowed to be assigned to the Daycare Center: 100
        Infant Count = 10
        Toddler Count = 11
        Preschooler Count = 118
        --------------------------
        139 > 100
    '''
    def __init__(self: Self, id: str, lat: float, lon: float, infants: int = 0, toddlers: int = 0, preschoolers: int = 0,
                 max_size: int = constants.DEFAULT_MAX_DAYCARE_SIZE):
        '''
        Default Constructor
        
        Parameters
        ----------
        id : str
            The id of the Daycare center
        lat : float
            The latitude of the Daycare Center
        lon : float
            The longitude of the Daycare Center
        infants : int
            The count of infants (0 <= age < 1) in the Daycare Center
        toddlers : int
            The count of toddlers (1 <= age < 3) in the Daycare Center
        preschoolers : int
            The count of preschool-aged (3 <= age < 5) in the Daycare Center
        max_size : int
            The maximum number of children that may be assigned to the Daycare Center
        '''
        
        if infants + toddlers + preschoolers > max_size:
            lines = []
            lines.append('Exceeds the maximum count of children allowed to be assigned to the Daycare Center: {0}'.format(max_size))
            lines.append('{0} = {1}'.format(constants.INFANTS, infants))
            lines.append('{0} = {1}'.format(constants.TODDLERS, toddlers))
            lines.append('{0} = {1}'.format(constants.PRESCHOOLERS, preschoolers))
            lines.append('--------------------------')
            lines.append('{0} > {1}'.format(infants + toddlers + preschoolers, max_size))
            msg = '\n'.join(lines)
            raise MaxAssignedOccupancyExceededError(msg = msg)
        
        super().__init__(id, lat, lon)
        self.__infant_count = infants
        self.__toddler_count = toddlers
        self.__preschooler_count = preschoolers
        self.__max_size = max_size
 
    @property
    def infant_count(self: Self):
        return self.__infant_count

    @infant_count.setter
    def infant_count(self: Self, value):
        if self.__toddler_count + self.__preschooler_count + value > self.__max_size:
            raise MaxAssignedOccupancyExceededError(constants.INFANTS, value, self.__toddler_count,
                self.__preschooler_count, self.__max_size)
        else:
            self.__infant_count = value
        
    @property
    def toddler_count(self: Self):
        return self.__toddler_count

    @toddler_count.setter
    def toddler_count(self: Self, value):
        if self.__infant_count + self.__preschooler_count + value > self.__max_size:
            raise MaxAssignedOccupancyExceededError(constants.TODDLERS, value, self.__infant_count,
                self.__preschooler_count, self.__max_size)
        else:
            self.__toddler_count = value
        
    @property
    def preschooler_count(self: Self):
        return self.__preschooler_count

    @preschooler_count.setter
    def preschooler_count(self: Self, value):
        if self.__infant_count + self.__toddler_count + value > self.__max_size:
            raise MaxAssignedOccupancyExceededError(constants.PRESCHOOLERS, value, self.__infant_count,
                self.__toddler_count, self.__max_size)
        else:
            self.__preschooler_count = value
            
    @property
    def max_size(self: Self):
        return self.__max_size

    @max_size.setter
    def max_size(self: Self, value):
        raise frederr.ReadOnlyPropertyError("max_size")
        
    def __repr__(self: Self):
        return 'location.DaycareCenter(\'{0}\', {1}, {2}, {3}, {4}, {5}, {6})'.format(self.id, self.latitude, self.longitude,
            self.__infant_count, self.__toddler_count, self.__preschooler_count, self.__max_size)
            
    def __str__(self: Self):
        return '{{id: \'{0}\', latitude: {1}, longitude: {2}, infant_count: {3}, toddler_count: {4}, preschooler_count: {5}, max_size: {6}}}'.format(
            self.id, self.latitude, self.longitude,  self.__infant_count, self.__toddler_count, self.__preschooler_count, self.__max_size)
    
class MaxAssignedOccupancyExceededError(frederr.FredError):
    '''Exception raised when we try to assign too many children to the Daycare Center.'''
    
    def __init__(self, which_count: str = constants.INFANTS, new_count: int = 0, other_assigned_count_1: int = 0,
                 other_assigned_count_2: int = 0, max_size: int = 0, msg: str = ''):
                 
        if msg == '' and which_count == constants.INFANTS:
            lines = []
            lines.append('If you update the {0} to {1}, you will exceed the maximum count of children allowed to be assigned to the Daycare Center: {2}'.format(
                constants.INFANTS, new_count, max_size))
            lines.append('{0} = {1}'.format(constants.INFANTS, new_count))
            lines.append('{0} = {1}'.format(constants.TODDLERS, other_assigned_count_1))
            lines.append('{0} = {1}'.format(constants.PRESCHOOLERS, other_assigned_count_2))
            lines.append('--------------------------')
            lines.append('{0} > {1}'.format(new_count + other_assigned_count_1 + other_assigned_count_2, max_size))
            msg = '\n'.join(lines)
        elif msg == '' and which_count == constants.TODDLERS:
            lines = []
            lines.append('If you update the {0} to {1}, you will exceed the maximum count of children allowed to be assigned to the Daycare Center: {2}'.format(
                constants.TODDLERS, new_count, max_size))
            lines.append('{0} = {1}'.format(constants.INFANTS, other_assigned_count_1))
            lines.append('{0} = {1}'.format(constants.TODDLERS, new_count))
            lines.append('{0} = {1}'.format(constants.PRESCHOOLERS, other_assigned_count_2))
            lines.append('--------------------------')
            lines.append('{0} > {1}'.format(other_assigned_count_1 + new_count + other_assigned_count_2, max_size))
            msg = '\n'.join(lines)
        elif msg == '' and which_count == constants.PRESCHOOLERS:
            lines = []
            lines.append('If you update the {0} to {1}, you will exceed the maximum count of children allowed to be assigned to the Daycare Center: {2}'.format(
                constants.PRESCHOOLERS, new_count, max_size))
            lines.append('{0} = {1}'.format(constants.INFANTS, other_assigned_count_1))
            lines.append('{0} = {1}'.format(constants.TODDLERS, other_assigned_count_2))
            lines.append('{0} = {1}'.format(constants.PRESCHOOLERS, new_count))
            lines.append('--------------------------')
            lines.append('{0} > {1}'.format(other_assigned_count_1 + other_assigned_count_2 + new_count, max_size))
            msg = '\n'.join(lines)
            
        super(frederr.FredError, self).__init__(msg)
    
