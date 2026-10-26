# Explicit value-type chooser

The independent C UI chooser retains only presence, ManualValueKind and NumberType plus presentation focus/active state. It never reads, copies, parses, coerces or changes raw/canonical value text. The App/editor producer remains responsible for target fences, strict model validation, budgets, actual edits and history.

Choices are Text, SQL NULL, Integer and Decimal. Integer exposes signed/unsigned and 8/16/32/64/128-bit widths. Decimal exposes precision 1–38 and scale 0–precision. Float is unsupported. Clear is a distinct explicit intent represented by sparse absence, not a stored type; SQL NULL and deliberate empty Text remain present values. Initial invalid metadata preserves the previous chooser. Invalid state cannot Apply; rejected field transitions preserve prior metadata. Reducing precision below scale rejects instead of silently changing scale.

Apply always emits metadata, even when the selected type is unchanged, because only the App knows whether the raw draft changed or a deliberate empty Text command is intended. Cancel/invalid Apply preserve the caller's output. Pure focus traversal skips unavailable integer/decimal fields and disabled Apply. No pointer to a source/model/value is retained.

The registered `value-type` pure release/sanitizer plan passes in 4.531 seconds (`20261026T024421-feature-24959`), including strict shared-model validation for all supported numeric descriptors, absence/NULL/Text distinctions, invalid metadata, limits, focus navigation and cancellation. All 15 catalogue contracts pass. It uses main's genuine `d9aca408` prerequisite mapped privately as `44588219`; root must skip that dependency copy on integration. Drawing, physical input and App/editor activation remain separate children.
