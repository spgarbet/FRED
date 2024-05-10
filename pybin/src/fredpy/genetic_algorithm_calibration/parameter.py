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
import math
import random
from typing import Self


class Parameter:
    '''
    Parameter class

    This is a class for a generic parameter that has a given value and can range from min_val to max_val
    '''
    def __init__(self: Self, name: str, val: float, min_val: float, max_val: float):
        '''
        Default Constructor
        
        Parameters
        ----------
        name: str
            The string that fred uses to identify the parameter
        val: float
            The actual value of this parameter
        min_val: float
            The minimum value that can be assigned to val
        max_val: float
            The maximum value that can be assigned to val
            
        '''
        if val < min_val or val > max_val:
            raise ValueError('Value, {0}, is out of range ({1}, {2}).'.format(val, min_val, max_val))
        if min_val >= max_val:
            raise ValueError('Min_val, {0}, must be strictly less than max_val, {1}.'.format(min_val, max_val))
        self.__name = name
        self.__val = val
        self.__min_val = min_val
        self.__max_val = max_val
        
    @property
    def name(self: Self):
        return self.__name
        
    @name.setter
    def name(self: Self, value):
        raise ValueError('Paramer.name is read-only and must be set in the Constructor')
        
    @property
    def val(self: Self):
        return self.__val

    @val.setter
    def val(self: Self, value):
        self.__val = value

    @property
    def min_val(self: Self):
        return self.__min_val

    @min_val.setter
    def min_val(self: Self, value):
        if value >= self.__max_val:
            raise ValueError('Tried to set a min_val, {0}, that is greater than or equal to the current max_val, {1}.'.format(value, self.__max_val))
        self.__min_val = value
        
    @property
    def max_val(self: Self):
        return self.__max_val

    @max_val.setter
    def max_val(self: Self, value):
        if value <= self.__min_val:
            raise ValueError('Tried to set a max_val, {0}, that is less than or equal to the current min_val, {1}.'.format(value, self.__min_val))
        self.__max_val = value
        
    def __repr__(self: Self):
        return 'parameter.Parameter({0}, {1}, {2})'.format(self.__val, self.__min_val, self.__max_val)
            
    def __str__(self: Self):
        return '{{val: {0}, min_val: {1}, max_val: {2}}}'.format(self.__val, self.__min_val, self.__max_val)

class BreedableParameter(Parameter):
    '''
    BreedableParameter class

    This is a generic class for parameter that is able to be "bred" with another BreedableParameter
    '''
    def __init__(self: Self, name: str, val: float, min_val: float, max_val: float, mutation_prob: float = 0.1):
        '''
        Default Constructor
        
        Parameters
        ----------
        name: str
            The string that fred uses to identify the parameter
        val: float
            The actual value of this parameter
        min_val: float
            The minimum value that can be assigned to val
        max_val: float
            The maximum value that can be assigned to val
            
        '''
        super().__init__(name, val, min_val, max_val)
        if mutation_prob < 0.0 or mutation_prob > 1.0:
            raise ValueError('Mutation_prob, {0} is out of allowable range [0.0, 1.0].'.format(mutation_prob))
        self.__mutation_prob = mutation_prob
        
    @property
    def mutation_prob(self: Self):
        return self.__mutation_prob

    @mutation_prob.setter
    def mutation_prob(self: Self, value):
        if value < 0.0 or value > 1.0:
            raise ValueError('Mutation_prob, {0} is out of allowable range [0.0, 1.0].'.format(value))
        self.__mutation_prob = value
        
    def __repr__(self: Self):
        return 'parameter.BreedableParameter({0}, {1}, {2}, {3}, {4})'.format(self.name, self.val, self.min_val, self.max_val, self.__mutation_prob)
            
    def __str__(self: Self):
        return '{{name: \'{0}\', val: {1}, min_val: {2}, max_val: {3}, mutation_prob: {4}}}'.format(self.name, self.val, self.min_val, self.max_val, self.__mutation_prob)
        
    def clone(self: Self):
        return BreedableParameter(self.name, self.val, self.min_val, self.max_val, self.mutation_prob)
                
    def breed(self: Self, other_param):
        if self.name != other_param.name:
            raise ValueError('Tried to breed two parameters with different names: {0} -> {1}.'.format(self.name , other_param.name))
        
        if self.val == other_param.val:
            return BreedableParameter.generate_random(self.name, self.min_val, self.max_val, self.min_val, self.max_val, self.mutation_prob)
        elif random.uniform(0, 1) < self.mutation_prob:
            rnd = random.uniform(0, 1)
            # Make it so that the random parameter is NOT between the two parents' values
            if self.val < other_param.val:
                total_range = (self.val - self.min_val) + (self.max_val - other_param.val)
                if rnd < (self.val - self.min_val) / total_range:
                    return BreedableParameter.generate_random(self.name, self.min_val, self.max_val, self.min_val, self.val, self.mutation_prob)
                else:
                    return BreedableParameter.generate_random(self.name, self.min_val, self.max_val, other_param.val, self.max_val, self.mutation_prob)
            else:
                total_range = (other_param.val - self.min_val) + (self.max_val - self.val)
                if rnd < (other_param.val - self.min_val) / total_range:
                    return BreedableParameter.generate_random(self.name, self.min_val, self.max_val, self.min_val, other_param.val, self.mutation_prob)
                else:
                    return BreedableParameter.generate_random(self.name, self.min_val, self.max_val, self.val, self.max_val, self.mutation_prob)
        elif self.val < other_param.val:
            return BreedableParameter(self.name, random.uniform(self.val, other_param.val), self.min_val, self.max_val, self.mutation_prob)
        else:
            return BreedableParameter(self.name, random.uniform(other_param.val, self.val), self.min_val, self.max_val, self.mutation_prob)
            
    def move_towards_value(self: Self, target: float, rate: float):
        if rate < 0 or rate > 1:
            raise ValueError('For move_towards_value, rate must be between 0.0 and 1.0. Rate: {0}.'.format(rate))
        if target > self.max_val:
            total_distance = abs(self.max_val - self.val)
        elif target < self.min_val:
            total_distance = abs(self.val - self.min_val)
        else:
            total_distance = abs(target - self.val)
            
        if self.val < target:
            return BreedableParameter(self.name, (self.val + total_distance * rate), self.min_val, self.max_val, self.mutation_prob)
        elif self.val > target:
            return BreedableParameter(self.name, (self.val - total_distance * rate), self.min_val, self.max_val, self.mutation_prob)
        else:
            return BreedableParameter(self.name, self.val, self.min_val, self.max_val, self.mutation_prob)


    @staticmethod
    def generate_random(name: str, min_val: float, max_val: float, min_rnd_val: float, max_rnd_val: float, mutation_prob: float = 0.1):
        return BreedableParameter(name, random.uniform(min_rnd_val, max_rnd_val), min_val, max_val, mutation_prob)
