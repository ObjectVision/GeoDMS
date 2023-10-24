print('Hello GeoDms tester')
print('...setting sys path')
from pprint import pprint

import sys
sys.path.append('../bin/Debug/x64')


print('...importing GeoDmsPython')
from GeoDmsPython import *

print('...creating an Engine singleton, which loads and holds the GeoDms dlls')
c = Engine()

# print('members of Engine c:')
# pprint(dir(c), indent=2)

print('...loading a model configuration file');
c.loadConfig('c:/dev/tst/Operator/cfg/Operator.dms')

print('...accessing the root of the loaded model');
r = c.getRoot();

# print('members of root:')
# pprint(dir(r), indent=2)

print('...finding a sepecific meta-info item')
i = r.find('/MetaInfo/OperatorList/Operators')

# print('members of item i:')
# pprint(dir(i), indent=2)

print('name: ', i.name())
input('Goodbye');




