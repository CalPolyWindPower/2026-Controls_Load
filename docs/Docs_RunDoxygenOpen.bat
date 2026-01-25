@echo Generating Documentation

"C:\Program Files\doxygen\bin\doxygen.exe" ".\docs\config\Doxyfile"

@echo Opening HTML documentation in browser...
.\docs\doxygen-out\html\index.html

