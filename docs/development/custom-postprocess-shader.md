# Writing a Custom Postprocess Shader

The **Post Process Overlay** feature can load and run a user-supplied compute
shader from your `Data` folder, completely separate from the shaders shipped
with Community Shaders. This lets you (or end users) drop in their own
screen-space effect — color grading, film grain, custom distortion, overlays,
debug visualizations, etc. — without building a plugin or shipping a feature.

This is the same mechanism end users see as **"Enable Custom Postpass
(PostProcess.hlsl)"** in the Post Process Overlay settings.

## Quick start

1. Create the file **`Data\Shaders\PostProcess.hlsl`** in your Skyrim
   installation (next to the `Data\Shaders\PostProcessOverlay\` folder that
   ships with CS — note the file is directly in `Shaders`, not in a
   subfolder).
2. Write a `cs_5_0` compute shader with an entry point named `main` (see the
   contract below).
3. In-game, open the CS menu → **Post Process Overlay** → check **"Enable
   Custom Postpass (PostProcess.hlsl)"**.
4. CS compiles the file the first time the checkbox is enabled (or the cache
   is cleared) and dispatches it every frame, right before the built-in
   Letterbox/Vignette pass and Skyrim's UI is drawn.

If the file doesn't exist, CS logs `PostProcess.hlsl not found at
Data\Shaders\PostProcess.hlsl` and silently skips the pass. If it fails to
compile, CS logs the HLSL compiler error/warning text and skips the pass —
nothing crashes, and the rest of the overlay (vignette, letterbox, underwater
distortion) still runs normally. Check `CommunityShaders.log` (or the in-game
log viewer) for either message.

## Shader contract

Your file must compile as **Shader Model 5.0 compute** (`cs_5_0`) with an
entry point function named **`main`**:

```hlsl
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    ...
}
```

CS dispatches it once per frame with thread groups sized to cover the full
render target at an **8×8** group size:

```cpp
dispatchX = (width + 7) / 8;
dispatchY = (height + 7) / 8;
context->Dispatch(dispatchX, dispatchY, 1);
```

so `[numthreads(8, 8, 1)]` is the layout the dispatch math assumes — using a
different group size still compiles and runs, but you'll need to do your own
bounds math against the actual dispatch grid. The standard bounds check is:

```hlsl
uint width, height;
OutputTex.GetDimensions(width, height);
if (dispatchThreadID.x >= width || dispatchThreadID.y >= height)
    return;
```

## Available resources

CS binds the following slots before dispatching your shader. You don't have
to declare/use all of them — HLSL will happily compile with unused registers
— but anything you do declare must match these bindings exactly (slot,
type, and dimensionality), or the compile/dispatch will fail or read garbage.

| Slot | Resource | Type | Notes |
| ---- | -------- | ---- | ----- |
| `b0` | Postprocess params | `cbuffer` | See layout below — the same buffer the built-in Vignette/Letterbox shader uses |
| `b5` | Shared frame data | `cbuffer` | CS's per-frame `SharedDataCB` (camera, lighting, water, timer, etc.) — only bound if `globals::state->sharedDataCB` exists, which is always true once CS has finished initializing |
| `t0` | *(reserved)* | `Texture2D` | Currently always bound `nullptr` — reserved for future use, do not rely on it |
| `t1` | Scene depth | `Texture2D<float>` | Current scene depth buffer SRV (`Util::GetCurrentSceneDepthSRV(true)`, prefers 16-bit); `nullptr` if unavailable |
| `t2` | Bloom texture | `Texture2D<float4>` | The Bloom & Lens feature's bloom accumulation SRV; `nullptr` if Bloom isn't loaded/enabled |
| `u0` | Output / scene color | `RWTexture2D<float4>` | The final composited frame, read-write **in place** — read your input color from here and write your result back to the same texel |

All three SRV slots are explicitly cleared (`nullptr`) again immediately after
your dispatch, and the compute shader stage itself is unbound — you don't
need to (and shouldn't try to) clean up bindings yourself.

### `b0` — Postprocess params

```hlsl
cbuffer Params : register(b0)
{
    float letterboxHeight;  // current "Letterbox Height" setting [0, 0.5]
    float vignetteAmount;   // current "Vignette Amount" setting [0, 5]
    float enableLetterbox;  // 1.0 if Letterbox is enabled, else 0.0
    float enableVignette;   // 1.0 if Vignette is enabled, else 0.0
    float timer;            // running engine timer in seconds (globals::state->timer); good for animation
    float3 padding;
};
```

This is the exact same constant buffer the built-in
`PostProcessOverlay/PostProcessCS.hlsl` uses (see that file for a worked
example of reading it). You're not limited to using these values for
letterbox/vignette — `timer` in particular is generic and useful for any
time-based animation in your own effect.

### `b5` — Shared frame data (`SharedDataCB`)

A larger struct with general per-frame rendering state, defined in
`src/State.h`. Useful members for postprocessing:

```hlsl
cbuffer SharedData : register(b5)
{
    float4 WaterData[25];
    float3x4 DirectionalAmbient;
    float4 DirLightDirection;
    float4 DirLightColor;
    float4 CameraData;
    float4 BufferDim;               // render target dimensions
    float Timer;
    uint FrameCount;
    uint FrameCountAlwaysActive;
    uint InInterior;
    uint HasDirectionalShadows;
    uint InMapMenu;
    uint HideSky;
    float MipBias;
    float WaterSystemHeight;
    uint EffectFlags;
    float AdaptationSpeedLightToDark;
    float AdaptationSpeedDarkToLight;
    float4 AmbientSHR;
    float4 AmbientSHG;
    float4 AmbientSHB;
    float4 HDRData;
};
```

Match the field order/types exactly if you declare this struct yourself (HLSL
constant buffers are laid out by declaration order with 16-byte vector
packing — `src/State.h`'s `SharedDataCB` is the source of truth, including
the comments documenting each field's exact meaning/encoding).

> Tip: rather than retyping the whole struct, you can `#include` a copy of it
> from a shared location, or just declare the handful of fields you actually
> need followed by enough padding to keep later fields' offsets correct — but
> the simplest and most robust approach is to copy the full struct verbatim.

### `t1` / `t2` — depth and bloom

```hlsl
Texture2D<float> DepthTex : register(t1);
Texture2D<float4> BloomTex : register(t2);
SamplerState LinearSampler; // declare your own if you need to sample — none is bound for you
```

Both can be `nullptr` (e.g., Bloom disabled, or depth unavailable in some edge
cases) — a `nullptr` SRV bound to a `Texture2D` register reads as all-zero in
the shader, it will not crash, but you should not assume either is populated
with meaningful data. Treat them as optional enhancements.

Depth is in Skyrim's standard reversed/non-linear device depth — you'll
typically want to reconstruct view/world position from it using the camera
data in `SharedDataCB` (see `package/Shaders/Common/` for CS's own
depth-reconstruction helpers if you want to mirror their conventions, though
note that `Common/` headers are written for CS's internal include system and
may need adapting for a standalone file).

### `u0` — output

`OutputTex` is the fully-composited HDR/SDR scene color, immediately before
Skyrim's own UI/menus are drawn on top. Read your input with
`OutputTex[dispatchThreadID.xy]`, transform it, and write the result back to
the same texel — there is no separate input texture, this is an in-place
read-modify-write target.

## Pipeline position

Within `PostProcessOverlay::Present()`, passes run in this fixed order, all
operating on the same `outputUAV` in place:

1. **Underwater distortion** (if enabled) — warps the frame while the camera
   is submerged
2. **Your custom postpass** (`PostProcess.hlsl`, if enabled and compiled)
3. **Built-in Vignette / Letterbox** (if either is enabled)

So your shader sees the post-distortion frame as input, and anything you draw
will itself be vignetted/letterboxed afterward if those are also enabled —
plan your effect's compositing accordingly (e.g., if you're drawing a
full-screen overlay that shouldn't be vignetted, you may prefer to disable
the built-in Vignette and replicate it yourself from `b0`).

## Compilation & reloading

-   The shader is compiled **once**, lazily, the first time
    `EnablePostpass` is true and the pass runs — not on every frame.
-   Unchecking **"Enable Custom Postpass"** releases the compiled shader
    immediately (so re-enabling it recompiles from the current file on disk).
-   Clearing the shader cache (CS menu → Advanced → shader cache controls, or
    the boot-time clear-cache prompt) also releases it and forces a
    recompile on next use.
-   There is **no file-watcher / hot-reload** for `PostProcess.hlsl` — unlike
    CS's own shaders under `Advanced → Use FileWatcher`, editing the file on
    disk does not trigger an automatic recompile. Toggle the checkbox off and
    on (or clear the shader cache) to pick up changes while iterating.
-   Compilation always uses the standard `cs_5_0` profile via
    `Util::CompileShader` with the `COMPUTESHADER`, `WINPC`, `DX11` (and, on
    VR builds, `VR`) preprocessor defines automatically defined — the same
    baseline every CS compute shader gets. Your file does not need to (and
    cannot) opt out of these.

## Example: a simple animated tint

```hlsl
cbuffer Params : register(b0)
{
    float letterboxHeight;
    float vignetteAmount;
    float enableLetterbox;
    float enableVignette;
    float timer;
    float3 padding;
};

RWTexture2D<float4> OutputTex : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint width, height;
    OutputTex.GetDimensions(width, height);

    if (dispatchThreadID.x >= width || dispatchThreadID.y >= height)
        return;

    float4 color = OutputTex[dispatchThreadID.xy];

    // Slowly cycle a subtle color tint over time, just to prove the pass runs.
    float3 tint = 0.5 + 0.5 * cos(timer * 0.5 + float3(0.0, 2.0, 4.0));
    color.rgb = lerp(color.rgb, color.rgb * tint, 0.15);

    OutputTex[dispatchThreadID.xy] = color;
}
```

Drop this in as `Data\Shaders\PostProcess.hlsl`, enable the checkbox, and
you should see a slow color cycle over the whole screen.

## Debugging tips

-   **Log first.** Compile errors/warnings are written to `CommunityShaders.log`
    at `warn`/`debug` level with the full HLSL compiler output — this is the
    fastest way to find a typo or register mismatch.
-   **RenderDoc.** CS includes a built-in RenderDoc capture feature (CS menu
    → RenderDoc) — capture a frame and look for your dispatch (it shows up as
    an unnamed `cs_5_0` dispatch writing to the swapchain-sized UAV,
    immediately before the built-in PostProcessCS dispatch when both run) to
    inspect bound resources, constant buffer values, and the output texture
    directly.
-   **Isolate first.** Start from the tint example above to confirm the file
    loads and the pass executes before adding complex sampling/reconstruction
    logic — that way a black screen or compile failure is easy to attribute.
-   **Mind partial precision / strict mode.** Like all CS shaders, yours is
    compiled with `D3DCOMPILE_ENABLE_STRICTNESS` (and optionally
    `D3DCOMPILE_PARTIAL_PRECISION` / `D3DCOMPILE_AVOID_FLOW_CONTROL`,
    depending on the user's Advanced settings) — code that compiles loosely
    elsewhere may warn or fail here.

## Limitations

-   Only **one** custom postpass file is supported (`PostProcess.hlsl`); there
    is no mechanism for loading multiple user shaders or chaining several
    custom passes.
-   No `#include` support beyond what the standard HLSL compiler/include
    handler provides for relative paths — you cannot `#include` CS's internal
    `Common/` headers without copying them alongside your file (and adapting
    any CS-specific include-path assumptions they make).
-   No way to pass additional user-configurable parameters from the CS menu
    into your shader — your only per-frame inputs are the fixed `b0`/`b5`
    buffers and the bound textures described above. If you need user-tunable
    parameters, bake them into constants in the file itself and have users
    edit the file directly (combined with the lack of hot-reload, this means
    a toggle-off/toggle-on cycle to apply changes).
