# Paout

paout displays pulseaudio's output level, that is, the current volume of audio output

These output modes are supported by command-line flags:

* `-1` output one value then quit. The return code is 0 if there is audible output, and 1 if there is silence

* `-s` output values continually, separated by carriage return

* `-b` output a bar graph continually

### Usage

`paout -1`

### Building

`make`

### Installing

Build the binary, then copy it into your PATH.