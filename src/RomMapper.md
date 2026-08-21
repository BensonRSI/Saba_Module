

-----------------------------       0x0000
|                           |
|    BIOS + Builtin Games   |
|                           |
-----------------------------       0x0800
|                           |
|    Start Rom + Irq        |
|                           |
-----------------------------       0x2800
|                           |
|    RAM                    |
|                           |
-----------------------------       0x3000
|                           |
|                           |
|                           |
|    Fixed Rom              |
|                           |
|                           |
|                           |
|                           |
-----------------------------       0xC000
|    Bank 0                 |
-----------------------------       0xD000
|    Bank 1                 |
-----------------------------       0xE000
|    Bank 2                 |
-----------------------------       0xF000
|    Bank 3                 |
-----------------------------       0x10000
|    Extension Rom 0        |
-----------------------------       0x11000
|    Extension Rom 1        |
-----------------------------       0x12000
  ........
|    Extension Rom 0f       |
-----------------------------       0x20000

Each Extension Rom can be mapped to each Bank ,
by writting Port C with (Bank << 4  + Extension )  
To restore the orignal content , 
writing  Port C with Bit 7 set has to be done.
Eg. 0x90 , will restore Bank1 to original.
