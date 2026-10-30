/*  Copyright (C) 2002 Kevin Shanahan

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/

#pragma once

#include <array>
#include <vector>

#include <common/qvec.hh>
#include <common/fs.hh>

struct mbsp_t;
struct bspdata_t;

constexpr size_t MAXLIGHTMAPSSUP = 16;
constexpr uint16_t INVALID_LIGHTSTYLE = 0xffffu;

/* internal representation for bspx/lit2 */
struct facesup_t
{
    float lmscale;
    uint16_t styles[MAXLIGHTMAPSSUP]; /* scaled styles */
    int32_t lightofs; /* scaled lighting */
    twosided<uint16_t> extent;
};

void WriteLitFile(const mbsp_t *bsp, const std::vector<facesup_t> &facesup, const fs::path &filename, int version,
    const std::vector<uint8_t> &lit_filebase, const std::vector<uint8_t> &lux_filebase,
    const std::vector<uint8_t> &hdr_filebase);
void WriteLuxFile(const mbsp_t *bsp, const fs::path &filename, int version, const std::vector<uint8_t> &lux_filebase);

// One instance per face/style; no shared state or scheduling-dependent RNG.
class lightmap_quantizer_t
{
    std::vector<qvec3f> current, next;
    bool shape;
public:
    lightmap_quantizer_t(size_t width, bool noise_shape);
    qvec3b quantize(const qvec3f &color, uint32_t face, uint32_t s, uint32_t t, bool dither);
};

enum class lightmap_filter_t;
// Samples belong to one surface, not a packed lightmap atlas.
std::vector<qvec4f> ReconstructLightmap(const std::vector<qvec4f> &input, int width, int height,
    int factor, lightmap_filter_t filter);

qvec3f EncodeLightmapColor(const qvec3f &color, float gamma, bool monochrome, bool linearfilter);

qvec3f LightmapLuminance(const qvec3f &color);

uint8_t QuantizeLight(float value, float threshold);
float LightDitherThreshold(uint32_t face, uint32_t s, uint32_t t);

void SaveLightmapSurfaces(bspdata_t *bspdata, const fs::path &source);
