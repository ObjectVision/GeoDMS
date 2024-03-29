print('Geodms python test module')
import os
import sys

sys.path.append('../bin/Debug/x64')

def pause_func(message:str="Press the <ENTER> key to continue..."): 
    programPause = input(message)

try: 
    from geodms import *

    engine = Engine()

    # load geodms configuration file
    config = engine.loadConfig('basic_data_test.dms')
    
    # get root item of configuration
    
    root = config.getRoot()
    
    print(type(root))
    
    found_item = root.find("/reference/IntegerAtt")

    found_item.update()

    #pause_func("as const item")
    #const_root   = mutable_root.asItem()

    pause_func("Done")

    #current_item = const_root.find("/reference/IntegerAtt")
    
    # get integer attribute data as numpy array
    #data item view naar data 
    #shadow tile 
    #handle naar object waar je memory view naar hebt
    #een dataitem moet aangeven hoeveel tiles deze heeft , of geef hele tile
    #data object-> tiles -> future data object generator of future tiles
    
    #pause("tmp")
    
    #adi = AsDataItem(current_item) # adu can request number of tiles 
    
    #ado = adi.getDataObject()

    #ado.getNumberOfTiles()
    
    #ado.getTile(-1)


    #size = adi.size();

    #adu = adi.getDomainUnit()

except Exception as e:
    input(f"Error: {e}")



