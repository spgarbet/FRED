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
from typing import Self

from fredpy.genetic_algorithm_calibration.parameter import BreedableParameter

class BreedableParameterGoup():
    '''
    BreedableParameterGoup class

    This is a class that acts as a container for many BreedableParameters
    '''
    def __init__(self: Self):
        '''
        Default Constructor
        '''
        self.__param_list = []
        self.__param_index_dict = {}
        
    @property
    def param_list(self: Self):
        return self.__param_list
        
    @param_list.setter
    def name(self: Self, value):
        self.__param_list = value
        
    @property
    def param_index_dict(self: Self):
        return self.__param_index_dict

    @param_index_dict.setter
    def val(self: Self, value):
        self.__param_index_dict = value
                   
    def __str__(self: Self):
        object_string = ''
        first_item = True
        for param in self.__param_list:
            if first_item:
                object_string = '{0}'.format(param)
                first_item = False
            else:
                object_string = '{0}, {1}'.format(object_string, param)
            
        return '{{params: [{0}]}}'.format(object_string)
        
        
    def append(self: Self, parameter):
        if parameter.name in self.__param_index_dict.keys():
            raise ValueError('Unable to append an existing parameter, {0}, to the Parameter List'.format(parameter.name))
        else:
            self.__param_list.append(parameter)
            self.__param_index_dict[parameter.name] = len(self.__param_list) - 1
            
    def remove(self: Self, parameter):
        if parameter.name in self.__param_index_dict.keys():
            # Get the index of the parameter to remove
            idx = self.__param_index_dict[parameter.name]
            # Update the index of all parameters that have higher indexes
            for key, value in self.__param_index_dict.items():
                if value > idx:
                    self.__param_index_dict[key] = value - 1
            # Remove the parameter from the dictionary
            del self.__param_index_dict[parameter.name]
            # Remove the parameter from the list
            self.__param_list.pop([idx])
            
    def clear(self: Self):
        self.__param_list = ()
        self.__param_index_dict = {}
        
    def get_as_dictionary(self: Self):
        ret_val = {}
        for name, idx in self.__param_index_dict.items():
            ret_val[name] = self.__param_list[idx].val
            
        return ret_val
        
    def randomize(self: Self):
    
        # Create a new parameter calibration group
        new_group = BreedableParameterGoup()
        
        for name, idx in self.__param_index_dict.items():
            new_param = BreedableParameter.generate_random(self.__param_list[idx].name, self.__param_list[idx].min_val, self.__param_list[idx].max_val, self.__param_list[idx].min_val, self.__param_list[idx].max_val, self.__param_list[idx].mutation_prob)
            new_group.append(new_param)
            
        return new_group
        
    def breed(self: Self, other_param_group):
        # Make sure that the parameter lists are the same size
        if len(self.__param_list) != len(other_param_group.__param_list):
            raise ValueError('Tried to breed two parameter groups with different parameter list sizes: {0} -> {1}.'.format(len(self.__param_list), len(other_param_group.__param_list)))
        # Make sure dictionaries have the same keys
        if self.__param_index_dict.keys() != other_param_group.__param_index_dict.keys():
            raise ValueError('Tried to breed two parameter groups with different parameter index dictionaries: {0} -> {1}.'.format(self.__param_index_dict, other_param_group.__param_index_dict))
   
        # Create a new parameter calibration group
        new_group = BreedableParameterGoup()
        
        for name, idx in self.__param_index_dict.items():
            other_idx = other_param_group.__param_index_dict[name]
            new_group.append(self.__param_list[idx].breed(other_param_group.__param_list[other_idx]))
            
        return new_group


    def gradient_update_to_target(self: Self, outcomes, targets, target_to_parameters_weights):
        # Create a new parameter calibration group
        new_group = BreedableParameterGoup()
        

        # I need outcome to be multiplied by 1 / (current_outcome / target)
        # If current_outcome = 0 or target = 0 ... special cases
        target_multipliers = {}
        for name, outcome in outcomes.items():
            if targets[name] == 0.0 or outcome == 0.0:
                continue
            else:
                target_multipliers[name] = 1.0 / (outcome / targets[name])
                
        for param_name, idx in self.__param_index_dict.items():
            found = False
            target_param_val = 0.0
            for target_name, weight_dict in target_to_parameters_weights.items():
                if param_name in weight_dict.keys():
                    found = True
                    print('DEBUG: target_multipliers = {0}'.format(target_multipliers))
                    target_param_val = self.__param_list[idx].val * weight_dict[param_name] * target_multipliers[target_name]
                    break
                    
            if found:
                new_param = self.__param_list[idx].move_towards_value(target_param_val, 0.85)
            else:
                new_param = self.__param_list[idx].clone()
        
            new_group.append(new_param)
            
        return new_group
