################################
FredPy - the FRED Python Project
################################

**FredPy** is a Python project of the Public Health Dynamics Laboratory (PHDL) at the University of Pittsburgh (`PHDL Website <https://www.phdl.pitt.edu>`_) that aims to extend and replace the Perl scripts currently utilized by the Framework for Reconstructing Epidemiological Dynamics (FRED) System (`FRED Website <https://fred.publichealth.pitt.edu>`_). Going forward, executable scripts that support the FRED platform will be written in Python unless there are extenuating circumstances to do otherwise.

*******************************************
Python Version and Installation Information
*******************************************

PHDL uses Python 3+ for all of its Python Programming. Apple machines (in particular) still default to using Python 2.7. However, if you don't have Python 3 installed, it can easily be installed from the `Official Python Site <https://www.python.org>`_


Python Packages
===============

Some common packages that our Python scripts will use should be installed. If the intent is to install the additional packages for all users, then these updates should be applied through the OS package manager: e.g. yum or apt-get.

For our purposes, additional required packages are best installed for an individual user by using Python's own package manager, pip

When you try to use pip to install, you will probably get a message that your pip is out of date. This is when you SHOULD use the sudo command to upgrade pip for all users (assuming your user has sudo permissions, of course).

.. code-block:: text

  sudo python3 -m pip install --upgrade pip

After pip has been upgraded, you can use it to install for your user only from that point on.

.. code-block:: text

  python3 -m pip install --user numpy
  python3 -m pip install --user scipy
  python3 -m pip install --user matplotlib
  python3 -m pip install --user pandas
  python3 -m pip install --user sympy
  python3 -m pip install --user pytest
  python3 -m pip install --user requests
  python3 -m pip install --user sphinx

**********************************
Creating Executable Python Scripts
**********************************

Giving Execute Permission on a file
===================================

On MacOS or Linux machines, it is common to make a script executable by updating the permissions of the file to make it executable. The 711 means
Read-Write-Execute Permissions for the owner of the file, and Execute-only permissions for both the owner's group and the world.

.. code-block:: text

  chmod 711 FileToRun

HashBang (or Shebang)
=====================

When the OS encounters a HashBang "#!" (Also called a "Shebang") at the top of a program, it knows to execute the command after the "#!". The preferred way to do this for Python is to use the env command to let the OS find the correct version of Python.

.. code-block:: text

  #!/usr/bin/env python3


**************************
Baseline Project Structure
**************************

.. code-block:: text

  README.rst
  bin
  ├── fred_cd.py
  ├── fred_id.py
  └── fred_popsize.py
  docs
  ├── Makefile
  ├── build
  ├── make.bat
  └── source
      ├── _static
      ├── _templates
      ├── conf.py
      ├── index.rst
      └── readme.rst
  src
  └── fredpy
      ├── __init.py__
      ├── frederr.py
      └── util
          ├── __init.py__
          ├── constants.py
          └── fredutil.py
  tests
  └── test_fredutil.py


****************************
PHDL Python Coding Standards
****************************

Python Code Style
=================

No tabs
-------
Seriously ... no tabs!

Indents will be four (4) spaces. Please make sure that your editor substitutes spaces for tabs.

Single Quotes
-------------
To keep things standardized, use single quotes for strings instead of double quotes. If you need to use an actual single quote inside of a string, escape it rather than mixing double quotes with single quotes.

.. code-block:: python

  >>> string_bad = "You may be tempted to mix double quotes with 'single quotes'"
  >>> print(string_bad)
  You may be tempted to mix double quotes with 'single quotes'

  >>> string_good = 'But please don\'t. If you need to use \'single quotes\' within a string, escape them using \\\''
  >>> print(string_good)
  But please don't. If you need to use 'single quotes' within a string, escape them using \'

Google Style
------------
Utilize `Google's Coding Standards <https://google.github.io/styleguide/pyguide.html>`_

Exceptions to Google
--------------------

Double Underscore
^^^^^^^^^^^^^^^^^
DO use double underscore for instance variables. Google is correct that Python does not actually allow private instance variables in classes. However, the Python name mangling that occurs when using the double underscore makes it significantly more difficult to access the "private" variables. Take advantage of it and force other programmers to work to get to the private variables.

.. code-block:: python

  class Coordinates:

      # Note the use of the double underscore to define the instance
      # variables
      def __init__(self, x, y, z):
        self.__x = x
        self.__y = y
        self.__z = z

Variable Naming
^^^^^^^^^^^^^^^

Use _str or _path for clarity when dealing with directory or file name variable name is extraneous. However, when dealing with Path objects versus the string representation of the Path, it is useful to add the additional characters to the variable name to avoid confusion.

.. code-block:: python

    # This is the Path object that points to the directory
    fred_job_meta_dir_path = Path('FRED_HOME_DIR', 'JOB', 'JOBID', 'META')

    # Whereas this is the string representation of the full Path
    # This is more useful for string operations and this is what
    # the method will return
    fred_job_meta_dir_str = str(fred_job_meta_dir_path.resolve())

    # We can use the Path object's methods for file operations
    if not fred_job_meta_dir_path.exists():
        raise FileNotFoundError('Can\'t find directory [' + fred_job_meta_dir_str + ']')
    elif not fred_job_meta_dir_path.is_dir():
        raise NotADirectoryError('File [' + fred_job_meta_dir_str + '] is not a directory')

    # Return the string, not the Path
    return fred_job_meta_dir_str

Command line arguments
-------------------------

The argparse library is the recommended Python3 way to handle command line arguments. The argparse library pretty much generates the "usage" and "help" from the way arguments are added to a Parser object.

Any required arguments should be added as positional arguments to the parser, and optional arguments can be added as necessary. There are many optional parameters when adding an argument to the parser; these parameters will tailor the usage of the argument towards its intended use. For more detail, be sure to view the `argparse documentation <https://docs.python.org/3/library/argparse.html>`_.

An example of setting up and parsing a Parser in argparse is shown below: 

.. code-block:: python

  import argparse
  import logging

  from fredpy.util import fredutil
  from fredpy.util import constants
  from fredpy import frederr

  def main():

      # Creates and sets up the parser
      parser = argparse.ArgumentParser(description=
          'Prints the full path to the OUT directory of the Job associated with the key.')
      parser.add_argument('key', type=str, help='The FRED key to look up')
      parser.add_argument('--loglevel', action='store', default='ERROR',
                          choices=['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL'],
                          help='set the logging level of this script')

      # Parses the arguments
      args = parser.parse_args()
      fred_key = args.key
      loglevel = args.loglevel


Python Testing
==============

PHDL uses pytest to test modules.

The `Pytest website <https://docs.pytest.org/>`_ has much more detail than what we are pointing to here.

Create the Tests
----------------

When pytest is called on a module, it will try to execute methods that begin with "test\_". Additionally pytest allows for fixtures (read the pytest documentation). The salient point is that in the following example, we are using the built-in fixture tmp_path which creates a temporary directory that can be used for the test.

Example Test Case
^^^^^^^^^^^^^^^^^
.. code-block:: python

  def test_get_fred_home_dir(tmp_path):
      """ Test the get_fred_home_dir function

      Check for FRED_HOME not being set
      Check to make sure that the value returned from the function matches the value in the environment variables

      Parameters
      ----------
      tmp_path : pathlib.Path
          Built in @pytest.fixture decoration that is a path to a temporary directory which is unique to each test function.
      """
      tmp_fred_home_dir_path = Path(str(tmp_path.resolve()), 'HOME')
      tmp_fred_home_dir_str = str(tmp_fred_home_dir_path.resolve())

      # Check FRED_HOME is not set
      old_environ = dict(os.environ)
      for env_var in ['FRED_HOME', 'FRED_RESULTS']:
          os.environ.pop(env_var, default=None)

      # Check the case where FRED_HOME environment variable is not set
      with pytest.raises(frederr.FredHomeUnsetError):
          fred_home_dir_str = fredutil.get_fred_home_dir_str()

      # Now set the FRED_HOME environment variable
      os.environ.update({
          'FRED_HOME': tmp_fred_home_dir_str,
      })

      fred_home_dir_str = fredutil.get_fred_home_dir_str()
      assert fred_home_dir_str == tmp_fred_home_dir_str

      os.environ.clear()
      os.environ.update(old_environ)

      # Now make sure that the user has actually set his FRED_HOME
      user_fred_home_dir = os.environ.get('FRED_HOME')

      if user_fred_home_dir == None:
          print('Please set environmental variable FRED_HOME to location of FRED home directory')
          assert False

      fred_home_dir_str = fredutil.get_fred_home_dir_str()

      assert fred_home_dir_str == user_fred_home_dir

Running the Tests
-----------------

To run the test module, we execute pytest as a module and pass the test module as the argument.

Example
^^^^^^^

.. code-block:: text

  tests % python3 -m pytest test_fredutil.py
  ============================ test session starts ============================
  platform darwin -- Python 3.9.2, pytest-6.2.4, py-1.10.0, pluggy-0.13.1
  rootdir: /Users/gallowdd/FredPythonProject/tests
  collected 6 items

  test_fredutil.py ...... [100%]
  ============================ 6 passed in 0.03s =============================

Python Documentation
====================

Docstring Style
---------------
PHDL uses numpydoc style for docstrings. This style is similar to Google's docstring style, but it seems to be more easily handled by reStructuredText converters. Speaking of reStructuredText, numpydoc style is not nearly as obtuse as that style.

Example
^^^^^^^

.. code-block:: python

  def get_fred_id(fred_key):
      """Get the ID of the job that FRED associated with the supplied key

      Parameters
      ----------
      fred_key : str
          The key to lookup

      Returns
      -------
      str
          A string that is the ID of the job that FRED associated with fred_key, or None if the key is not found.

      Raises
      ------
      frederr.FredHomeUnsetError
          If the the FRED_HOME environment variable is unset

      Examples
      --------
      >>> from fredpy.util import fredutil
      >>> fred_id = fredutil.get_fred_id('abcd123')
      >>> print(fred_id)
      2

      If the key does not exist, return None

      >>> fred_id = fredutil.get_fred_id('badkey')
      >>> print(fred_id)
      None
      """

      fred_id = _scan_key_file_for_id(fred_key)

      return fred_id

`Here <https://numpydoc.readthedocs.io/en/latest/format.html>`_ is a link to the numpydoc documentation.

Documentation Generator
-----------------------
For our C++ code, we use Doxygen to generate documentation from Javadoc style comments. However, the de facto standard for Python seems to be Sphinx, so we have decided to use Sphinx to generate HTML documentation from reStructuredText. Sphinx is able to convert numpydoc style into reStructuredText.

Useful links:

- `Official reStructuredText site <https://docutils.sourceforge.io/rst.html>`_
- `Sphinx Official site <https://www.sphinx-doc.org/en/master/>`_


.. seealso:: `Thomas Cokelaer's Sphinx and RST syntax guide <https://thomas-cokelaer.info/tutorials/sphinx/rest_syntax.html#python-software>`_

.. seealso:: `Shun Huang's Step by Step Guide to Using Sphinx for Python Documentation <https://shunsvineyard.info/2019/09/19/use-sphinx-for-python-documentation/>`_
