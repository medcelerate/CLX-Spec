# ECNet Waveform Data

We follow the waveform data as specified by the BBC here https://github.com/bbc/audiowaveform/blob/master/doc/DataFormat.md

## Additional Data
In order to carry color information we appened color information per line to the bottom of the data. Examples are as below.

```json
{
  "version": 2,
  "channels": 2,
  "sample_rate": 48000,
  "samples_per_pixel": 512,
  "bits": 8,
  "length": 3,
  "data": [-65,63,-66,64,-40,41,-39,45,-55,43,-55,44]
  "colors":["rgb(20,20,20)"
}
```

### Binary

We add a `CLRS` section to the binary which represent the color for each sample.
