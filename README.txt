			ESP: The Engineering Sketch Pad 


1. Prerequisites

	The most significant prerequisite for this software is OpenCASCADE at 
release 6.3.0 or greater (which now includes the OpenCASCADE Community Edition).
This can be found at http://www.opencascade.org/getocc/download/loadocc or
https://github.com/tpaviot/oce. Prebuilt versions are available at these sites
for Windows using various versions of Visual Studio and for MAC OSX at 64-bits.
Any other configuration must be built from source (unless you are using a
Debian variant of LINUX, such as Ubuntu, where there are available prebuilt
packages as part of the LINUX distribution). Note that 6.5 is recommended but
there are SBO robustness problems at 6.5.3, so currently ESP works best with
6.5.2.

	Another prerequisite is a WebGL/Websocket capable Browser. In general
these include Mozilla's FireFox and Google Chrome. Apple's Safari works at
rev 6.0 or greater. Note that there is some problems with Intel Graphics and
some WebGL Browsers with Linux.

1.1 Source Distribution Layout

	In the discussions that follow, $DISTROOT will be used as the
name of the directory that contains:

README.txt - this file
bin        - a directly that will contain executables
config     - files that allow for automatic configuration
data       - test and example scripts
doc        - documentation
ESP        - Web code for the Engineering Sketch Pad
lib        - a directly that will contain libraries, shared objects and DLLs
src        - source files (contains EGADS, wvServer & OpenCSM)
wvClient   - simple examples of Web viewing


2. Building the Software

	The config subdirectory contains scripts that need to be used to
generate the environment both to build and run the software here. There
are two different procedures based on the OS:

2.1 Linux and Mac OSX

	The configuration is built using the path where where the
OpenCASCADE runtime distribution can be found. The pointer size (32 or 64bit)
is determined from the shared objects/dynamic libraries found in the 
distribution. This path can be located in an OpenCASCADE distribution by 
looking for a subdirectory that includes an "inc" or "include" directory and 
either a "lib" or "$ARCH/lib" (where $ARCH is the name of your architecture) 
directory.  For Debian prebuilt packaged installs this location is 
"/usr/include/opencascade".  Once that is found, execute the commands:

	% cd $DISTROOT/config
	% makeEnv **name_of_directory_containing_inc_and_lib**

An optional second argument to makeEnv is required if the distribution
of OpenCASCADE has multiple architectures. In this case it is the
subdirectory name that contains the libraries for the build of interest 
(CASARCH).

	This procedure produces 2 files at the top level: GEMenv.sh and
GEMenv.csh. These are the environments for both sh (bash) and csh (tcsh)
respectively. The appropriate file can be "source"d or included in the
user's startup scripts. This must be done before either building and/or
running the software.  For example, if using the csh or tcsh:

	% cd $DISTROOT
	% source GEMenv.csh

or if using bash:

	$ cd $DISTROOT
	$ source GEMenv.sh


2.2 Windows Configuration

	The configuration is built from the path where where the OpenCASCADE 
runtime distribution can be found. The pointer size (32 or 64bit) is determined
from the MS Visual Studio environment in a command shell (the C/C++ compiler
is run). This is executed simply by going to the config subdirectory and 
executing the script "winEnv" in a bash shell (run from the command window):

	C:\> cd $DISTROOT\config
	C:\> bash winEnv D:\OpenCASCADE6.5.2\ros

winEnv (like makeEnv) has an optional second argument that is only required 
if the distribution of OpenCASCADE has multiple architectures. In this case 
it is the subdirectory name that contains the libraries for the build of 
interest (CASARCH).

	This procedure produces a single file at the top level: GEMenv.bat.
This file needs to be executed before either building and/or running the 
software.  This is done with:

	C:\> cd $DISTROOT
	C:\> GEMenv

2.3 The Build

	For any of the operating systems, after properly setting the
environment in the command window (or shell), follow this simple procedure:

	% cd $DISTROOT/src
	% make

or

	C:\> cd $DISTROOT\src
	C:\> make

You can use "make clean" which will clean up all object modules or 
"make cleanall" to remove all objects, executables, libraries, shared objects
and dynamic libraries.

2.3.1 Note on compiling 32-bit on MACs using gcc 4.2.1

	There appears to be a bug in the compiler that is tripped up by the
file "src/OpenCSM/OpenCSM.c". Follow the above build instructions, then in
$DISTROOT/src:
	1) remove "OpenCSM/OpenCSM.o"
	2) edit "../EGADS/include/DARWIN"
	   on line 18 change "-O" to "-O0"
	   and save
	3) again type "make"


3.0 Running

	To execute any of these programs, first ensure that the appropriate 
environment variables are set (from above).  For example in Linux or Mac OS:

	% cd $DISTROOT/bin
	% serveCSM ../data/OpenCSM/bottle2

and in Windows:

	C:\> cd $DISTROOT\bin
	C:\> serveCSM ..\data\OpenCSM\bottle2


3.1 serveCSM and ESP

The Tutorial starts with a pre-made part that is defined by the file 
tutorial.csm.

To start ESP there are two steps: (1) start the "server" and (2) start the 
"browser". This can be done in a variety of ways, but the two most common 
follow.  First get into the ESP directory.

3.1.1 On a Mac Issue the two commands:

        setenv WV_START "open -a /Applications/Firefox.app ESP.html"
or
        export WV_START="open -a /Applications/Firefox.app ESP.html"
depending on the shell in use.

       ../bin/serveCSM ../data/tutorial
      
The first of these tells serveCSM to open FireFox on the file /ESP.html when 
serveCSM has generated a graphical representation of the configuration. The 
second of these actually starts the serveCSM server. As long as the browser 
stays connected to serveCSM, serveCSM will stay alive and handle requests sent 
to it from the browser. Once the last browser that is connected to serveCSM 
exits, serveCSM will shut down.

3.1.2 Issue the command:

        ../bin/serveCSM ../data/tutorial
      
Once the server starts, start a browser (for example: FireFox, GoogleChrome or
Safari at Rev 6) and open the page ESP.html. As above, serveCSM will stay alive
as long as there is a browser attached to it.

Note that the default "port" used by serveCSM is 7681. One can change the port 
in the call to serveCSM with a command such as:

        ../bin/serveCSM ../data/tutorial -port 7788

Once the browser starts, you will be prompted for a "hostname:port". Make the
appropriate responce depending on the network situation. Once the ESP GUI is
functional, press the "help" button in the upper left and continue with the
tutorial.


3.2 egads2cart

	This example takes an input geometry file and generates a Cart3D "tri"
file. The acceptable input is STEP, EGADS or OpenCASCADE BRep files (which can 
all be generated from an OpenCSM "dump" command).

	% egads2cart geomFilePath [angle relSide relSag]


3.3 vTess and wvClient

	vTess allows for the exanimation of geometry through its discrete
representation. Like egads2cart, the acceptable geometric input is STEP, EGADS 
or OpenCASCADE BRep files. vTess acts like serveCSM and wvClient should be
used like ESP in the descussion in Section 3.1 above.

	% vTess geomFilePath [angle maxlen sag]
