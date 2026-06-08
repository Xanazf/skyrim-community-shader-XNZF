import os

def patch_file(filepath, search_str, replace_str):
    if not os.path.exists(filepath):
        print(f"Warning: file not found: {filepath}")
        return
    with open(filepath, 'r') as f:
        content = f.read()
    if search_str in content:
        content = content.replace(search_str, replace_str)
        with open(filepath, 'w') as f:
            f.write(content)
        print(f"Patched: {filepath}")
    else:
        print(f"Already patched or target not found in: {filepath}")

# 1. CMakeLists.txt (disable MSVC checks)
patch_file("CMakeLists.txt", "if(NOT MSVC)", "if(FALSE)")
patch_file("CMakeLists.txt", "if(MSVC_VERSION VERSION_LESS 1937)", "if(FALSE)")

# 2. Platform.h (case sensitive windows.h, UUID specializations, and D3D12 ABI helpers)
patch_file("src/Public/Platform.h", "#include <Windows.h>", "#include <windows.h>\n#include <d3d12.h>")

uuid_block = """#if !defined(_MSC_VER)
#define _uuidof __uuidof
struct IDxcIncludeHandler;
struct IDxcBlob;
struct IDxcResult;
struct IDxcBlobUtf8;
struct IDxcUtils;
struct IDxcCompiler3;
struct ID3D12ShaderReflection;
struct ID3D12PipelineState;
struct ID3D12Device;
struct ID3D12Device12;
struct ID3D12CommandAllocator;
struct ID3D12GraphicsCommandList9;
struct ID3D12CommandQueue;
struct ID3D12Resource2;
struct ID3D12DescriptorHeap;
struct ID3D12Fence1;
struct ID3D12RootSignature;
struct ID3D12VersionedRootSignatureDeserializer;
struct ID3D12Debug6;
struct ID3D12InfoQueue;
struct ID3D12DebugDevice2;

__CRT_UUID_DECL(IDxcIncludeHandler, 0x7F61FC7D, 0x950D, 0x467F, 0xA3, 0xE3, 0xDE, 0x19, 0x2C, 0x48, 0x74, 0xEE)
__CRT_UUID_DECL(IDxcBlob, 0x8BA5FB08, 0x5195, 0x40E2, 0xAC, 0x58, 0x0D, 0x98, 0x9C, 0x3A, 0x01, 0x02)
__CRT_UUID_DECL(IDxcResult, 0x58346CDA, 0xDDE7, 0x4497, 0x94, 0x61, 0x6F, 0x87, 0xAF, 0x5E, 0x06, 0x59)
__CRT_UUID_DECL(IDxcBlobUtf8, 0x3DA636C9, 0xBA71, 0x4024, 0xA3, 0x01, 0x30, 0xCB, 0xF1, 0x25, 0x30, 0x5B)
__CRT_UUID_DECL(IDxcUtils, 0x4605C4CB, 0x2019, 0x492A, 0xAD, 0xA4, 0x65, 0xF2, 0x0B, 0xB7, 0xD6, 0x7F)
__CRT_UUID_DECL(IDxcCompiler3, 0x228B4687, 0x5A6A, 0x4730, 0x90, 0x0C, 0x97, 0x02, 0xB2, 0x20, 0x3F, 0x54)
__CRT_UUID_DECL(ID3D12ShaderReflection, 0x5A58797D, 0xA72C, 0x478D, 0x8B, 0xA2, 0xEF, 0xC6, 0xB0, 0xEF, 0xE8, 0x8E)
__CRT_UUID_DECL(ID3D12PipelineState, 0x765A30F3, 0xF624, 0x4C6F, 0xA8, 0x28, 0xAC, 0xE9, 0x48, 0x62, 0x24, 0x45)
__CRT_UUID_DECL(ID3D12Device, 0x189819F1, 0x1DB6, 0x4B57, 0xBE, 0x54, 0x18, 0x21, 0x33, 0x9B, 0x85, 0xF7)
__CRT_UUID_DECL(ID3D12Device12, 0x5AF5C532, 0x4C91, 0x4CD0, 0xB5, 0x41, 0x15, 0xA4, 0x05, 0x39, 0x5F, 0xC5)
__CRT_UUID_DECL(ID3D12CommandAllocator, 0x6102DEE4, 0xAF59, 0x4B09, 0xB9, 0x99, 0xB4, 0x4D, 0x73, 0xF0, 0x9B, 0x24)
__CRT_UUID_DECL(ID3D12GraphicsCommandList9, 0x34ED2808, 0xFFE6, 0x4C2B, 0xB1, 0x1A, 0xCA, 0xBD, 0x2B, 0x0C, 0x59, 0xE1)
__CRT_UUID_DECL(ID3D12CommandQueue, 0x0EC870A6, 0x5D7E, 0x4C22, 0x8C, 0xFC, 0x5B, 0xAA, 0xE0, 0x76, 0x16, 0xED)
__CRT_UUID_DECL(ID3D12Resource2, 0xBE36EC3B, 0xEA85, 0x4AEB, 0xA4, 0x5A, 0xE9, 0xD7, 0x64, 0x04, 0xA4, 0x95)
__CRT_UUID_DECL(ID3D12DescriptorHeap, 0x8EFB471D, 0x616C, 0x4F49, 0x90, 0xF7, 0x12, 0x7B, 0xB7, 0x63, 0xFA, 0x51)
__CRT_UUID_DECL(ID3D12Fence1, 0x433685FE, 0xE22B, 0x4CA0, 0xA8, 0xDB, 0xB5, 0xB4, 0xF4, 0xDD, 0x0E, 0x4A)
__CRT_UUID_DECL(ID3D12RootSignature, 0xC54A6B66, 0x72DF, 0x4EE8, 0x8B, 0xE5, 0xA9, 0x46, 0xA1, 0x42, 0x92, 0x14)
__CRT_UUID_DECL(ID3D12VersionedRootSignatureDeserializer, 0x7F91CE67, 0x090C, 0x4BB7, 0xB7, 0x8E, 0xED, 0x8F, 0xF2, 0xE3, 0x1D, 0xA0)
__CRT_UUID_DECL(ID3D12Debug6, 0x82A816D6, 0x5D01, 0x4157, 0x97, 0xD0, 0x49, 0x75, 0x46, 0x3F, 0xD1, 0xED)
__CRT_UUID_DECL(ID3D12InfoQueue, 0x0742A90B, 0xC387, 0x483F, 0xB9, 0x46, 0x30, 0xA7, 0xE4, 0xE6, 0x14, 0x58)
__CRT_UUID_DECL(ID3D12DebugDevice2, 0x60ECCBC1, 0x378D, 0x4DF1, 0x89, 0x4C, 0xF8, 0xAC, 0x5C, 0xE4, 0xD7, 0xDD)

template<typename Interface, typename Desc>
inline Desc GetDescHelper(Interface* obj) {
    Desc desc;
    obj->GetDesc(&desc);
    return desc;
}

template<typename Interface, typename Desc>
inline Desc GetDesc1Helper(Interface* obj) {
    Desc desc;
    obj->GetDesc1(&desc);
    return desc;
}

inline D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandleHelper(ID3D12DescriptorHeap* heap) {
    D3D12_GPU_DESCRIPTOR_HANDLE handle;
    heap->GetGPUDescriptorHandleForHeapStart(&handle);
    return handle;
}

inline D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandleHelper(ID3D12DescriptorHeap* heap) {
    D3D12_CPU_DESCRIPTOR_HANDLE handle;
    heap->GetCPUDescriptorHandleForHeapStart(&handle);
    return handle;
}
#endif"""

patch_file("src/Public/Platform.h", "#include <windows.h>\n#include <d3d12.h>", "#include <windows.h>\n#include <d3d12.h>\n" + uuid_block)


# 3. IncludeHandler.h (case sensitive Unknwn.h)
patch_file("src/Public/D3D12/Shader/IncludeHandler.h", "#include <Unknwn.h>", "#include <unknwn.h>")

# 4. VirtualShaderDirectoryMappingManager.h (missing vector)
patch_file("src/Public/D3D12/Shader/VirtualShaderDirectoryMappingManager.h", 
           '#include "Utility/Expected.h"', 
           '#include "Utility/Expected.h"\n#include <vector>')

# 5. TypeByteReader.h (missing cstring)
patch_file("src/Public/Framework/TypeByteReader.h", 
           '#include <cstddef>', 
           '#include <cstddef>\n#include <cstring>')

# 6. ShaderTestCommon.cpp (missing cstring)
patch_file("src/Private/Framework/ShaderTestCommon.cpp", 
           '#include <ranges>', 
           '#include <cstring>\n#include <ranges>')

# 7. Expected.h and Error.h (replace std::print with std::format)
patch_file("src/Public/Utility/Expected.h", 
           'std::print(InOutStream, "Unstreamable expected type: {}", stf::TypeToString<T>());',
           'InOutStream << std::format("Unstreamable expected type: {}", stf::TypeToString<T>());')
patch_file("src/Public/Utility/Expected.h", 
           'std::print(InOutStream, "Unstreamable error type: {}", stf::TypeToString<E>());',
           'InOutStream << std::format("Unstreamable error type: {}", stf::TypeToString<E>());')
patch_file("src/Public/Utility/Error.h", 
           'std::print(InOutStream, "{}", InError);',
           'InOutStream << std::format("{}", InError);')

# 8. EnumReflection.h (GCC-specific __PRETTY_FUNCTION__ parser)
enum_funcsig = """#if !defined(_MSC_VER) && (defined(__clang__) || defined(__GNUC__))
#define __FUNCSIG__ __PRETTY_FUNCTION__
#endif"""
patch_file("src/Public/Utility/EnumReflection.h", "#pragma once", enum_funcsig + "\n\n#pragma once")

gcc_enum_parser = """#if defined(__GNUC__) && !defined(__clang__)
            const std::size_t valIdx = funcSig.find("Val = ");
            if (valIdx == std::string_view::npos)
            {
                return {};
            }
            const std::size_t start = valIdx + 6;
            std::size_t end = funcSig.find(';', start);
            if (end == std::string_view::npos)
            {
                end = funcSig.find_last_of(']');
            }
            if (end == std::string_view::npos || end <= start)
            {
                return {};
            }
            return funcSig.substr(start, end - start);
#else"""

patch_file("src/Public/Utility/EnumReflection.h",
           "            const std::size_t end = funcSig.find_last_of(EnumEndToken);",
           gcc_enum_parser + "\n            const std::size_t end = funcSig.find_last_of(EnumEndToken);")

patch_file("src/Public/Utility/EnumReflection.h",
           "            return funcSig.substr(begin, end - begin);\n        }",
           "            return funcSig.substr(begin, end - begin);\n#endif\n        }")

# 9. Type.h (__FUNCSIG__ mapping and clang/gnuc compatibility)
type_funcsig = """#if !defined(_MSC_VER) && (defined(__clang__) || defined(__GNUC__))
#define __FUNCSIG__ __PRETTY_FUNCTION__
#endif"""
patch_file("src/Public/Utility/Type.h", "#pragma once", type_funcsig + "\n\n#pragma once")
patch_file("src/Public/Utility/Type.h", "TypeBeginToken =\n#ifdef __clang__", "TypeBeginToken =\n#if defined(__clang__) || defined(__GNUC__)")
patch_file("src/Public/Utility/Type.h", "TypeEndToken =\n#ifdef __clang__", "TypeEndToken =\n#if defined(__clang__) || defined(__GNUC__)")

# 10. Enum reflection Clang/GCC tokens
patch_file("src/Public/Utility/EnumReflection.h", "EnumBeginToken =\n#ifdef __clang__", "EnumBeginToken =\n#if defined(__clang__) || defined(__GNUC__)")
patch_file("src/Public/Utility/EnumReflection.h", "EnumEndToken =\n#ifdef __clang__", "EnumEndToken =\n#if defined(__clang__) || defined(__GNUC__)")

# 11. D3D12 COM method ABI workarounds (GetDesc / GetCPUDescriptorHandle etc.)
patch_file("src/Private/D3D12/CommandQueue.cpp", 
           "return m_Queue->GetDesc().Type;", 
           "return GetDescHelper<ID3D12CommandQueue, D3D12_COMMAND_QUEUE_DESC>(m_Queue.Get()).Type;")

patch_file("src/Private/D3D12/DescriptorHeap.cpp",
           "const u64 gpuPtr = GetAccess() == D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE ? m_Heap->GetGPUDescriptorHandleForHeapStart().ptr : 0ull;",
           "const u64 gpuPtr = GetAccess() == D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE ? GetGPUDescriptorHandleHelper(m_Heap.Get()).ptr : 0ull;")

patch_file("src/Private/D3D12/DescriptorHeap.cpp",
           "const D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle{ m_Heap->GetCPUDescriptorHandleForHeapStart().ptr + offset };",
           "const D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle{ GetCPUDescriptorHandleHelper(m_Heap.Get()).ptr + offset };")

patch_file("src/Private/D3D12/DescriptorHeap.cpp",
           "return m_Heap->GetDesc().NumDescriptors;",
           "return GetDescHelper<ID3D12DescriptorHeap, D3D12_DESCRIPTOR_HEAP_DESC>(m_Heap.Get()).NumDescriptors;")

patch_file("src/Private/D3D12/DescriptorHeap.cpp",
           "return m_Heap->GetDesc().Type;",
           "return GetDescHelper<ID3D12DescriptorHeap, D3D12_DESCRIPTOR_HEAP_DESC>(m_Heap.Get()).Type;")

patch_file("src/Private/D3D12/DescriptorHeap.cpp",
           "return m_Heap->GetDesc().Flags;",
           "return GetDescHelper<ID3D12DescriptorHeap, D3D12_DESCRIPTOR_HEAP_DESC>(m_Heap.Get()).Flags;")

patch_file("src/Private/D3D12/GPUResource.cpp",
           "return m_Resource->GetDesc1();",
           "return GetDesc1Helper<ID3D12Resource2, D3D12_RESOURCE_DESC1>(m_Resource.Get());")

patch_file("src/Private/D3D12/GPUResource.cpp",
           "D3D12_RANGE range{ 0, m_Resource->GetDesc().Width };",
           "D3D12_RANGE range{ 0, GetDescHelper<ID3D12Resource2, D3D12_RESOURCE_DESC>(m_Resource.Get()).Width };")

# 12. ShaderBinding.h and ShaderHash.h (missing cstring)
patch_file("src/Public/D3D12/Shader/ShaderBinding.h", "#pragma once", "#pragma once\n#include <cstring>")
patch_file("src/Public/D3D12/Shader/ShaderHash.h", "#pragma once", "#pragma once\n#include <cstring>")

# 13. Copy mock_pix3.h to WinPixEventRuntime/pix3.h to mock PIX on non-MSVC platforms
mock_src = os.path.join(os.path.dirname(__file__), 'mock_pix3.h')
dest_dir = os.path.join('src', 'Public', 'WinPixEventRuntime')
os.makedirs(dest_dir, exist_ok=True)
import shutil
shutil.copy(mock_src, os.path.join(dest_dir, 'pix3.h'))
print(f"Copied mock_pix3.h to {os.path.join(dest_dir, 'pix3.h')}")

