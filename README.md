JSON to PPM raycaster with lighting by Chris Blazer

Use "make" to implement the makefile, which compiles the program using gcc.

Usage is just how the project specified; "raycast width height input.json output.ppm".

Can output to ppm 3 or 6, but project did not specify to put that into command line arguments so it is hardcoded into write_scene() function call in main().

Program will create the output file if it does not exist.