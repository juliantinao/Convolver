// Application artwork for the window, taskbar and dialog icons.
//
// The artwork is embedded as an SVG (packaging/icon/appicon.svg) through
// juce_add_binary_data, and rasterised here at whatever size the caller asks
// for.
//
// Vector matters at these sizes. Windows renders the taskbar button from a
// 16x16 icon and the title bar from a 32x32 one, and JUCE hands Windows a
// single HICON that it uses for both ICON_BIG and ICON_SMALL
// (juce_Windowing_windows.cpp), so the bitmap we return is what gets downscaled
// for the taskbar. Rendering the geometry natively at 16 keeps that icon crisp;
// resampling a 512px bitmap down to 16 in one step leaves the disc's edge
// ragged and the waveform stroke mushy.
//
// ICON_BIG in CMakeLists.txt consumes a PNG rather than this SVG, because
// juceaide cannot build a Windows .ico out of an SVG. Both files are generated
// from one geometry definition by tools/icons/make-app-icon.ps1, so the .exe
// icon and the in-app icons cannot disagree.
#pragma once

#include <ConvolverAssets.h>
#include <JuceHeader.h>

/** Returns the application icon rasterised as a square image of the given size.

    Windows asks for several sizes depending on where the icon is shown - 16 for
    the taskbar, 32 for the title bar and Alt-Tab, 256 for large Explorer views
    - so callers pass what they actually need. Because the source artwork is
    vector, every size is rendered natively rather than resampled.

    @param size  Edge length in pixels. Must be positive; the default of 32
                 matches the title bar and taskbar use in Main.cpp and
                 HelpWindow.cpp.
*/
inline juce::Image createAppIcon (int size = 32)
{
    // Parsed once and cached. The SVG is embedded in the executable, so this
    // never touches the disk.
    static const std::unique_ptr<juce::Drawable> artwork =
        juce::Drawable::createFromImageData (ConvolverAssets::appicon_svg,
                                             (size_t) ConvolverAssets::appicon_svgSize);

    if (artwork == nullptr || size <= 0)
        return {};

    juce::Image image (juce::Image::ARGB, size, size, true);
    juce::Graphics g (image);

    // drawWithin scales the vector to fill the square; the renderer anti-aliases
    // the geometry at exactly this resolution.
    artwork->drawWithin (g, image.getBounds().toFloat(),
                         juce::RectanglePlacement::centred, 1.0f);

    return image;
}
