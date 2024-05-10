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
import sys
from typing import Self

from fredpy.genetic_algorithm_calibration import genetic_calibration_param_group

class GeneticCalibration():
    '''
    GeneticCalibration class

    This is a class that actas as a container for many BreedableParameters
    '''
    def __init__(self: Self):
        '''
        Default Constructor
        '''
        self.__param_group = genetic_calibration_param_group.BreedableParameterGoup()
        self.__targets = {}
        self.__outcomes = {}
        self.__target_to_parameters_weights = {}
        self.__threshold = 0.1
        self.__evaluated_error = sys.float_info.max
        self.__fred_key = None
 
    @property
    def param_group(self: Self):
        return self.__param_group
        
    @param_group.setter
    def param_group(self: Self, value):
        self.__param_group = value
        
    @property
    def targets(self: Self):
        return self.__targets

    @targets.setter
    def targets(self: Self, value: dict[str, float]):
        self.__targets = value
        
    @property
    def outcomes(self: Self):
        return self.__outcomes

    @outcomes.setter
    def outcomes(self: Self, value: dict[str, float]):
        raise ValueError('GeneticCalibration.outcomes is read-only and can only be altered using the evaluate() method')
        
    @property
    def target_to_parameters_weights(self: Self):
        return self.__target_to_parameters_weights

    @target_to_parameters_weights.setter
    def target_to_parameters_weights(self: Self, value: dict[str, dict[str, float]]):
        self.__target_to_parameters_weights = value
        
    @property
    def threshold(self: Self):
        return self.__threshold

    @threshold.setter
    def threshold(self: Self, value: int):
        self.__threshold = value
        
    @property
    def evaluated_error(self: Self):
        return self.__evaluated_error

    @evaluated_error.setter
    def evaluated_error(self: Self, value: int):
        raise ValueError('GeneticCalibration.evaluated_error is read-only and can only be altered using the evaluate() method')
        
    @property
    def fred_key(self: Self):
        return self.__fred_key
        
    @fred_key.setter
    def fred_key(self: Self, value: int):
        self.__fred_key = value
                   
    def __str__(self: Self):
        return '{{param_group: {0}, targets: {1}, outcomes: {2}, threshold: {3}, evaluated_error: {4}, fred_key: {5}}}'.format(self.__param_group, self.__targets, self.__outcomes, self.__threshold, self.__evaluated_error, self.__fred_key)

    def __eq__(self: Self, other):
        return self.__evaluated_error == other.__evaluated_error

    def __ne__(self: Self, other):
        return self.__evaluated_error != other.__evaluated_error
        
    def __gt__(self: Self, other):
        return self.__evaluated_error > other.__evaluated_error
        
    def __ge__(self: Self, other):
        return self.__evaluated_error >= other.__evaluated_error
        
    def __lt__(self: Self, other):
        return self.__evaluated_error < other.__evaluated_error
        
    def __le__(self: Self, other):
        return self.__evaluated_error <= other.__evaluated_error
        
    def setup(self: Self):
        # Using the intial param_group, create start off with a randomized set of those parameters
        self.__param_group = self.__param_group.randomize()
        
    def evaluate(self: Self, outcomes: dict[str, float]):
        # Make sure targets dictionary and outcomes dictionary have the same keys
        if self.__targets.keys() != outcomes.keys():
            raise ValueError('The targets dictionary and the outcomes dictionary have different keys: {0} -> {1}.'.format(self.__targets, outcomes))
       
        self.__outcomes = outcomes
        squared_error = 0.0
        for target_name, target_value in self.__targets.items():
            other_value = outcomes[target_name]
            if target_value == 0.0:
                squared_error += ((target_value - other_value) / 0.001) ** 2
            else:
                squared_error += ((target_value - other_value) / target_value) ** 2
            
        self.__evaluated_error = math.sqrt(squared_error)
        
    def evaluate_individual_error_contributions(self: Self):
        ret_val = {}
        if self.__outcomes == None:
            raise ValueError('The outcomes must be set by using the evaluate method before we can evaluate individual contributions')
        # Make sure targets dictionary and outcomes dictionary have the same keys
        if self.__targets.keys() != self.__outcomes.keys():
            raise ValueError('The targets dictionary and the outcomes dictionary have different keys: {0} -> {1}.'.format(self.__targets, outcomes))
            
        for target_name, target_value in self.__targets.items():
            other_value = self.__outcomes[target_name]
            
            if target_value == 0.0:
                error = math.sqrt(((target_value - other_value) / 0.001) ** 2)
            else:
                error = math.sqrt(((target_value - other_value) / target_value) ** 2)
                
            ret_val[target_name] = error
        
        return ret_val
        
    def breed(self: Self, other_genetic_calibration):
        # Make sure that the parameter lists are the same size
        if len(self.param_group.param_list) != len(other_genetic_calibration.param_group.param_list):
            raise ValueError('Tried to breed two parameter groups with different parameter list sizes: {0} -> {1}.'.format(len(self.param_group.param_list), len(other_genetic_calibration.param_group.param_list)))
        # Make sure dictionaries have the same keys
        if self.param_group.param_index_dict.keys() != other_genetic_calibration.param_group.param_index_dict.keys():
            raise ValueError('Tried to breed two parameter groups with different parameter index dictionaries: {0} -> {1}.'.format(self.param_group.param_index_dict, other_genetic_calibration.param_group.param_index_dic))
   
        # Create a new GeneticCalibration
        gc = GeneticCalibration()
        gc.param_group = self.param_group.breed(other_genetic_calibration.param_group)
        gc.targets = self.targets
        gc.threshold = self.threshold
        gc.target_to_parameters_weights = self.target_to_parameters_weights
        return gc
        
        #individual_error_contributions: {'opioid_deaths': 0.638125, 'opioid_use_disorder': 0.5410416666666666}
    def gradient_update_to_target(self: Self):

        # Create a new GeneticCalibration
        gc = GeneticCalibration()
        gc.param_group = self.param_group.gradient_update_to_target(self.outcomes, self.targets, self.target_to_parameters_weights)
        gc.targets = self.targets
        gc.threshold = self.threshold
        gc.target_to_parameters_weights = self.target_to_parameters_weights
        return gc

