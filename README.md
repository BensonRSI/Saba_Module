# Saba_Module
Replacement for a game -module for the SABA Videoplay console

SABA Videoplay is a gaming console connected to a TV-set from the mid 1970s 
( also know as "The Channel F" (initially named Fairchild Video Entertainment System) )
see : https://channelf.se/veswiki/index.php?title=Main_Page or 
https://en.wikipedia.org/wiki/Fairchild_Channel_F


The replacement module consist of a Raspberry RP2040 microcontroller  ( + a levelshifter for 3.3 / 5 V conversion ) 
that emulates an F3851 adressdecoder IC and holds the ROMs and additional RAM for the programms.

It samples the module extension port on a high rate and implements the statemachine, that will do the generation of the programm-counter and adress-generator.
To archieve a high sample-speed these parts are written in ARM assembler ( thumb ) and running exclusivly on core 1

The first core does some convenience functions. You can connect through the USB-line, with a serial terminal on 115200 and get a little menu to 
diplay the memory, load different ROMs from the build-in flash or upload ROMs with xmodem-protocol.

## How to build

Using vscode, you can either use a codespace on github or use "reopen in container" , which wil install all neccessary tools and submodules.

Open a new terminal and simply type "make" . This is a wrapper to call the cmake and build the binary.

## How to download

connect an USB-cable. Enter Firmware upload by holding the button when connecting.

type "make go" this will build and upload the firmware.



## ROM upload with upload_roms.sh

The repository contains a helper script at [upload_roms.sh](upload_roms.sh) to upload multiple ROM images automatically via XMODEM.

Typical workflow:

1. Create a `roms/` directory and place your `.BIN` files there.
2. Run:

```bash
./upload_roms.sh --setup
```

This scans the ROM folder and creates a names file such as `roms/rom_names.txt`.

3. Edit the generated file in the format:

```text
01.BIN=ASTEROIDS
02.BIN=SPACEWAR
```

The left side is the filename, the right side is the cart name shown in the module.

4. Upload the ROMs to the module:

```bash
./upload_roms.sh --upload
```

The script sends each ROM sequentially through the configured serial port and writes it into the next slot. The default port is `/dev/ttyACM0`, which can be overridden with:

```bash
DEVICE=/dev/ttyACM1 ./upload_roms.sh --upload
```

Useful option:

```bash
./upload_roms.sh --dry-run
```

This previews the commands without sending the data.

Example end-to-end workflow:

```bash
mkdir -p roms
cp ~/Downloads/asteroids.bin roms/01.BIN
cp ~/Downloads/spacewar.bin roms/02.BIN
./upload_roms.sh --setup
# edit roms/rom_names.txt to:
# 01.BIN=ASTEROIDS
# 02.BIN=SPACEWAR
./upload_roms.sh --upload
```

The script writes the ROMs to consecutive slots and uses the cart names as labels inside the module.

## Additions to the Rom-Emulation 

Even if it was seductive to add some extra computation power, i restricted it to extensions, which could have been existed in the early days of the console. ( But ofcourse nobody would have paid for 144 kb of ROM ,that time ;) )

### Timer

The module emulates an 8‑bit timer (F3853-like) which is controlled via the I/O area:

- I/O offsets used in the implementation (see `src/core0.c`):
	- `ICR_OFFSET_ADR` = 0x0a — IRQ / timer control (enable/disable timer IRQs)
	- `TIMER_VAL_ADR` = 0x0b — 8‑bit timer value; writing `0xff` stops the timer, any other value starts it
- Internals: the timer value is converted using the `timerval_to_counter[]` table and multiplied by `TIMER_CLOCK_MULTIPLIER` (806) to produce a delay in microseconds. When the timer expires and timer IRQs are enabled the emulator sets an IRQ flag (`irq_triggered`) which is used to signal an interrupt to the emulated system.
- Console: press `r` in the serial console to toggle a simulated timer (calls `simulate_timer()` in `src/core0.c`).

See implementation: [src/core0.c](src/core0.c#L1-L220).

### Bank switching

The emulator supports mapping extension ROM banks into the four switchable banks of the emulated address space. This is for modules, that exceeding the 64kb boundary. The idea is to have 4 "windows" looking into the upper memory. (While we were developing our demo "Channel T", we had the fear of crossing that boundary, with code and music data. At the end, we optimized a bit and ended at 44k.) 

- Control address: `ROM_BANK_ADR` = 0x0c (write a single byte to select mapping).
- Encoding: write `PortC = (Bank << 4) | ExtensionIndex` to map an extension ROM into `Bank` (0..3). To restore a bank to its original ROM content, set bit 7 when writing to `PortC` (i.e. `0x80 + (Bank << 4)`). Detailed mapping is documented in [src/RomMapper.md](src/RomMapper.md#L66-L83).
- Memory layout: switchable banks start at `BANK_START_ADDRESS` (`0xC000`) with `BANK_SIZE` bytes per bank (`0x1000`). When a bank change is detected the emulator copies either from the shadow copy of the original ROM or from the uploaded extension data into the runtime memory.
- Console: press `b` in the serial console to enter a bank value interactively; the console writes the value to `ROM_BANK_ADR` and prints the resulting mapping.

See implementation: [src/core0.c](src/core0.c#L220-L700) and [src/RomMapper.md](src/RomMapper.md#L54-L83).



