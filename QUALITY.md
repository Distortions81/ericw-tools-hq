# ericw-tools HQ

Quake 1 compiler fork focused on compile accuracy at the existing output resolution.
Based on ericwa/ericw-tools commit 36eec1da. Upstream licensing and attribution apply.

```sh
qbsp -quality max map.map
vis map.bsp
light -quality max map.bsp
```

These are opt-in presets. With no new options the original sampling defaults remain.
Full VIS is already the normal VIS mode; do not use `-fast` for a final compile.

| Control | Upstream default | high | max |
|---|---:|---:|---:|
| Samples per luxel | 1 | 16×16 (256) | 32×32 (1024) |
| Sun/sky target samples | 64 | 1024 | 4096 |
| AO angular grid, when AO is enabled | 16×3 | 32×6 | 64×12 |
| Light contribution gate | 0.001 | 0 | 0 |
| Emissive sampling | low | high | high |
| Approximate visibility culling | automatic | none | none |
| Bounce emitter subdivision (world units) | 64 | 16 | 8 |
| Surface emitter subdivision (world units) | 128 | 64 | 32 |
| Radiosity subdivision (world units) | 16 | 8 | 4 |
| Final 8-bit dithering / error diffusion | off | on | on |

Sun sample counts are targets; actual counts depend on the sun/sky algorithm.
Emitter subdivision changes integration accuracy, not BSP geometry or lightmap density.
AO, bounce lighting, colored output, HDR, and extra bounce passes are not enabled by
these presets: the map's lighting design remains under mapper control. To add indirect
lighting deliberately, for example, pass `-bounce 4 -lit`.

Explicit command-line settings override the preset regardless of argument order;
explicit worldspawn keys also retain precedence over implied preset values.
Individual controls include:

```sh
light -extra16 -dither -noiseshape map.bsp
light -extra32 -dither -noiseshape map.bsp
light -samples 24 -sunsamples 2048 -dirtangle_steps 48 -dirtelevation_steps 8 map.bsp
light -quality max -samples 16 -noiseshape 0 map.bsp
```

`-extra` and `-extra4` retain their old meanings. `-extra8`, `-extra16`, `-extra32`,
and `-samples N` (1–32, per axis) extend the original supersampling pipeline.
All samples are downsampled into the original lightmap dimensions. No preset sets
`-lightmap_scale`, `-lmshift`, `-world_units_per_luxel`, or changes the BSP format.

## Precision and quantization

Lighting accumulation, filtering and downsampling already use floating point,
which retains more precision than a 16-bit fixed-point intermediate. HQ preserves
that precision until output quantization. `-dither` applies deterministic stochastic
rounding at the final byte conversion. `-noiseshape` applies Floyd–Steinberg error
diffusion in lightmap space, after downsampling and output color clamping. Each
face/style has independent error buffers. Shared dither thresholds across RGB and
styles avoid introducing chromatic noise in neutral colors or temporal randomness.
The grayscale BSP intensity is the maximum quantized RGB component, preserving
colored-light compatibility. HDR values and direction vectors are unaffected.

Error diffusion redistributes quantization error; it cannot restore details that
do not fit in the existing lightmap. Dithering can be more visible on very small
lightmaps; use the independent switches to compare the result in the target engine.
The dither is hashed noise, not a claimed blue-noise sequence.

## BSP calculation

Both quality presets force the expensive existing BrushBSP plane search, including
brush models, disable the size/fraction shortcuts, and keep minimum-weight
T-junction repair. High and max currently use the same BSP algorithm. This spends
more compile effort; it does not guarantee fewer nodes or faster rendering for
every map, and does not invent geometry detail.

Plane scoring also fixes hint constraints being overwritten by later brush tests:
a candidate which cuts any hint is rejected unless it is itself a hint splitter.
This correctness fix applies to all presets.

## Cost and validation

32×32 uses four times the sample storage/work of 16×16, and 64 times that of
upstream `-extra4`, before multiplying by sun/AO/emitter costs. Memory use can be
large. Upstream TBB threading and `-threads N` remain available unchanged. There
is no new OpenCL backend.

Build instructions remain in README.md. Tests include preset precedence,
supersampling aliases, dither statistics, error-diffusion mean preservation, and
actual Quake 1 compiles confirming that 16×16 and 32×32 keep the lightmap size.
Visual quality and compile cost should still be assessed on representative maps.

Local validation: Release build with GCC 13, Embree 4.4.1 and oneTBB;
398 tests ran, 382 passed, 16 upstream tests skipped. This includes seven HQ
regression tests and real high/max preset compiles. A separate eight-thread
`qbsp -quality max` → full `vis` → `light -quality max -lit` smoke compile passed
on `q1_lightignore.map`. In-engine visual A/B review remains pending.
