#include <gtest/gtest.h>
#include <light/light.hh>
#include <light/sampling.hh>
#include <light/entities.hh>
#include <light/ltface.hh>
#include <light/write.hh>
#include <qbsp/qbsp.hh>
#include "test_qbsp.hh"

TEST(quality, ExplicitSettingsOverridePresetInEitherOrder)
{
    for (bool preset_first : {false, true}) {
        settings::light_settings cfg;
        std::vector<const char *> args = preset_first
            ? std::vector<const char *>{"-quality", "max", "-samples", "8", "-dither", "0"}
            : std::vector<const char *>{"-samples", "8", "-dither", "0", "-quality", "max"};
        token_parser_t parser{static_cast<int>(args.size()), args.data(), {}};
        cfg.parse(parser);
        cfg.apply_quality();
        EXPECT_EQ(cfg.extra.value(), 8);
        EXPECT_FALSE(cfg.dither.value());
        EXPECT_EQ(cfg.sunsamples.value(), 4096);
        EXPECT_EQ(cfg.lightmap_scale.value(), 0);
        EXPECT_FALSE(cfg.lmshift.is_changed());
        EXPECT_FALSE(cfg.world_units_per_luxel.is_changed());
        EXPECT_FALSE(cfg.bounce.is_changed());
        EXPECT_FALSE(cfg.dirt.value());
    }
}

TEST(quality, SupersamplingAliases)
{
    for (const auto &[name, count] : std::vector<std::pair<const char *, int>>{
        {"-extra", 2}, {"-extra4", 4}, {"-extra8", 8}, {"-extra16", 16}, {"-extra32", 32}}) {
        settings::light_settings cfg;
        const char *args[] = {name};
        token_parser_t parser{std::size(args), args, {}};
        cfg.parse(parser);
        EXPECT_EQ(cfg.extra.value(), count);
    }
}

TEST(quality, BspPreciseSearchPreservesFormatAndSubdivision)
{
    settings::qbsp_settings cfg;
    cfg.quality.set_value(settings::quality_t::max, settings::source::COMMANDLINE);
    cfg.apply_quality();
    EXPECT_TRUE(cfg.forcegoodtree.value());
    EXPECT_EQ(cfg.maxnodesize.value(), 0);
    EXPECT_FALSE(cfg.subdivide.is_changed());
    EXPECT_FALSE(cfg.lmscale.is_changed());
    EXPECT_FALSE(cfg.bsp2.value());
}

TEST(quality, DitherIsUnbiasedAndMonotonic)
{
    double total = 0;
    for (uint32_t t = 0; t < 256; ++t) {
        for (uint32_t s = 0; s < 256; ++s) {
            float threshold = LightDitherThreshold(7, s, t);
            EXPECT_GT(threshold, 0);
            EXPECT_LE(threshold, 1);
            auto a = QuantizeLight(64.25f, threshold);
            total += a;
            EXPECT_LE(a, QuantizeLight(64.75f, threshold));
            EXPECT_EQ(QuantizeLight(0, threshold), 0);
            EXPECT_EQ(QuantizeLight(255, threshold), 255);
        }
    }
    EXPECT_NEAR(total / 65536, 64.25, 0.01);
}

TEST(quality, ErrorDiffusionPreservesGradientMeanAndRgbIntensity)
{
    for (bool dither : {false, true}) {
        lightmap_quantizer_t quantizer(128, true);
        double error = 0;
        for (uint32_t t = 0; t < 128; ++t) {
            for (uint32_t s = 0; s < 128; ++s) {
                const float v = 50 + s / 128.0f;
                const auto q = quantizer.quantize(qvec3f{v}, 1, s, t, dither);
                EXPECT_EQ(q[0], q[1]);
                EXPECT_EQ(q[1], q[2]);
                EXPECT_LE(std::abs(q[0] - v), dither ? 2.0 : 1.0);
                error += q[0] - v;
            }
        }
        EXPECT_NEAR(error / (128 * 128), 0, 0.01);
    }
}

TEST(quality, SupersamplingPreservesLightmapSize)
{
    auto base = QbspVisLight_Q1("q1_lightignore.map", {"-lit"});
    for (const char *level : {"high", "max"}) {
        auto enhanced = QbspVisLight_Q1("q1_lightignore.map", {"-lit", "-quality", level});
        EXPECT_EQ(base.bsp.dlightdata.size(), enhanced.bsp.dlightdata.size());
        EXPECT_EQ(base.bsp.dfaces.size(), enhanced.bsp.dfaces.size());
    }
}

TEST(quality, AmbientOcclusionGridIsConfigurable)
{
    settings::worldspawn_keys cfg;
    cfg.dirt.set_value(true, settings::source::COMMANDLINE);
    SetupDirt(cfg);
    EXPECT_EQ(numDirtVectors, 48);
    cfg.dirtangle_steps.set_value(64, settings::source::COMMANDLINE);
    cfg.dirtelevation_steps.set_value(12, settings::source::COMMANDLINE);
    SetupDirt(cfg);
    EXPECT_EQ(numDirtVectors, 768);
    light_reset();
}

TEST(quality, ReconstructionPreservesDimensionsConstantAndHdr)
{
    for (auto filter : {lightmap_filter_t::BOX, lightmap_filter_t::TENT,
        lightmap_filter_t::MITCHELL, lightmap_filter_t::LANCZOS3}) {
        const std::vector<qvec4f> input(24 * 16, qvec4f{512, 42, 7, 1});
        const auto output = ReconstructLightmap(input, 24, 16, 4, filter);
        ASSERT_EQ(output.size(), 6 * 4);
        for (const auto &sample : output) {
            EXPECT_FLOAT_EQ(sample[0], 512);
            EXPECT_FLOAT_EQ(sample[1], 42);
            EXPECT_FLOAT_EQ(sample[2], 7);
            EXPECT_FLOAT_EQ(sample[3], 1);
        }
    }
    EXPECT_THROW(ReconstructLightmap({}, 0, 16, 4, lightmap_filter_t::MITCHELL), std::invalid_argument);
}

TEST(quality, ReconstructionSmoothsCoverageAndBoundsRinging)
{
    std::vector<qvec4f> input(64 * 8);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 64; ++x)
            input[y * 64 + x] = qvec4f(x >= 32 ? 100.0f : 0.0f, 0, 0, 1);
    const auto box = ReconstructLightmap(input, 64, 8, 4, lightmap_filter_t::BOX);
    EXPECT_FLOAT_EQ(box[7][0], 0);
    EXPECT_FLOAT_EQ(box[8][0], 100);
    for (auto filter : {lightmap_filter_t::TENT, lightmap_filter_t::MITCHELL, lightmap_filter_t::LANCZOS3}) {
        const auto filtered = ReconstructLightmap(input, 64, 8, 4, filter);
        EXPECT_GT(filtered[7][0], 0);
        EXPECT_LT(filtered[8][0], 100);
        for (const auto &sample : filtered) {
            EXPECT_GE(sample[0], 0);
            EXPECT_LE(sample[0], 100);
            EXPECT_FLOAT_EQ(sample[1], 0);
        }
    }
}

TEST(quality, ReconstructionHandlesInvalidSamples)
{
    for (auto filter : {lightmap_filter_t::TENT, lightmap_filter_t::MITCHELL, lightmap_filter_t::LANCZOS3}) {
        const std::vector<qvec4f> input(16 * 16, qvec4f{40, 40, 40, 0});
        const auto output = ReconstructLightmap(input, 16, 16, 4, filter);
        for (const auto &sample : output) {
            EXPECT_FLOAT_EQ(sample[0], 40);
            EXPECT_FLOAT_EQ(sample[3], 0);
        }
    }
}


TEST(quality, StratifiedSamplesStayInsideTheirCells)
{
    for (int t = 0; t < 64; ++t) {
        for (int s = 0; s < 64; ++s) {
            const auto uv = StratifiedLightmapOffset(s, t, 32, 42);
            EXPECT_GT(uv[0], s / 32.0f - 0.5f);
            EXPECT_LT(uv[0], (s + 1) / 32.0f - 0.5f);
            EXPECT_GT(uv[1], t / 32.0f - 0.5f);
            EXPECT_LT(uv[1], (t + 1) / 32.0f - 0.5f);
            EXPECT_EQ(uv, StratifiedLightmapOffset(s, t, 32, 42));
        }
    }
}

TEST(quality, AreaSamplesStayInSphereAndPreserveCentroid)
{
    for (int count : {1, 2, 3, 16, 63, 64}) {
        qvec3f sum{0};
        for (int i = 0; i < count; ++i) {
            const auto offset = PairedAreaLightOffset(i, count, 123);
            EXPECT_LE(qv::length(offset), 1);
            sum += offset;
        }
        EXPECT_NEAR(qv::length(sum), 0, 1e-5);
    }
}

TEST(quality, EnhancedLookKeepsMonochromeResolutionAndAoOptional)
{
    settings::light_settings cfg;
    cfg.look.set_value(lightmap_look_t::ENHANCED, settings::source::COMMANDLINE);
    cfg.bounce.set_value(1, settings::source::COMMANDLINE);
    cfg.apply_quality();
    EXPECT_TRUE(cfg.monochrome.value());
    EXPECT_EQ(cfg.bounce.value(), 1);
    EXPECT_EQ(cfg.bouncecolorscale.value(), 1);
    EXPECT_FALSE(cfg.dirt.value());
    EXPECT_EQ(cfg.filter.value(), lightmap_filter_t::MITCHELL);
    EXPECT_FALSE(cfg.lightmap_scale.is_changed());
    EXPECT_FALSE(cfg.world_units_per_luxel.is_changed());
    EXPECT_FALSE(cfg.lmshift.is_changed());
    EXPECT_FLOAT_EQ(LightmapLuminance(qvec3f{100, 100, 100})[0], 100);
    EXPECT_NEAR(LightmapLuminance(qvec3f{255, 0, 0})[0], 54.213, 0.001);
}

TEST(quality, EnhancedCompileNormalizesSourcesAndWritesMonochrome)
{
    const auto result = QbspVisLight_Q1("q1_lightignore.map",
        {"-look", "enhanced", "-samples", "2", "-light_samples", "8", "-bouncelightsubdivision", "64"});
    EXPECT_GT(result.bsp.dlightdata.size(), 0);
    EXPECT_TRUE(std::holds_alternative<lit_none>(result.lit));
    float weights = 0;
    for (const auto &light : GetLights()) {
        EXPECT_TRUE(light->area_weighted);
        weights += light->sample_weight;
    }
    EXPECT_EQ(GetLights().size(), 8);
    EXPECT_NEAR(weights, 1, 1e-6);
}

TEST(quality, SourceSampleCountPreservesLinearLightBrightness)
{
    const auto point = QbspVisLight_Q1("q1_lightignore.map",
        {"-look", "enhanced", "-samples", "2", "-bounce", "0", "-light_radius", "0"});
    const auto area = QbspVisLight_Q1("q1_lightignore.map",
        {"-look", "enhanced", "-samples", "2", "-bounce", "0", "-light_radius", "0.001", "-light_samples", "16"});
    ASSERT_EQ(point.bsp.dlightdata.size(), area.bsp.dlightdata.size());
    for (size_t i = 0; i < point.bsp.dlightdata.size(); ++i)
        EXPECT_LE(std::abs(int(point.bsp.dlightdata[i]) - int(area.bsp.dlightdata[i])), 2);
}


TEST(quality, GammaIsAppliedAfterLinearCoverageIntegration)
{
    std::vector<qvec4f> input(4 * 4, qvec4f{0, 0, 0, 1});
    for (int i = 8; i < 16; ++i)
        input[i] = qvec4f{255, 255, 255, 1};
    const auto average = ReconstructLightmap(input, 4, 4, 4, lightmap_filter_t::BOX);
    const auto output = EncodeLightmapColor(average[0].xyz(), 2.0f, true, true);
    EXPECT_NEAR(output[0], std::sqrt(0.5f) * 255, 0.001);
    EXPECT_EQ(output[0], output[1]);
    // Legacy path has already applied gamma upstream, so must not apply it twice.
    EXPECT_EQ(EncodeLightmapColor(qvec3f{123}, 2.0f, false, false), qvec3f{123});
}
