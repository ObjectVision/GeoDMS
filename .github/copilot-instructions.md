# Copilot Instructions

## Project Guidelines
- When searching code in the GeoDMS workspace, skip vcpkg folders (vcpkg_installed, vc_archives, vc_downloads,  vcpkg-triplets, etc.) and intermediate result folders (bin, obj, build) as indicated in .gitignore to avoid false matches from third-party code.
- assume that the user knows all of the code in the project, so consider asking the user when an issue needs to be sorted out about usage or intend instead of finding out everything by searching in the codebase. This is to avoid doing a lot of work that may not be needed.
- for refactoring questings, first produce a plan of how to do the refactoring, then ask if the plan is good before doing the actual refactoring. This is to avoid doing a lot of work that may not be needed.
- present and commit intermediate steps.
- Close code windows after the user has confirmed that the code is good, to avoid cluttering the workspace with too many open files and Intellisense memory usage
- assume after each step that the user might close and restart Visual Studio to reduce committed memory