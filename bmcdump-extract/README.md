# bmcdump-extract

Script for going through OpenBMC dump files when a BMC is Out of Memory (OOM) or
panics.

## Requirements

- Python 3.10+ (uses `X | None` type hints, so anything older will blow up)
- `dd` and `tar` — should already be on any normal Linux box
- `unzstd` — on Debian/Ubuntu this is `apt install zstd`

## Installation

```bash
chmod +x bmcdump-extract
./bmcdump-extract oom <file>
```

or just throw it on your PATH:

```bash
export PATH=$PATH:/path/to/openbmc-tools/bmcdump-extract
```

## Usage

```text
bmcdump-extract oom [<bmcdump_file> | --out DIR]
```

You can point it at a raw BMCDUMP file or a directory you've already extracted.

```bash
# raw dump straight off the BMC
bmcdump-extract oom dump_file

# same thing but dump it somewhere specific instead of a dir
bmcdump-extract oom --out /my_dump/dump_file

# just point at the folder
bmcdump-extract oom /path/to/dump_file
```

If you don't pass `--out`, it extracts to a throwaway temp dir and prints the
path.

## What it actually does

**`oom` analysis** goes looking for a dmesg/ramoops/kmsg file in whatever got
extracted and pulls out whatever's useful depending on what actually happened:

- if it was an **OOM**, it parses the process memory table out of dmesg, ranks
  everything by RSS so you can see the top 10 memory hogs at a glance, and dumps
  the overall memory state (watermarks, free memory, anon vs file vs shared,
  slab)
- if it was a **kernel oops**, it'll pull the fault reason, which process was
  running when it died, CPU number, kernel version, PC/LR, register dump, and
  the full call trace
- **kernel panic** that wasn't OOM-related gets the panic reason plus whatever
  call trace it can find

## Known limitations

- Only handles the `oom` subcommand right now. Might add others later if needed.
- The process table regex assumes the kernel's dumping in the usual format
- Doesn't try to correlate anything against journal logs or systemd unit state.
