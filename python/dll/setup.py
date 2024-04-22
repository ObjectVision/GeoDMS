from setuptools import setup

setup(name='geodms',
      packages=['geodms'],
      package_data={'': ['*.pyd', '*.dll', '*.lib', 'gdaldata/*.*', 'proj4data/*.*']},
      )