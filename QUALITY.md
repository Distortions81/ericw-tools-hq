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
| Sample placement | regular grid | deterministic stratified | deterministic stratified |
| Reconstruction | box average | Mitchell | Mitchell |
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

Initial fork validation (before the reconstruction/enhanced-look additions):
Release build with GCC 13, Embree 4.4.1 and oneTBB;
398 tests ran, 382 passed, 16 upstream tests skipped. This includes seven HQ
regression tests and real high/max preset compiles. A separate eight-thread
`qbsp -quality max` → full `vis` → `light -quality max -lit` smoke compile passed
on `q1_lightignore.map`. In-engine visual A/B review remains pending.


## Richer lighting in the same low-resolution monochrome map

```sh
qbsp -quality max map.map
vis map.bsp
light -look enhanced map.bsp
```

`-look enhanced` implies max sampling quality, grayscale output, two indirect
bounces, full texture-color reflectance for bounce calculations, and finite-size
positive light sources. Original textures remain unchanged. Bounce uses each
texture's average color; it does not infer height or occlusion from painted details.
The final filtered RGB illumination is converted to Rec.709-weighted luminance,
then gamma-encoded (if requested) and dithered/error-diffused into the original
8-bit monochrome lightmap. A .lit file
is not automatically enabled in monochrome mode; explicitly requesting one writes
grayscale RGB data. Lightmap dimensions, BSP version and renderer remain unchanged.

Finite sources default to an 8-world-unit radius with 64 samples. The deterministic
sphere sequence uses antipodal pairs to preserve its center before clipping.
Generated positions are clipped to the source's side of shadow-casting walls.
Per-sample energy is normalized after attenuation, preserving the brightness and
reach of linear lights as well as inverse/inverse-square lights. This normalization
is specific to the enhanced look; legacy light behavior remains available.
Authored `_deviance` and `_samples` override these source defaults. Sun/dome,
negative and non-additive minimum lights do not receive automatic finite radii.

AO remains disabled by default: geometric visibility already occludes bounced
light, and a separate AO multiplier can over-darken it. This remains the existing
surface-emitter radiosity solver, not a full path tracer.

Explicit CLI values override presets regardless of argument order. Authored map
keys retain precedence over implied preset defaults. Useful controls include:

```sh
light -look enhanced -light_radius 4 -light_samples 128 map.bsp
light -look enhanced -bounce 3 map.bsp
light -look enhanced -light_radius 0 map.bsp
light -look enhanced -monochrome 0 -lit map.bsp
light -quality max -filter lanczos3 map.bsp
light -extra32 -filter mitchell -samplepattern stratified -monochrome -noiseshape map.bsp
```

The source-sample count multiplies the receiver-sample cost. The enhanced defaults
can be very expensive on large maps. `-samples 8` can be used for a quicker preview
while keeping the final lightmap resolution exactly the same.

### Reconstruction filters

- `box`: original upstream texel-footprint average, retained unchanged.
- `tent`: positive-weight smoothing over a one-luxel radius.
- `mitchell`: Mitchell-Netravali cubic, B=C=1/3, over a two-luxel radius. Selected
  for both quality presets as a practical smoothing/sharpness compromise.
- `lanczos3`: windowed sinc over a three-luxel radius. Available for sharper
  reconstruction; it can emphasize ringing around high-contrast shadows.

The new filters use separable double-precision accumulation, normalize weights,
retain validity masks, extend each surface's border independently, and clamp
negative-lobe overshoot to the input footprint's color range without clipping HDR
to 255. Light direction vectors retain upstream box downsampling. A factor of 1
bypasses filtering. The normal legacy defaults remain grid sampling and box filtering.

Stratified placement takes one deterministic jittered sample in each supersample
cell, reducing regular-grid correlation. It is reproducible for a fixed BSP
regardless of worker scheduling. Reconstruction improves the estimate stored in
each texel; it cannot recover details smaller than the existing lightmap can carry.
It also does not antialias geometry silhouettes, which require renderer support.


Quality presets, non-box reconstruction, and monochrome output also imply
`-linearfilter`: non-default gamma is applied after sample integration and final
luminance conversion, rather than to each supersample. `-linearfilter 0` restores
the old gamma-before-filter order. The legacy default path remains unchanged.
Coplanar seam stitching and replacing the fixed surface-ray offset are not part
of this change; those need dedicated topology/visibility regression cases.

Enhanced-look validation: Release build passed 391 tests with 16 upstream skips
(407 total). New coverage includes constant/HDR preservation, shadow-edge filter
coverage, invalid samples, bounded ringing, deterministic sample bounds/centroid,
linear-light gamma order, grayscale output, and source energy conservation.
Small-map enhanced-look integration compiles pass at unchanged resolution.
