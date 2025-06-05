INTRO
This document describes the steps to use the GeoDMSPerformance.py script, which creates an interactive representation of the performance of GeoDMS for a given item in a configuration.

PREREQUISITES
Python 3.x is required in order to run, and can be installed from https://www.python.org/downloads/. Make sure to register to system path.
The script requires the following python packages, which can be installed using: pip install <package>, but more easily installed using pip install -r requirements.txt from
the script folder:
- psutil
- bokeh

OVERVIEW
GeoDMSPerformance.py is a tool that helps the GeoDMS modeler get insight into the performance of a model. 
The script lets the user define performance experiments and make comparisons between revisions in terms of:
- cpu usage
- virtual and physical memory
- number of threads
- bytes read and written. 

EXPERIMENT FILE
An experiment is an individual piece of software to be profiled. This does not have to be GeoDMS. Experiments are defined using an experiment file. 
This is a csv file, using ; as separator, with a mandatory headerline and each line defining its own experiment.

The following fields are expected:
name;command;experiment_folder;environment_variables;cwd;geodms_logfile;binary_experiment_file

name: the name of the experiment, this should be unique, as it is also reflected in the final profile visualization
command: the command to be run, can be relative, like: ../Runs/run_my_awesome_geodms_model.bat
experiment_folder: the folder where experiments are written to
environment_variables: to be implemented
cwd: current working directory, if for instance the batchfile expects to be run from a certain folder you can specify that here
geodms_logfile: a geodms logfile can be combined with the final profile, giving the user extra information on the experiment
binary_experiment_file: a pickle dumped binary experiment file, such that experiments from multiple locations can be combined in one single profile visualization

USAGE
To run a given experiment file use command: python GeoDMSPerformance.py <experiment.file>.

OUTPUT
Three outputs are generated:
1. one log per experiment, which is a temporary file and can be discarded afterwards
2. experiment data is stored as storage_fldr/experiment_data//{filename}.bin where the filename is build from the following parameters:
{name}_{name_command_hash}.bin with:
name: name of the experiment as given by the user
name_command_hash: 15-digit hash of experiment name + command parameters using sha256 method.

3. Interactive html figure is the output of the performance experiments and is stored in compare.html file in the user defined output_fldr parameter