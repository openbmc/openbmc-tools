# bi2cp: Beagle I2C Parser

`bi2cp` parses CSV dumps from the Beagle logic analyzer to lift the raw I2C
transfers to expose PMBus semantics.

`bi2cp` can also test PEC bytes in transfers among other features - external
dependencies are listed in `requirements.txt`:

```sh
pip3 install --user --requirement requirements.txt
```

## Example Run

```sh
$ ./bi2cp --address 0x11 --pmbus 'UCD recreate beagle scan 1.csv'  | head
00:30.757419 0.000211 0x11 READ MFR_SPECIFIC_45 | 00:30.757631 0.002693 [ 1b 55 43 44 39 30 33 32 30 7c 33 2e 30 2e 30 2e 33 30 32 39 7c 31 36 30 39 31 35 00 ]
00:30.767143 0.000207 0x11 READ MFR_SPECIFIC_06 | 00:30.767350 0.000203 [ 20 ]
00:30.767574 0.000205 0x11 READ MFR_SPECIFIC_05 | 00:30.767780 0.003184 [ 20 20 21 22 23 24 25 26 27 28 29 2a 2b 2c 2d 2e 2f 30 31 32 33 34 35 36 37 38 39 3a 3b 3c 3d 3e 3f ]
00:30.771015 0.000302 0x11 WRITE MFR_SPECIFIC_42 [ 00 ]
00:30.771332 0.000205 0x11 READ MFR_SPECIFIC_43 | 00:30.771538 0.000208 [ 08 ]
00:30.771758 0.000301 0x11 WRITE MFR_SPECIFIC_42 [ 01 ]
00:30.772072 0.000205 0x11 READ MFR_SPECIFIC_43 | 00:30.772278 0.000204 [ 00 ]
00:30.772493 0.000301 0x11 WRITE MFR_SPECIFIC_42 [ 02 ]
00:30.772805 0.000205 0x11 READ MFR_SPECIFIC_43 | 00:30.773010 0.000204 [ 00 ]
00:30.773226 0.000301 0x11 WRITE MFR_SPECIFIC_42 [ 03 ]
```
