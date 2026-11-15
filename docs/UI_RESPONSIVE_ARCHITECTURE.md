
### 7.3 Standard layout

Standard screens can use:

- A dashboard with a small number of summary panels.
- Bottom navigation where appropriate.
- One-column lists and detail views.
- More descriptive labels.
- Comfortable spacing.
- Visible scan controls and radio status.

The ThingPulse 320x480 display is the initial standard/compact validation
target. Its final layout class is determined using usable logical
dimensions and configured breakpoints.

### 7.4 Expanded layout

Expanded screens can use:

- Two-column dashboard composition.
- Navigation rail.
- Side-by-side summaries and detail panels.
- Larger device lists.
- More visible scan and inventory status.
- Persistent secondary information where useful.

Tablet displays are expected to use this layout when their dimensions
provide sufficient usable space.

### 7.5 Wide layout

Wide screens can use:

- Sidebar navigation.
- Three-column dashboard composition where content supports it.
- Persistent summary and detail regions.
- Wider tables and analysis views.
- Side-by-side device and detail panels.
- A maximum content width where full-width content would become difficult
  to scan.

Large displays should use their available space to improve information
visibility and workflow efficiency, not merely enlarge every control.

## 8. Input adaptation

Display geometry does not imply input capability.

A screen must inspect supplied capabilities before depending on touch,
swipe gestures, physical buttons, or rotary input.

Every essential navigation action must have an accessible interaction path.
Swipe must not be the only way to reach an essential screen.

The initial DisplayCapabilities model reports input availability. A future
input abstraction will translate hardware events into application-level
navigation and action events.

### 8.1 Touch interaction

Touch controls must remain usable at compact resolutions.

The initial layout profiles provide minimum recommended touch-target
dimensions. Actual controls must also account for spacing between adjacent
targets to reduce accidental activation.

### 8.2 Physical controls

When a target provides physical buttons or rotary input, the UI should be
able to use those controls for navigation and selection.

The application must not assume that every target has a touchscreen.

### 8.3 Gesture interaction

Swipe and gesture navigation may improve wearable usability, but gestures
must not be the only way to access essential functions.

Where practical, provide visible navigation or an alternative physical
input action.

## 9. Theme integration

Theme values are independent of layout selection.

The ThemeManager owns:

- Background and surface colors.
- Primary, secondary, and accent colors.
- Text colors and status colors.
- Typography.
- Component styling.
- Borders and corner radii.
- Visual density and spacing preferences.
- Wireless signal and security indicator colors.

Layout profiles own composition, spacing recommendations, and navigation
patterns.

A screen must not hardcode theme colors or create its own independent theme.

Initial theme presets:

- Dark
- Light
- High Contrast
- Recon Green

Theme selection must support runtime changes without rebooting the device.

Theme persistence through ESP32 NVS/Preferences will be implemented in a
later step.

Future customization may allow users to change colors and typography
without changing application screen logic.

The LVGL backend translates theme values into LVGL styles. A future native
LovyanGFX backend may translate the same theme into drawing operations.

## 10. UI backend independence

The responsive foundation must not depend on LVGL or LovyanGFX.

The application and screen models provide content and state. The selected
backend renders that content using the active layout and theme.

The initial backend is LVGL 9.

A future native LovyanGFX backend may draw the application UI directly
without LVGL.

The two rendering strategies do not need identical internal widget models.
They must honor the same:

- Application state.
- Navigation semantics.
- Layout decisions.
- Theme data.
- User interaction contracts.

`LVGLManager` should remain responsible for LVGL integration rather than
becoming the owner of all application screens and navigation logic.

The existing display and touch hardware interfaces remain below the
rendering backend.

## 11. Memory and performance

The initial responsive foundation:

- Uses fixed-size value types.
- Performs no dynamic allocation.
- Does not create LVGL objects.
- Does not change display draw-buffer allocation.
- Does not change radio scanner behavior.
- Does not add fonts, images, or LVGL widgets.

The existing partial display buffer and DMA completion behavior remain
unchanged.

Responsive layout selection should be inexpensive and should not run
unnecessarily on every application loop iteration.

The selected profile should be recalculated when display capabilities
change, such as after a supported orientation change or display
reconfiguration.

## 12. Validation strategy

Responsive behavior will be validated in stages.

### Stage A: Native logic validation

Test profile selection for:

- Invalid dimensions.
- 240x320 compact display.
- 320x480 display.
- Explicit wearable form factor.
- Square wearable display.
- Circular wearable display.
- Tablet form factor.
- Expanded display dimensions.
- Wide display dimensions.
- Safe insets reducing usable dimensions.
- Displays with and without touch.
- Displays with physical buttons or rotary input.

### Stage B: ThingPulse hardware

Verify:

- Existing display initialization remains unchanged.
- Existing touch input remains functional.
- The responsive profile is selected correctly.
- Existing LVGL flush behavior is unchanged.
- No regression in Wi-Fi or BLE scanning.
- Memory usage remains within the validated baseline.

### Stage C: ELEGOO hardware

Verify the compact layout on the 240x320 display after the display and
touch-controller configuration is validated.

Verify:

- Text readability.
- Touch-target usability.
- Navigation behavior.
- Device-list pagination.
- Screen refresh performance.
- Available internal RAM and flash usage.

### Stage D: Future watch

Once the watch model is identified, define its exact display dimensions,
shape, controller, input hardware, safe-area requirements, and available
memory.

Select `FormFactor::Wearable` and tune the wearable layout against the
actual device.

### Stage E: Tablet and large display

Test expanded and wide layouts on representative resolutions and input
methods.

Confirm that additional space improves information visibility without
creating excessive empty space, overly long scan paths, or difficult
navigation.

## 13. Non-regression requirements

Responsive UI implementation must not:

- Change Wi-Fi scanning behavior.
- Change BLE scanning behavior.
- Change WirelessCoordinator lifecycle behavior.
- Change DeviceInventory ownership or semantics.
- Change ObservationQueue behavior.
- Change the validated display driver's DMA handling.
- Increase the existing draw-buffer allocation without measurement.
- Enable additional LVGL fonts or widgets without measuring flash and RAM.
- Introduce board-specific dependencies into application-level UI code.

## 14. Current limitations

- The actual ESP32 watch model and display dimensions are not yet known.
- Circular safe-area geometry is initially represented through safe insets.
- Input event normalization is a later implementation step.
- No application screens consume these profiles yet.
- Breakpoints are initial values and require usability validation.
- Tablet and large-display hardware targets have not yet been selected.
- The native LovyanGFX UI backend is a future implementation.

These limitations are intentional. The current phase establishes a stable,
portable responsive contract before screen implementation begins.

## 15. Related documentation

- [Main Architecture](ARCHITECTURE.md)
- [Phase 6 UI Implementation](PHASE6_UI_IMPLEMENTATION.md)
- [Wi-Fi Architecture](WIFI_ARCHITECTURE.md)
- [Bluetooth Architecture](BLUETOOTH_ARCHITECTURE.md)
- [Radio Lifecycle](RADIO_LIFECYCLE.md)
- [Development Guide](DEV.md)

## 16. Implementation status

This document defines the responsive UI architecture and its design
requirements.

The initial source foundation is expected to include:

- `include/ui/display/DisplayCapabilities.h`
- `include/ui/layout/Breakpoints.h`
- `include/ui/layout/LayoutProfile.h`
- `include/ui/layout/ResponsiveLayout.h`

The foundation must pass native logic tests before being integrated into
the application UI.

The document must be updated as implementation decisions change. Any
deviation from these design requirements should be documented with its
reason and impact on supported display targets.
