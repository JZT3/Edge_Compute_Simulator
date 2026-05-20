this redundant readme is required for the `pip install -e .`

otherwise you get the following error

Getting requirements to build editable ... error
  error: subprocess-exited-with-error
  
  × Getting requirements to build editable did not run successfully.
  │ exit code: 1
  ╰─> [8 lines of output]
      /private/var/folders/h2/qwqppvws1zzcrk__g3grcwjc0000gn/T/pip-build-env-i_vfmnqj/overlay/lib/python3.12/site-packages/setuptools/config/expand.py:128: SetuptoolsWarning: File '/Users/jzt/Desktop/RF_Practice/Edge_Compute_Simulator/simulator/gui/README.md' cannot be found
        for path in _filter_existing_files(_filepaths)
      running egg_info
      writing src/sigint_gui.egg-info/PKG-INFO
      writing dependency_links to src/sigint_gui.egg-info/dependency_links.txt
      writing requirements to src/sigint_gui.egg-info/requires.txt
      writing top-level names to src/sigint_gui.egg-info/top_level.txt
      error: package directory 'src/sigint_gui/views' does not exist
      [end of output]
  
  note: This error originates from a subprocess, and is likely not a problem with pip.
ERROR: Failed to build 'file:///Users/jzt/Desktop/RF_Practice/Edge_Compute_Simulator/simulator/gui' when getting requirements to build editable
(.venv) FAIL: 1