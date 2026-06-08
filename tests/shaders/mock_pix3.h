#pragma once
#include <d3d12.h>
#include <wtypes.h>

#define PIX_COLOR(r, g, b) 0
#define PIX_COLOR_INDEX(i) 0
#define PIXScopedEvent(cmdList, color, format, ...)
#define PIXBeginEvent(cmdList, color, format, ...)
#define PIXEndEvent(cmdList)
#define PIXSetMarker(cmdList, color, format, ...)

#define PIX_CAPTURE_GPU 0

union PIXCaptureParameters {
    struct {
        const wchar_t* FileName;
    } GpuCaptureParameters;
};

inline HRESULT PIXBeginCapture(DWORD, const PIXCaptureParameters*) { return S_OK; }
inline HRESULT PIXEndCapture(BOOL) { return S_OK; }
inline HMODULE PIXLoadLatestWinPixGpuCapturerLibrary() { return nullptr; }
