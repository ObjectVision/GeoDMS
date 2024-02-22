print('Geodms python test module')
import os
import sys

sys.path.append('../bin/Debug/x64')

def pause(message:str): 
    programPause = input("Press the <ENTER> key to continue...")

try: 
    from geodms import *
    engine = Engine()

    # load geodms configuration file
    config = engine.loadConfig('basic_data_test.dms')
    
    # get root item of configuration
    root = config.getRoot()
    current_item = root.find("/reference/IntegerAtt")
    
    # get integer attribute data as numpy array
    #data item view naar data 
    #shadow tile 
    #handle naar object waar je memory view naar hebt
    #een dataitem moet aangeven hoeveel tiles deze heeft , of geef hele tile
    #data object-> tiles -> future data object generator of future tiles
    
    pause("tmp")
    
    adi = AsDataItem(current_item)
    
    size = adi.size();

    adu = adi.getDomainUnit()

except Exception as e:
    pause(f"Error: {e}")



