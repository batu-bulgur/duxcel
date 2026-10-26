# Explicit value-type chooser

The independent C UI chooser retains only presence, ManualValueKind and NumberType plus presentation focus/active state. It never reads, copies, parses, coerces or changes raw/canonical value text. The App/editor producer remains responsible for target fences, strict model validation, budgets, actual edits and history.

Choices are Text, SQL NULL, Integer and Decimal. Integer exposes signed/unsigned and 8/16/32/64/128-bit widths. Decimal exposes precision 1–38 and scale 0–precision. Float is unsupported. Clear is a distinct explicit intent represented by sparse absence, not a stored type; SQL NULL and deliberate empty Text remain present values. Initial invalid metadata preserves the previous chooser. Invalid state cannot Apply; rejected field transitions preserve prior metadata. Reducing precision below scale rejects instead of silently changing scale.

Apply always emits metadata, even when the selected type is unchanged, because only the App knows whether the raw draft changed or a deliberate empty Text command is intended. Cancel/invalid Apply preserve the caller's output. Pure focus traversal skips unavailable integer/decimal fields and disabled Apply. No pointer to a source/model/value is retained.

The registered `value-type` pure release/sanitizer plan passes in 4.531 seconds (`20261026T024421-feature-24959`), including strict shared-model validation for all supported numeric descriptors, absence/NULL/Text distinctions, invalid metadata, limits, focus navigation and cancellation. All 15 catalogue contracts pass. It uses main's genuine `d9aca408` prerequisite mapped privately as `44588219`; root must skip that dependency copy on integration. Drawing, physical input and App/editor activation remain separate children.

## Standalone widget boundary

`value_type_view.h` supplies bounded geometry and an independent existing-widget modal. Text/NULL/Integer/Decimal choices and the explicit Clear value action remain metadata only. Integer sign/width and Decimal precision/scale controls use existing buttons and focus rings. Apply/Cancel, Enter/Escape, Tab/Shift-Tab and choice/field arrows emit only metadata/events. A passive draw does not dequeue navigation or change chooser/output. Invalid metadata disables Apply, while cancellation remains available.

The three-module pure release/sanitizer plan passes in 4.735 seconds (`20261026T025046-feature-30743`), with all 15 catalogue contracts passing. Geometry covers bounded narrow layouts and 150% scale. The separate 24-stage hidden TEST fixture compiles and is queued for root's desktop window; it additionally covers actual gestures/keys, integer/decimal parameters, explicit NULL/Text/Clear, passive queue ownership and invalid Apply. Compilation is not runtime acceptance. Source owns the later editor/command wiring; this child adds no App code or runtime activation.
