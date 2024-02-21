print('Hello GeoDms tester')
print('...setting sys path')
import os

import sys
sys.path.append('../bin/Debug/x64')

def pause(message:str): 
    programPause = input("Press the <ENTER> key to continue...")

try: 
    print('...importing GeoDmsPython')
    from geodms import *

    print('...creating an Engine singleton, which loads and holds the GeoDms dlls')
    e = Engine()

    # print('members of Engine e:')
    # pprint(dir(e), indent=2)
    # input('OK?')

    print(os.getcwd())
    pause('...loading a model configuration file')
    c = e.loadConfig('c:/dev/tst/Operator/cfg/Operator.dms')

    # print('members of Config c:')
    # pprint(dir(c), indent=2)
    # input('OK?')

    print('...accessing the root of the loaded model')
    r = c.getRoot()

    # print('members of root:')
    # pprint(dir(r), indent=2)
    # input('OK?')

    print('...finding a sepecific meta-info item')
    i = r.find('/MetaInfo/OperatorList/Operators')

    # print('members of item i:')
    # pprint(dir(i), indent=2)
    # input('OK?')

    adi = AsDataItem(i)
    
    print('name i: ', i.name())
    
    j = r.find('/xxx')
    print('name j: ', j.name())
    

    input('Goodbye');
except Exception as e:
    print(e)
    input('got it?')



