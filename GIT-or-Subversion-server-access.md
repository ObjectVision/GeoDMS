## server

Several GeoDMS projects are maintained at github or subversion servers. Contact us if you like to make us of this service for your project.

## client tools

You can download the GeoDMS sourcecode and projects using a github/svn client, for example: [TortoiseGit](http://tortoisegit.org) or 
[TortoiseSvn](https://tortoisesvn.net).

Tortoise is a client tool, implemented as a Windows shell extension, resulting in some menu options added to the pop-up menu in the Windows Explorer. 

## svn
To download a project, first browse in the Windows Explorer to your project [[folder|Folders and Placeholders]]. Next Activate the pop up menu option: SVN Checkout and configure the url of the project at the Subversion server in the textbox: *<span><u>U</u></span>RL of repositry*, as shown in the next example:

[[images/svn_dialogI.jpg]]

Click OK to download the project in the Checkout directory.

After a Checkout, the directory is under version control. This is indicated by colored icons, indicating if the files are consistent with the last updated version. The two most important pop-up menu options available for directories under version control are:

1.  SVN Update: To update the directory with the latest version(revision) from our SVN server (for some projects you need to be authorized).
2.  SCN Commit: To commit your changes to a new revision at our SVN server (you need to be authorized).

The pop-up menu option TortoiseSVN offers more options, e.g., comparing your local version with a revision at our server. For more information on these options we refer to the TortoiseSVN [manual](http://tortoisesvn.net/support.html).

#### organisation

In organizing your svn project, you often find the following concepts:

-   **branches**: forked versions
-   **trunk**: latest (mostly untested versions) are maintained here
-   **tags**: tagged revisions; revisions are tagged when major versions are made available You can Checkout the latest version from te trunk to for example "c:\\svn\\GeoDMS" or Export a tagged version to any directory