# Sound source provenance

The 26 PCM sets in `../raw-22050`, `../raw-11025` and `../raw-7350`
(committed - they are build/runtime inputs) were converted with
`tools/wav2raw.c` (decimation factors 1, 2 and 3) from the WAV files
shipped in the C++ PC-Port's `sound/` directory - themselves 8-bit
mono 22050 Hz recordings of the original CoCo DAC output.

The WAVs are not duplicated here; to regenerate the raws, point
wav2raw at a checkout of the PC-Port (see LICENSE.md at the repo root
for the lineage):

    gcc -O2 -o wav2raw tools/wav2raw.c
    for w in <port>/sound/*.wav; do
        ./wav2raw "$w" assets/raw-22050/$(basename "${w%.wav}").raw 1
        ./wav2raw "$w" assets/raw-11025/$(basename "${w%.wav}").raw 2
        ./wav2raw "$w" assets/raw-7350/$(basename "${w%.wav}").raw 3
    done

(Exact invocation is documented at the top of tools/wav2raw.c.)
