#pragma once

#include "common.h"

#include <objidl.h>
#include <gdiplus.h>
#include <memory>

namespace ds::ui {

enum class TextAlign { Left, Center, Right };

void initialize();
void shutdown();

Gdiplus::Color color(uint32_t rgb, BYTE alpha = 255);
Gdiplus::Color fromArgb(BYTE a, BYTE r, BYTE g, BYTE b);

void fillRoundRect(Gdiplus::Graphics& graphics, const Gdiplus::RectF& rect, float radius,
                   const Gdiplus::Color& fill);
void strokeRoundRect(Gdiplus::Graphics& graphics, const Gdiplus::RectF& rect, float radius,
                     const Gdiplus::Color& stroke, float width = 1.0f);
void fillEllipse(Gdiplus::Graphics& graphics, const Gdiplus::RectF& rect,
                 const Gdiplus::Color& fill);

void drawText(Gdiplus::Graphics& graphics, const std::wstring& text,
              const Gdiplus::Font& font, const Gdiplus::Color& color,
              const Gdiplus::RectF& rect, TextAlign align = TextAlign::Left, bool wrap = false,
              bool ellipsis = true, int lineLimit = 0, bool centerVertically = false);

// Convenience overload so call sites can pass the font owners directly.
inline void drawText(Gdiplus::Graphics& graphics, const std::wstring& text,
                     const std::unique_ptr<Gdiplus::Font>& font,
                     const Gdiplus::Color& color, const Gdiplus::RectF& rect,
                     TextAlign align = TextAlign::Left, bool wrap = false, bool ellipsis = true,
                     int lineLimit = 0, bool centerVertically = false) {
    if (font) {
        drawText(graphics, text, *font, color, rect, align, wrap, ellipsis, lineLimit,
                 centerVertically);
    }
}
float measureTextHeight(Gdiplus::Graphics& graphics, const std::wstring& text,
                        const Gdiplus::Font& font, float width, bool wrap = true);
float measureTextWidth(Gdiplus::Graphics& graphics, const std::wstring& text,
                       const Gdiplus::Font& font);

std::unique_ptr<Gdiplus::Font> createFont(const wchar_t* family, float pixelSize, bool bold);
std::unique_ptr<Gdiplus::Bitmap> loadImageFromResource(HINSTANCE instance, int resourceId,
                                                       IStream** keepAlive);
std::unique_ptr<Gdiplus::Bitmap> loadImageFromFile(const fs::path& path, IStream** keepAlive);
Gdiplus::RectF fitInto(const Gdiplus::RectF& bounds, float aspect);

} // namespace ds::ui
