print('Geodms python test module')
#from distutils.util import change_root
import os
import sys

sys.path.append('../bin/Release/x64')

def pause_func(message:str="Press the <ENTER> key to continue..."):
    programPause = input(message)

def test_find_in_function(root):
    print(root.is_null())
    param_item = root.find("/parameters/test_param")

try:
    from geodms import *

    engine = Engine()

    # load geodms configuration file
    config = engine.load_config('basic_data_test.dms')
    
    #endogene expressies ter gevolgen van template expressie
    #items last change state -> opgevraagde data is alleen geldig zolang last change < change_roo
    #range blijft bestaan bij opvragen (ie tile), zelfs bij expr aanpassen, dan komt er een nieuwe range
    

    # get root item of configuration
    
    root = config.root()
    
    param_item = root.find("/parameters/test_param")
    print(param_item.is_null())

    test_find_in_function(root)

    test_invalid_item = root.find("/askjvfhakjfghsdkjghsdjkg/asfiuhsdigjuhsduig")
    test_validity:bool = test_invalid_item.is_null()

    param_item.set_expr("3b")

    result_item = root.find("/export/IntegerAtt")
    result_item.update()

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



