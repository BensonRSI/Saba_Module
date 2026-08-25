

# ROM Map

```
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
```


The table below describes the memory layout (addresses in hex):

| Start   | End     | Description |
|--------:|--------:|:------------|
| 0x0000  | 0x07FF  | BIOS + built-in games |
| 0x0800  | 0x27FF  | Start ROM + IRQ vectors |
| 0x2800  | 0x2FFF  | RAM |
| 0x3000  | 0xBFFF  | Fixed ROM |
| 0xC000  | 0xCFFF  | Bank 0 (switchable) |
| 0xD000  | 0xDFFF  | Bank 1 (switchable) |
| 0xE000  | 0xEFFF  | Bank 2 (switchable) |
| 0xF000  | 0xFFFF  | Bank 3 (switchable) |
| 0x10000 | 0x10FFF | Extension ROM 0 |
| 0x11000 | 0x11FFF | Extension ROM 1 |
| 0x12000 | 0x12FFF | Extension ROM 2 |
| ...     | ...     | ... |
| 0x1F000 | 0x1FFFF | Extension ROM 0x0F |

Note: Extension ROMs are organized in 0x1000-sized blocks (for example 0x10000, 0x11000, 0x12000, ...).

## Bank Mapping

Each Extension ROM can be mapped into any of the banks (0..3) by writing a value to Port C using the following encoding:

```
PortC = (Bank << 4) | ExtensionIndex
```

Example: write `0x21` to map Extension `0x1` into Bank `0x2` (Bank=2, Extension=1).


To restore a bank to its original content, set bit 7 when writing to Port C.

```
PortC = 0x80 + (Bank << 4)
```

Example:  write `0x90`  restores Bank 1 to original

---


