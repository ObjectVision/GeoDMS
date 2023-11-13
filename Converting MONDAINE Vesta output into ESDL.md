If you have generated Vesta output files, you can now convert those into
ESDL-files using a python script.

1.  Download the Vesta configuration
    [here](https://github.com/mondaine-esdl/vesta-esdl/archive/refs/heads/master.zip)
    or make a git-clone of that
    [repository](https://github.com/mondaine-esdl/vesta-esdl.git)
2.  Extract the zip file and move it to your ProjDir-folder, for example
    "C:/GeoDMS/ProjDir/Vesta-ESDL"
3.  Install a python editor, such as Spyder Anaconda
4.  Open the python file called
    *Vesta_to_ESDL_PerPlanRegio_versimpeld.py*
5.  There scroll to the bottom and uncomment/comment the lines that are
    needed. Comment all lines that are not needed and only uncomment
    what is needed.
    1.  Choose the required time steps *'TijdstapNamen*'
    2.  Choose the study area *'RegioNamen*'
    3.  Choose the scenario *'ScenarioNaam*'
        1.  Each time there is the ScenarioNaam on one line, and the
            under it holds the strategies. Comment/uncomment in duo's.
        2.  If you have selected a scenario, run the file by pressing
            F5. If it is done, choose the next scenario and run again.
    4.  The output is automatically saved to the MONDAINE hub and to
        your harddrive, in the folder *ProjDir\\Vesta-ESDL\\output*