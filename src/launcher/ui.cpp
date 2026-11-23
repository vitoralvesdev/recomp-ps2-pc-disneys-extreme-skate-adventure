#include "ui.h"

#include <shlwapi.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace ds::ui {
namespace {

ULONG_PTR gdiplusToken = 0;
bool gdiplusReady = false;

void buildRoundedPath(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& rect, float radius) {
    path.Reset();
    const float r = std::min({radius, rect.Width / 2.0f, rect.Height / 2.0f});
    if (r <= 0.5f) {
        path.AddRectangle(rect);
        return;
    }
    const float d = r * 2.0f;
    path.AddArc(rect.X, rect.Y, d, d, 180.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - d, rect.Y, d, d, 270.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - d, rect.Y + rect.Height - d, d, d, 0.0f, 90.0f);
    path.AddArc(rect.X, rect.Y + rect.Height - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

std::unique_ptr<Gdiplus::Bitmap> loadFromStream(IStream* stream) {
    if (!stream) return nullptr;
    auto bitmap = std::make_unique<Gdiplus::Bitmap>(stream);
    if (bitmap->GetLastStatus() != Gdiplus::Ok) return nullptr;
    return bitmap;
}

} // namespace

void initialize() {
    if (gdiplusReady) return;
    Gdiplus::GdiplusStartupInput input;
    if (Gdiplus::GdiplusStartup(&gdiplusToken, &input, nullptr) == Gdiplus::Ok) {
        gdiplusReady = true;
    }
}

void shutdown() {
    if (!gdiplusReady) return;
    Gdiplus::GdiplusShutdown(gdiplusToken);
    gdiplusToken = 0;
    gdiplusReady = false;
}

Gdiplus::Color color(uint32_t rgb, BYTE alpha) {
    return Gdiplus::Color(alpha, static_cast<BYTE>((rgb >> 16) & 0xff),
                          static_cast<BYTE>((rgb >> 8) & 0xff), static_cast<BYTE>(rgb & 0xff));
}

Gdiplus::Color fromArgb(BYTE a, BYTE r, BYTE g, BYTE b) { return Gdiplus::Color(a, r, g, b); }

void fillRoundRect(Gdiplus::Graphics& graphics, const Gdiplus::RectF& rect, float radius,
                   const Gdiplus::Color& fill) {
    Gdiplus::GraphicsPath path;
    buildRoundedPath(path, rect, radius);
    Gdiplus::SolidBrush brush(fill);
    graphics.FillPath(&brush, &path);
}

void strokeRoundRect(Gdiplus::Graphics& graphics, const Gdiplus::RectF& rect, float radius,
                     const Gdiplus::Color& stroke, float width) {
    Gdiplus::GraphicsPath path;
    buildRoundedPath(path, rect, radius);
    Gdiplus::Pen pen(stroke, width);
    graphics.DrawPath(&pen, &path);
}

void fillEllipse(Gdiplus::Graphics& graphics, const Gdiplus::RectF& rect,
                 const Gdiplus::Color& fill) {
    Gdiplus::SolidBrush brush(fill);
    graphics.FillEllipse(&brush, rect);
}

void drawText(Gdiplus::Graphics& graphics, const std::wstring& text,
              const Gdiplus::Font& font, const Gdiplus::Color& textColor,
              const Gdiplus::RectF& rect, TextAlign align, bool wrap, bool ellipsis,
              int lineLimit) {
    if (text.empty()) return;
    Gdiplus::StringFormat format;
    format.SetAlignment(align == TextAlign::Left     ? Gdiplus::StringAlignmentNear
                        : align == TextAlign::Center ? Gdiplus::StringAlignmentCenter
                                                     : Gdiplus::StringAlignmentFar);
    format.SetLineAlignment(Gdiplus::StringAlignmentNear);
    if (!wrap) {
        format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
    }
    if (ellipsis && !wrap) {
        format.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    }
    if (lineLimit > 0) {
        format.SetFormatFlags(format.GetFormatFlags() | Gdiplus::StringFormatFlagsLineLimit);
    }
    Gdiplus::SolidBrush brush(textColor);
    graphics.DrawString(text.c_str(), static_cast<INT>(text.size()), &font, rect, &format,
                        &brush);
}

float measureTextHeight(Gdiplus::Graphics& graphics, const std::wstring& text,
                        const Gdiplus::Font& font, float width, bool wrap) {
    if (text.empty()) return 0.0f;
    Gdiplus::RectF layout(0, 0, width, 100000.0f);
    Gdiplus::RectF bounds;
    Gdiplus::StringFormat format;
    format.SetAlignment(Gdiplus::StringAlignmentNear);
    format.SetLineAlignment(Gdiplus::StringAlignmentNear);
    if (!wrap) format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
    graphics.MeasureString(text.c_str(), static_cast<INT>(text.size()), &font, layout, &format,
                           &bounds);
    return bounds.Height;
}

float measureTextWidth(Gdiplus::Graphics& graphics, const std::wstring& text,
                       const Gdiplus::Font& font) {
    Gdiplus::RectF bounds;
    Gdiplus::StringFormat format;
    format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
    graphics.MeasureString(text.c_str(), static_cast<INT>(text.size()), &font,
                           Gdiplus::PointF(0, 0), &format, &bounds);
    return bounds.Width;
}

std::unique_ptr<Gdiplus::Font> createFont(const wchar_t* family, float pixelSize, bool bold) {
    auto font = std::make_unique<Gdiplus::Font>(
        family, pixelSize, bold ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular,
        Gdiplus::UnitPixel);
    if (font->GetLastStatus() != Gdiplus::Ok) {
        font = std::make_unique<Gdiplus::Font>(L"Arial", pixelSize,
                                               bold ? Gdiplus::FontStyleBold
                                                    : Gdiplus::FontStyleRegular,
                                               Gdiplus::UnitPixel);
    }
    return font;
}

std::unique_ptr<Gdiplus::Bitmap> loadImageFromResource(HINSTANCE instance, int resourceId,
                                                       IStream** keepAlive) {
    *keepAlive = nullptr;
    HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (!resource) return nullptr;
    HGLOBAL loaded = LoadResource(instance, resource);
    if (!loaded) return nullptr;
    const void* data = LockResource(loaded);
    const DWORD size = SizeofResource(instance, resource);
    if (!data || size == 0) return nullptr;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!memory) return nullptr;
    void* destination = GlobalLock(memory);
    if (!destination) {
        GlobalFree(memory);
        return nullptr;
    }
    std::memcpy(destination, data, size);
    GlobalUnlock(memory);
    IStream* stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream))) {
        GlobalFree(memory);
        return nullptr;
    }
    auto bitmap = loadFromStream(stream);
    if (!bitmap) {
        stream->Release();
        return nullptr;
    }
    *keepAlive = stream;
    return bitmap;
}

std::unique_ptr<Gdiplus::Bitmap> loadImageFromFile(const fs::path& path, IStream** keepAlive) {
    *keepAlive = nullptr;
    const std::wstring wide = path.wstring();
    IStream* stream = nullptr;
    if (FAILED(SHCreateStreamOnFileEx(wide.c_str(), STGM_READ, FILE_ATTRIBUTE_NORMAL, FALSE,
                                      nullptr, &stream))) {
        return nullptr;
    }
    auto bitmap = loadFromStream(stream);
    if (!bitmap) {
        stream->Release();
        return nullptr;
    }
    *keepAlive = stream;
    return bitmap;
}

Gdiplus::RectF fitInto(const Gdiplus::RectF& bounds, float aspect) {
    const float width = std::min(bounds.Width, bounds.Height * aspect);
    const float height = width / aspect;
    return Gdiplus::RectF(bounds.X + (bounds.Width - width) / 2.0f,
                          bounds.Y + (bounds.Height - height) / 2.0f, width, height);
}

} // namespace ds::ui
