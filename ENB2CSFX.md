# Implementation Plan - Porting ENB Post-Processing Effects to Community Shaders

> [!NOTE]
>
> * Option A: Using `hlslkit-compile`  (if available on the system):
>
> ```bash
> hlslkit-compile --shader-dir package/Shaders/ISHDR.hlsl --config .github/configs/shader-validation.yaml --validator dxc
> hlslkit-compile --shader-dir package/Shaders/Lighting.hlsl --config .github/configs/shader-validation.yaml --validator dxc
> hlslkit-compile --shader-dir package/Shaders/Water.hlsl --config .github/configs/shader-validation.yaml --validator dxc
> hlslkit-compile --shader-dir features/Bloom/Shaders/Bloom/BloomCS.hlsl --config .github/configs/shader-validation.yaml --validator dxc
> hlslkit-compile --shader-dir features/Sunsprite/Shaders/Sunsprite/SunspriteCS.hlsl --config .github/configs/shader-validation.yaml --validator dxc
> hlslkit-compile --shader-dir features/Post\ Process\ Overlay/Shaders/PostProcessOverlay/PostProcessCS.hlsl --config .github/configs/shader-validation.yaml --validator dxc
> ```
>
> ***
>
> Option B: Using raw `dxc` compiler (standard DXC tool on Linux):
>
> ```bash
> dxc -T ps_5_0 -E main -Fo /dev/null package/Shaders/ISHDR.hlsl
> dxc -T ps_5_0 -E main -Fo /dev/null package/Shaders/Lighting.hlsl
> dxc -T ps_5_0 -E main -Fo /dev/null package/Shaders/Water.hlsl
> dxc -T cs_5_0 -E main -Fo /dev/null features/Bloom/Shaders/Bloom/BloomCS.hlsl
> dxc -T cs_5_0 -E main -Fo /dev/null features/Sunsprite/Shaders/Sunsprite/SunspriteCS.hlsl
> dxc -T cs_5_0 -E main -Fo /dev/null features/Post\ Process\Overlay/Shaders/PostProcessOverlay/PostProcessCS.hlsl
> ```
>
> ***
>
> Build the project preset  ALL  using the  pwsh  command line tool:
>
> ```bash
> pwsh -Command "./BuildRelease.bat ALL"
> ```

This document outlines the step-by-step implementation plan to port core ENB screen-space post-processing effects (Sunsprite, Underwater, Adaptation, Bloom, Lens, and Postpass) into the Skyrim Community Shaders (CS) ecosystem.

Each implementation is designed to align with CS's core philosophy:

* **Modularity**: Separate features that can be toggled independently.
* **Performance-First**: Offloading processing to compute shaders, minimizing draw calls, and avoiding redundant passes.
* **Engine Integration**: Utilizing existing engine values (like imagespace settings) and CS ecosystem features (like `SharedData` parameters).
* **Physical Accuracy**: Leveraging PBR features (such as emissive properties) to govern screen-space effects.

***

### 1. Physically-Based Bloom (`enbbloom.fx`)

Instead of copying ENB's CPU-intensive multi-target Gaussian blur loop, CS can implement a modern, physically-based emissive bloom.

### CS Leverage Points

* **Render Target Wrappers:** Leverage `Texture2D::Create` (e.g., using `DXGI_FORMAT_R16G16B16A16_FLOAT` as in `HDRDisplay.cpp`) to instantiate downsampled bloom targets, eliminating D3D11 boilerplate.
* **Emissive Integration:** Integrate with `TruePBR` / `ExtendedMaterials`. During the geometry pass, write emissive texture intensities into the **alpha channel of the HDR render target**, which can then be read during highlight extraction.
* **Settings Integration:** Bloom user-configurable settings (threshold, intensity, etc.) are integrated directly inside `SettingsUser.json` as a native feature settings object.

### Step-by-Step Plan

1. **Highlight Extraction Pass**:
   * Write a pixel shader to sample the pre-tonemapped HDR scene buffer.
   * Extract pixels exceeding a threshold. Filter using the HDR target's alpha channel (emissive strength) so glowing objects bloom, while non-glowing white textures in bright sunlight do not.
2. **Compute-Based Dual Filtering**:
   * Implement a compute shader that downsamples the highlight target iteratively to $1/2, 1/4, 1/8, 1/16$ resolution using Karis's dual filtering algorithm.
   * Leverage CS's `ShaderCache` manager to load, compile, and hot-reload this compute shader dynamically.
   * Upsample back with progressive bilinear interpolation and wide-tap blending to generate a smooth, organic bloom spread without color banding.
3. **Compositing**:
   * Blend the final blurred bloom texture back into the main HDR scene buffer before the final display mapping pass.

***

## 2. Screen-Space Lens FX (`enblens.fx`)

Reframe ENB's static lens flares to react dynamically to all light sources (magic, fires, torches) and adapt to environmental exposure.

### CS Leverage Points

* **Feature Pipeline Dependency:** Integrate Lens FX as a sub-pass of the Bloom feature to share render targets and avoid separate texture allocations, fitting the performance-first philosophy.
* **Exposure Adaptation Integration:** Read the adapted luminance buffer computed in `HDRDisplay` to dynamically modulate the visibility of the lens dirt texture.
* **Settings Integration:** Lens FX user settings (intensity, dirt scale) are integrated inside the Bloom settings object in `SettingsUser.json`.

### Step-by-Step Plan

1. **Diffraction Glare Pass**:
   * Run a directional radial/anamorphic blur pass on the extracted bloom highlights.
   * Combine multiple offset/rotated blurs to create diffraction spikes (starburst glare) around light sources.
2. **Dynamic Lens Dirt**:
   * Load a high-resolution lens dirt texture (e.g. `lensmask.png`) using the CS filesystem loader.
   * Sample the dirt texture and multiply it by the bloom highlight texture.
   * **Exposure Modulation**: Scale the visibility of the lens dirt based on the current scene exposure and eye adaptation value (from `HDRDisplay` / adaptation pass).
3. **Additive Blending**:
   * Additively composite the glare and dirt contributions over the final frame.

***

## 3. Eye Adaptation FX (`enbadaptation.fx`)

Align eye adaptation directly with the HDR pipeline to dynamically adjust exposure based on the player's focus.

### CS Leverage Points

* **Engine Texture Hooking:** Instead of writing a custom downsampling chain from scratch, hook and read Skyrim's built-in `AvgTex` (average luminance texture at register `t2`) and `AdaptTex` (adaptation texture at register `t1`) as mapped in `ISHDR.hlsl`.
* **C++ Pipeline Integration:** Feed the exposure factors directly into `HDRDisplay::HDRDataCB` (Constant Buffer 0) to scale the final output.

### Step-by-Step Plan

1. **Engine Integration & Hooking**:
   * Intercept and copy the values from `AvgTex` and `AdaptTex` during the presentation phase.
2. **Temporal Integration**:
   * Read the engine-calculated average luminance and apply a custom temporal adjustment/smoothing pass if the user configures custom adaptation speeds in the CS settings.
3. **Exposure Multiplication**:
   * Feed the adapted luminance value directly into the `HDRDisplay` display mapping shader to scale exposure dynamically.

***

## 4. Sunsprite FX (`enbsunsprite.fx`)

Leverage CS's existing sun tracking features to render dynamic sun flares that respond to occlusion by trees, buildings, and mountains.

### CS Leverage Points

* **Sun Tracking:** Leverage `SkySync::CalculateSunDirectionAndDistance` to retrieve the sun direction vector, or read `SharedData::DirLightDirection` from `SharedData.hlsli` in the shader.
* **VR Stereoscopic Alignment:** Use the `Stereo` namespace (specifically `Stereo::ConvertToStereoUV`) to ensure the depth occlusion and coordinates are correctly calculated for both eye viewports in VR mode.
* **Depth Buffer:** Sample the engine's main depth buffer using `SharedData::DepthTexture` (register `t17`) to compute real-time sun occlusion.
* **Settings Integration:** Sunsprite user-configurable settings are integrated directly inside `SettingsUser.json` as a native feature settings object.

### Step-by-Step Plan

1. **Sun Tracking**:
   * Retrieve the sun direction vector from `SharedData::DirLightDirection.xyz`.
   * Project the 3D world-space sun direction vector onto the screen coordinate space.
2. **Occlusion Check**:
   * Check if the sun is visible by sampling `SharedData::DepthTexture` around the projected sun coordinates.
   * Ensure the depth texture and UV coordinates are properly converted for VR mode using CS's built-in `Stereo::ConvertToStereoUV`.
   * If the sampled depth is less than the sun's distance, calculate a partial occlusion factor (e.g., to fade sprites out behind trees).
3. **Sprite Rendering**:
   * Draw flare elements (using textures like `enbsunsprite.png`) aligned along the vector from the sun's position to the center of the screen.
   * Multiply the opacity by the occlusion factor and light intensity.

***

## 5. Underwater FX (`enbunderwater.fx`)

Rather than creating a brand-new standalone feature, integrate underwater screen-space post-processing directly with existing water features to maintain a clean codebase.

### CS Leverage Points

* **Codebase Consolidation:** Append the underwater screen-space post-processing logic directly inside `UnifiedWater` or `WaterEffects`.
* **Resource Reuse:** Reuse the noise and normal map textures already bound by the water system to calculate screen-space UV offsets, saving VRAM.

### Step-by-Step Plan

1. **Integration with `UnifiedWater` / `WaterEffects`**:
   * Check the player state using the engine's native underwater detection.
2. **Wavy Screen Distortion**:
   * Apply screen-space UV offsets inside the water rendering post-pass using the same scrolling normal/noise textures already loaded by the water system.
3. **Underwater Fog & Color Grading**:
   * Blend a deep water absorption color over the frame based on the screen depth buffer.
   * Fade distant objects into a thick volumetric fog to simulate limited underwater visibility.

***

## 6. Extensible Effect Postpass (`enbeffectpostpass.fx`)

Rather than forcing preset authors to write custom shaders for basic settings, CS isolates standard post-processing effects into native, first-class features. The custom postpass shader loader acts strictly as a **global overwrite / extensibility mechanism** for advanced customization.

### CS Leverage Points

* **Feature Settings Lifecycle:** Use the standard `Feature` base class lifecycle (`LoadSettings`/`SaveSettings`/`DrawSettings` using `nlohmann::json` serialization wrappers) to manage Vignette, Letterbox, Adaptation, and DoF configuration parameters out-of-the-box.
* **Shader Cache Compilation:** Leverage the `ShaderCache` class to handle runtime HLSL compilation and hot-reloading for the custom `PostProcess.hlsl` loader.

### Step-by-Step Plan

1. **Leverage Native Feature Isolation**:
   * **Vignette & Letterbox**: Handled out-of-the-box by the built-in `PostProcessOverlay` feature. Authors configure these natively via `SettingsUser.json` parameters (`EnableVignette`, `VignetteAmount`, `EnableLetterbox`, `LetterboxHeight`) without writing shader code.
   * **Exposure & Adaptation**: Processed natively inside the `HDRDisplay` feature.
   * **Depth of Field**: Handled natively by the `DepthOfField` feature.
   * **Sharpening**: Processed natively by the `Upscaling` feature.
2. **Global Postpass Overwrite Hook (Advanced)**:
   * Provide an optional, advanced C++ shader loader in CS that checks for a user-provided `PostProcess.hlsl` file in the shaders directory.
   * If present, this custom shader acts as a **global overwrite/extension pass** executed at the very end of the post-processing chain.
   * Expose the core input textures (Scene, Depth, Bloom) and parameters (Adaptation, Timer) to the custom pass so authors can implement unique custom grading or filter passes that are not supported by the built-in modules.
3. **Performance Profiling**:
   * Ensure the C++ hook is optimized and integrates with the CS Performance Overlay to profile custom shader code execution times.
