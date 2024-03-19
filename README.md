# Saba_Module
Replacement for a game -module for the SABA Videoplay console

SABA Videoplay is a gaming console connected to a TV-set from the mid 1970s 
( also know as "The Channel F" (initially named Fairchild Video Entertainment System) )
see : https://channelf.se/veswiki/index.php?title=Main_Page or 
https://en.wikipedia.org/wiki/Fairchild_Channel_F


The replacement module consist of a Raspberry RP2040 microcontroller ( + a levelshifter for 3.3 / 5 V conversion ) 
that emulates an F3851 adressdecoder IC and holds the ROMs and additional RAM for the programms.

It samples the module extension port on a high rate and implements the statemachine, that will do the generation of the programm-counter and adress-generator.
To archieve a high sample-speed these parts arm written in ARM assembler ( thumb ) and running exclusivly on core 1

The first core does some convenience functions. You can connect through the USB-line, with a serial terminal on 115200 and get a little menu to 
diplay the memory, load different ROMs from the build-in flash or upload ROMs with xmodem-protocol.

## How to build

Using vscode, you can either use a codespace on github or use "reopen in container" , which wil install all neccessary tools and submodules.

Open a new terminal and simply type "make" . This is a wrapper to call the cmake and build the binary.

## How to download

connect an USB-cable. Enter Firmware upload by holding the button when connecting.

type "make go" this will build and upload the firmware.



