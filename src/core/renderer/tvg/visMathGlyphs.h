// Outlines from Noto Sans Math, SIL OFL 1.1; see NotoSansMath-OFL.txt.
#ifndef VIS_MATH_GLYPHS_H
#define VIS_MATH_GLYPHS_H

struct MathGlyph
{
    const char* text;
    const tvg::PathCommand* commands;
    const tvg::Point* points;
    uint32_t commandCount;
    uint32_t pointCount;
    float advance;
    tvg::Point min;
    tvg::Point max;
};

static constexpr tvg::PathCommand MATH_2220_COMMANDS[] = {
    tvg::PathCommand::MoveTo, tvg::PathCommand::LineTo, tvg::PathCommand::LineTo, tvg::PathCommand::LineTo, tvg::PathCommand::LineTo, tvg::PathCommand::LineTo,
    tvg::PathCommand::LineTo, tvg::PathCommand::Close,
};
static constexpr tvg::Point MATH_2220_POINTS[] = {
    {0.05100000f, 0.00000000f}, {0.05100000f, 0.05000000f}, {0.46100000f, 0.51500000f}, {0.51100000f, 0.46400000f},
    {0.16600000f, 0.07100000f}, {0.60100000f, 0.07100000f}, {0.60100000f, 0.00000000f},
};
static constexpr tvg::PathCommand MATH_B0_COMMANDS[] = {
    tvg::PathCommand::MoveTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo,
    tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::Close, tvg::PathCommand::MoveTo, tvg::PathCommand::CubicTo,
    tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo,
    tvg::PathCommand::CubicTo, tvg::PathCommand::Close,
};
static constexpr tvg::Point MATH_B0_POINTS[] = {
    {0.21400000f, 0.41700000f}, {0.16600000f, 0.41700000f}, {0.12750000f, 0.43116667f}, {0.09850000f, 0.45950000f},
    {0.06950000f, 0.48783333f}, {0.05500000f, 0.52466667f}, {0.05500000f, 0.57000000f}, {0.05500000f, 0.61533333f},
    {0.06933333f, 0.65233333f}, {0.09800000f, 0.68100000f}, {0.12666667f, 0.70966667f}, {0.16533333f, 0.72400000f},
    {0.21400000f, 0.72400000f}, {0.26133333f, 0.72400000f}, {0.29966667f, 0.70966667f}, {0.32900000f, 0.68100000f},
    {0.35833333f, 0.65233333f}, {0.37300000f, 0.61533333f}, {0.37300000f, 0.57000000f}, {0.37300000f, 0.52466667f},
    {0.35833333f, 0.48783333f}, {0.32900000f, 0.45950000f}, {0.29966667f, 0.43116667f}, {0.26133333f, 0.41700000f},
    {0.21400000f, 0.41700000f}, {0.21500000f, 0.47600000f}, {0.24700000f, 0.47600000f}, {0.27050000f, 0.48466667f},
    {0.28550000f, 0.50200000f}, {0.30050000f, 0.51933333f}, {0.30800000f, 0.54200000f}, {0.30800000f, 0.57000000f},
    {0.30800000f, 0.59933333f}, {0.30016667f, 0.62266667f}, {0.28450000f, 0.64000000f}, {0.26883333f, 0.65733333f},
    {0.24566667f, 0.66600000f}, {0.21500000f, 0.66600000f}, {0.18233333f, 0.66600000f}, {0.15833333f, 0.65733333f},
    {0.14300000f, 0.64000000f}, {0.12766667f, 0.62266667f}, {0.12000000f, 0.59933333f}, {0.12000000f, 0.57000000f},
    {0.12000000f, 0.54200000f}, {0.12766667f, 0.51933333f}, {0.14300000f, 0.50200000f}, {0.15833333f, 0.48466667f},
    {0.18233333f, 0.47600000f}, {0.21500000f, 0.47600000f},
};
static constexpr tvg::PathCommand MATH_22A5_COMMANDS[] = {
    tvg::PathCommand::MoveTo, tvg::PathCommand::LineTo, tvg::PathCommand::LineTo, tvg::PathCommand::LineTo, tvg::PathCommand::LineTo, tvg::PathCommand::LineTo,
    tvg::PathCommand::LineTo, tvg::PathCommand::LineTo, tvg::PathCommand::Close,
};
static constexpr tvg::Point MATH_22A5_POINTS[] = {
    {0.05100000f, 0.00000000f}, {0.05100000f, 0.07100000f}, {0.34300000f, 0.07100000f}, {0.34300000f, 0.65400000f},
    {0.41400000f, 0.65400000f}, {0.41400000f, 0.07100000f}, {0.70500000f, 0.07100000f}, {0.70500000f, 0.00000000f},
};
static constexpr tvg::PathCommand MATH_2225_COMMANDS[] = {
    tvg::PathCommand::MoveTo, tvg::PathCommand::LineTo, tvg::PathCommand::LineTo, tvg::PathCommand::LineTo, tvg::PathCommand::Close, tvg::PathCommand::MoveTo,
    tvg::PathCommand::LineTo, tvg::PathCommand::LineTo, tvg::PathCommand::LineTo, tvg::PathCommand::Close,
};
static constexpr tvg::Point MATH_2225_POINTS[] = {
    {0.05100000f, -0.16200000f}, {0.05100000f, 0.71800000f}, {0.12200000f, 0.71800000f}, {0.12200000f, -0.16200000f},
    {0.23800000f, -0.16200000f}, {0.23800000f, 0.71800000f}, {0.30900000f, 0.71800000f}, {0.30900000f, -0.16200000f},
};
static constexpr tvg::PathCommand MATH_25B3_COMMANDS[] = {
    tvg::PathCommand::MoveTo, tvg::PathCommand::LineTo, tvg::PathCommand::LineTo, tvg::PathCommand::Close, tvg::PathCommand::MoveTo, tvg::PathCommand::LineTo,
    tvg::PathCommand::LineTo, tvg::PathCommand::Close,
};
static constexpr tvg::Point MATH_25B3_POINTS[] = {
    {0.05100000f, 0.00000000f}, {0.45400000f, 0.69700000f}, {0.85600000f, 0.00000000f}, {0.17500000f, 0.07200000f},
    {0.73200000f, 0.07200000f}, {0.45400000f, 0.55400000f},
};
static constexpr tvg::PathCommand MATH_2245_COMMANDS[] = {
    tvg::PathCommand::MoveTo, tvg::PathCommand::LineTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo,
    tvg::PathCommand::CubicTo, tvg::PathCommand::LineTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo,
    tvg::PathCommand::CubicTo, tvg::PathCommand::Close, tvg::PathCommand::MoveTo, tvg::PathCommand::LineTo, tvg::PathCommand::LineTo, tvg::PathCommand::LineTo,
    tvg::PathCommand::Close, tvg::PathCommand::MoveTo, tvg::PathCommand::LineTo, tvg::PathCommand::LineTo, tvg::PathCommand::LineTo, tvg::PathCommand::Close,
};
static constexpr tvg::Point MATH_2245_POINTS[] = {
    {0.05100000f, 0.35900000f}, {0.05100000f, 0.43700000f}, {0.08433333f, 0.47233333f}, {0.12400000f, 0.49000000f},
    {0.17000000f, 0.49000000f}, {0.21133333f, 0.49000000f}, {0.25333333f, 0.48000000f}, {0.29600000f, 0.46000000f},
    {0.33866667f, 0.44066667f}, {0.37533333f, 0.43100000f}, {0.40600000f, 0.43100000f}, {0.42333333f, 0.43100000f},
    {0.44216667f, 0.43616667f}, {0.46250000f, 0.44650000f}, {0.48283333f, 0.45683333f}, {0.50233333f, 0.47133333f},
    {0.52100000f, 0.49000000f}, {0.52100000f, 0.41300000f}, {0.48833333f, 0.37700000f}, {0.44833333f, 0.35900000f},
    {0.40100000f, 0.35900000f}, {0.36033333f, 0.35900000f}, {0.31833333f, 0.36900000f}, {0.27500000f, 0.38900000f},
    {0.23233333f, 0.40833333f}, {0.19533333f, 0.41800000f}, {0.16400000f, 0.41800000f}, {0.14800000f, 0.41800000f},
    {0.13000000f, 0.41300000f}, {0.11000000f, 0.40300000f}, {0.09000000f, 0.39300000f}, {0.07033333f, 0.37833333f},
    {0.05100000f, 0.35900000f}, {0.05100000f, 0.22600000f}, {0.05100000f, 0.29700000f}, {0.52100000f, 0.29700000f},
    {0.52100000f, 0.22600000f}, {0.05100000f, 0.06600000f}, {0.05100000f, 0.13700000f}, {0.52100000f, 0.13700000f},
    {0.52100000f, 0.06600000f},
};
static constexpr tvg::PathCommand MATH_223C_COMMANDS[] = {
    tvg::PathCommand::MoveTo, tvg::PathCommand::LineTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo,
    tvg::PathCommand::CubicTo, tvg::PathCommand::LineTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo,
    tvg::PathCommand::CubicTo, tvg::PathCommand::Close,
};
static constexpr tvg::Point MATH_223C_POINTS[] = {
    {0.05100000f, 0.21300000f}, {0.05100000f, 0.29100000f}, {0.08300000f, 0.32633333f}, {0.12266667f, 0.34400000f},
    {0.17000000f, 0.34400000f}, {0.20933333f, 0.34400000f}, {0.25133333f, 0.33433333f}, {0.29600000f, 0.31500000f},
    {0.34000000f, 0.29633333f}, {0.37666667f, 0.28500000f}, {0.40600000f, 0.28500000f}, {0.42333333f, 0.28500000f},
    {0.44250000f, 0.29016667f}, {0.46350000f, 0.30050000f}, {0.48450000f, 0.31083333f}, {0.50366667f, 0.32533333f},
    {0.52100000f, 0.34400000f}, {0.52100000f, 0.26700000f}, {0.48833333f, 0.23100000f}, {0.44833333f, 0.21300000f},
    {0.40100000f, 0.21300000f}, {0.35966667f, 0.21300000f}, {0.31733333f, 0.22300000f}, {0.27400000f, 0.24300000f},
    {0.23133333f, 0.26233333f}, {0.19466667f, 0.27200000f}, {0.16400000f, 0.27200000f}, {0.14666667f, 0.27200000f},
    {0.12800000f, 0.26683333f}, {0.10800000f, 0.25650000f}, {0.08800000f, 0.24616667f}, {0.06900000f, 0.23166667f},
    {0.05100000f, 0.21300000f},
};
static constexpr tvg::PathCommand MATH_3C0_COMMANDS[] = {
    tvg::PathCommand::MoveTo, tvg::PathCommand::CubicTo, tvg::PathCommand::LineTo, tvg::PathCommand::LineTo, tvg::PathCommand::LineTo, tvg::PathCommand::LineTo,
    tvg::PathCommand::LineTo, tvg::PathCommand::LineTo, tvg::PathCommand::LineTo, tvg::PathCommand::LineTo, tvg::PathCommand::LineTo, tvg::PathCommand::LineTo,
    tvg::PathCommand::LineTo, tvg::PathCommand::LineTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo,
    tvg::PathCommand::LineTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::Close,
};
static constexpr tvg::Point MATH_3C0_POINTS[] = {
    {0.54600000f, -0.01000000f}, {0.47200000f, -0.01000000f}, {0.43500000f, 0.03266667f}, {0.43500000f, 0.11800000f},
    {0.43500000f, 0.46300000f}, {0.20800000f, 0.46300000f}, {0.20800000f, 0.00000000f}, {0.12100000f, 0.00000000f},
    {0.12100000f, 0.46300000f}, {0.01200000f, 0.46300000f}, {0.01200000f, 0.50100000f}, {0.08400000f, 0.53600000f},
    {0.62700000f, 0.53600000f}, {0.62700000f, 0.46300000f}, {0.52200000f, 0.46300000f}, {0.52200000f, 0.12400000f},
    {0.52200000f, 0.10066667f}, {0.52633333f, 0.08450000f}, {0.53500000f, 0.07550000f}, {0.54366667f, 0.06650000f},
    {0.55466667f, 0.06200000f}, {0.56800000f, 0.06200000f}, {0.57733333f, 0.06200000f}, {0.58583333f, 0.06300000f},
    {0.59350000f, 0.06500000f}, {0.60116667f, 0.06700000f}, {0.60733333f, 0.06933333f}, {0.61200000f, 0.07200000f},
    {0.61200000f, 0.00300000f}, {0.60666667f, 0.00033333f}, {0.59816667f, -0.00250000f}, {0.58650000f, -0.00550000f},
    {0.57483333f, -0.00850000f}, {0.56133333f, -0.01000000f}, {0.54600000f, -0.01000000f},
};
static constexpr tvg::PathCommand MATH_3B8_COMMANDS[] = {
    tvg::PathCommand::MoveTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo,
    tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::Close,
    tvg::PathCommand::MoveTo, tvg::PathCommand::LineTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo,
    tvg::PathCommand::Close, tvg::PathCommand::MoveTo, tvg::PathCommand::CubicTo, tvg::PathCommand::CubicTo, tvg::PathCommand::LineTo, tvg::PathCommand::CubicTo,
    tvg::PathCommand::CubicTo, tvg::PathCommand::Close,
};
static constexpr tvg::Point MATH_3B8_POINTS[] = {
    {0.29600000f, -0.01000000f}, {0.21333333f, -0.01000000f}, {0.15250000f, 0.02433333f}, {0.11350000f, 0.09300000f},
    {0.07450000f, 0.16166667f}, {0.05500000f, 0.25633333f}, {0.05500000f, 0.37700000f}, {0.05500000f, 0.45833333f},
    {0.06300000f, 0.52800000f}, {0.07900000f, 0.58600000f}, {0.09500000f, 0.64400000f}, {0.12083333f, 0.68833333f},
    {0.15650000f, 0.71900000f}, {0.19216667f, 0.74966667f}, {0.23866667f, 0.76500000f}, {0.29600000f, 0.76500000f},
    {0.37933333f, 0.76500000f}, {0.44083333f, 0.73116667f}, {0.48050000f, 0.66350000f}, {0.52016667f, 0.59583333f},
    {0.54000000f, 0.50033333f}, {0.54000000f, 0.37700000f}, {0.54000000f, 0.29633333f}, {0.53183333f, 0.22716667f},
    {0.51550000f, 0.16950000f}, {0.49916667f, 0.11183333f}, {0.47316667f, 0.06750000f}, {0.43750000f, 0.03650000f},
    {0.40183333f, 0.00550000f}, {0.35466667f, -0.01000000f}, {0.29600000f, -0.01000000f}, {0.14400000f, 0.42400000f},
    {0.45000000f, 0.42400000f}, {0.44666667f, 0.51333333f}, {0.43300000f, 0.58050000f}, {0.40900000f, 0.62550000f},
    {0.38500000f, 0.67050000f}, {0.34733333f, 0.69300000f}, {0.29600000f, 0.69300000f}, {0.24533333f, 0.69300000f},
    {0.20816667f, 0.67050000f}, {0.18450000f, 0.62550000f}, {0.16083333f, 0.58050000f}, {0.14733333f, 0.51333333f},
    {0.14400000f, 0.42400000f}, {0.29600000f, 0.06200000f}, {0.34800000f, 0.06200000f}, {0.38633333f, 0.08600000f},
    {0.41100000f, 0.13400000f}, {0.43566667f, 0.18200000f}, {0.44900000f, 0.25433333f}, {0.45100000f, 0.35100000f},
    {0.14400000f, 0.35100000f}, {0.14533333f, 0.25566667f}, {0.15783333f, 0.18366667f}, {0.18150000f, 0.13500000f},
    {0.20516667f, 0.08633333f}, {0.24333333f, 0.06200000f}, {0.29600000f, 0.06200000f},
};

static constexpr MathGlyph MATH_GLYPHS[] = {
    {"\xe2\x88\xa0", MATH_2220_COMMANDS, MATH_2220_POINTS, 8u, 7u, 0.65200000f, {0.05100000f, 0.00000000f}, {0.60100000f, 0.51500000f}},
    {"\xc2\xb0", MATH_B0_COMMANDS, MATH_B0_POINTS, 20u, 50u, 0.42800000f, {0.05500000f, 0.41700000f}, {0.37300000f, 0.72400000f}},
    {"\xe2\x8a\xa5", MATH_22A5_COMMANDS, MATH_22A5_POINTS, 9u, 8u, 0.75600000f, {0.05100000f, 0.00000000f}, {0.70500000f, 0.65400000f}},
    {"\xe2\x88\xa5", MATH_2225_COMMANDS, MATH_2225_POINTS, 10u, 8u, 0.36000000f, {0.05100000f, -0.16200000f}, {0.30900000f, 0.71800000f}},
    {"\xe2\x96\xb3", MATH_25B3_COMMANDS, MATH_25B3_POINTS, 8u, 6u, 0.90700000f, {0.05100000f, 0.00000000f}, {0.85600000f, 0.69700000f}},
    {"\xe2\x89\x85", MATH_2245_COMMANDS, MATH_2245_POINTS, 24u, 41u, 0.57200000f, {0.05100000f, 0.06600000f}, {0.52100000f, 0.49000000f}},
    {"\xe2\x88\xbc", MATH_223C_COMMANDS, MATH_223C_POINTS, 14u, 33u, 0.57200000f, {0.05100000f, 0.21300000f}, {0.52100000f, 0.34400000f}},
    {"\xcf\x80", MATH_3C0_COMMANDS, MATH_3C0_POINTS, 22u, 35u, 0.65900000f, {0.01200000f, -0.01000000f}, {0.62700000f, 0.53600000f}},
    {"\xce\xb8", MATH_3B8_COMMANDS, MATH_3B8_POINTS, 26u, 59u, 0.59500000f, {0.05500000f, -0.01000000f}, {0.54000000f, 0.76500000f}},
};

#endif
