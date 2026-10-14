---
name: MyWallpaper Animations des fenêtres
description: Observed styling of the native sample window; settings inherit MyWallpaper's existing renderer.
colors:
  preview-canvas: "#182436"
  preview-heading: "#E8F1FA"
  preview-copy: "#A5C2DA"
  preview-bar-1: "#46A7D8"
  preview-bar-2: "#54ACCF"
  preview-bar-3: "#62B1C6"
  preview-bar-4: "#70B6BD"
  preview-bar-5: "#7EBBB4"
  preview-bar-6: "#8CC0AB"
  preview-bar-7: "#9AC5A2"
typography:
  title:
    fontFamily: "Segoe UI"
    fontSize: "22px"
    fontWeight: 400
  body:
    fontFamily: "Segoe UI"
    fontSize: "16px"
    fontWeight: 400
rounded:
  animated-capture: "12px"
spacing:
  preview-content-inset: "40px"
  preview-title-top: "32px"
  preview-caption-top: "95px"
  preview-bar-width: "56px"
  preview-bar-pitch: "74px"
  preview-bottom-inset: "40px"
---

# Design System: MyWallpaper Animations des fenêtres

## Overview

This add-on extends MyWallpaper's usual settings surface and uses the host's native-only settings renderer. It has no separate settings identity. “Fluide et expressif” is the chosen motion default; it describes the effect preset, not a visual theme.

The native sample canvas is defined in src/preview.cpp. The supplied PNGs are outputs from the add-on Render pass, based on that captured window image, under Windows 11 build 26200 at 96 DPI. They show animated image output after the add-on rounded mask and shadow handling, with optional contour processing; the qualified runtime frames use contour off. The caption chrome in the source image is Windows-drawn. They do not show the MyWallpaper sidebar, settings controls, or the preview controller and its buttons.

**Key Characteristics:**
- Settings appearance is inherited from MyWallpaper; no add-on-specific settings palette or control styling was observed.
- The sample canvas uses a deep navy field, cool light text, and a blue-to-sage bar series.
- Windows supplies the caption chrome; the add-on renderer applies a rounded mask and shadow and can add a configurable contour to animation output.

## Colors

Every color token below applies to content painted inside the native test canvas. The inherited settings UI colors were not captured.

### Neutral
- **Preview canvas** (preview-canvas): dark fill behind the sample text and bars.
- **Preview heading** (preview-heading): cool near-white title text.
- **Preview copy** (preview-copy): muted blue-gray supporting text.

### Secondary
- **Sample bar sweep** (preview-bar-1 through preview-bar-7): seven discrete steps from blue through teal to muted sage, used only by the sample chart.

## Typography

**Display Font:** Not observed in the settings UI.
**Body Font:** Segoe UI, as explicitly selected for the native preview.

### Hierarchy
- **Title** (typography.title): normal-weight Segoe UI at the size recorded in frontmatter; used for the sample heading.
- **Body** (typography.body): normal-weight Segoe UI at the size recorded in frontmatter; used for the two-line sample caption.

The source sets no line-height, letter-spacing, or other type roles. Windows draws the outer caption text.

## Layout

The sample uses fixed client-area coordinates. The title and caption use their named top offsets; the text and bars share the recorded content inset. The chart has seven columns with the recorded width and pitch, and ends at the recorded bottom inset. Bar tops step vertically by a fixed ten-pixel increment. These measurements belong to the test canvas, not to MyWallpaper's settings layout; no settings breakpoints or spacing values were observed.

## Elevation & Depth

The caption chrome is Windows-drawn into the captured source image. The add-on Render pass then applies a rounded pixel mask, can draw a configured contour, and constructs a shadow from rendered alpha with a box blur and downward offset. The qualified runtime test explicitly sets cornerRadius to the manifest default recorded by animated-capture. It inherits manifest defaults of shadowSize 24 px and shadowOpacity 22%. borderStyle defaults to none and borderWidth to 2 px, so no add-on contour is drawn in these frames. At 96 DPI, the default shadow configuration yields a 12 px box-blur radius and a 6 px downward offset. The PNG alpha halo is Render output, not a DWM-only shadow measurement.

## Shapes

Windows provides the source caption frame. On animation frames, Render applies a rounded mask using cornerRadius; the qualified test sets it to the manifest default recorded by animated-capture. A separate contour is configurable, but these frames use borderStyle none, so the default 2 px borderWidth is inactive. The preview content itself uses rectangular bars and no drawn card or input shapes.

## Components

### Settings surface
The manifest defines grouped settings with native boolean, range, select, text, and color controls. MyWallpaper supplies their renderer and device snapshots. This mission did not render that surface, so its colors, dimensions, typography, focus treatment, and other control states are undocumented here.

### Native test window
The PNGs are output frames from Render, derived from the Win32 sample window; they are not raw screenshots of the canvas. The source caption chrome is Windows-drawn, while the animation mask and default shadow are add-on-rendered. The source also creates a separate controller using standard Windows BUTTON controls; those buttons do not appear in the supplied evidence/native-frames/*.png captures. No HTML or CSS representation is appropriate for these native controls.

## Do's and Don'ts

### Do:
- **Do** keep the preview tokens scoped to the native test canvas.
- **Do** treat settings appearance as inherited from MyWallpaper until the host surface is visually observed.
- **Do** distinguish Windows-drawn caption chrome from the add-on-rendered mask, optional contour, and shadow.

### Don't:
- **Don't** present the native-frame PNGs as captures of the MyWallpaper sidebar or its controls.
- **Don't** infer settings colors, spacing, typography, hover, focus, or disabled states from the sample window.
- **Don't** represent Windows controls with invented HTML or CSS snippets.