#include <gtest/gtest.h>
#include <light/light.hh>
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
