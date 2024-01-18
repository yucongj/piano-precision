
/*
 * Convert SVG from Verovio (SVG-1.1 full) to profile supported by Qt
 * (SVG-1.2 Tiny). This code was proposed for use with Macaw
 * (https://github.com/exyte/Macaw, MIT license) by Alpha (alpha0010)
 * https://github.com/exyte/Macaw/issues/759#issuecomment-1022216541
 */
#ifndef SV_VRV_TRIM_H
#define SV_VRV_TRIM_H

#include <string>

class VrvTrim
{
public:
    /**
     * Convert svg symbol defs to path defs, and other manipulations
     * needed to conform to SVG 1.2 Tiny.
     */
    static std::string transformSvgToTiny(const std::string &svg);
};

#endif
