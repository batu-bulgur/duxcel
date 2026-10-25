CC = clang
.DEFAULT_GOAL := all
# Use eight compilation slots by default, including with Apple's Make 3.81.
# BUILD_JOBS is the portable override (e.g. make BUILD_JOBS=1 for a baseline).
# Recursive makes inherit the parent's jobserver instead of creating a new pool.
BUILD_JOBS ?= 8
ifeq ($(MAKELEVEL),0)
ifeq ($(filter --jobserver%,$(MAKEFLAGS)),)
MAKEFLAGS += -j$(BUILD_JOBS)
endif
endif
CFLAGS ?= -O2 -g
WARN = -std=c99 -Wall -Wextra -Wpedantic
SANFLAGS = -fsanitize=address,undefined -fno-omit-frame-pointer
PKG_CONFIG ?= pkg-config
BREW_PREFIX := $(shell brew --prefix 2>/dev/null)
RAYLIB_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags raylib)
RAYLIB_LIBS ?= $(shell $(PKG_CONFIG) --libs raylib)
DUCKDB_PREFIX ?= $(BREW_PREFIX)
DUCKDB_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags duckdb 2>/dev/null || { test -z "$(DUCKDB_PREFIX)" || echo -I$(DUCKDB_PREFIX)/include; })
DUCKDB_LIBS ?= $(shell $(PKG_CONFIG) --libs duckdb 2>/dev/null || { test -z "$(DUCKDB_PREFIX)" || echo -L$(DUCKDB_PREFIX)/lib; }) -lduckdb
THREAD_LIBS = -pthread
ifeq ($(OS),Windows_NT)
THREAD_LIBS =
endif
CPPFLAGS += -Isrc $(DUCKDB_CFLAGS) $(RAYLIB_CFLAGS)
LDLIBS = $(RAYLIB_LIBS) $(DUCKDB_LIBS) $(THREAD_LIBS) -lm
CORE = src/workbook/workbook_width_command.c src/common.c src/platform/platform.c src/transforms/distinct.c src/transforms/analysis.c src/transforms/record_sql.c src/transforms/number.c src/transforms/pivot.c src/transforms/chart_other.c src/transforms/marks.c src/data/database.c src/data/page.c src/data/source_digest.c src/workbook/clipboard.c src/workbook/cells_tsv_scan.c src/workbook/cells.c src/workbook/format.c src/workbook/source_scope.c src/workbook/source_versions_codec.c src/workbook/objects.c src/workbook/workbook.c src/workbook/column_widths.c src/workbook/column_width_history.c src/workbook/column_width_command.c src/workbook/cell_command.c src/workbook/format_command.c src/workbook/source_format_command.c src/workbook/analysis_command.c src/workbook/object_source_plan.c src/workbook/table_command.c src/workbook/object_fence.c src/workbook/object_name_command.c src/workbook/object_presentation_command.c src/workbook/object_layout_command.c src/workbook/object_pivot_command.c src/workbook/object_binding_command.c src/workbook/object_delete_command.c src/workbook/object_create_command.c src/workbook/object_import_draft.c src/workbook/object_move_command.c src/workbook/cell_delta.c src/workbook/cell_delta_codec.c src/workbook/format_delta_codec.c src/workbook/format_dependencies.c src/workbook/save_fence.c src/workbook/history.c src/workbook/lifecycle.c src/workbook/sheet_lifecycle.c src/workbook/history_sheet_lifecycle.c src/workbook/sheet_lifecycle_command.c src/workbook/identity.c src/workbook/workbook_codec.c src/workbook/schema.c src/workbook/version_index.c src/workbook/history_anchors.c src/workbook/history_window.c src/workbook/history_path.c src/workbook/history_stream.c src/workbook/history_format.c src/workbook/analysis_asset.c src/workbook/definition_wire.c src/workbook/pivot_asset.c src/workbook/marks_asset.c src/workbook/objects_asset.c src/workbook/definition_dependencies.c src/workbook/definition_state.c src/workbook/definition_event.c src/workbook/history_definition.c src/workbook/definition_baseline.c src/workbook/definition_origin.c src/workbook/version_tag.c src/workbook/version_chunks.c src/workbook/version_codec.c src/data/worker.c src/workbook/cell_range_plan.c src/workbook/range_event.c src/workbook/history_range.c src/workbook/range_command.c src/workbook/cell_paste_plan.c
CORE += src/workbook/mixed_range_plan.c
NATIVE_PERSISTENCE = src/app/versionpreview.c src/workbook/version_preview.c src/app/versionstate.c src/data/worker_archive.c src/workbook/version_save.c src/workbook/archive_write.c src/workbook/archive_read.c src/workbook/version_restore.c src/workbook/version_read.c src/workbook/version_sql.c src/workbook/version_stage.c src/workbook/core_compare.c src/workbook/core_payload.c src/platform/directory.c src/platform/file_lease.c src/workbook/journal.c src/workbook/journal_queue.c src/workbook/journal_session.c src/workbook/recovery.c src/workbook/recovery_catalog.c src/workbook/recovery_worker.c
SRC = $(CORE) $(NATIVE_PERSISTENCE) src/ui/cellreadout.c src/ui/choiceview.c src/ui/popup.c src/ui/motion.c src/ui/fontmetrics.c src/ui/paintlayer.c src/ui/paintlayer_backend.c src/ui/ui.c src/ui/ribbon.c src/ui/ribbon_spec.c src/ui/keytips.c src/ui/flyout.c src/ui/statusview.c src/ui/dockview.c src/ui/marksview.c src/ui/workspace.c src/ui/inspector.c src/ui/statsview.c src/ui/guideview.c src/ui/recoveryview.c src/ui/versionview.c src/ui/versionpreviewview.c src/ui/stepview.c src/ui/preview.c src/ui/chartview.c src/ui/chartencoding.c src/ui/textview.c src/ui/sortview.c src/ui/brand.c src/platform/icon.c src/grid/grid.c src/grid/selection_ladder.c src/app/step_preview.c src/app/selection.c src/app/build_info.c src/app/app.c src/main.c
ifeq ($(shell uname -s),Darwin)
SRC += src/platform/macos.m src/platform/macos_about.m
LDLIBS += -framework AppKit -framework UniformTypeIdentifiers
endif
ifeq ($(OS),Windows_NT)
WINDRES ?= windres
ifneq ($(shell command -v $(WINDRES) 2>/dev/null),)
SRC += build/duxcel-icon.o
build/duxcel-icon.o: packaging/windows/duxcel.rc assets/icons/duxcel.ico
	mkdir -p build
	$(WINDRES) -i packaging/windows/duxcel.rc -o $@
endif
endif
HEADERS = $(shell find src -name '*.h')
SRC += src/ui/filterlabel.c src/ui/selectview.c src/ui/shadowview.c
ICON_INPUTS = assets/icons/duxcel.svg tools/generate_icons.py
ICON_SIZES = 16 20 24 30 32 36 40 48 60 64 72 80 96 128 160 192 256 320 384 512 640 768 1024
ICON_MAC_SIZES = 16 32 128 256 512
ICON_LINUX_SIZES = 16 24 32 48 64 96 128 256 512
ICON_LINUX_2X = 16 24 32 48 64 128 256 512
ICON_OUTPUTS = src/ui/brand_geometry.h src/ui/brand_bitmap.h src/platform/brand_images.h assets/icons/duxcel.icns assets/icons/duxcel.ico assets/icons/manifest.json \
	$(foreach size,$(ICON_SIZES),assets/icons/png/duxcel-$(size).png) \
	$(foreach size,$(ICON_MAC_SIZES),assets/icons/duxcel.iconset/icon_$(size)x$(size).png assets/icons/duxcel.iconset/icon_$(size)x$(size)@2x.png) \
	$(foreach size,$(ICON_LINUX_SIZES),packaging/linux/icons/hicolor/$(size)x$(size)/apps/dev.duxcel.desktop.png) \
	$(foreach size,$(ICON_LINUX_2X),packaging/linux/icons/hicolor/$(size)x$(size)@2/apps/dev.duxcel.desktop.png) \
	packaging/linux/icons/hicolor/scalable/apps/dev.duxcel.desktop.svg

# Prebuilt derivatives keep ordinary builds on the existing C toolchain. Only a
# changed canonical vector/generator (or a missing derivative) needs Python 3.
# GNU Make 3.81 has no grouped target syntax. One stamp validates every
# checked-in derivative before invoking the generator once if repair is needed.
# A missing output forces that shared check, including when the stamp exists.
ICON_STAMP = build/icons.stamp
ifneq ($(strip $(filter-out $(wildcard $(ICON_OUTPUTS)),$(ICON_OUTPUTS))),)
$(ICON_STAMP): missing-icon-output
endif
$(ICON_STAMP): $(ICON_INPUTS)
	mkdir -p $(@D)
	python3 tools/generate_icons.py --check || python3 tools/generate_icons.py
	touch $@
$(ICON_OUTPUTS): $(ICON_STAMP)
	@test -f "$@"
	@touch "$@"
.PHONY: missing-icon-output
icons:
	python3 tools/generate_icons.py
	mkdir -p build
	touch $(ICON_STAMP)
icons-check: $(ICON_OUTPUTS)
	python3 tools/generate_icons.py --check
icons-verify: $(ICON_OUTPUTS)
	python3 tools/generate_icons.py --verify
icon-smoke: build/bin/duxcel-debug icons-check
	mkdir -p build/tests/results/icons
	ASAN_OPTIONS=halt_on_error=1 ./build/bin/duxcel-debug --icon-smoke --frames 360 --screenshot build/tests/results/icons/final.png
unique-smoke: build/bin/duxcel-debug
	mkdir -p build/tests/generated build/tests/results/unique-rows
	ASAN_OPTIONS=halt_on_error=1 ./build/bin/duxcel-debug --unique-smoke --frames 3600 --screenshot build/tests/results/unique-rows/final.png
query-smoke: build/bin/duxcel-debug
	mkdir -p build/tests/generated build/tests/results/source-query
	ASAN_OPTIONS=halt_on_error=1 ./build/bin/duxcel-debug --query-smoke --frames 3600 --screenshot build/tests/results/source-query/final.png
ifeq ($(shell uname -s),Darwin)
build/check-macos-icon: tools/check_macos_icon.m
	mkdir -p build
	$(CC) -O2 $(WARN) $< -framework AppKit -o $@
mac-icon-check: bundle build/check-macos-icon
	./build/check-macos-icon build/Duxcel.app
endif

all: build/bin/duxcel
design-fixture:
	python3 tools/create_workbook.py
design-large-fixture:
	python3 tools/create_workbook.py --large
design-smoke: build/bin/duxcel-debug design-fixture
	mkdir -p build/tests/results/design-definition
	ASAN_OPTIONS=halt_on_error=1 ./build/bin/duxcel-debug build/tests/generated/design-definition.dxls --frames 240 --screenshot build/tests/results/design-definition/authoring.png
# App and fixture-assertion compilation are incremental and isolated by exact
# configuration. Module tests retain their original independent commands below;
# in particular, renderer stubs must never reuse the application's UI objects.
APP_SOURCES = $(filter %.c %.m,$(SRC))
APP_EXTRA_OBJECTS = $(filter %.o,$(SRC))
APP_RELEASE_OBJECTS = $(addprefix build/obj/release/app/,$(addsuffix .o,$(basename $(APP_SOURCES))))
APP_SANITIZE_OBJECTS = $(addprefix build/obj/sanitize/app/,$(addsuffix .o,$(basename $(APP_SOURCES))))
APP_EDIT_OBJECTS = $(addprefix build/obj/edit-sanitize/app/,$(addsuffix .o,$(basename $(APP_SOURCES))))
FIXTURE_PERSISTENCE = src/platform/directory.c src/platform/file_lease.c src/workbook/core_payload.c src/workbook/core_compare.c src/workbook/version_sql.c src/workbook/version_stage.c src/workbook/version_read.c src/workbook/version_restore.c src/workbook/archive_read.c src/workbook/archive_write.c src/workbook/version_save.c
FIXTURE_SOURCES = $(CORE) $(FIXTURE_PERSISTENCE) tests/test_fixture.c
FIXTURE_OBJECTS = $(addprefix build/obj/release/fixtures/,$(addsuffix .o,$(basename $(FIXTURE_SOURCES))))
FIXTURE_SANITIZE_OBJECTS = $(addprefix build/obj/sanitize/fixtures/,$(addsuffix .o,$(basename $(FIXTURE_SOURCES))))
INCREMENTAL_OBJECTS = $(APP_RELEASE_OBJECTS) $(APP_SANITIZE_OBJECTS) $(APP_EDIT_OBJECTS) $(FIXTURE_OBJECTS)
INCREMENTAL_OBJECTS += $(FIXTURE_SANITIZE_OBJECTS)
REVIEW_SOURCES = $(APP_SOURCES) src/dev/review_registry.c src/dev/review_store.c src/dev/review_input.c src/dev/review_capture.c
REVIEW_OBJECTS = $(addprefix build/obj/review/app/,$(addsuffix .o,$(basename $(REVIEW_SOURCES))))
REVIEW_SANITIZE_OBJECTS = $(addprefix build/obj/review-sanitize/app/,$(addsuffix .o,$(basename $(REVIEW_SOURCES))))
INCREMENTAL_OBJECTS += $(REVIEW_OBJECTS) $(REVIEW_SANITIZE_OBJECTS)
REVIEW_CPPFLAGS = $(CPPFLAGS) -DDUXCEL_DEV_REVIEW -include src/dev/review_input.h
REVIEW_FLAGS = $(REVIEW_CPPFLAGS) -Ibuild/review -O2 -g $(WARN)
REVIEW_SANITIZE_FLAGS = $(REVIEW_CPPFLAGS) -Ibuild/review-sanitize -O1 -g $(WARN) $(SANFLAGS)
APP_RELEASE_FLAGS = $(CPPFLAGS) $(CFLAGS) $(WARN)
APP_SANITIZE_FLAGS = $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS)
# Opt-in edit loop only. Optimized release/O1 sanitizer acceptance stays separate.
APP_EDIT_FLAGS = $(CPPFLAGS) -O0 -g $(WARN) $(SANFLAGS)
FIXTURE_FLAGS = -Isrc $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN)
FIXTURE_SANITIZE_FLAGS = -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) $(SANFLAGS)

# A no-op signature recipe preserves its timestamp. Changed compiler, includes,
# defines or options therefore invalidate the appropriate objects, not every
# other build configuration. Each target has its own temporary output paths.
define build_signature
	@set -e; mkdir -p "$(@D)"; trap 'rm -f "$@.tmp"' 0; \
	printf '%s\n' $(1) > "$@.tmp"; \
	if ! cmp -s "$@.tmp" "$@"; then mv -f "$@.tmp" "$@"; fi
endef
build/obj/release/app/compile.flags: force-build-signature
	$(call build_signature,$(CC) $(APP_RELEASE_FLAGS) -MMD -MP -c)
build/obj/sanitize/app/compile.flags: force-build-signature
	$(call build_signature,$(CC) $(APP_SANITIZE_FLAGS) -MMD -MP -c)
build/obj/edit-sanitize/app/compile.flags: force-build-signature
	$(call build_signature,$(CC) $(APP_EDIT_FLAGS) -MMD -MP -c)
build/obj/release/fixtures/compile.flags: force-build-signature
	$(call build_signature,$(CC) $(FIXTURE_FLAGS) -MMD -MP -c)
build/obj/sanitize/fixtures/compile.flags: force-build-signature
	$(call build_signature,$(CC) $(FIXTURE_SANITIZE_FLAGS) -MMD -MP -c)
build/obj/release/app/link.flags: force-build-signature
	$(call build_signature,$(CC) $(CFLAGS) $(WARN) $(APP_RELEASE_OBJECTS) $(APP_EXTRA_OBJECTS) $(LDFLAGS) $(LDLIBS))
build/obj/sanitize/app/link.flags: force-build-signature
	$(call build_signature,$(CC) -O1 -g $(WARN) $(SANFLAGS) $(APP_SANITIZE_OBJECTS) $(APP_EXTRA_OBJECTS) $(LDFLAGS) $(LDLIBS) $(SANFLAGS))
build/obj/edit-sanitize/app/link.flags: force-build-signature
	$(call build_signature,$(CC) -O0 -g $(WARN) $(SANFLAGS) $(APP_EDIT_OBJECTS) $(APP_EXTRA_OBJECTS) $(LDFLAGS) $(LDLIBS) $(SANFLAGS))
build/obj/release/fixtures/link.flags: force-build-signature
	$(call build_signature,$(CC) $(CFLAGS) $(WARN) $(FIXTURE_OBJECTS) $(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/obj/sanitize/fixtures/link.flags: force-build-signature
	$(call build_signature,$(CC) -O1 -g $(WARN) $(SANFLAGS) $(FIXTURE_SANITIZE_OBJECTS) $(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
.PHONY: force-build-signature

build/obj/review/app/compile.flags: force-build-signature
	$(call build_signature,$(CC) $(REVIEW_FLAGS) -MMD -MP -c)
build/obj/review-sanitize/app/compile.flags: force-build-signature
	$(call build_signature,$(CC) $(REVIEW_SANITIZE_FLAGS) -MMD -MP -c)
build/obj/review/app/link.flags: force-build-signature
	$(call build_signature,$(CC) -O2 -g $(WARN) $(REVIEW_OBJECTS) $(LDFLAGS) $(LDLIBS))
build/obj/review-sanitize/app/link.flags: force-build-signature
	$(call build_signature,$(CC) -O1 -g $(WARN) $(SANFLAGS) $(REVIEW_SANITIZE_OBJECTS) $(LDFLAGS) $(LDLIBS) $(SANFLAGS))
build/review/review_build_info.h: force-build-signature
	python3 tools/review_build_info.py --header $@ --flags '$(REVIEW_FLAGS)'
build/review-sanitize/review_build_info.h: force-build-signature
	python3 tools/review_build_info.py --header $@ --flags '$(REVIEW_SANITIZE_FLAGS)'
$(REVIEW_OBJECTS): | $(ICON_OUTPUTS)
$(REVIEW_SANITIZE_OBJECTS): | $(ICON_OUTPUTS)
build/obj/review/app/src/dev/review_capture.o: build/review/review_build_info.h
build/obj/review-sanitize/app/src/dev/review_capture.o: build/review-sanitize/review_build_info.h
build/obj/review/app/%.o: %.c build/obj/review/app/compile.flags
	$(call compile_object,$(REVIEW_FLAGS))
build/obj/review/app/%.o: %.m build/obj/review/app/compile.flags
	$(call compile_object,$(REVIEW_FLAGS))
build/obj/review-sanitize/app/%.o: %.c build/obj/review-sanitize/app/compile.flags
	$(call compile_object,$(REVIEW_SANITIZE_FLAGS))
build/obj/review-sanitize/app/%.o: %.m build/obj/review-sanitize/app/compile.flags
	$(call compile_object,$(REVIEW_SANITIZE_FLAGS))
review: build/review/duxcel-review
review-debug: build/review-sanitize/duxcel-review
build/review/assets:
	mkdir -p $(@D)
	ln -s ../../assets $@
build/review-sanitize/assets:
	mkdir -p $(@D)
	ln -s ../../assets $@
build/review/duxcel-review: build/review/assets $(REVIEW_OBJECTS) build/obj/review/app/link.flags
	$(call link_binary,-O2 -g $(WARN) $(REVIEW_OBJECTS),$(LDFLAGS) $(LDLIBS))
	python3 tools/review_build_info.py --binary $@
build/review-sanitize/duxcel-review: build/review-sanitize/assets $(REVIEW_SANITIZE_OBJECTS) build/obj/review-sanitize/app/link.flags
	$(call link_binary,-O1 -g $(WARN) $(SANFLAGS) $(REVIEW_SANITIZE_OBJECTS),$(LDFLAGS) $(LDLIBS) $(SANFLAGS))
	python3 tools/review_build_info.py --binary $@
.PHONY: review review-debug

# Feature-only App fixture: the production App object has no test hook/state.
FORMAT_PLAN_GUI_OBJECT = build/obj/feature-format-plan/app.o
$(FORMAT_PLAN_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_FORMAT_PLAN)
FORMAT_PLAN_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(FORMAT_PLAN_GUI_OBJECT)
build/tests/test_format_plan-gui: tests/test_format_plan_gui.c $(FORMAT_PLAN_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_format_plan_gui.c $(FORMAT_PLAN_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(FORMAT_PLAN_GUI_OBJECT:.o=.d)

OBJECT_MARKS_COMMAND_GUI_OBJECT = build/obj/feature-object-marks-command/app.o
$(OBJECT_MARKS_COMMAND_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_OBJECT_MARKS_COMMAND)
OBJECT_MARKS_COMMAND_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(OBJECT_MARKS_COMMAND_GUI_OBJECT)
build/tests/test_object_marks_command-gui: tests/test_object_marks_command_gui.c $(OBJECT_MARKS_COMMAND_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_object_marks_command_gui.c $(OBJECT_MARKS_COMMAND_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(OBJECT_MARKS_COMMAND_GUI_OBJECT:.o=.d)
OBJECT_LAYOUT_COMMAND_GUI_OBJECT = build/obj/feature-object-layout-command/app.o
$(OBJECT_LAYOUT_COMMAND_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_OBJECT_LAYOUT_COMMAND)
OBJECT_LAYOUT_COMMAND_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(OBJECT_LAYOUT_COMMAND_GUI_OBJECT)
build/tests/test_object_layout_command-gui: tests/test_object_layout_command_gui.c $(OBJECT_LAYOUT_COMMAND_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_object_layout_command_gui.c $(OBJECT_LAYOUT_COMMAND_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(OBJECT_LAYOUT_COMMAND_GUI_OBJECT:.o=.d)
OBJECT_PIVOT_COMMAND_GUI_OBJECT = build/obj/feature-object-pivot-command/app.o
$(OBJECT_PIVOT_COMMAND_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_OBJECT_PIVOT_COMMAND)
OBJECT_PIVOT_COMMAND_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(OBJECT_PIVOT_COMMAND_GUI_OBJECT)
build/tests/test_object_pivot_command-gui: tests/test_object_pivot_command_gui.c $(OBJECT_PIVOT_COMMAND_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_object_pivot_command_gui.c $(OBJECT_PIVOT_COMMAND_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(OBJECT_PIVOT_COMMAND_GUI_OBJECT:.o=.d)
OBJECT_PRESENTATION_COMMAND_GUI_OBJECT = build/obj/feature-object-presentation-command/app.o
$(OBJECT_PRESENTATION_COMMAND_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_OBJECT_PRESENTATION_COMMAND)
OBJECT_PRESENTATION_COMMAND_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(OBJECT_PRESENTATION_COMMAND_GUI_OBJECT)
build/tests/test_object_presentation_command-gui: tests/test_object_presentation_command_gui.c $(OBJECT_PRESENTATION_COMMAND_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_object_presentation_command_gui.c $(OBJECT_PRESENTATION_COMMAND_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(OBJECT_PRESENTATION_COMMAND_GUI_OBJECT:.o=.d)
OBJECT_NAME_COMMAND_GUI_OBJECT = build/obj/feature-object-name-command/app.o
$(OBJECT_NAME_COMMAND_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_OBJECT_NAME_COMMAND)
OBJECT_NAME_COMMAND_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(OBJECT_NAME_COMMAND_GUI_OBJECT)
build/tests/test_object_name_command-gui: tests/test_object_name_command_gui.c $(OBJECT_NAME_COMMAND_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_object_name_command_gui.c $(OBJECT_NAME_COMMAND_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(OBJECT_NAME_COMMAND_GUI_OBJECT:.o=.d)

PRIMARY_REPLACEMENT_GUI_OBJECT = build/obj/feature-primary-replacement/app.o
$(PRIMARY_REPLACEMENT_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_PRIMARY_REPLACEMENT)
PRIMARY_REPLACEMENT_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(PRIMARY_REPLACEMENT_GUI_OBJECT)
build/tests/test_primary_replacement-gui: tests/test_primary_replacement_gui.c $(PRIMARY_REPLACEMENT_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_primary_replacement_gui.c $(PRIMARY_REPLACEMENT_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(PRIMARY_REPLACEMENT_GUI_OBJECT:.o=.d)
OBJECT_BINDING_COMMAND_GUI_OBJECT = build/obj/feature-object-binding-command/app.o
$(OBJECT_BINDING_COMMAND_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_OBJECT_BINDING_COMMAND)
OBJECT_BINDING_COMMAND_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(OBJECT_BINDING_COMMAND_GUI_OBJECT)
build/tests/test_object_binding_command-gui: tests/test_object_binding_command_gui.c $(OBJECT_BINDING_COMMAND_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_object_binding_command_gui.c $(OBJECT_BINDING_COMMAND_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(OBJECT_BINDING_COMMAND_GUI_OBJECT:.o=.d)
OBJECT_DELETE_COMMAND_GUI_OBJECT = build/obj/feature-object-delete-command/app.o
$(OBJECT_DELETE_COMMAND_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_OBJECT_DELETE_COMMAND)
OBJECT_DELETE_COMMAND_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(OBJECT_DELETE_COMMAND_GUI_OBJECT)
build/tests/test_object_delete_command-gui: tests/test_object_delete_command_gui.c $(OBJECT_DELETE_COMMAND_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_object_delete_command_gui.c $(OBJECT_DELETE_COMMAND_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(OBJECT_DELETE_COMMAND_GUI_OBJECT:.o=.d)
DEFINITION_ORIGIN_GUI_OBJECT = build/obj/feature-definition-origin/app.o
$(DEFINITION_ORIGIN_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_DEFINITION_ORIGIN)
DEFINITION_ORIGIN_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(DEFINITION_ORIGIN_GUI_OBJECT)
build/tests/test_definition_origin-gui: tests/test_definition_origin_gui.c $(DEFINITION_ORIGIN_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_definition_origin_gui.c $(DEFINITION_ORIGIN_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(DEFINITION_ORIGIN_GUI_OBJECT:.o=.d)
TABLE_ORIGIN_GUI_OBJECT = build/obj/feature-table-origin/app.o
$(TABLE_ORIGIN_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_TABLE_ORIGIN)
TABLE_ORIGIN_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(TABLE_ORIGIN_GUI_OBJECT)
build/tests/test_table_origin-gui: tests/test_table_origin_gui.c $(TABLE_ORIGIN_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_table_origin_gui.c $(TABLE_ORIGIN_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(TABLE_ORIGIN_GUI_OBJECT:.o=.d)
OBJECT_CREATE_COMMAND_GUI_OBJECT = build/obj/feature-object-create-command/app.o
$(OBJECT_CREATE_COMMAND_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_OBJECT_CREATE_COMMAND)
OBJECT_CREATE_COMMAND_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(OBJECT_CREATE_COMMAND_GUI_OBJECT)
build/tests/test_object_create_command-gui: tests/test_object_create_command_gui.c $(OBJECT_CREATE_COMMAND_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_object_create_command_gui.c $(OBJECT_CREATE_COMMAND_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(OBJECT_CREATE_COMMAND_GUI_OBJECT:.o=.d)
TEMPLATE_CELLS_GUI_OBJECT = build/obj/feature-template-cells/app.o
$(TEMPLATE_CELLS_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_TEMPLATE_CELLS)
TEMPLATE_CELLS_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(TEMPLATE_CELLS_GUI_OBJECT)
build/tests/test_template_cells-gui: tests/test_template_cells_gui.c $(TEMPLATE_CELLS_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_template_cells_gui.c $(TEMPLATE_CELLS_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(TEMPLATE_CELLS_GUI_OBJECT:.o=.d)
MIXED_COLUMNS_GUI_OBJECT = build/obj/feature-mixed-columns/app.o
$(MIXED_COLUMNS_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_MIXED_COLUMNS)
MIXED_COLUMNS_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(MIXED_COLUMNS_GUI_OBJECT)
build/tests/test_mixed_columns-gui: tests/test_mixed_columns_gui.c $(MIXED_COLUMNS_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_mixed_columns_gui.c $(MIXED_COLUMNS_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(MIXED_COLUMNS_GUI_OBJECT:.o=.d)
MIXED_ROWS_GUI_OBJECT = build/obj/feature-mixed-rows/app.o
$(MIXED_ROWS_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_MIXED_ROWS)
MIXED_ROWS_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(MIXED_ROWS_GUI_OBJECT)
build/tests/test_mixed_rows-gui: tests/test_mixed_rows_gui.c $(MIXED_ROWS_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_mixed_rows_gui.c $(MIXED_ROWS_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(MIXED_ROWS_GUI_OBJECT:.o=.d)
CLEAR_RANGE_GUI_OBJECT = build/obj/feature-clear-range/app.o
$(CLEAR_RANGE_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_CLEAR_RANGE)
CLEAR_RANGE_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(CLEAR_RANGE_GUI_OBJECT)
build/tests/test_clear_range-gui: tests/test_clear_range_gui.c $(CLEAR_RANGE_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_clear_range_gui.c $(CLEAR_RANGE_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(CLEAR_RANGE_GUI_OBJECT:.o=.d)
MIXED_RANGE_GUI_OBJECT = build/obj/feature-mixed-range/app.o
$(MIXED_RANGE_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_MIXED_RANGE)
MIXED_RANGE_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(MIXED_RANGE_GUI_OBJECT)
build/tests/test_mixed_range-gui: tests/test_mixed_range_gui.c $(MIXED_RANGE_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_mixed_range_gui.c $(MIXED_RANGE_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(MIXED_RANGE_GUI_OBJECT:.o=.d)
RANGE_COMMAND_GUI_OBJECT = build/obj/feature-range-command/app.o
COLUMN_WIDTH_GUI_OBJECT = build/obj/feature-column-width/app.o
PASTE_COMMAND_GUI_OBJECT = build/obj/feature-paste-command/app.o
$(PASTE_COMMAND_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_PASTE_COMMAND)
PASTE_COMMAND_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(PASTE_COMMAND_GUI_OBJECT)
build/tests/test_paste_command-gui: tests/test_paste_command_gui.c $(PASTE_COMMAND_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_paste_command_gui.c $(PASTE_COMMAND_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(PASTE_COMMAND_GUI_OBJECT:.o=.d)
$(COLUMN_WIDTH_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_COLUMN_WIDTH)
COLUMN_WIDTH_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(COLUMN_WIDTH_GUI_OBJECT)
build/tests/test_column_width-gui: tests/test_column_width_gui.c $(COLUMN_WIDTH_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_column_width_gui.c $(COLUMN_WIDTH_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(COLUMN_WIDTH_GUI_OBJECT:.o=.d)
$(RANGE_COMMAND_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_RANGE_COMMAND)
RANGE_COMMAND_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(RANGE_COMMAND_GUI_OBJECT)
build/tests/test_range_command-gui: tests/test_range_command_gui.c $(RANGE_COMMAND_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_range_command_gui.c $(RANGE_COMMAND_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(RANGE_COMMAND_GUI_OBJECT:.o=.d)
OBJECT_MOVE_COMMAND_GUI_OBJECT = build/obj/feature-object-move-command/app.o
$(OBJECT_MOVE_COMMAND_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_OBJECT_MOVE_COMMAND)
OBJECT_MOVE_COMMAND_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(OBJECT_MOVE_COMMAND_GUI_OBJECT)
build/tests/test_object_move_command-gui: tests/test_object_move_command_gui.c $(OBJECT_MOVE_COMMAND_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_object_move_command_gui.c $(OBJECT_MOVE_COMMAND_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(OBJECT_MOVE_COMMAND_GUI_OBJECT:.o=.d)
TABLE_REPLACEMENT_GUI_OBJECT = build/obj/feature-table-replacement/app.o
$(TABLE_REPLACEMENT_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_TABLE_REPLACEMENT)
TABLE_REPLACEMENT_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(TABLE_REPLACEMENT_GUI_OBJECT)
build/tests/test_table_replacement-gui: tests/test_table_replacement_gui.c $(TABLE_REPLACEMENT_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_table_replacement_gui.c $(TABLE_REPLACEMENT_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(TABLE_REPLACEMENT_GUI_OBJECT:.o=.d)
TABLE_COMMAND_GUI_OBJECT = build/obj/feature-table-command/app.o
$(TABLE_COMMAND_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_TABLE_COMMAND)
TABLE_COMMAND_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(TABLE_COMMAND_GUI_OBJECT)
build/tests/test_table_command-gui: tests/test_table_command_gui.c $(TABLE_COMMAND_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_table_command_gui.c $(TABLE_COMMAND_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(TABLE_COMMAND_GUI_OBJECT:.o=.d)

PRIMARY_STEPS_GUI_OBJECT = build/obj/feature-primary-steps/app.o
$(PRIMARY_STEPS_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_PRIMARY_STEPS)
PRIMARY_STEPS_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(PRIMARY_STEPS_GUI_OBJECT)
build/tests/test_primary_steps-gui: tests/test_primary_steps_gui.c $(PRIMARY_STEPS_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_primary_steps_gui.c $(PRIMARY_STEPS_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(PRIMARY_STEPS_GUI_OBJECT:.o=.d)

PRIMARY_QUERY_GUI_OBJECT = build/obj/feature-primary-query/app.o
$(PRIMARY_QUERY_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_PRIMARY_QUERY)
PRIMARY_QUERY_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(PRIMARY_QUERY_GUI_OBJECT)
build/tests/test_primary_query-gui: tests/test_primary_query_gui.c $(PRIMARY_QUERY_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_primary_query_gui.c $(PRIMARY_QUERY_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(PRIMARY_QUERY_GUI_OBJECT:.o=.d)

PRIMARY_TRANSFORMS_GUI_OBJECT = build/obj/feature-primary-transforms/app.o
$(PRIMARY_TRANSFORMS_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_PRIMARY_TRANSFORMS)
PRIMARY_TRANSFORMS_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(PRIMARY_TRANSFORMS_GUI_OBJECT)
build/tests/test_primary_transforms-gui: tests/test_primary_transforms_gui.c $(PRIMARY_TRANSFORMS_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_primary_transforms_gui.c $(PRIMARY_TRANSFORMS_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(PRIMARY_TRANSFORMS_GUI_OBJECT:.o=.d)

ANALYSIS_COMMAND_GUI_OBJECT = build/obj/feature-analysis-command/app.o
$(ANALYSIS_COMMAND_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_ANALYSIS_COMMAND)
ANALYSIS_COMMAND_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(ANALYSIS_COMMAND_GUI_OBJECT)
build/tests/test_analysis_command-gui: tests/test_analysis_command_gui.c $(ANALYSIS_COMMAND_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_analysis_command_gui.c $(ANALYSIS_COMMAND_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(ANALYSIS_COMMAND_GUI_OBJECT:.o=.d)

CELL_COMMAND_GUI_OBJECT = build/obj/feature-cell-command/app.o
$(CELL_COMMAND_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_CELL_COMMAND)
CELL_COMMAND_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(CELL_COMMAND_GUI_OBJECT)
build/tests/test_cell_command-native: tests/test_cell_command_gui.c $(CELL_COMMAND_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_cell_command_gui.c $(CELL_COMMAND_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(CELL_COMMAND_GUI_OBJECT:.o=.d)

IDENTITY_GUI_OBJECT = build/obj/feature-workbook-identity/app.o
$(IDENTITY_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_WORKBOOK_IDENTITY)
IDENTITY_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(IDENTITY_GUI_OBJECT)
build/tests/test_workbook_identity-gui: tests/test_workbook_identity_gui.c $(IDENTITY_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_workbook_identity_gui.c $(IDENTITY_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(IDENTITY_GUI_OBJECT:.o=.d)

HISTORY_GUI_OBJECT = build/obj/feature-workbook-history/app.o
$(HISTORY_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_WORKBOOK_HISTORY)
HISTORY_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(HISTORY_GUI_OBJECT)
build/tests/test_workbook_history-gui: tests/test_workbook_history_gui.c $(HISTORY_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_workbook_history_gui.c $(HISTORY_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(HISTORY_GUI_OBJECT:.o=.d)

LIFECYCLE_GUI_OBJECT = build/obj/feature-workbook-lifecycle/app.o
$(LIFECYCLE_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_WORKBOOK_LIFECYCLE)
LIFECYCLE_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(LIFECYCLE_GUI_OBJECT)
build/tests/test_workbook_lifecycle-gui: tests/test_workbook_lifecycle_gui.c $(LIFECYCLE_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_workbook_lifecycle_gui.c $(LIFECYCLE_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(LIFECYCLE_GUI_OBJECT:.o=.d)

JOURNAL_SESSION_GUI_OBJECT = build/obj/feature-journal-session/app.o
$(JOURNAL_SESSION_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_JOURNAL_SESSION)
JOURNAL_SESSION_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(JOURNAL_SESSION_GUI_OBJECT)
build/tests/test_journal_session-gui: tests/test_journal_session_gui.c $(JOURNAL_SESSION_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_journal_session_gui.c $(JOURNAL_SESSION_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(JOURNAL_SESSION_GUI_OBJECT:.o=.d)



# Only a successful compiler/linker invocation replaces an accepted artifact.
# Separate object and dependency temporary files preserve the previous pair on
# a compilation failure. -MT records the final path, not its temporary sibling.
define compile_object
	@mkdir -p "$(@D)"
	@set -e; trap 'rm -f "$@.new" "$(@:.o=.d).new"' 0; \
	$(CC) $(1) $(OBJECT_CPPFLAGS) -MMD -MP -MF "$(@:.o=.d).new" -MT "$@" -c "$<" -o "$@.new"; \
	mv -f "$(@:.o=.d).new" "$(@:.o=.d)"; mv -f "$@.new" "$@"
endef
# Metadata changes recompile one small translation unit, never the whole App.
BUILD_INFO_HEADER = build/generated/product_build_info.h
$(BUILD_INFO_HEADER): force-build-info tools/build_info.py
	@python3 tools/build_info.py --output $@
.PHONY: force-build-info
BUILD_INFO_OBJECTS = $(filter %/src/app/build_info.o,$(INCREMENTAL_OBJECTS))
$(BUILD_INFO_OBJECTS): OBJECT_CPPFLAGS = -include $(BUILD_INFO_HEADER)
$(BUILD_INFO_OBJECTS): $(BUILD_INFO_HEADER)

build/obj/release/app/%.o: %.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS))
build/obj/release/app/%.o: %.m build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS))
build/obj/sanitize/app/%.o: %.c build/obj/sanitize/app/compile.flags
	$(call compile_object,$(APP_SANITIZE_FLAGS))
build/obj/sanitize/app/%.o: %.m build/obj/sanitize/app/compile.flags
	$(call compile_object,$(APP_SANITIZE_FLAGS))
build/obj/edit-sanitize/app/%.o: %.c build/obj/edit-sanitize/app/compile.flags
	$(call compile_object,$(APP_EDIT_FLAGS))
build/obj/edit-sanitize/app/%.o: %.m build/obj/edit-sanitize/app/compile.flags
	$(call compile_object,$(APP_EDIT_FLAGS))
build/obj/release/fixtures/%.o: %.c build/obj/release/fixtures/compile.flags
	$(call compile_object,$(FIXTURE_FLAGS))
build/obj/sanitize/fixtures/%.o: %.c build/obj/sanitize/fixtures/compile.flags
	$(call compile_object,$(FIXTURE_SANITIZE_FLAGS))
$(APP_RELEASE_OBJECTS) $(APP_SANITIZE_OBJECTS) $(APP_EDIT_OBJECTS): | $(ICON_OUTPUTS)

define link_binary
	@mkdir -p "$(@D)"
	@set -e; trap 'rm -f "$@.new"' 0; \
	$(CC) $(1) -o "$@.new" $(2); mv -f "$@.new" "$@"
endef
build/bin/duxcel: build/bin/assets $(APP_RELEASE_OBJECTS) $(APP_EXTRA_OBJECTS) build/obj/release/app/link.flags
	$(call link_binary,$(CFLAGS) $(WARN) $(APP_RELEASE_OBJECTS) $(APP_EXTRA_OBJECTS),$(LDFLAGS) $(LDLIBS))
build/bin/duxcel-debug: build/bin/assets $(APP_SANITIZE_OBJECTS) $(APP_EXTRA_OBJECTS) build/obj/sanitize/app/link.flags
	$(call link_binary,-O1 -g $(WARN) $(SANFLAGS) $(APP_SANITIZE_OBJECTS) $(APP_EXTRA_OBJECTS),$(LDFLAGS) $(LDLIBS) $(SANFLAGS))
build/edit-sanitize/assets:
	mkdir -p $(@D)
	ln -s ../../assets $@
build/edit-sanitize/duxcel: build/edit-sanitize/assets $(APP_EDIT_OBJECTS) $(APP_EXTRA_OBJECTS) build/obj/edit-sanitize/app/link.flags
	$(call link_binary,-O0 -g $(WARN) $(SANFLAGS) $(APP_EDIT_OBJECTS) $(APP_EXTRA_OBJECTS),$(LDFLAGS) $(LDLIBS) $(SANFLAGS))
# The lifecycle driver uses real application objects, never renderer stubs.
LIFECYCLE_RELEASE_OBJECTS = $(filter-out build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) build/obj/release/app/tests/test_app_lifecycle.o
LIFECYCLE_SANITIZE_OBJECTS = $(filter-out build/obj/sanitize/app/src/main.o,$(APP_SANITIZE_OBJECTS)) build/obj/sanitize/app/tests/test_app_lifecycle.o
LIFECYCLE_NATIVE_OBJECTS =
ifeq ($(shell uname -s),Darwin)
LIFECYCLE_RELEASE_OBJECTS += build/obj/release/app/tests/test_app_lifecycle_native.o
LIFECYCLE_SANITIZE_OBJECTS += build/obj/sanitize/app/tests/test_app_lifecycle_native.o
LIFECYCLE_NATIVE_OBJECTS += build/obj/release/app/tests/test_app_lifecycle_native.o build/obj/sanitize/app/tests/test_app_lifecycle_native.o
endif
INCREMENTAL_OBJECTS += build/obj/release/app/tests/test_app_lifecycle.o build/obj/sanitize/app/tests/test_app_lifecycle.o $(LIFECYCLE_NATIVE_OBJECTS)
build/obj/release/app/lifecycle-link.flags: force-build-signature
	$(call build_signature,$(CC) $(CFLAGS) $(WARN) $(LIFECYCLE_RELEASE_OBJECTS) $(LDFLAGS) $(LDLIBS))
build/obj/sanitize/app/lifecycle-link.flags: force-build-signature
	$(call build_signature,$(CC) -O1 -g $(WARN) $(SANFLAGS) $(LIFECYCLE_SANITIZE_OBJECTS) $(LDFLAGS) $(LDLIBS) $(SANFLAGS))
build/lifecycle/assets:
	mkdir -p $(@D)
	ln -s ../../assets $@
build/lifecycle/test-app: build/lifecycle/assets $(LIFECYCLE_RELEASE_OBJECTS) build/obj/release/app/lifecycle-link.flags
	$(call link_binary,$(CFLAGS) $(WARN) $(LIFECYCLE_RELEASE_OBJECTS),$(LDFLAGS) $(LDLIBS))
build/lifecycle/test-app-sanitize: build/lifecycle/assets $(LIFECYCLE_SANITIZE_OBJECTS) build/obj/sanitize/app/lifecycle-link.flags
	$(call link_binary,-O1 -g $(WARN) $(SANFLAGS) $(LIFECYCLE_SANITIZE_OBJECTS),$(LDFLAGS) $(LDLIBS) $(SANFLAGS))

build/tests/test_fixture: $(FIXTURE_OBJECTS) build/obj/release/fixtures/link.flags
	$(call link_binary,$(CFLAGS) $(WARN) $(FIXTURE_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_fixture-debug: $(FIXTURE_SANITIZE_OBJECTS) build/obj/sanitize/fixtures/link.flags
	$(call link_binary,-O1 -g $(WARN) $(SANFLAGS) $(FIXTURE_SANITIZE_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))

# Losing a sidecar must not silently discard its header tracking. An absent
# empty-rule dependency forces its object to recreate the compiler's sidecar.
$(INCREMENTAL_OBJECTS): %.o: %.d
$(INCREMENTAL_OBJECTS:.o=.d):
-include $(INCREMENTAL_OBJECTS:.o=.d)
run: build/bin/duxcel
	./build/bin/duxcel $(ARGS)
debug: build/bin/duxcel-debug
run-debug: build/bin/duxcel-debug
	./build/bin/duxcel-debug $(ARGS)
build/tests/test_brand: tests/test_brand.c src/ui/brand_geometry.h | build/tests
	$(CC) -Isrc $(CFLAGS) $(WARN) tests/test_brand.c -o $@ -lm
build/tests/test_brand-debug: tests/test_brand.c src/ui/brand_geometry.h | build/tests
	$(CC) -Isrc -O1 -g $(WARN) $(SANFLAGS) tests/test_brand.c -o $@ -lm $(SANFLAGS)
FORMAT_TEST_OBJECTS = build/obj/release/app/src/common.o build/obj/release/app/src/workbook/format.o build/obj/release/app/src/transforms/number.o build/obj/release/app/tests/test_format.o
FORMAT_TEST_SAN_OBJECTS = build/obj/sanitize/app/src/common.o build/obj/sanitize/app/src/workbook/format.o build/obj/sanitize/app/src/transforms/number.o build/obj/sanitize/app/tests/test_format.o
build/tests/test_format: $(FORMAT_TEST_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(FORMAT_TEST_OBJECTS),-lm)
build/tests/test_format-debug: $(FORMAT_TEST_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(FORMAT_TEST_SAN_OBJECTS),-lm $(SANFLAGS))
-include $(FORMAT_TEST_OBJECTS:.o=.d) $(FORMAT_TEST_SAN_OBJECTS:.o=.d)
CHART_MAPPING_SRC = src/common.c src/transforms/analysis.c src/transforms/number.c src/transforms/pivot.c src/transforms/chart_other.c src/data/page.c src/data/source_digest.c
CHARTVIEW_SRC = $(CHART_MAPPING_SRC) src/ui/chartview.c tests/test_chartview.c
build/tests/test_chartview: $(CHARTVIEW_SRC) $(HEADERS) | build/tests
	$(CC) -Isrc $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN) $(CHARTVIEW_SRC) -o $@ -lm
build/tests/test_chartview-debug: $(CHARTVIEW_SRC) $(HEADERS) | build/tests
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(CHARTVIEW_SRC) -o $@ -lm $(SANFLAGS)
ENCODING_SRC = $(CHART_MAPPING_SRC) src/transforms/marks.c src/ui/chartview.c src/ui/chartencoding.c tests/test_chartencoding.c
build/tests/test_chartencoding: $(ENCODING_SRC) $(HEADERS) | build/tests
	$(CC) -Isrc -Isrc/ui $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN) $(ENCODING_SRC) -o $@ -lm
build/tests/test_chartencoding-debug: $(ENCODING_SRC) $(HEADERS) | build/tests
	$(CC) -Isrc -Isrc/ui $(DUCKDB_CFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(ENCODING_SRC) -o $@ -lm $(SANFLAGS)
GRID_PAGE_SRC = src/transforms/analysis.c src/transforms/record_sql.c src/transforms/pivot.c src/transforms/chart_other.c src/data/page.c src/data/source_digest.c
build/tests/test_grid: src/common.c $(GRID_PAGE_SRC) src/workbook/format.c src/transforms/number.c src/ui/motion.c src/ui/fontmetrics.c src/ui/paintlayer.c src/ui/paintlayer_backend.c src/ui/ui.c src/ui/ribbon.c src/ui/ribbon_spec.c src/ui/keytips.c src/ui/flyout.c src/ui/statusview.c src/ui/dockview.c src/ui/workspace.c src/ui/chartview.c src/ui/textview.c src/grid/grid.c tests/test_grid.c $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) src/common.c $(GRID_PAGE_SRC) src/workbook/format.c src/transforms/number.c src/ui/motion.c src/ui/fontmetrics.c src/ui/paintlayer.c src/ui/paintlayer_backend.c src/ui/ui.c src/ui/ribbon.c src/ui/ribbon_spec.c src/ui/keytips.c src/ui/flyout.c src/ui/statusview.c src/ui/dockview.c src/ui/workspace.c src/ui/chartview.c src/ui/textview.c src/grid/grid.c tests/test_grid.c -o $@ $(RAYLIB_LIBS) -lm
build/tests/test_grid-debug: src/common.c $(GRID_PAGE_SRC) src/workbook/format.c src/transforms/number.c src/ui/motion.c src/ui/fontmetrics.c src/ui/paintlayer.c src/ui/paintlayer_backend.c src/ui/ui.c src/ui/ribbon.c src/ui/ribbon_spec.c src/ui/keytips.c src/ui/flyout.c src/ui/statusview.c src/ui/dockview.c src/ui/workspace.c src/ui/chartview.c src/ui/textview.c src/grid/grid.c tests/test_grid.c $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) src/common.c $(GRID_PAGE_SRC) src/workbook/format.c src/transforms/number.c src/ui/motion.c src/ui/fontmetrics.c src/ui/paintlayer.c src/ui/paintlayer_backend.c src/ui/ui.c src/ui/ribbon.c src/ui/ribbon_spec.c src/ui/keytips.c src/ui/flyout.c src/ui/statusview.c src/ui/dockview.c src/ui/workspace.c src/ui/chartview.c src/ui/textview.c src/grid/grid.c tests/test_grid.c -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
SOURCE_SCOPE_TEST_SRC = src/common.c src/transforms/analysis.c src/transforms/number.c src/data/source_digest.c src/workbook/source_scope.c tests/test_source_scope.c
OBJECT_TEST_SRC = src/common.c src/transforms/analysis.c src/transforms/number.c src/transforms/pivot.c src/transforms/marks.c src/workbook/cells_tsv_scan.c src/workbook/cells.c src/workbook/objects.c tests/test_objects.c
build/tests/test_objects: $(OBJECT_TEST_SRC) $(HEADERS) | build/tests
	$(CC) -Isrc $(CFLAGS) $(WARN) -Werror $(OBJECT_TEST_SRC) -o $@ -lm
build/tests/test_objects-debug: $(OBJECT_TEST_SRC) $(HEADERS) | build/tests
	$(CC) -Isrc -O1 -g $(WARN) -Werror $(SANFLAGS) $(OBJECT_TEST_SRC) -o $@ -lm $(SANFLAGS)
SOURCE_SCOPE_FLAGS = -Isrc $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN) -Werror
SOURCE_SCOPE_SAN_FLAGS = -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) -Werror $(SANFLAGS)
SOURCE_SCOPE_OBJECTS = $(addprefix build/obj/release/source-scope/,$(addsuffix .o,$(basename $(SOURCE_SCOPE_TEST_SRC))))
SOURCE_SCOPE_SAN_OBJECTS = $(addprefix build/obj/sanitize/source-scope/,$(addsuffix .o,$(basename $(SOURCE_SCOPE_TEST_SRC))))
build/obj/release/source-scope/compile.flags: force-build-signature
	$(call build_signature,$(CC) $(SOURCE_SCOPE_FLAGS) -MMD -MP -c)
build/obj/sanitize/source-scope/compile.flags: force-build-signature
	$(call build_signature,$(CC) $(SOURCE_SCOPE_SAN_FLAGS) -MMD -MP -c)
build/obj/release/source-scope/%.o: %.c build/obj/release/source-scope/compile.flags
	$(call compile_object,$(SOURCE_SCOPE_FLAGS))
build/obj/sanitize/source-scope/%.o: %.c build/obj/sanitize/source-scope/compile.flags
	$(call compile_object,$(SOURCE_SCOPE_SAN_FLAGS))
build/tests/test_source_scope: $(SOURCE_SCOPE_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(SOURCE_SCOPE_OBJECTS),-lm)
build/tests/test_source_scope-debug: $(SOURCE_SCOPE_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(SOURCE_SCOPE_SAN_OBJECTS),-lm $(SANFLAGS))
-include $(SOURCE_SCOPE_OBJECTS:.o=.d) $(SOURCE_SCOPE_SAN_OBJECTS:.o=.d)
SELECTION_LADDER_SRC = src/grid/selection_ladder.c tests/test_selection_ladder.c
build/tests/test_build_info: src/app/build_info.c src/app/build_info.h tests/test_build_info.c | build/tests
	$(CC) $(CFLAGS) $(WARN) -Isrc src/app/build_info.c tests/test_build_info.c -o $@
build/tests/test_build_info-debug: src/app/build_info.c src/app/build_info.h tests/test_build_info.c | build/tests
	$(CC) -O1 -g $(WARN) $(SANFLAGS) -Isrc src/app/build_info.c tests/test_build_info.c -o $@ $(SANFLAGS)
SELECTION_SRC = src/common.c src/workbook/cells_tsv_scan.c src/workbook/cells.c src/grid/selection_ladder.c src/app/selection.c tests/test_selection.c
SELECTION_HEADERS = src/common.h src/workbook/cells.h src/grid/grid.h src/grid/selection_ladder.h src/app/selection.h src/ui/ui.h src/workbook/format.h src/data/database.h src/platform/platform.h
build/tests/test_selection: $(SELECTION_SRC) $(SELECTION_HEADERS) | build/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(SELECTION_SRC) -o $@ -lm
build/tests/test_selection-debug: $(SELECTION_SRC) $(SELECTION_HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(SELECTION_SRC) -o $@ -lm $(SANFLAGS)

# Adapter GUI checks link two product modules, not the App or DuckDB engine.
ifeq ($(shell uname -s),Darwin)
build/tests/test_about-native-debug: tests/test_about_native.m assets/icons/duxcel.icns src/platform/macos_about.h src/platform/platform.h src/app/build_info.h build/obj/sanitize/app/src/platform/macos_about.o build/obj/sanitize/app/src/app/build_info.o | build/tests
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) tests/test_about_native.m build/obj/sanitize/app/src/platform/macos_about.o build/obj/sanitize/app/src/app/build_info.o -o $@ -framework AppKit $(SANFLAGS)
endif
CELL_REGIONS_SRC = src/common.c src/workbook/cells_tsv_scan.c src/workbook/cells.c tests/test_cell_regions.c
build/tests/test_cell_regions: $(CELL_REGIONS_SRC) $(HEADERS) | build/tests
	$(CC) -Isrc -O2 -g $(WARN) $(CELL_REGIONS_SRC) -o $@ -lm
build/tests/test_cell_regions-debug: $(CELL_REGIONS_SRC) $(HEADERS) | build/tests
	$(CC) -Isrc -O1 -g $(WARN) $(SANFLAGS) $(CELL_REGIONS_SRC) -o $@ -lm $(SANFLAGS)
build/tests/test_selection_ladder: $(SELECTION_LADDER_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(SELECTION_LADDER_SRC) -o $@
build/tests/test_selection_ladder-debug: $(SELECTION_LADDER_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(SELECTION_LADDER_SRC) -o $@ $(SANFLAGS)
PAGE_SRC = src/common.c src/transforms/analysis.c src/transforms/record_sql.c src/transforms/number.c src/transforms/pivot.c src/data/page.c src/data/source_digest.c tests/test_page.c
build/tests/test_page: $(PAGE_SRC) $(HEADERS) | build/tests
	$(CC) -Isrc $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN) $(PAGE_SRC) -o $@ -lm
build/tests/test_page-debug: $(PAGE_SRC) $(HEADERS) | build/tests
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(PAGE_SRC) -o $@ -lm $(SANFLAGS)
PAGE_ENGINE_SRC = $(PAGE_SRC) src/platform/platform.c src/transforms/distinct.c src/transforms/chart_other.c src/data/database.c
build/tests/test_page-engine: $(PAGE_ENGINE_SRC) $(HEADERS) | build/tests
	$(CC) -Isrc $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN) -DDUXCEL_PAGE_ENGINE_TEST $(PAGE_ENGINE_SRC) -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm
build/tests/test_page-engine-debug: $(PAGE_ENGINE_SRC) $(HEADERS) | build/tests
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) $(SANFLAGS) -DDUXCEL_PAGE_ENGINE_TEST $(PAGE_ENGINE_SRC) -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS)
RECORD_SQL_SRC = src/common.c src/transforms/analysis.c src/transforms/number.c src/transforms/record_sql.c
RECORD_PAGE_SRC = $(RECORD_SQL_SRC) src/platform/platform.c src/transforms/pivot.c src/transforms/chart_other.c src/transforms/distinct.c src/data/page.c src/data/source_digest.c src/data/database.c
build/tests/test_record_sql: $(RECORD_SQL_SRC) tests/test_record_sql.c $(HEADERS) | build/tests
	$(CC) -Isrc $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN) $(RECORD_SQL_SRC) tests/test_record_sql.c -o $@ $(DUCKDB_LIBS) -lm
build/tests/test_record_sql-debug: $(RECORD_SQL_SRC) tests/test_record_sql.c $(HEADERS) | build/tests
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(RECORD_SQL_SRC) tests/test_record_sql.c -o $@ $(DUCKDB_LIBS) -lm $(SANFLAGS)
CHART_OTHER_SRC = $(RECORD_PAGE_SRC) src/ui/chartview.c tests/test_chart_other.c
build/tests/test_chart_other: $(CHART_OTHER_SRC) $(HEADERS) | build/tests
	$(CC) -Isrc $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN) $(CHART_OTHER_SRC) -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm
build/tests/test_chart_other-debug: $(CHART_OTHER_SRC) $(HEADERS) | build/tests
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(CHART_OTHER_SRC) -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS)
test-chart-other: build/tests/test_chart_other build/tests/test_chart_other-debug
	./build/tests/test_chart_other
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_chart_other-debug
DATABASE_CHART_OTHER_SRC = $(RECORD_PAGE_SRC) tests/test_database_chart_other.c
build/tests/test_database_chart_other: $(DATABASE_CHART_OTHER_SRC) $(HEADERS) | build/tests
	$(CC) -Isrc $(DUCKDB_CFLAGS) -DDUXCEL_TEST_CHART_OTHER $(CFLAGS) $(WARN) $(DATABASE_CHART_OTHER_SRC) -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm
build/tests/test_database_chart_other-debug: $(DATABASE_CHART_OTHER_SRC) $(HEADERS) | build/tests
	$(CC) -Isrc $(DUCKDB_CFLAGS) -DDUXCEL_TEST_CHART_OTHER -O1 -g $(WARN) $(SANFLAGS) $(DATABASE_CHART_OTHER_SRC) -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS)
WORKER_CHART_OTHER_SRC = $(CORE) src/ui/chartview.c src/ui/chartencoding.c tests/test_worker_chart_other.c
build/tests/test_worker_chart_other: $(WORKER_CHART_OTHER_SRC) $(HEADERS) | build/tests
	$(CC) -Isrc $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN) $(WORKER_CHART_OTHER_SRC) -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm
build/tests/test_worker_chart_other-debug: $(WORKER_CHART_OTHER_SRC) $(HEADERS) | build/tests
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(WORKER_CHART_OTHER_SRC) -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS)
test-chart-other-native: build/tests/test_database_chart_other build/tests/test_database_chart_other-debug build/tests/test_worker_chart_other build/tests/test_worker_chart_other-debug
	./build/tests/test_database_chart_other
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_database_chart_other-debug
	./build/tests/test_worker_chart_other
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_worker_chart_other-debug
.PHONY: test-chart-other-native
.PHONY: test-chart-other
build/tests/test_record_pages: $(RECORD_PAGE_SRC) tests/test_record_pages.c $(HEADERS) | build/tests
	$(CC) -Isrc $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN) $(RECORD_PAGE_SRC) tests/test_record_pages.c -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm
build/tests/test_record_pages-debug: $(RECORD_PAGE_SRC) tests/test_record_pages.c $(HEADERS) | build/tests
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(RECORD_PAGE_SRC) tests/test_record_pages.c -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS)
test-records:
	+python3 tools/test.py records $(TEST_ARGS)
.PHONY: test-records

# Native source verification prerequisites. The helper has no database/window;
# the isolated worker gate proves cancellation without opening a connection.
SOURCE_DIGEST_TEST = src/data/source_digest.c tests/test_source_digest.c
build/data-tests/test-source-digest: $(SOURCE_DIGEST_TEST) src/data/source_digest.h
	mkdir -p $(@D)
	$(CC) -Isrc $(CFLAGS) $(WARN) -Werror $(SOURCE_DIGEST_TEST) -o $@
build/data-tests/test-source-digest-sanitize: $(SOURCE_DIGEST_TEST) src/data/source_digest.h
	mkdir -p $(@D)
	$(CC) -Isrc -O1 -g $(WARN) -Werror $(SANFLAGS) $(SOURCE_DIGEST_TEST) -o $@
build/data-tests/test-source-digest-unsupported: $(SOURCE_DIGEST_TEST) src/data/source_digest.h
	mkdir -p $(@D)
	$(CC) -Isrc $(CFLAGS) $(WARN) -Werror -D__EMSCRIPTEN__ $(SOURCE_DIGEST_TEST) -o $@
test-source-digest: build/data-tests/test-source-digest build/data-tests/test-source-digest-sanitize build/data-tests/test-source-digest-unsupported
	./build/data-tests/test-source-digest
	ASAN_OPTIONS=halt_on_error=1 ./build/data-tests/test-source-digest-sanitize
	./build/data-tests/test-source-digest-unsupported
.PHONY: test-source-digest
WORKER_SOURCE_DIGEST_TEST = $(CORE) tests/test_worker_source_digest.c
build/data-tests/test-worker-source-digest: $(WORKER_SOURCE_DIGEST_TEST) $(HEADERS)
	mkdir -p $(@D)
	$(CC) -Isrc $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN) -Werror -DDUXCEL_TEST_SOURCE_DIGEST $(WORKER_SOURCE_DIGEST_TEST) -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm
build/data-tests/test-worker-source-digest-sanitize: $(WORKER_SOURCE_DIGEST_TEST) $(HEADERS)
	mkdir -p $(@D)
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) -Werror $(SANFLAGS) -DDUXCEL_TEST_SOURCE_DIGEST $(WORKER_SOURCE_DIGEST_TEST) -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm
test-worker-source-digest: build/data-tests/test-worker-source-digest build/data-tests/test-worker-source-digest-sanitize
	./build/data-tests/test-worker-source-digest
	ASAN_OPTIONS=halt_on_error=1 ./build/data-tests/test-worker-source-digest-sanitize
.PHONY: test-worker-source-digest
RECORD_SELECTION_TEST = $(RECORD_PAGE_SRC) tests/test_record_selection.c
WORKER_RECORD_SELECTION_TEST = $(CORE) tests/test_record_selection_worker.c
build/data-tests/test-record-selection: $(RECORD_SELECTION_TEST) $(HEADERS)
	mkdir -p $(@D)
	$(CC) -Isrc $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN) -Werror $(RECORD_SELECTION_TEST) -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm
build/data-tests/test-record-selection-sanitize: $(RECORD_SELECTION_TEST) $(HEADERS)
	mkdir -p $(@D)
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) -Werror $(SANFLAGS) $(RECORD_SELECTION_TEST) -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm
build/data-tests/test-worker-record-selection: $(WORKER_RECORD_SELECTION_TEST) $(HEADERS)
	mkdir -p $(@D)
	$(CC) -Isrc $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN) -Werror $(WORKER_RECORD_SELECTION_TEST) -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm
build/data-tests/test-worker-record-selection-sanitize: $(WORKER_RECORD_SELECTION_TEST) $(HEADERS)
	mkdir -p $(@D)
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) -Werror $(SANFLAGS) $(WORKER_RECORD_SELECTION_TEST) -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm
test-record-selection: build/data-tests/test-record-selection build/data-tests/test-record-selection-sanitize build/data-tests/test-worker-record-selection build/data-tests/test-worker-record-selection-sanitize
	./build/data-tests/test-record-selection
	ASAN_OPTIONS=halt_on_error=1 ./build/data-tests/test-record-selection-sanitize
	./build/data-tests/test-worker-record-selection
	ASAN_OPTIONS=halt_on_error=1 ./build/data-tests/test-worker-record-selection-sanitize
.PHONY: test-record-selection
WORKBOOK_CODEC_TEST = src/workbook/workbook_codec.c tests/test_workbook_codec.c
build/tests/test_workbook_codec: $(WORKBOOK_CODEC_TEST) $(HEADERS) | build/tests
	$(CC) -Isrc $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN) -Werror $(WORKBOOK_CODEC_TEST) -o $@
build/tests/test_workbook_codec-debug: $(WORKBOOK_CODEC_TEST) $(HEADERS) | build/tests
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) -Werror $(SANFLAGS) $(WORKBOOK_CODEC_TEST) -o $@
WORKBOOK_CODEC_READ_TEST = src/common.c src/transforms/analysis.c src/transforms/number.c src/transforms/pivot.c src/transforms/marks.c src/data/source_digest.c src/workbook/source_scope.c src/workbook/format.c src/workbook/workbook_codec.c src/workbook/workbook_codec_read.c tests/test_workbook_codec_read.c
WORKBOOK_CODEC_READ_OBJECTS = $(addprefix build/obj/release/source-scope/,$(addsuffix .o,$(basename $(WORKBOOK_CODEC_READ_TEST))))
WORKBOOK_CODEC_READ_SAN_OBJECTS = $(addprefix build/obj/sanitize/source-scope/,$(addsuffix .o,$(basename $(WORKBOOK_CODEC_READ_TEST))))
build/tests/test_workbook_codec_read: $(WORKBOOK_CODEC_READ_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(WORKBOOK_CODEC_READ_OBJECTS),-lm)
build/tests/test_workbook_codec_read-debug: $(WORKBOOK_CODEC_READ_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(WORKBOOK_CODEC_READ_SAN_OBJECTS),-lm $(SANFLAGS))
-include $(WORKBOOK_CODEC_READ_OBJECTS:.o=.d) $(WORKBOOK_CODEC_READ_SAN_OBJECTS:.o=.d)
test-workbook-codec-read: build/tests/test_workbook_codec_read build/tests/test_workbook_codec_read-debug
	./build/tests/test_workbook_codec_read
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_workbook_codec_read-debug
.PHONY: test-workbook-codec-read
WORKBOOK_CODEC_NATIVE_OBJECTS = $(addprefix build/obj/release/app/,$(addsuffix .o,$(basename $(CORE)))) build/obj/release/app/src/workbook/workbook_codec_read.o build/obj/release/app/tests/test_workbook_codec_native.o
WORKBOOK_CODEC_NATIVE_SAN_OBJECTS = $(addprefix build/obj/sanitize/app/,$(addsuffix .o,$(basename $(CORE)))) build/obj/sanitize/app/src/workbook/workbook_codec_read.o build/obj/sanitize/app/tests/test_workbook_codec_native.o
build/tests/test_workbook_codec_native: $(WORKBOOK_CODEC_NATIVE_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(WORKBOOK_CODEC_NATIVE_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_workbook_codec_native-debug: $(WORKBOOK_CODEC_NATIVE_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(WORKBOOK_CODEC_NATIVE_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(WORKBOOK_CODEC_NATIVE_OBJECTS:.o=.d) $(WORKBOOK_CODEC_NATIVE_SAN_OBJECTS:.o=.d)

test-workbook-codec-native: build/tests/test_workbook_codec_native build/tests/test_workbook_codec_native-debug
	./build/tests/test_workbook_codec_native
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_workbook_codec_native-debug
.PHONY: test-workbook-codec-native
test-workbook-codec: build/tests/test_workbook_codec build/tests/test_workbook_codec-debug
	./build/tests/test_workbook_codec
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_workbook_codec-debug
.PHONY: test-workbook-codec
CORE_TEST_OBJECTS = $(addprefix build/obj/release/app/,$(addsuffix .o,$(basename $(CORE))))
CORE_TEST_SAN_OBJECTS = $(addprefix build/obj/sanitize/app/,$(addsuffix .o,$(basename $(CORE))))
WORKBOOK_TEST_OBJECTS = $(CORE_TEST_OBJECTS) build/obj/release/app/tests/test_workbook.o
WORKBOOK_TEST_SAN_OBJECTS = $(CORE_TEST_SAN_OBJECTS) build/obj/sanitize/app/tests/test_workbook.o
build/tests/test_workbook: $(WORKBOOK_TEST_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(WORKBOOK_TEST_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_workbook-debug: $(WORKBOOK_TEST_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(WORKBOOK_TEST_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(WORKBOOK_TEST_OBJECTS:.o=.d) $(WORKBOOK_TEST_SAN_OBJECTS:.o=.d)
test-analysis-journal: build/tests/test_workbook build/tests/test_workbook-debug
	./build/tests/test_workbook --journal-only
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_workbook-debug --journal-only
.PHONY: test-analysis-journal
test-format-budget: build/tests/test_workbook build/tests/test_workbook-debug
	./build/tests/test_workbook --format-budget-only
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_workbook-debug --format-budget-only
.PHONY: test-format-budget
CORE_ENGINE_TEST_OBJECTS = $(CORE_TEST_OBJECTS) build/obj/release/app/tests/test_core.o
CORE_ENGINE_TEST_SAN_OBJECTS = $(CORE_TEST_SAN_OBJECTS) build/obj/sanitize/app/tests/test_core.o
build/tests/test_core: $(CORE_ENGINE_TEST_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(CORE_ENGINE_TEST_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_core-debug: $(CORE_ENGINE_TEST_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(CORE_ENGINE_TEST_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(CORE_ENGINE_TEST_OBJECTS:.o=.d) $(CORE_ENGINE_TEST_SAN_OBJECTS:.o=.d)
SHADOWVIEW_SRC = src/ui/shadowview.c tests/test_shadowview.c
# The frozen pixel oracle remains a test fixture; it never enters App sources.
build/tests/test_shadowview: $(SHADOWVIEW_SRC) tests/fixtures/rendering/shadow_reference.h tests/fixtures/rendering/shadow_corner_reference.h $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(SHADOWVIEW_SRC) -o $@ -lm
build/tests/test_shadowview-debug: $(SHADOWVIEW_SRC) tests/fixtures/rendering/shadow_reference.h tests/fixtures/rendering/shadow_corner_reference.h $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(SHADOWVIEW_SRC) -o $@ -lm $(SANFLAGS)
INSPECTOR_SRC = src/ui/inspector.c tests/test_inspector.c
STEPVIEW_SRC = src/common.c src/platform/platform.c src/transforms/number.c src/transforms/analysis.c src/ui/stepview.c tests/test_stepview.c
PREVIEW_SRC = $(CORE) src/ui/motion.c src/ui/fontmetrics.c src/ui/paintlayer.c src/ui/paintlayer_backend.c src/ui/ui.c src/ui/preview.c src/ui/stepview.c src/grid/grid.c src/app/step_preview.c tests/test_preview.c
STATSVIEW_SRC = src/common.c src/transforms/number.c src/workbook/format.c src/ui/motion.c src/ui/fontmetrics.c src/ui/paintlayer.c src/ui/paintlayer_backend.c src/ui/ui.c src/ui/statsview.c tests/test_statsview.c
GUIDEVIEW_SRC = src/common.c src/transforms/number.c src/workbook/format.c src/ui/motion.c src/ui/fontmetrics.c src/ui/paintlayer.c src/ui/paintlayer_backend.c src/ui/ui.c src/ui/textview.c src/ui/guideview.c tests/test_guideview.c
FONTMETRICS_SRC = src/ui/fontmetrics.c tests/test_fontmetrics.c
UI_FONT_FALLBACK_SRC = src/common.c src/ui/fontmetrics.c src/ui/ui.c tests/test_ui_font_fallback.c
CHOICEVIEW_SRC = src/ui/choiceview.c src/ui/selectview.c tests/test_choiceview.c
build/tests/test_choiceview: $(CHOICEVIEW_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(CHOICEVIEW_SRC) -o $@ $(RAYLIB_LIBS) -lm
build/tests/test_choiceview-debug: $(CHOICEVIEW_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(CHOICEVIEW_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
# Small actual-module input/paint recorders; never linked into App binaries.
PREVIEW_INPUT_SRC = src/ui/preview.c tests/test_preview_input.c
TEXTVIEW_INPUT_SRC = src/common.c src/ui/textview.c tests/test_textview.c
build/tests/test_preview_input: $(PREVIEW_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(PREVIEW_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm
build/tests/test_preview_input-debug: $(PREVIEW_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(PREVIEW_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
build/tests/test_preview_input_browser: $(PREVIEW_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -DDUXCEL_BROWSER_HOST $(CFLAGS) $(WARN) $(PREVIEW_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm
build/tests/test_preview_input_browser-debug: $(PREVIEW_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -DDUXCEL_BROWSER_HOST -O1 -g $(WARN) $(SANFLAGS) $(PREVIEW_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
build/tests/test_textview: $(TEXTVIEW_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(TEXTVIEW_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm
build/tests/test_textview-debug: $(TEXTVIEW_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(TEXTVIEW_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
build/tests/test_textview_review: $(TEXTVIEW_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -DDUXCEL_DEV_REVIEW $(CFLAGS) $(WARN) $(TEXTVIEW_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm
build/tests/test_textview_review-debug: $(TEXTVIEW_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -DDUXCEL_DEV_REVIEW -O1 -g $(WARN) $(SANFLAGS) $(TEXTVIEW_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
# Actual shared UI input, with section collection only in these test binaries.
UI_INPUT_LINK = -ffunction-sections -Wl,--gc-sections
ifeq ($(shell uname -s),Darwin)
UI_INPUT_LINK = -ffunction-sections -Wl,-dead_strip
endif
STATS_INPUT_SRC = src/ui/ui.c src/ui/statsview.c tests/test_statsview_input.c
UI_UPDATE_INPUT_SRC = src/common.c src/ui/fontmetrics.c src/ui/ui.c tests/test_ui_update_input.c
GUIDE_INPUT_SRC = src/common.c src/ui/ui.c src/ui/textview.c src/ui/motion.c src/ui/guideview.c tests/test_guideview_input.c
SORT_INPUT_SRC = src/common.c src/ui/ui.c src/ui/motion.c src/transforms/analysis.c src/ui/sortview.c tests/test_sortview_input.c
build/tests/test_sortview_input: $(SORT_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(UI_INPUT_LINK) $(SORT_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm
build/tests/test_sortview_input-debug: $(SORT_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(UI_INPUT_LINK) $(SORT_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
build/tests/test_sortview_input_review: $(SORT_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -DDUXCEL_DEV_REVIEW -DDUXCEL_REVIEW_INPUT_IMPLEMENTATION -include src/dev/review_input.h $(CFLAGS) $(WARN) $(UI_INPUT_LINK) $(SORT_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm
build/tests/test_sortview_input_review-debug: $(SORT_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -DDUXCEL_DEV_REVIEW -DDUXCEL_REVIEW_INPUT_IMPLEMENTATION -include src/dev/review_input.h -O1 -g $(WARN) $(SANFLAGS) $(UI_INPUT_LINK) $(SORT_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
build/tests/test_guideview_input: $(GUIDE_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(UI_INPUT_LINK) $(GUIDE_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm
build/tests/test_guideview_input-debug: $(GUIDE_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(UI_INPUT_LINK) $(GUIDE_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
build/tests/test_guideview_input_review: $(GUIDE_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -DDUXCEL_DEV_REVIEW -DDUXCEL_REVIEW_INPUT_IMPLEMENTATION -include src/dev/review_input.h $(CFLAGS) $(WARN) $(UI_INPUT_LINK) $(GUIDE_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm
build/tests/test_guideview_input_review-debug: $(GUIDE_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -DDUXCEL_DEV_REVIEW -DDUXCEL_REVIEW_INPUT_IMPLEMENTATION -include src/dev/review_input.h -O1 -g $(WARN) $(SANFLAGS) $(UI_INPUT_LINK) $(GUIDE_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
build/tests/test_statsview_input: $(STATS_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(UI_INPUT_LINK) $(STATS_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm
build/tests/test_statsview_input-debug: $(STATS_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(UI_INPUT_LINK) $(STATS_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
build/tests/test_statsview_input_review: $(STATS_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -DDUXCEL_DEV_REVIEW -DDUXCEL_REVIEW_INPUT_IMPLEMENTATION -include src/dev/review_input.h $(CFLAGS) $(WARN) $(UI_INPUT_LINK) $(STATS_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm
build/tests/test_statsview_input_review-debug: $(STATS_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -DDUXCEL_DEV_REVIEW -DDUXCEL_REVIEW_INPUT_IMPLEMENTATION -include src/dev/review_input.h -O1 -g $(WARN) $(SANFLAGS) $(UI_INPUT_LINK) $(STATS_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
build/tests/test_ui_update_input: $(UI_UPDATE_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(UI_INPUT_LINK) $(UI_UPDATE_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm
build/tests/test_ui_update_input-debug: $(UI_UPDATE_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(UI_INPUT_LINK) $(UI_UPDATE_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
build/tests/test_ui_update_input_browser: $(UI_UPDATE_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -DDUXCEL_BROWSER_HOST -U__APPLE__ $(CFLAGS) $(WARN) $(UI_INPUT_LINK) $(UI_UPDATE_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm
build/tests/test_ui_update_input_browser-debug: $(UI_UPDATE_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -DDUXCEL_BROWSER_HOST -U__APPLE__ -O1 -g $(WARN) $(SANFLAGS) $(UI_INPUT_LINK) $(UI_UPDATE_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
build/tests/test_ui_update_input_control: $(UI_UPDATE_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -U__APPLE__ $(CFLAGS) $(WARN) $(UI_INPUT_LINK) $(UI_UPDATE_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm
build/tests/test_ui_update_input_control-debug: $(UI_UPDATE_INPUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -U__APPLE__ -O1 -g $(WARN) $(SANFLAGS) $(UI_INPUT_LINK) $(UI_UPDATE_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
MOTION_SRC = src/ui/motion.c tests/test_motion.c
build/tests/test_motion: $(MOTION_SRC) src/ui/motion.h | build/tests
	$(CC) -Isrc $(CFLAGS) $(WARN) $(MOTION_SRC) -o $@ -lm
build/tests/test_motion-debug: $(MOTION_SRC) src/ui/motion.h | build/tests
	$(CC) -Isrc -O1 -g $(WARN) $(SANFLAGS) $(MOTION_SRC) -o $@ -lm $(SANFLAGS)
build/tests/test_ui_font_fallback: $(UI_FONT_FALLBACK_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(UI_INPUT_LINK) $(UI_FONT_FALLBACK_SRC) -o $@ $(RAYLIB_LIBS) -lm
build/tests/test_ui_font_fallback-debug: $(UI_FONT_FALLBACK_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(UI_INPUT_LINK) $(UI_FONT_FALLBACK_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
build/tests/test_fontmetrics: $(FONTMETRICS_SRC) $(HEADERS) | build/tests
	$(CC) -Isrc $(CFLAGS) $(WARN) $(FONTMETRICS_SRC) -o $@ -lm
build/tests/test_fontmetrics-debug: $(FONTMETRICS_SRC) $(HEADERS) | build/tests
	$(CC) -Isrc -O1 -g $(WARN) $(SANFLAGS) $(FONTMETRICS_SRC) -o $@ -lm $(SANFLAGS)
build/tests/test_guideview: $(GUIDEVIEW_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(GUIDEVIEW_SRC) -o $@ $(RAYLIB_LIBS) -lm
build/tests/test_guideview-debug: $(GUIDEVIEW_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(GUIDEVIEW_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
build/tests/test_statsview: $(STATSVIEW_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(STATSVIEW_SRC) -o $@ $(RAYLIB_LIBS) -lm
build/tests/test_statsview-debug: $(STATSVIEW_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(STATSVIEW_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
build/tests/test_preview: $(PREVIEW_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(PREVIEW_SRC) -o $@ $(LDLIBS)
build/tests/test_preview-debug: $(PREVIEW_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(PREVIEW_SRC) -o $@ $(LDLIBS) $(SANFLAGS)
build/tests/test_inspector: $(INSPECTOR_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(INSPECTOR_SRC) -o $@ -lm
build/tests/test_inspector-debug: $(INSPECTOR_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(INSPECTOR_SRC) -o $@ -lm $(SANFLAGS)
build/tests/test_stepview: $(STEPVIEW_SRC) $(HEADERS) | build/tests
	$(CC) -Isrc $(CFLAGS) $(WARN) $(STEPVIEW_SRC) -o $@ $(THREAD_LIBS) -lm
build/tests/test_stepview-debug: $(STEPVIEW_SRC) $(HEADERS) | build/tests
	$(CC) -Isrc -O1 -g $(WARN) $(SANFLAGS) $(STEPVIEW_SRC) -o $@ $(THREAD_LIBS) -lm $(SANFLAGS)
build/tests/test_paintlayer: src/ui/paintlayer.c tests/test_paintlayer.c $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) src/ui/paintlayer.c tests/test_paintlayer.c -o $@ -lm
build/tests/test_paintlayer-debug: src/ui/paintlayer.c tests/test_paintlayer.c $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) src/ui/paintlayer.c tests/test_paintlayer.c -o $@ -lm $(SANFLAGS)
PAINT_NATIVE_SRC = src/common.c src/transforms/number.c src/workbook/format.c src/ui/motion.c src/ui/fontmetrics.c src/ui/paintlayer.c src/ui/paintlayer_backend.c src/ui/ui.c tests/test_paint_native.c
build/tests/test_paint-native-debug: $(PAINT_NATIVE_SRC) $(HEADERS) | build/tests/assets
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(PAINT_NATIVE_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
paint-smoke: build/tests/test_paint-native-debug
	mkdir -p build/tests/results/native-authoring
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_paint-native-debug
test: build/tests/test_keytips build/tests/test_cellreadout build/tests/test_core build/tests/test_grid build/tests/test_workbook build/tests/test_brand build/tests/test_format build/tests/test_chartview build/tests/test_chartencoding build/tests/test_shadowview build/tests/test_inspector build/tests/test_stepview build/tests/test_preview build/tests/test_statsview build/tests/test_guideview build/tests/test_fontmetrics build/tests/test_ui_font_fallback build/tests/test_motion build/tests/test_choiceview icons-check build/tests/test_paintlayer build/tests/test_popup build/tests/test_marksview build/tests/test_preview_input build/tests/test_preview_input_browser build/tests/test_textview build/tests/test_textview_review build/tests/test_statsview_input build/tests/test_statsview_input_review build/tests/test_ui_update_input build/tests/test_ui_update_input_browser build/tests/test_ui_update_input_control build/tests/test_guideview_input build/tests/test_guideview_input_review build/tests/test_sortview_input build/tests/test_sortview_input_review
	mkdir -p build/tests/generated/workspace-old build/tests/generated/workspace-new
	./build/tests/test_filterlabel
	./build/tests/test_cellreadout
	./build/tests/test_core
	./build/tests/test_grid
	./build/tests/test_keytips
	./build/tests/test_workbook
	./build/tests/test_brand
	./build/tests/test_format
	./build/tests/test_chartview
	./build/tests/test_chartencoding
	./build/tests/test_shadowview
	./build/tests/test_inspector
	./build/tests/test_stepview
	./build/tests/test_preview
	./build/tests/test_statsview
	./build/tests/test_guideview
	./build/tests/test_fontmetrics
	./build/tests/test_ui_font_fallback
	./build/tests/test_motion
	./build/tests/test_choiceview
	./build/tests/test_paintlayer
	./build/tests/test_popup
	./build/tests/test_marksview
	./build/tests/test_preview_input
	./build/tests/test_preview_input_browser
	./build/tests/test_textview
	./build/tests/test_textview_review
	./build/tests/test_statsview_input
	./build/tests/test_statsview_input_review
	./build/tests/test_ui_update_input
	./build/tests/test_ui_update_input_browser
	./build/tests/test_ui_update_input_control
	./build/tests/test_guideview_input
	./build/tests/test_guideview_input_review
	./build/tests/test_sortview_input
	./build/tests/test_sortview_input_review
test-debug: build/tests/test_keytips-debug build/tests/test_cellreadout-debug build/tests/test_core-debug build/tests/test_grid-debug build/tests/test_workbook-debug build/tests/test_brand-debug build/tests/test_format-debug build/tests/test_chartview-debug build/tests/test_chartencoding-debug build/tests/test_shadowview-debug build/tests/test_inspector-debug build/tests/test_stepview-debug build/tests/test_preview-debug build/tests/test_statsview-debug build/tests/test_guideview-debug build/tests/test_fontmetrics-debug build/tests/test_ui_font_fallback-debug build/tests/test_motion-debug build/tests/test_choiceview-debug icons-check build/tests/test_paintlayer-debug build/tests/test_popup-debug build/tests/test_marksview-debug build/tests/test_preview_input-debug build/tests/test_preview_input_browser-debug build/tests/test_textview-debug build/tests/test_textview_review-debug build/tests/test_statsview_input-debug build/tests/test_statsview_input_review-debug build/tests/test_ui_update_input-debug build/tests/test_ui_update_input_browser-debug build/tests/test_ui_update_input_control-debug build/tests/test_guideview_input-debug build/tests/test_guideview_input_review-debug build/tests/test_sortview_input-debug build/tests/test_sortview_input_review-debug
	mkdir -p build/tests/generated/workspace-old build/tests/generated/workspace-new
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_filterlabel-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_cellreadout-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_core-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_grid-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_keytips-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_workbook-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_brand-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_format-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_chartview-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_chartencoding-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_shadowview-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_inspector-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_stepview-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_preview-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_statsview-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_guideview-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_fontmetrics-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_ui_font_fallback-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_motion-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_choiceview-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_paintlayer-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_popup-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_marksview-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_preview_input-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_preview_input_browser-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_textview-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_textview_review-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_statsview_input-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_statsview_input_review-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_ui_update_input-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_ui_update_input_browser-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_ui_update_input_control-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_guideview_input-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_guideview_input_review-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_sortview_input-debug
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_sortview_input_review-debug
spill-test: build/tests/test_cellreadout build/tests/test_core build/tests/test_core-debug
	./build/tests/test_core --spill-probe
	TMPDIR=/ ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_core-debug --spill-probe
unique-large-test: build/tests/test_cellreadout build/tests/test_core build/tests/test_core-debug
	test -f data/downloads/yellow_tripdata_2025-01.parquet
	./build/tests/test_core --unique-large data/downloads/yellow_tripdata_2025-01.parquet
	TMPDIR=/ ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_core-debug --unique-large data/downloads/yellow_tripdata_2025-01.parquet
smoke: build/bin/duxcel-debug
	./build/bin/duxcel-debug --smoke --frames 240 --screenshot /tmp/duxcel-smoke.png
workspace-smoke: build/bin/duxcel-debug
	mkdir -p build/tests/generated
	ASAN_OPTIONS=halt_on_error=1 ./build/bin/duxcel-debug --workspace-smoke --frames 240 --screenshot /tmp/duxcel-workspace-smoke.png
filter-smoke: build/bin/duxcel-debug
	mkdir -p build/tests/results/filter-keyboard
	ASAN_OPTIONS=halt_on_error=1 ./build/bin/duxcel-debug --filter-smoke --frames 6200 --screenshot /tmp/duxcel-filter-smoke.png
pivot-smoke: build/bin/duxcel-debug
	mkdir -p build/tests/results
	ASAN_OPTIONS=halt_on_error=1 ./build/bin/duxcel-debug --pivot-smoke --frames 4800 --screenshot build/tests/results/pivot-smoke.png
export-smoke: build/bin/duxcel-debug
	mkdir -p build/tests/generated
	ASAN_OPTIONS=halt_on_error=1 ./build/bin/duxcel-debug --export-smoke --frames 840 --screenshot /tmp/duxcel-export-smoke.png
count-smoke: build/bin/duxcel-debug
	mkdir -p build/tests/generated
	ASAN_OPTIONS=halt_on_error=1 ./build/bin/duxcel-debug --count-smoke --frames 720 --screenshot /tmp/duxcel-count-smoke.png
clipboard-smoke: build/bin/duxcel-debug
	mkdir -p build/tests/generated
	ASAN_OPTIONS=halt_on_error=1 ./build/bin/duxcel-debug --clipboard-smoke --frames 720 --screenshot /tmp/duxcel-clipboard-smoke.png
sort-smoke: build/bin/duxcel-debug
	mkdir -p build/tests/generated build/tests/results/multisort
	ASAN_OPTIONS=halt_on_error=1 ./build/bin/duxcel-debug --sort-smoke --frames 720 --screenshot /tmp/duxcel-sort-smoke.png
layout-smoke: build/bin/duxcel-debug design-fixture
	mkdir -p build/tests/generated build/tests/results/workspace-layout
	ASAN_OPTIONS=halt_on_error=1 ./build/bin/duxcel-debug --layout-smoke --frames 20000 --screenshot /tmp/duxcel-layout-smoke.png
navigation-smoke: build/bin/duxcel-debug
	mkdir -p build/tests/generated build/tests/results/sheet-navigation
	ASAN_OPTIONS=halt_on_error=1 ./build/bin/duxcel-debug --navigation-smoke --frames 720 --screenshot /tmp/duxcel-navigation-smoke.png
startup-smoke: navigation-smoke
	ASAN_OPTIONS=halt_on_error=1 ./build/bin/duxcel-debug build/tests/generated/navigation.dxls --frames 180
	cp tests/fixtures/null-text.csv build/tests/generated/invalid-workspace.dxls
	ASAN_OPTIONS=halt_on_error=1 ./build/bin/duxcel-debug build/tests/generated/invalid-workspace.dxls --frames 60
nyc-smoke: build/bin/duxcel-debug
	test -f data/downloads/yellow_tripdata_2025-01.parquet
	mkdir -p build/tests/generated build/tests/results/current-workspace
	ASAN_OPTIONS=halt_on_error=1 ./build/bin/duxcel-debug --nyc-smoke --frames 720 --screenshot build/tests/results/current-workspace/nyc-analysis.png
document-smoke: build/bin/duxcel-debug
	mkdir -p build/tests/generated build/tests/results/current-workspace
	ASAN_OPTIONS=halt_on_error=1 ./build/bin/duxcel-debug --document-smoke --frames 2400 --screenshot build/tests/results/current-workspace/document-guard.png
chart-smoke: build/bin/duxcel-debug
	mkdir -p build/tests/generated build/tests/results/chart-marks
	ASAN_OPTIONS=halt_on_error=1 ./build/bin/duxcel-debug --chart-smoke --frames 4800 --screenshot build/tests/results/chart-marks/final.png
statistics-smoke: build/bin/duxcel-debug
	mkdir -p build/tests/generated build/tests/results/selection-statistics
	ASAN_OPTIONS=halt_on_error=1 ./build/bin/duxcel-debug --statistics-smoke --frames 2500 --screenshot build/tests/results/selection-statistics/final.png
table-smoke: build/bin/duxcel-debug build/tests/test_core
	mkdir -p build/tests/generated build/tests/results/table-sources
	if test ! -f build/tests/generated/million.parquet || test ! -f "build/tests/generated/odd'name.parquet" || test ! -f build/tests/generated/fixture.duckdb; then ./build/tests/test_core; fi
	ASAN_OPTIONS=halt_on_error=1 ./build/bin/duxcel-debug --table-smoke --frames 7200 --screenshot build/tests/results/table-sources/final.png
bundle: build/bin/duxcel
	mkdir -p build/Duxcel.app/Contents/MacOS build/Duxcel.app/Contents/Resources
	cp build/bin/duxcel build/Duxcel.app/Contents/MacOS/duxcel.new
	mv build/Duxcel.app/Contents/MacOS/duxcel.new build/Duxcel.app/Contents/MacOS/duxcel
	cp -R assets build/Duxcel.app/Contents/Resources/
	cp packaging/Info.plist build/Duxcel.app/Contents/
	cp assets/icons/duxcel.icns build/Duxcel.app/Contents/Resources/
ifeq ($(shell uname -s),Darwin)
	codesign --force --sign - build/Duxcel.app
endif
clean:
	python3 tools/workspace.py clean
distclean:
	python3 tools/workspace.py distclean
.PHONY: all run debug run-debug test test-debug spill-test unique-large-test smoke workspace-smoke filter-smoke pivot-smoke export-smoke count-smoke clipboard-smoke sort-smoke layout-smoke navigation-smoke startup-smoke nyc-smoke document-smoke chart-smoke statistics-smoke table-smoke icon-smoke query-smoke unique-smoke mac-icon-check bundle clean icons icons-check icons-verify paint-smoke
# Only the incremental native graph is admitted to parallel compilation for now.
# Legacy module-test recipes compile whole source lists and some execution goals
# mutate shared fixtures or desktop state. Make 3.81 cannot serialize individual
# targets with .NOTPARALLEL prerequisites, so those invocations remain serial.
PARALLEL_NATIVE_GOALS = build/tests/test_column_widths build/tests/test_column_widths-debug build/tests/test_column_width_history build/tests/test_column_width_history-debug build/tests/test_definition_origin-gui build/tests/test_sheet_lifecycle_command build/tests/test_sheet_lifecycle_command-debug build/tests/test_range_command-gui build/tests/test_cell_range_plan build/tests/test_cell_range_plan-debug build/tests/test_range_event build/tests/test_range_event-debug build/tests/test_range_command build/tests/test_range_command-debug build/tests/test_history_range build/tests/test_history_range-debug build/tests/test_definition_origin build/tests/test_definition_origin-debug build/tests/test_sheet_lifecycle build/tests/test_sheet_lifecycle-debug build/tests/test_object_import_draft build/tests/test_object_import_draft-debug build/tests/test_object_create_command-gui build/tests/test_object_create_command build/tests/test_object_create_command-debug build/tests/test_source_format_command build/tests/test_source_format_command-debug build/tests/test_object_move_command-gui build/tests/test_object_move_command build/tests/test_object_move_command-debug build/tests/test_object_delete_command-gui build/tests/test_object_delete_command build/tests/test_object_delete_command-debug build/tests/test_object_binding_command-gui build/tests/test_object_binding_command build/tests/test_object_binding_command-debug build/tests/test_primary_replacement-gui build/tests/test_primary_replacement build/tests/test_primary_replacement-debug build/tests/test_table_replacement-gui build/tests/test_table_replacement build/tests/test_table_replacement-debug build/tests/test_object_pivot_command-gui build/tests/test_object_pivot_command build/tests/test_object_pivot_command-debug build/tests/test_object_layout_command-gui build/tests/test_object_layout_command build/tests/test_object_layout_command-debug build/tests/test_object_marks_command-gui build/tests/test_object_marks_command build/tests/test_object_marks_command-debug build/tests/test_object_presentation_command-gui build/tests/test_object_presentation_command build/tests/test_object_presentation_command-debug build/tests/test_object_name_command-gui build/tests/test_object_name_command build/tests/test_object_name_command-debug build/tests/test_table_command-gui build/tests/test_table_command build/tests/test_table_command-debug build/tests/test_object_source_plan build/tests/test_object_source_plan-debug build/tests/test_primary_steps-gui build/tests/test_primary_query-gui build/tests/test_primary_transforms-gui build/tests/test_analysis_command-gui build/tests/test_analysis_command build/tests/test_analysis_command-debug build/tests/test_definition_baseline build/tests/test_definition_baseline-debug build/tests/test_history_definition build/tests/test_history_definition-debug build/tests/test_definition_event build/tests/test_definition_event-debug build/tests/test_workbook_codec_read build/tests/test_workbook_codec_read-debug build/tests/test_definition_state build/tests/test_definition_state-debug build/tests/test_definition_dependencies build/tests/test_definition_dependencies-debug build/tests/test_objects_asset build/tests/test_objects_asset-debug build/tests/test_marks_asset build/tests/test_marks_asset-debug build/tests/test_pivot_asset build/tests/test_pivot_asset-debug build/tests/test_analysis_asset build/tests/test_analysis_asset-debug build/tests/test_format_command build/tests/test_format_command-debug build/tests/test_history_format build/tests/test_history_format-debug build/tests/test_format_dependencies build/tests/test_format_dependencies-debug build/tests/test_source_versions_codec build/tests/test_source_versions_codec-debug build/tests/test_format build/tests/test_format-debug build/tests/test_source_scope build/tests/test_source_scope-debug build/tests/test_format_delta_codec build/tests/test_format_delta_codec-debug build/tests/test_format_plan-gui build/tests/test_history_stream build/tests/test_history_stream-debug build/tests/test_cell_delta_codec build/tests/test_cell_delta_codec-debug build/tests/test_cell_delta build/tests/test_cell_delta-debug build/tests/test_history_path build/tests/test_history_path-debug build/tests/test_history_anchors build/tests/test_history_anchors-debug build/tests/test_history_preview-gui-debug build/tests/test_history_window build/tests/test_history_window-debug build/tests/test_history_preview-gui build/tests/test_app_preview build/tests/test_app_preview-debug build/tests/test_file_conflict build/tests/test_file_conflict-debug build/tests/test_preview_worker build/tests/test_preview_worker-debug build/tests/test_version_preview build/tests/test_version_preview-debug build/tests/test_version_controls-gui build/tests/test_version_tag_save build/tests/test_version_tag_save-debug build/tests/test_version_tag build/tests/test_version_tag-debug build/tests/test_version_checkpoint build/tests/test_version_checkpoint-debug build/tests/test_app_versions build/tests/test_app_versions-debug build/tests/test_worker_archive build/tests/test_worker_archive-debug build/tests/test_worker_native build/tests/test_worker_native-debug build/tests/test_record_selection_worker build/tests/test_record_selection_worker-debug build/tests/test_worker_source_digest build/tests/test_worker_source_digest-debug build/tests/test_version_lifetime build/tests/test_version_lifetime-debug build/tests/test_version_save build/tests/test_version_save-debug build/tests/test_archive_write build/tests/test_archive_write-debug build/tests/test_archive_read build/tests/test_archive_read-debug build/tests/test_version_restore build/tests/test_version_restore-debug build/tests/test_core_restore build/tests/test_core_restore-debug build/tests/test_version_read build/tests/test_version_read-debug build/tests/test_version_stage build/tests/test_version_stage-debug build/tests/test_version_chunks build/tests/test_version_chunks-debug build/tests/test_core_compare build/tests/test_core_compare-debug build/tests/test_core_payload build/tests/test_core_payload-debug build/tests/test_workbook_payload build/tests/test_workbook_payload-debug all debug duxcel duxcel-debug bundle review review-debug build/tests/test_table_origin-gui build/tests/test_column_width-gui build/tests/test_primary_source_draft build/tests/test_primary_source_draft-debug build/tests/test_sheet_presence_asset build/tests/test_sheet_presence_asset-debug \
	build/tests/test_core build/tests/test_core-debug build/tests/test_workbook_schema build/tests/test_workbook_schema-debug build/tests/test_version_index build/tests/test_version_index-debug build/tests/test_workbook build/tests/test_workbook-debug build/tests/test_workbook_identity build/tests/test_workbook_identity-debug build/tests/test_workbook_identity_native build/tests/test_workbook_identity_native-debug build/tests/test_workbook_identity-gui \
	build/tests/test_workbook_codec_native build/tests/test_workbook_codec_native-debug \
	build/tests/test_workbook_history build/tests/test_workbook_history-debug build/tests/test_workbook_history-gui \
	build/tests/test_workbook_lifecycle build/tests/test_workbook_lifecycle-debug build/tests/test_workbook_lifecycle-gui \
	build/tests/test_workbook_journal build/tests/test_workbook_journal-debug build/tests/test_journal_queue build/tests/test_journal_queue-debug build/tests/test_journal_session build/tests/test_journal_session-debug build/tests/test_journal_session-gui build/tests/test_journal_performance build/tests/test_journal_performance-debug build/tests/test_workbook_save build/tests/test_workbook_save-debug build/tests/test_worker_save build/tests/test_worker_save-debug build/tests/test_workbook_save-gui build/tests/test_file_lease build/tests/test_file_lease-debug build/tests/test_workbook_recovery build/tests/test_workbook_recovery-debug build/tests/test_directory_cursor build/tests/test_directory_cursor-debug build/tests/test_recovery_catalog build/tests/test_recovery_catalog-debug build/tests/test_recovery_worker build/tests/test_recovery_worker-debug build/tests/test_recovery-gui build/tests/test_recovery_process build/tests/test_recovery_process-debug \
	build/tests/test_cell_command build/tests/test_cell_command-debug build/tests/test_cell_command-native \
	build/bin/duxcel build/bin/duxcel-debug build/edit-sanitize/duxcel \
	build/review/duxcel-review build/review-sanitize/duxcel-review \
	build/lifecycle/test-app build/lifecycle/test-app-sanitize build/tests/test_fixture build/tests/test_fixture-debug
PARALLEL_NATIVE_GOALS += build/tests/test_paste_command-gui build/tests/test_mixed_range-gui build/tests/test_clear_range-gui build/tests/test_mixed_rows-gui build/tests/test_mixed_columns-gui build/tests/test_template_cells-gui
PARALLEL_NATIVE_GOALS += build/tests/test_source_versions_admission build/tests/test_source_versions_admission-debug
PARALLEL_NATIVE_GOALS += build/tests/test_source_dependency_admission build/tests/test_source_dependency_admission-debug
ifneq ($(strip $(filter-out $(PARALLEL_NATIVE_GOALS) build/obj/%,$(MAKECMDGOALS))),)
.NOTPARALLEL:
endif

# Short edit loop, a named native slice, or the recorded full integration tier.
# Incremental native builds use BUILD_JOBS slots. Legacy module builds and
# mutable engine/desktop checks remain serialized; TEST_ARGS may set --jobs 2
# for independent already-built headless checks. See tools/test.py --list.
test-fast:
	+python3 tools/test.py fast $(TEST_ARGS)
test-feature:
	+python3 tools/test.py feature --feature $(FEATURE) $(TEST_ARGS)
test-engines:
	+python3 tools/test.py engines $(TEST_ARGS)
test-milestone:
	+python3 tools/test.py full $(TEST_ARGS)
.PHONY: test-feature test-milestone test-engines
test-affected:
	+python3 tools/test.py affected --feature $(FEATURE) $(TEST_ARGS)
test-native:
	+python3 tools/test.py native --scenario $(SCENARIO) $(TEST_ARGS)

build/tests/test_background-native-debug: tests/test_background_native.m | build/tests
	$(CC) $(RAYLIB_CFLAGS) -O1 -g $(WARN) $(SANFLAGS) $< -o $@ $(RAYLIB_LIBS) -framework AppKit $(SANFLAGS)
build/tests/native-alert-guard.dylib: tests/support/native_alert_guard.m | build/tests
	$(CC) $(WARN) -O1 -g -dynamiclib $< -o $@ -framework AppKit
test-full:
	+python3 tools/test.py full $(TEST_ARGS)
test-runner:
	python3 -m tools.tests.test_runner
test-build:
	python3 -m tools.tests.test_build
.PHONY: test-fast test-affected test-native test-full test-runner test-build


build/tests/test_popup: src/ui/motion.c src/ui/popup.c tests/test_popup.c $(HEADERS) | build/tests
	$(CC) -Isrc $(CFLAGS) $(WARN) src/ui/motion.c src/ui/popup.c tests/test_popup.c -o $@ -lm

build/tests/test_popup-debug: src/ui/motion.c src/ui/popup.c tests/test_popup.c $(HEADERS) | build/tests
	$(CC) -Isrc -O1 -g $(WARN) $(SANFLAGS) src/ui/motion.c src/ui/popup.c tests/test_popup.c -o $@ -lm $(SANFLAGS)


build/tests/test_marksview: src/ui/marksview.c tests/test_marksview.c $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) src/ui/marksview.c tests/test_marksview.c -o $@ -lm

build/tests/test_marksview-debug: src/ui/marksview.c tests/test_marksview.c $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) src/ui/marksview.c tests/test_marksview.c -o $@ -lm $(SANFLAGS)

CELLREADOUT_SRC = src/ui/cellreadout.c tests/test_cellreadout.c
build/tests/test_cellreadout: $(CELLREADOUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(CELLREADOUT_SRC) -o $@
build/tests/test_cellreadout-debug: $(CELLREADOUT_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(CELLREADOUT_SRC) -o $@ $(SANFLAGS)

FILTERLABEL_SRC = src/common.c src/transforms/number.c src/transforms/analysis.c src/ui/filterlabel.c tests/test_filterlabel.c
build/tests/test_filterlabel: $(FILTERLABEL_SRC) src/ui/filterlabel.h src/transforms/analysis.h src/transforms/number.h src/common.h | build/tests
	$(CC) -Isrc $(CFLAGS) $(WARN) $(FILTERLABEL_SRC) -o $@ -lm
build/tests/test_filterlabel-debug: $(FILTERLABEL_SRC) src/ui/filterlabel.h src/transforms/analysis.h src/transforms/number.h src/common.h | build/tests
	$(CC) -Isrc -O1 -g $(WARN) $(SANFLAGS) $(FILTERLABEL_SRC) -o $@ -lm $(SANFLAGS)
test: build/tests/test_filterlabel build/tests/test_statsview_input build/tests/test_statsview_input_review build/tests/test_ui_update_input build/tests/test_ui_update_input_browser build/tests/test_ui_update_input_control
test-debug: build/tests/test_filterlabel-debug build/tests/test_statsview_input-debug build/tests/test_statsview_input_review-debug build/tests/test_ui_update_input-debug build/tests/test_ui_update_input_browser-debug build/tests/test_ui_update_input_control-debug

# Review helpers compile without renderer/database/platform dependencies.
REVIEW_REGISTRY_TEST = src/dev/review_registry.c tests/test_review_registry.c
REVIEW_STORE_TEST = src/dev/review_store.c tests/test_review_store.c
build/review/test_capture_layout: tests/test_review_capture.c src/dev/review_capture_layout.h
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -O2 -g $(WARN) tests/test_review_capture.c -o $@ -lm
build/review-sanitize/test_capture_layout: tests/test_review_capture.c src/dev/review_capture_layout.h
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) tests/test_review_capture.c -o $@ -lm $(SANFLAGS)
build/review/test_registry: $(REVIEW_REGISTRY_TEST) src/dev/review_registry.h
	mkdir -p $(@D)
	$(CC) -Isrc -O2 -g $(WARN) $(REVIEW_REGISTRY_TEST) -o $@ -lm
build/review-sanitize/test_registry: $(REVIEW_REGISTRY_TEST) src/dev/review_registry.h
	mkdir -p $(@D)
	$(CC) -Isrc -O1 -g $(WARN) $(SANFLAGS) $(REVIEW_REGISTRY_TEST) -o $@ -lm $(SANFLAGS)
build/review/test_store: $(REVIEW_STORE_TEST) src/dev/review_store.h
	mkdir -p $(@D)
	$(CC) -Isrc -O2 -g $(WARN) $(REVIEW_STORE_TEST) -o $@ $(THREAD_LIBS)
build/review-sanitize/test_store: $(REVIEW_STORE_TEST) src/dev/review_store.h
	mkdir -p $(@D)
	$(CC) -Isrc -O1 -g $(WARN) $(SANFLAGS) $(REVIEW_STORE_TEST) -o $@ $(THREAD_LIBS) $(SANFLAGS)
test-review-core: build/review/test_registry build/review-sanitize/test_registry build/review/test_store build/review-sanitize/test_store build/review/test_capture_layout build/review-sanitize/test_capture_layout
	./build/review/test_registry
	ASAN_OPTIONS=halt_on_error=1 ./build/review-sanitize/test_registry
	./build/review/test_store
	ASAN_OPTIONS=halt_on_error=1 ./build/review-sanitize/test_store
	./build/review/test_capture_layout
	ASAN_OPTIONS=halt_on_error=1 ./build/review-sanitize/test_capture_layout
	python3 -m tools.tests.test_native_review
	python3 tools/test_review_build.py --self-test
test-review:
	python3 tools/test_native_review.py $(TEST_ARGS)
.PHONY: test-review-core test-review

# External development tooling only; neither application links this service.
review-serve:
	python3 tools/review_service.py serve --agent codex
test-review-queue:
	python3 -m tools.tests.test_review_queue
	python3 -m tools.tests.test_review_service
	node --test web/tests/review_host.test.mjs web/tests/review_queue.test.mjs
.PHONY: review-serve test-review-queue

# Browser proof uses the same C renderer/controllers and a separate backend.
# The compiler/dependencies live only in the ignored project-local web cache.
WEB_SOURCES = $(filter-out src/data/database.c $(NATIVE_PERSISTENCE),$(filter %.c,$(APP_SOURCES))) src/workbook/workbook_browser.c src/data/page_wire.c src/data/browser_query.c src/data/distinct_page.c
web-duckdb-flags:
	@printf '%s\n' '$(DUCKDB_CFLAGS)'
web-sources:
	@printf '%s\n' $(WEB_SOURCES)
web:
	python3 tools/build_web.py

# BP0 uses the existing native codec and official browser Worker. It is an
# isolated compatibility proof; no browser Save UI or new runtime dependency.
build/web-tests/test-persistence: $(CORE) tests/test_browser_persistence.c $(HEADERS)
	mkdir -p $(@D)
	$(CC) -Isrc $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN) $(CORE) tests/test_browser_persistence.c -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm
build/web-tests/test-persistence-sanitize: $(CORE) tests/test_browser_persistence.c $(HEADERS)
	mkdir -p $(@D)
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(CORE) tests/test_browser_persistence.c -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS)
test-browser-persistence:
	python3 tools/test_web_app.py --persistence
.PHONY: test-browser-persistence
WEB_REVIEW_SOURCES = $(WEB_SOURCES) src/dev/review_registry.c src/dev/review_input.c src/dev/review_capture.c src/dev/review_browser.c
web-review-sources:
	@printf '%s\n' $(WEB_REVIEW_SOURCES)
web-review:
	python3 tools/build_web.py --review
.PHONY: web-review web-review-sources
.PHONY: web web-sources web-duckdb-flags

# No-window C/official-Worker integration driver; no UI or renderer linked.
WEB_WORKER_SOURCES = $(filter-out src/data/database.c,$(filter %.c,$(CORE))) src/workbook/workbook_browser.c src/data/page_wire.c src/data/browser_query.c src/data/distinct_page.c tests/test_browser_worker.c
web-worker-sources:
	@printf '%s\n' $(WEB_WORKER_SOURCES)
.PHONY: web-worker-sources

# The browser protocol and workbook model compile with C99/ASan independently
# of either engine or renderer. No desktop or mutable database fixtures.
BROWSER_QUERY_TEST_SRC = src/common.c src/transforms/analysis.c src/transforms/number.c src/transforms/pivot.c src/data/page.c src/data/source_digest.c src/data/page_wire.c src/data/browser_query.c
# Partial meta/sheets decoder proof only: deliberately outside App/CORE.
WEB_CODEC_READ_SOURCES = $(BROWSER_QUERY_TEST_SRC) src/transforms/marks.c src/workbook/source_scope.c src/workbook/format.c src/workbook/workbook_codec.c src/workbook/workbook_codec_read.c tests/test_browser_codec_read.c
web-codec-read-sources:
	@printf '%s\n' $(WEB_CODEC_READ_SOURCES)
test-browser-codec-read:
	python3 tools/test_web_app.py --codec-read-driver
test-browser-codec-sorts:
	python3 tools/test_web_app.py --codec-read-driver --codec-sorts
test-browser-codec-preferences:
	python3 tools/test_web_app.py --codec-read-driver --codec-preferences
test-browser-codec-objects:
	python3 tools/test_web_app.py --codec-read-driver --codec-objects
test-browser-codec-queries:
	python3 tools/test_web_app.py --codec-read-driver --codec-queries
test-browser-codec-pivot-marks:
	python3 tools/test_web_app.py --codec-read-driver --codec-pivot-marks
test-browser-codec-tables:
	python3 tools/test_web_app.py --codec-read-driver --codec-tables
test-browser-codec-source-options:
	python3 tools/test_web_app.py --codec-read-driver --codec-source-options
test-workbook-codec-sorts: build/tests/test_workbook_codec_native build/tests/test_workbook_codec_native-debug
	./build/tests/test_workbook_codec_native --sorts-only
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_workbook_codec_native-debug --sorts-only
test-workbook-codec-preferences: build/tests/test_workbook_codec_native build/tests/test_workbook_codec_native-debug
	./build/tests/test_workbook_codec_native --preferences-only
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_workbook_codec_native-debug --preferences-only
test-workbook-codec-objects: build/tests/test_workbook_codec_native build/tests/test_workbook_codec_native-debug
	./build/tests/test_workbook_codec_native --objects-only
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_workbook_codec_native-debug --objects-only
test-workbook-codec-queries: build/tests/test_workbook_codec_native build/tests/test_workbook_codec_native-debug
	./build/tests/test_workbook_codec_native --queries-only
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_workbook_codec_native-debug --queries-only
.PHONY: web-codec-read-sources test-browser-codec-read test-browser-codec-sorts test-workbook-codec-sorts
.PHONY: test-browser-codec-preferences test-workbook-codec-preferences
.PHONY: test-browser-codec-objects test-workbook-codec-objects
.PHONY: test-browser-codec-queries test-workbook-codec-queries
test-workbook-codec-source-options: build/tests/test_workbook_codec_native build/tests/test_workbook_codec_native-debug
	./build/tests/test_workbook_codec_native --source-options-only
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_workbook_codec_native-debug --source-options-only
.PHONY: test-browser-codec-source-options test-workbook-codec-source-options
test-workbook-codec-tables: build/tests/test_workbook_codec_native build/tests/test_workbook_codec_native-debug
	./build/tests/test_workbook_codec_native --tables-only
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_workbook_codec_native-debug --tables-only
.PHONY: test-browser-codec-tables test-workbook-codec-tables
test-workbook-codec-pivot-marks: build/tests/test_workbook_codec_native build/tests/test_workbook_codec_native-debug
	./build/tests/test_workbook_codec_native --pivot-marks-only
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_workbook_codec_native-debug --pivot-marks-only
.PHONY: test-browser-codec-pivot-marks test-workbook-codec-pivot-marks
BROWSER_MODEL_TEST_SRC = $(filter-out src/platform/platform.c src/data/database.c src/data/worker.c,$(CORE)) src/workbook/workbook_browser.c tests/test_workbook_model.c
# Pure command/model checks use the existing shared-C browser-conditioned model
# and native platform stubs, with per-file objects and no GUI/engine dependency.
CELL_COMMAND_TEST_SRC = $(filter-out tests/test_workbook_model.c,$(BROWSER_MODEL_TEST_SRC)) tests/test_cell_command.c
CELL_COMMAND_TEST_OBJECTS = $(addprefix build/obj/release/cell-command/,$(addsuffix .o,$(basename $(CELL_COMMAND_TEST_SRC))))
CELL_COMMAND_TEST_SAN_OBJECTS = $(addprefix build/obj/sanitize/cell-command/,$(addsuffix .o,$(basename $(CELL_COMMAND_TEST_SRC))))
build/obj/release/cell-command/%.o: %.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_BROWSER_HOST)
build/obj/sanitize/cell-command/%.o: %.c build/obj/sanitize/app/compile.flags
	$(call compile_object,$(APP_SANITIZE_FLAGS) -DDUXCEL_BROWSER_HOST)
build/tests/test_cell_command: $(CELL_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(CELL_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_cell_command-debug: $(CELL_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(CELL_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(CELL_COMMAND_TEST_OBJECTS:.o=.d) $(CELL_COMMAND_TEST_SAN_OBJECTS:.o=.d)
FORMAT_COMMAND_TEST_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_OBJECTS)) build/obj/release/cell-command/tests/test_format_command.o
FORMAT_COMMAND_TEST_SAN_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_SAN_OBJECTS)) build/obj/sanitize/cell-command/tests/test_format_command.o
build/tests/test_format_command: $(FORMAT_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(FORMAT_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_format_command-debug: $(FORMAT_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(FORMAT_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(FORMAT_COMMAND_TEST_OBJECTS:.o=.d) $(FORMAT_COMMAND_TEST_SAN_OBJECTS:.o=.d)
ANALYSIS_COMMAND_TEST_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_OBJECTS)) build/obj/release/cell-command/tests/test_analysis_command.o
ANALYSIS_COMMAND_TEST_SAN_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_SAN_OBJECTS)) build/obj/sanitize/cell-command/tests/test_analysis_command.o
build/tests/test_analysis_command: $(ANALYSIS_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(ANALYSIS_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_analysis_command-debug: $(ANALYSIS_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(ANALYSIS_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(ANALYSIS_COMMAND_TEST_OBJECTS:.o=.d) $(ANALYSIS_COMMAND_TEST_SAN_OBJECTS:.o=.d)
PRIMARY_REPLACEMENT_TEST_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_OBJECTS)) build/obj/release/cell-command/tests/test_primary_replacement.o
PRIMARY_REPLACEMENT_TEST_SAN_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_SAN_OBJECTS)) build/obj/sanitize/cell-command/tests/test_primary_replacement.o
build/tests/test_primary_replacement: $(PRIMARY_REPLACEMENT_TEST_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(PRIMARY_REPLACEMENT_TEST_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_primary_replacement-debug: $(PRIMARY_REPLACEMENT_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(PRIMARY_REPLACEMENT_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(PRIMARY_REPLACEMENT_TEST_OBJECTS:.o=.d) $(PRIMARY_REPLACEMENT_TEST_SAN_OBJECTS:.o=.d)
OBJECT_BINDING_COMMAND_TEST_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_OBJECTS)) build/obj/release/cell-command/tests/test_object_binding_command.o
OBJECT_BINDING_COMMAND_TEST_SAN_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_SAN_OBJECTS)) build/obj/sanitize/cell-command/tests/test_object_binding_command.o
build/tests/test_object_binding_command: $(OBJECT_BINDING_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(OBJECT_BINDING_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_object_binding_command-debug: $(OBJECT_BINDING_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(OBJECT_BINDING_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(OBJECT_BINDING_COMMAND_TEST_OBJECTS:.o=.d) $(OBJECT_BINDING_COMMAND_TEST_SAN_OBJECTS:.o=.d)
OBJECT_DELETE_COMMAND_TEST_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_OBJECTS)) build/obj/release/cell-command/tests/test_object_delete_command.o
OBJECT_DELETE_COMMAND_TEST_SAN_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_SAN_OBJECTS)) build/obj/sanitize/cell-command/tests/test_object_delete_command.o
build/tests/test_object_delete_command: $(OBJECT_DELETE_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(OBJECT_DELETE_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_object_delete_command-debug: $(OBJECT_DELETE_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(OBJECT_DELETE_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(OBJECT_DELETE_COMMAND_TEST_OBJECTS:.o=.d) $(OBJECT_DELETE_COMMAND_TEST_SAN_OBJECTS:.o=.d)
OBJECT_CREATE_COMMAND_TEST_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_OBJECTS)) build/obj/release/cell-command/tests/test_object_create_command.o
OBJECT_CREATE_COMMAND_TEST_SAN_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_SAN_OBJECTS)) build/obj/sanitize/cell-command/tests/test_object_create_command.o
build/tests/test_object_create_command: $(OBJECT_CREATE_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(OBJECT_CREATE_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_object_create_command-debug: $(OBJECT_CREATE_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(OBJECT_CREATE_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(OBJECT_CREATE_COMMAND_TEST_OBJECTS:.o=.d) $(OBJECT_CREATE_COMMAND_TEST_SAN_OBJECTS:.o=.d)
OBJECT_IMPORT_DRAFT_TEST_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_OBJECTS)) build/obj/release/cell-command/tests/test_object_import_draft.o
OBJECT_IMPORT_DRAFT_TEST_SAN_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_SAN_OBJECTS)) build/obj/sanitize/cell-command/tests/test_object_import_draft.o
build/tests/test_object_import_draft: $(OBJECT_IMPORT_DRAFT_TEST_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(OBJECT_IMPORT_DRAFT_TEST_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_object_import_draft-debug: $(OBJECT_IMPORT_DRAFT_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(OBJECT_IMPORT_DRAFT_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(OBJECT_IMPORT_DRAFT_TEST_OBJECTS:.o=.d) $(OBJECT_IMPORT_DRAFT_TEST_SAN_OBJECTS:.o=.d)
OBJECT_MOVE_COMMAND_TEST_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_OBJECTS)) build/obj/release/cell-command/tests/test_object_move_command.o
OBJECT_MOVE_COMMAND_TEST_SAN_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_SAN_OBJECTS)) build/obj/sanitize/cell-command/tests/test_object_move_command.o
build/tests/test_object_move_command: $(OBJECT_MOVE_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(OBJECT_MOVE_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_object_move_command-debug: $(OBJECT_MOVE_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(OBJECT_MOVE_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(OBJECT_MOVE_COMMAND_TEST_OBJECTS:.o=.d) $(OBJECT_MOVE_COMMAND_TEST_SAN_OBJECTS:.o=.d)
TABLE_REPLACEMENT_TEST_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_OBJECTS)) build/obj/release/cell-command/tests/test_table_replacement.o
TABLE_REPLACEMENT_TEST_SAN_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_SAN_OBJECTS)) build/obj/sanitize/cell-command/tests/test_table_replacement.o
build/tests/test_table_replacement: $(TABLE_REPLACEMENT_TEST_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(TABLE_REPLACEMENT_TEST_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_table_replacement-debug: $(TABLE_REPLACEMENT_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(TABLE_REPLACEMENT_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(TABLE_REPLACEMENT_TEST_OBJECTS:.o=.d) $(TABLE_REPLACEMENT_TEST_SAN_OBJECTS:.o=.d)
TABLE_COMMAND_TEST_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_OBJECTS)) build/obj/release/cell-command/tests/test_table_command.o
TABLE_COMMAND_TEST_SAN_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_SAN_OBJECTS)) build/obj/sanitize/cell-command/tests/test_table_command.o
build/tests/test_table_command: $(TABLE_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(TABLE_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_table_command-debug: $(TABLE_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(TABLE_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(TABLE_COMMAND_TEST_OBJECTS:.o=.d) $(TABLE_COMMAND_TEST_SAN_OBJECTS:.o=.d)
OBJECT_MARKS_COMMAND_TEST_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_OBJECTS)) build/obj/release/cell-command/tests/test_object_marks_command.o
OBJECT_MARKS_COMMAND_TEST_SAN_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_SAN_OBJECTS)) build/obj/sanitize/cell-command/tests/test_object_marks_command.o
build/tests/test_object_marks_command: $(OBJECT_MARKS_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(OBJECT_MARKS_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_object_marks_command-debug: $(OBJECT_MARKS_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(OBJECT_MARKS_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(OBJECT_MARKS_COMMAND_TEST_OBJECTS:.o=.d) $(OBJECT_MARKS_COMMAND_TEST_SAN_OBJECTS:.o=.d)
OBJECT_LAYOUT_COMMAND_TEST_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_OBJECTS)) build/obj/release/cell-command/tests/test_object_layout_command.o
OBJECT_LAYOUT_COMMAND_TEST_SAN_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_SAN_OBJECTS)) build/obj/sanitize/cell-command/tests/test_object_layout_command.o
build/tests/test_object_layout_command: $(OBJECT_LAYOUT_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(OBJECT_LAYOUT_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_object_layout_command-debug: $(OBJECT_LAYOUT_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(OBJECT_LAYOUT_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(OBJECT_LAYOUT_COMMAND_TEST_OBJECTS:.o=.d) $(OBJECT_LAYOUT_COMMAND_TEST_SAN_OBJECTS:.o=.d)
OBJECT_PIVOT_COMMAND_TEST_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_OBJECTS)) build/obj/release/cell-command/tests/test_object_pivot_command.o
OBJECT_PIVOT_COMMAND_TEST_SAN_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_SAN_OBJECTS)) build/obj/sanitize/cell-command/tests/test_object_pivot_command.o
build/tests/test_object_pivot_command: $(OBJECT_PIVOT_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(OBJECT_PIVOT_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_object_pivot_command-debug: $(OBJECT_PIVOT_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(OBJECT_PIVOT_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(OBJECT_PIVOT_COMMAND_TEST_OBJECTS:.o=.d) $(OBJECT_PIVOT_COMMAND_TEST_SAN_OBJECTS:.o=.d)
OBJECT_PRESENTATION_COMMAND_TEST_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_OBJECTS)) build/obj/release/cell-command/tests/test_object_presentation_command.o
OBJECT_PRESENTATION_COMMAND_TEST_SAN_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_SAN_OBJECTS)) build/obj/sanitize/cell-command/tests/test_object_presentation_command.o
build/tests/test_object_presentation_command: $(OBJECT_PRESENTATION_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(OBJECT_PRESENTATION_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_object_presentation_command-debug: $(OBJECT_PRESENTATION_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(OBJECT_PRESENTATION_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(OBJECT_PRESENTATION_COMMAND_TEST_OBJECTS:.o=.d) $(OBJECT_PRESENTATION_COMMAND_TEST_SAN_OBJECTS:.o=.d)
OBJECT_NAME_COMMAND_TEST_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_OBJECTS)) build/obj/release/cell-command/tests/test_object_name_command.o
OBJECT_NAME_COMMAND_TEST_SAN_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_SAN_OBJECTS)) build/obj/sanitize/cell-command/tests/test_object_name_command.o
build/tests/test_object_name_command: $(OBJECT_NAME_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(OBJECT_NAME_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_object_name_command-debug: $(OBJECT_NAME_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(OBJECT_NAME_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(OBJECT_NAME_COMMAND_TEST_OBJECTS:.o=.d) $(OBJECT_NAME_COMMAND_TEST_SAN_OBJECTS:.o=.d)
# Reuse compiled model/core objects; focused edits do not rebuild unrelated UI.
HISTORY_MODEL_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_OBJECTS)) build/obj/release/cell-command/tests/test_workbook_history.o
HISTORY_MODEL_SAN_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_SAN_OBJECTS)) build/obj/sanitize/cell-command/tests/test_workbook_history.o
build/tests/test_workbook_history: $(HISTORY_MODEL_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(HISTORY_MODEL_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_workbook_history-debug: $(HISTORY_MODEL_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(HISTORY_MODEL_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(HISTORY_MODEL_OBJECTS:.o=.d) $(HISTORY_MODEL_SAN_OBJECTS:.o=.d)
LIFECYCLE_MODEL_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_OBJECTS)) build/obj/release/cell-command/tests/test_workbook_lifecycle.o
LIFECYCLE_MODEL_SAN_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_SAN_OBJECTS)) build/obj/sanitize/cell-command/tests/test_workbook_lifecycle.o
build/tests/test_workbook_lifecycle: $(LIFECYCLE_MODEL_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(LIFECYCLE_MODEL_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_workbook_lifecycle-debug: $(LIFECYCLE_MODEL_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(LIFECYCLE_MODEL_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(LIFECYCLE_MODEL_OBJECTS:.o=.d) $(LIFECYCLE_MODEL_SAN_OBJECTS:.o=.d)


IDENTITY_MODEL_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_OBJECTS)) build/obj/release/cell-command/tests/test_workbook_identity.o
IDENTITY_MODEL_SAN_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_SAN_OBJECTS)) build/obj/sanitize/cell-command/tests/test_workbook_identity.o
build/tests/test_workbook_identity: $(IDENTITY_MODEL_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(IDENTITY_MODEL_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_workbook_identity-debug: $(IDENTITY_MODEL_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(IDENTITY_MODEL_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
IDENTITY_NATIVE_OBJECTS = $(addprefix build/obj/release/app/,$(addsuffix .o,$(basename $(CORE)))) build/obj/release/app/tests/test_workbook_identity_native.o
IDENTITY_NATIVE_SAN_OBJECTS = $(addprefix build/obj/sanitize/app/,$(addsuffix .o,$(basename $(CORE)))) build/obj/sanitize/app/tests/test_workbook_identity_native.o
build/tests/test_workbook_identity_native: $(IDENTITY_NATIVE_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(IDENTITY_NATIVE_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_workbook_identity_native-debug: $(IDENTITY_NATIVE_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(IDENTITY_NATIVE_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(IDENTITY_MODEL_OBJECTS:.o=.d) $(IDENTITY_MODEL_SAN_OBJECTS:.o=.d) $(IDENTITY_NATIVE_OBJECTS:.o=.d) $(IDENTITY_NATIVE_SAN_OBJECTS:.o=.d)


BROWSER_DISTINCT_TEST_SRC = src/common.c src/transforms/analysis.c src/transforms/number.c src/transforms/pivot.c src/transforms/distinct.c
BROWSER_DISTINCT_PAGE_TEST_SRC = $(BROWSER_DISTINCT_TEST_SRC) src/data/page.c src/data/source_digest.c src/data/distinct_page.c
build/web-tests/test-distinct-query: $(BROWSER_DISTINCT_TEST_SRC) tests/test_distinct_query.c $(HEADERS)
	mkdir -p $(@D)
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O2 -g $(WARN) $(BROWSER_DISTINCT_TEST_SRC) tests/test_distinct_query.c -o $@ -lm
build/web-tests/test-distinct-query-sanitize: $(BROWSER_DISTINCT_TEST_SRC) tests/test_distinct_query.c $(HEADERS)
	mkdir -p $(@D)
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(BROWSER_DISTINCT_TEST_SRC) tests/test_distinct_query.c -o $@ -lm $(SANFLAGS)
build/web-tests/test-distinct-page: $(BROWSER_DISTINCT_PAGE_TEST_SRC) tests/test_distinct_page.c $(HEADERS)
	mkdir -p $(@D)
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O2 -g $(WARN) $(BROWSER_DISTINCT_PAGE_TEST_SRC) tests/test_distinct_page.c -o $@ -lm
build/web-tests/test-distinct-page-sanitize: $(BROWSER_DISTINCT_PAGE_TEST_SRC) tests/test_distinct_page.c $(HEADERS)
	mkdir -p $(@D)
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(BROWSER_DISTINCT_PAGE_TEST_SRC) tests/test_distinct_page.c -o $@ -lm $(SANFLAGS)
build/web-tests/test-wire: $(BROWSER_QUERY_TEST_SRC) tests/test_page_wire.c $(HEADERS)
	mkdir -p $(@D)
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O2 -g $(WARN) $(BROWSER_QUERY_TEST_SRC) tests/test_page_wire.c -o $@ -lm
build/web-tests/test-wire-sanitize: $(BROWSER_QUERY_TEST_SRC) tests/test_page_wire.c $(HEADERS)
	mkdir -p $(@D)
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(BROWSER_QUERY_TEST_SRC) tests/test_page_wire.c -o $@ -lm $(SANFLAGS)
build/web-tests/test-query: $(BROWSER_QUERY_TEST_SRC) tests/test_browser_query.c $(HEADERS)
	mkdir -p $(@D)
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O2 -g $(WARN) $(BROWSER_QUERY_TEST_SRC) tests/test_browser_query.c -o $@ -lm
build/web-tests/test-query-sanitize: $(BROWSER_QUERY_TEST_SRC) tests/test_browser_query.c $(HEADERS)
	mkdir -p $(@D)
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(BROWSER_QUERY_TEST_SRC) tests/test_browser_query.c -o $@ -lm $(SANFLAGS)
# The host-conditioned model retains real native locator tests. Compile that
# boundary separately without browser JS bridges or a native GUI dependency.
build/web-tests/platform-model.o: src/platform/platform.c $(HEADERS)
	mkdir -p $(@D)
	$(CC) -Isrc -O2 -g $(WARN) -c src/platform/platform.c -o $@
build/web-tests/platform-model-sanitize.o: src/platform/platform.c $(HEADERS)
	mkdir -p $(@D)
	$(CC) -Isrc -O1 -g $(WARN) $(SANFLAGS) -c src/platform/platform.c -o $@
build/web-tests/test-model: $(BROWSER_MODEL_TEST_SRC) $(HEADERS) build/web-tests/platform-model.o
	mkdir -p $(@D)
	$(CC) -Isrc $(DUCKDB_CFLAGS) -DDUXCEL_BROWSER_HOST -O2 -g $(WARN) $(BROWSER_MODEL_TEST_SRC) build/web-tests/platform-model.o -o $@ $(THREAD_LIBS) -lm
build/web-tests/test-model-sanitize: $(BROWSER_MODEL_TEST_SRC) $(HEADERS) build/web-tests/platform-model-sanitize.o
	mkdir -p $(@D)
	$(CC) -Isrc $(DUCKDB_CFLAGS) -DDUXCEL_BROWSER_HOST -O1 -g $(WARN) $(SANFLAGS) $(BROWSER_MODEL_TEST_SRC) build/web-tests/platform-model-sanitize.o -o $@ $(THREAD_LIBS) -lm $(SANFLAGS)
test-browser-core: build/web-tests/test-wire build/web-tests/test-wire-sanitize build/web-tests/test-query build/web-tests/test-query-sanitize build/web-tests/test-model build/web-tests/test-model-sanitize build/web-tests/test-distinct-query build/web-tests/test-distinct-query-sanitize build/web-tests/test-distinct-page build/web-tests/test-distinct-page-sanitize
	./build/web-tests/test-wire
	ASAN_OPTIONS=halt_on_error=1 ./build/web-tests/test-wire-sanitize
	./build/web-tests/test-query
	ASAN_OPTIONS=halt_on_error=1 ./build/web-tests/test-query-sanitize
	./build/web-tests/test-model
	ASAN_OPTIONS=halt_on_error=1 ./build/web-tests/test-model-sanitize
	./build/web-tests/test-distinct-query
	ASAN_OPTIONS=halt_on_error=1 ./build/web-tests/test-distinct-query-sanitize
	./build/web-tests/test-distinct-page
	ASAN_OPTIONS=halt_on_error=1 ./build/web-tests/test-distinct-page-sanitize
	node --test web/tests/official_worker.test.mjs web/tests/page_wire.test.mjs web/tests/engine_host.test.mjs web/tests/official_worker_probe.test.mjs web/tests/metadata_stream.test.mjs web/tests/metadata_stream_probe.test.mjs web/tests/codec_stream_transport.test.mjs
	python3 -m tools.tests.test_web_vendor
.PHONY: test-browser-core

KEYTIPS_SRC = src/ui/keytips.c src/ui/ribbon_spec.c src/ui/ribbon.c tests/test_keytips.c
build/tests/test_keytips: $(KEYTIPS_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(KEYTIPS_SRC) -o $@ -lm
build/tests/test_keytips-debug: $(KEYTIPS_SRC) $(HEADERS) | build/tests
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(KEYTIPS_SRC) -o $@ -lm $(SANFLAGS)

test-browser-codec-filters:
	python3 tools/test_web_app.py --codec-read-driver --codec-filters
test-workbook-codec-filters: build/tests/test_workbook_codec_native build/tests/test_workbook_codec_native-debug
	./build/tests/test_workbook_codec_native --filters-only
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_workbook_codec_native-debug --filters-only
.PHONY: test-browser-codec-filters test-workbook-codec-filters

test-browser-codec-grid:
	python3 tools/test_web_app.py --codec-read-driver --codec-grid
test-workbook-codec-grid: build/tests/test_workbook_codec_native build/tests/test_workbook_codec_native-debug
	./build/tests/test_workbook_codec_native --grid-only
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_workbook_codec_native-debug --grid-only
.PHONY: test-browser-codec-grid test-workbook-codec-grid

test-browser-codec-counters:
	python3 tools/test_web_app.py --codec-read-driver --codec-counters
test-workbook-codec-counters: build/tests/test_workbook_codec_native build/tests/test_workbook_codec_native-debug
	./build/tests/test_workbook_codec_native --counters-only
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_workbook_codec_native-debug --counters-only
.PHONY: test-browser-codec-counters test-workbook-codec-counters

test-browser-codec-source-counters:
	python3 tools/test_web_app.py --codec-read-driver --codec-source-counters
test-workbook-codec-source-counters: build/tests/test_workbook_codec_native build/tests/test_workbook_codec_native-debug
	./build/tests/test_workbook_codec_native --source-counters-only
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_workbook_codec_native-debug --source-counters-only
.PHONY: test-browser-codec-source-counters test-workbook-codec-source-counters

test-browser-codec-source-versions:
	python3 tools/test_web_app.py --codec-read-driver --codec-source-versions
test-workbook-codec-source-versions: build/tests/test_workbook_codec_native build/tests/test_workbook_codec_native-debug
	./build/tests/test_workbook_codec_native --source-versions-only
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_workbook_codec_native-debug --source-versions-only
.PHONY: test-browser-codec-source-versions test-workbook-codec-source-versions

test-browser-codec-source-bindings:
	python3 tools/test_web_app.py --codec-read-driver --codec-source-bindings
test-workbook-codec-source-bindings: build/tests/test_workbook_codec_native build/tests/test_workbook_codec_native-debug
	./build/tests/test_workbook_codec_native --source-bindings-only
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_workbook_codec_native-debug --source-bindings-only
.PHONY: test-browser-codec-source-bindings test-workbook-codec-source-bindings


test-browser-codec-format-rows:
	python3 tools/test_web_app.py --codec-read-driver --codec-format-rows
test-workbook-codec-format-rows: build/tests/test_workbook_codec_native build/tests/test_workbook_codec_native-debug
	./build/tests/test_workbook_codec_native --format-rows-only
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_workbook_codec_native-debug --format-rows-only
.PHONY: test-browser-codec-format-rows test-workbook-codec-format-rows

# Compatibility command names are phony; executable files live only in build/.
.PHONY: duxcel duxcel-debug distclean
duxcel: build/bin/duxcel
duxcel-debug: build/bin/duxcel-debug
build/bin/assets build/tests/assets:
	mkdir -p $(@D)
	ln -s ../../assets $@
build/tests:
	mkdir -p $@

# Python produces inputs; C tests verify the actual product APIs separately.
test-fixture: build/tests/test_fixture build/tests/test_fixture-debug design-fixture design-large-fixture
	./build/tests/test_fixture
	ASAN_OPTIONS=halt_on_error=1 ./build/tests/test_fixture-debug
.PHONY: test-fixture design-fixture design-large-fixture

# Native-only journal primitive: no App, raylib, worker or canonical fixture.
TEST_DL_LIBS = $(if $(filter Darwin,$(shell uname -s)),,-ldl)
JOURNAL_TEST_OBJECTS = build/obj/release/app/src/common.o build/obj/release/app/src/workbook/cell_delta.o build/obj/release/app/src/workbook/journal.o build/obj/release/app/tests/test_workbook_journal.o
JOURNAL_TEST_SAN_OBJECTS = build/obj/sanitize/app/src/common.o build/obj/sanitize/app/src/workbook/cell_delta.o build/obj/sanitize/app/src/workbook/journal.o build/obj/sanitize/app/tests/test_workbook_journal.o
build/tests/test_workbook_journal: $(JOURNAL_TEST_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(JOURNAL_TEST_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) $(TEST_DL_LIBS) -lm)
build/tests/test_workbook_journal-debug: $(JOURNAL_TEST_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(JOURNAL_TEST_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) $(TEST_DL_LIBS) -lm $(SANFLAGS))
-include $(JOURNAL_TEST_OBJECTS:.o=.d) $(JOURNAL_TEST_SAN_OBJECTS:.o=.d)

JOURNAL_QUEUE_TEST_OBJECTS = build/obj/release/app/src/common.o build/obj/release/app/src/platform/platform.o build/obj/release/app/src/workbook/cell_delta.o build/obj/release/app/src/workbook/journal.o build/obj/release/app/src/workbook/journal_queue.o build/obj/release/app/tests/test_journal_queue.o
JOURNAL_QUEUE_TEST_SAN_OBJECTS = build/obj/sanitize/app/src/common.o build/obj/sanitize/app/src/platform/platform.o build/obj/sanitize/app/src/workbook/cell_delta.o build/obj/sanitize/app/src/workbook/journal.o build/obj/sanitize/app/src/workbook/journal_queue.o build/obj/sanitize/app/tests/test_journal_queue.o
build/tests/test_journal_queue: $(JOURNAL_QUEUE_TEST_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(JOURNAL_QUEUE_TEST_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) $(TEST_DL_LIBS) -lm)
build/tests/test_journal_queue-debug: $(JOURNAL_QUEUE_TEST_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(JOURNAL_QUEUE_TEST_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) $(TEST_DL_LIBS) -lm $(SANFLAGS))
-include $(JOURNAL_QUEUE_TEST_OBJECTS:.o=.d) $(JOURNAL_QUEUE_TEST_SAN_OBJECTS:.o=.d)

JOURNAL_SESSION_OBJECTS = $(addprefix build/obj/release/app/,$(addsuffix .o,$(basename $(CORE) $(NATIVE_PERSISTENCE)))) build/obj/release/app/tests/test_journal_session.o
JOURNAL_SESSION_SAN_OBJECTS = $(addprefix build/obj/sanitize/app/,$(addsuffix .o,$(basename $(CORE) $(NATIVE_PERSISTENCE)))) build/obj/sanitize/app/tests/test_journal_session.o
build/tests/test_journal_session: $(JOURNAL_SESSION_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(JOURNAL_SESSION_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_journal_session-debug: $(JOURNAL_SESSION_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(JOURNAL_SESSION_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(JOURNAL_SESSION_OBJECTS:.o=.d) $(JOURNAL_SESSION_SAN_OBJECTS:.o=.d)
VERSION_CHECKPOINT_OBJECTS = $(filter-out %/tests/test_journal_session.o,$(JOURNAL_SESSION_OBJECTS)) build/obj/release/app/tests/test_version_checkpoint.o
VERSION_CHECKPOINT_SAN_OBJECTS = $(filter-out %/tests/test_journal_session.o,$(JOURNAL_SESSION_SAN_OBJECTS)) build/obj/sanitize/app/tests/test_version_checkpoint.o
build/tests/test_version_checkpoint: $(VERSION_CHECKPOINT_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(VERSION_CHECKPOINT_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_version_checkpoint-debug: $(VERSION_CHECKPOINT_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(VERSION_CHECKPOINT_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(VERSION_CHECKPOINT_OBJECTS:.o=.d) $(VERSION_CHECKPOINT_SAN_OBJECTS:.o=.d)


# This private fault shim is never an App object, runtime asset or bundle input.
ifeq ($(shell uname -s),Darwin)
build/tests/libjournal_fault.dylib: tests/support/journal_fault.c | build/tests
	$(call link_binary,-O2 -g $(WARN) -dynamiclib $<,-pthread)
else
build/tests/libjournal_fault.dylib: tests/support/journal_fault.c | build/tests
	$(call link_binary,-O2 -g $(WARN) -fPIC -shared $<,-ldl -pthread)
endif

# Finite product-API workload; Python generates its TSV, never a C fixture writer.
JOURNAL_PERF_OBJECTS = $(filter-out %/tests/test_journal_session.o,$(JOURNAL_SESSION_OBJECTS)) build/obj/release/app/tests/test_journal_performance.o
JOURNAL_PERF_SAN_OBJECTS = $(filter-out %/tests/test_journal_session.o,$(JOURNAL_SESSION_SAN_OBJECTS)) build/obj/sanitize/app/tests/test_journal_performance.o
build/tests/test_journal_performance: $(JOURNAL_PERF_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(JOURNAL_PERF_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) $(TEST_DL_LIBS) -lm)
build/tests/test_journal_performance-debug: $(JOURNAL_PERF_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(JOURNAL_PERF_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) $(TEST_DL_LIBS) -lm $(SANFLAGS))
-include $(JOURNAL_PERF_OBJECTS:.o=.d) $(JOURNAL_PERF_SAN_OBJECTS:.o=.d)

build/obj/feature-archive-worker/adapter.o: src/data/worker_archive.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_ARCHIVE_WORKER)
build/obj/feature-archive-worker/test.o: tests/test_worker_archive.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_ARCHIVE_WORKER)
build/obj/feature-archive-worker-sanitize/adapter.o: src/data/worker_archive.c build/obj/sanitize/app/compile.flags
	$(call compile_object,$(APP_SANITIZE_FLAGS) -DDUXCEL_FEATURE_ARCHIVE_WORKER)
build/obj/feature-archive-worker-sanitize/test.o: tests/test_worker_archive.c build/obj/sanitize/app/compile.flags
	$(call compile_object,$(APP_SANITIZE_FLAGS) -DDUXCEL_FEATURE_ARCHIVE_WORKER)
WORKER_ARCHIVE_SOURCES = src/workbook/version_preview.c $(CORE) src/platform/directory.c src/platform/file_lease.c src/workbook/core_payload.c src/workbook/core_compare.c src/workbook/version_sql.c src/workbook/version_stage.c src/workbook/version_read.c src/workbook/version_restore.c src/workbook/archive_read.c src/workbook/archive_write.c src/workbook/version_save.c
WORKER_ARCHIVE_OBJECTS = $(addprefix build/obj/release/app/,$(addsuffix .o,$(basename $(WORKER_ARCHIVE_SOURCES)))) build/obj/feature-archive-worker/adapter.o build/obj/feature-archive-worker/test.o
WORKER_ARCHIVE_SAN_OBJECTS = $(addprefix build/obj/sanitize/app/,$(addsuffix .o,$(basename $(WORKER_ARCHIVE_SOURCES)))) build/obj/feature-archive-worker-sanitize/adapter.o build/obj/feature-archive-worker-sanitize/test.o
build/tests/test_worker_archive: $(WORKER_ARCHIVE_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(WORKER_ARCHIVE_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_worker_archive-debug: $(WORKER_ARCHIVE_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(WORKER_ARCHIVE_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(WORKER_ARCHIVE_OBJECTS:.o=.d) $(WORKER_ARCHIVE_SAN_OBJECTS:.o=.d)

build/obj/feature-preview-worker/adapter.o: src/data/worker_archive.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_PREVIEW_WORKER)
build/obj/feature-preview-worker/test.o: tests/test_preview_worker.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_PREVIEW_WORKER)
build/obj/feature-preview-worker-sanitize/adapter.o: src/data/worker_archive.c build/obj/sanitize/app/compile.flags
	$(call compile_object,$(APP_SANITIZE_FLAGS) -DDUXCEL_FEATURE_PREVIEW_WORKER)
build/obj/feature-preview-worker-sanitize/test.o: tests/test_preview_worker.c build/obj/sanitize/app/compile.flags
	$(call compile_object,$(APP_SANITIZE_FLAGS) -DDUXCEL_FEATURE_PREVIEW_WORKER)
PREVIEW_WORKER_OBJECTS = $(addprefix build/obj/release/app/,$(addsuffix .o,$(basename $(WORKER_ARCHIVE_SOURCES) src/app/versionstate.c))) build/obj/feature-preview-worker/adapter.o build/obj/feature-preview-worker/test.o
PREVIEW_WORKER_SAN_OBJECTS = $(addprefix build/obj/sanitize/app/,$(addsuffix .o,$(basename $(WORKER_ARCHIVE_SOURCES) src/app/versionstate.c))) build/obj/feature-preview-worker-sanitize/adapter.o build/obj/feature-preview-worker-sanitize/test.o
build/tests/test_preview_worker: $(PREVIEW_WORKER_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(PREVIEW_WORKER_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_preview_worker-debug: $(PREVIEW_WORKER_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(PREVIEW_WORKER_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(PREVIEW_WORKER_OBJECTS:.o=.d) $(PREVIEW_WORKER_SAN_OBJECTS:.o=.d)


APP_VERSION_SOURCES = $(WORKER_ARCHIVE_SOURCES) src/data/worker_archive.c src/app/versionstate.c
APP_VERSION_OBJECTS = $(addprefix build/obj/release/app/,$(addsuffix .o,$(basename $(APP_VERSION_SOURCES)))) build/obj/release/app/tests/test_app_versions.o
APP_VERSION_SAN_OBJECTS = $(addprefix build/obj/sanitize/app/,$(addsuffix .o,$(basename $(APP_VERSION_SOURCES)))) build/obj/sanitize/app/tests/test_app_versions.o
build/tests/test_app_versions: $(APP_VERSION_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(APP_VERSION_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_app_versions-debug: $(APP_VERSION_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(APP_VERSION_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(APP_VERSION_OBJECTS:.o=.d) $(APP_VERSION_SAN_OBJECTS:.o=.d)
FILE_CONFLICT_OBJECTS = $(filter-out %/tests/test_app_versions.o,$(APP_VERSION_OBJECTS)) build/obj/release/app/tests/test_file_conflict.o
FILE_CONFLICT_SAN_OBJECTS = $(filter-out %/tests/test_app_versions.o,$(APP_VERSION_SAN_OBJECTS)) build/obj/sanitize/app/tests/test_file_conflict.o
build/tests/test_file_conflict: $(FILE_CONFLICT_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(FILE_CONFLICT_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_file_conflict-debug: $(FILE_CONFLICT_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(FILE_CONFLICT_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(FILE_CONFLICT_OBJECTS:.o=.d) $(FILE_CONFLICT_SAN_OBJECTS:.o=.d)
APP_PREVIEW_OBJECTS = $(filter-out %/tests/test_app_versions.o,$(APP_VERSION_OBJECTS)) build/obj/release/app/src/app/versionpreview.o build/obj/release/app/tests/test_app_preview.o
APP_PREVIEW_SAN_OBJECTS = $(filter-out %/tests/test_app_versions.o,$(APP_VERSION_SAN_OBJECTS)) build/obj/sanitize/app/src/app/versionpreview.o build/obj/sanitize/app/tests/test_app_preview.o
build/tests/test_app_preview: $(APP_PREVIEW_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(APP_PREVIEW_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_app_preview-debug: $(APP_PREVIEW_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(APP_PREVIEW_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(APP_PREVIEW_OBJECTS:.o=.d) $(APP_PREVIEW_SAN_OBJECTS:.o=.d)




VERSION_SAVE_SOURCES = $(CORE) src/platform/directory.c src/platform/file_lease.c src/workbook/core_payload.c src/workbook/core_compare.c src/workbook/version_sql.c src/workbook/version_stage.c src/workbook/version_read.c src/workbook/version_restore.c src/workbook/archive_read.c src/workbook/archive_write.c src/workbook/version_save.c tests/test_version_save.c
VERSION_SAVE_OBJECTS = $(addprefix build/obj/release/app/,$(addsuffix .o,$(basename $(VERSION_SAVE_SOURCES))))
VERSION_SAVE_SAN_OBJECTS = $(addprefix build/obj/sanitize/app/,$(addsuffix .o,$(basename $(VERSION_SAVE_SOURCES))))
build/tests/test_version_save: $(VERSION_SAVE_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(VERSION_SAVE_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_version_save-debug: $(VERSION_SAVE_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(VERSION_SAVE_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(VERSION_SAVE_OBJECTS:.o=.d) $(VERSION_SAVE_SAN_OBJECTS:.o=.d)
VERSION_TAG_SAVE_OBJECTS = $(filter-out %/tests/test_version_save.o,$(VERSION_SAVE_OBJECTS)) build/obj/release/app/tests/test_version_tag_save.o
VERSION_TAG_SAVE_SAN_OBJECTS = $(filter-out %/tests/test_version_save.o,$(VERSION_SAVE_SAN_OBJECTS)) build/obj/sanitize/app/tests/test_version_tag_save.o
build/tests/test_version_tag_save: $(VERSION_TAG_SAVE_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(VERSION_TAG_SAVE_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_version_tag_save-debug: $(VERSION_TAG_SAVE_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(VERSION_TAG_SAVE_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(VERSION_TAG_SAVE_OBJECTS:.o=.d) $(VERSION_TAG_SAVE_SAN_OBJECTS:.o=.d)
VERSION_PREVIEW_OBJECTS = $(filter-out %/tests/test_version_tag_save.o,$(VERSION_TAG_SAVE_OBJECTS)) build/obj/release/app/src/workbook/version_preview.o build/obj/release/app/tests/test_version_preview.o
VERSION_PREVIEW_SAN_OBJECTS = $(filter-out %/tests/test_version_tag_save.o,$(VERSION_TAG_SAVE_SAN_OBJECTS)) build/obj/sanitize/app/src/workbook/version_preview.o build/obj/sanitize/app/tests/test_version_preview.o
build/tests/test_version_preview: $(VERSION_PREVIEW_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(VERSION_PREVIEW_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_version_preview-debug: $(VERSION_PREVIEW_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(VERSION_PREVIEW_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(VERSION_PREVIEW_OBJECTS:.o=.d) $(VERSION_PREVIEW_SAN_OBJECTS:.o=.d)



WORKER_NATIVE_OBJECTS = $(addprefix build/obj/release/app/,$(addsuffix .o,$(basename $(CORE)))) build/obj/release/app/tests/test_worker_native.o
WORKER_NATIVE_SAN_OBJECTS = $(addprefix build/obj/sanitize/app/,$(addsuffix .o,$(basename $(CORE)))) build/obj/sanitize/app/tests/test_worker_native.o
build/tests/test_worker_native: $(WORKER_NATIVE_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(WORKER_NATIVE_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_worker_native-debug: $(WORKER_NATIVE_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(WORKER_NATIVE_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(WORKER_NATIVE_OBJECTS:.o=.d) $(WORKER_NATIVE_SAN_OBJECTS:.o=.d)

# Existing ownership interactions now have focused per-file module targets.
RECORD_WORKER_OBJECTS = $(addprefix build/obj/release/app/,$(addsuffix .o,$(basename $(CORE)))) build/obj/release/app/tests/test_record_selection_worker.o
RECORD_WORKER_SAN_OBJECTS = $(addprefix build/obj/sanitize/app/,$(addsuffix .o,$(basename $(CORE)))) build/obj/sanitize/app/tests/test_record_selection_worker.o
build/tests/test_record_selection_worker: $(RECORD_WORKER_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(RECORD_WORKER_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_record_selection_worker-debug: $(RECORD_WORKER_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(RECORD_WORKER_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(RECORD_WORKER_OBJECTS:.o=.d) $(RECORD_WORKER_SAN_OBJECTS:.o=.d)

build/obj/feature-source-digest/worker.o: src/data/worker.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_TEST_SOURCE_DIGEST)
build/obj/feature-source-digest/test.o: tests/test_worker_source_digest.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_TEST_SOURCE_DIGEST)
build/obj/feature-source-digest-sanitize/worker.o: src/data/worker.c build/obj/sanitize/app/compile.flags
	$(call compile_object,$(APP_SANITIZE_FLAGS) -DDUXCEL_TEST_SOURCE_DIGEST)
build/obj/feature-source-digest-sanitize/test.o: tests/test_worker_source_digest.c build/obj/sanitize/app/compile.flags
	$(call compile_object,$(APP_SANITIZE_FLAGS) -DDUXCEL_TEST_SOURCE_DIGEST)
DIGEST_WORKER_OBJECTS = $(filter-out %/src/data/worker.o,$(addprefix build/obj/release/app/,$(addsuffix .o,$(basename $(CORE))))) build/obj/feature-source-digest/worker.o build/obj/feature-source-digest/test.o
DIGEST_WORKER_SAN_OBJECTS = $(filter-out %/src/data/worker.o,$(addprefix build/obj/sanitize/app/,$(addsuffix .o,$(basename $(CORE))))) build/obj/feature-source-digest-sanitize/worker.o build/obj/feature-source-digest-sanitize/test.o
build/tests/test_worker_source_digest: $(DIGEST_WORKER_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(DIGEST_WORKER_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_worker_source_digest-debug: $(DIGEST_WORKER_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(DIGEST_WORKER_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(DIGEST_WORKER_OBJECTS:.o=.d) $(DIGEST_WORKER_SAN_OBJECTS:.o=.d)

VERSION_LIFETIME_OBJECTS = $(filter-out %/tests/test_version_save.o,$(VERSION_SAVE_OBJECTS)) build/obj/release/app/tests/test_version_lifetime.o
VERSION_LIFETIME_SAN_OBJECTS = $(filter-out %/tests/test_version_save.o,$(VERSION_SAVE_SAN_OBJECTS)) build/obj/sanitize/app/tests/test_version_lifetime.o
build/tests/test_version_lifetime: $(VERSION_LIFETIME_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(VERSION_LIFETIME_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_version_lifetime-debug: $(VERSION_LIFETIME_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(VERSION_LIFETIME_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(VERSION_LIFETIME_OBJECTS:.o=.d) $(VERSION_LIFETIME_SAN_OBJECTS:.o=.d)

ARCHIVE_WRITE_SOURCES = $(CORE) src/platform/directory.c src/platform/file_lease.c src/workbook/core_payload.c src/workbook/version_sql.c src/workbook/version_stage.c src/workbook/version_read.c src/workbook/version_restore.c src/workbook/archive_read.c src/workbook/archive_write.c tests/test_archive_write.c
ARCHIVE_WRITE_OBJECTS = $(addprefix build/obj/release/app/,$(addsuffix .o,$(basename $(ARCHIVE_WRITE_SOURCES))))
ARCHIVE_WRITE_SAN_OBJECTS = $(addprefix build/obj/sanitize/app/,$(addsuffix .o,$(basename $(ARCHIVE_WRITE_SOURCES))))
build/tests/test_archive_write: $(ARCHIVE_WRITE_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(ARCHIVE_WRITE_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_archive_write-debug: $(ARCHIVE_WRITE_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(ARCHIVE_WRITE_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(ARCHIVE_WRITE_OBJECTS:.o=.d) $(ARCHIVE_WRITE_SAN_OBJECTS:.o=.d)

ARCHIVE_READ_SOURCES = $(CORE) src/platform/directory.c src/platform/file_lease.c src/workbook/core_payload.c src/workbook/version_sql.c src/workbook/version_stage.c src/workbook/version_read.c src/workbook/version_restore.c src/workbook/archive_read.c tests/test_archive_read.c
ARCHIVE_READ_OBJECTS = $(addprefix build/obj/release/app/,$(addsuffix .o,$(basename $(ARCHIVE_READ_SOURCES))))
ARCHIVE_READ_SAN_OBJECTS = $(addprefix build/obj/sanitize/app/,$(addsuffix .o,$(basename $(ARCHIVE_READ_SOURCES))))
build/tests/test_archive_read: $(ARCHIVE_READ_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(ARCHIVE_READ_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_archive_read-debug: $(ARCHIVE_READ_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(ARCHIVE_READ_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(ARCHIVE_READ_OBJECTS:.o=.d) $(ARCHIVE_READ_SAN_OBJECTS:.o=.d)

VERSION_RESTORE_SOURCES = $(CORE) src/platform/directory.c src/platform/file_lease.c src/workbook/core_payload.c src/workbook/version_sql.c src/workbook/version_stage.c src/workbook/version_read.c src/workbook/version_restore.c tests/test_version_restore.c
VERSION_RESTORE_OBJECTS = $(addprefix build/obj/release/app/,$(addsuffix .o,$(basename $(VERSION_RESTORE_SOURCES))))
VERSION_RESTORE_SAN_OBJECTS = $(addprefix build/obj/sanitize/app/,$(addsuffix .o,$(basename $(VERSION_RESTORE_SOURCES))))
build/tests/test_version_restore: $(VERSION_RESTORE_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(VERSION_RESTORE_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_version_restore-debug: $(VERSION_RESTORE_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(VERSION_RESTORE_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(VERSION_RESTORE_OBJECTS:.o=.d) $(VERSION_RESTORE_SAN_OBJECTS:.o=.d)

CORE_RESTORE_SOURCES = $(CORE) src/platform/directory.c src/platform/file_lease.c src/workbook/core_payload.c tests/test_core_restore.c
CORE_RESTORE_OBJECTS = $(addprefix build/obj/release/app/,$(addsuffix .o,$(basename $(CORE_RESTORE_SOURCES))))
CORE_RESTORE_SAN_OBJECTS = $(addprefix build/obj/sanitize/app/,$(addsuffix .o,$(basename $(CORE_RESTORE_SOURCES))))
build/tests/test_core_restore: $(CORE_RESTORE_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(CORE_RESTORE_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_core_restore-debug: $(CORE_RESTORE_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(CORE_RESTORE_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(CORE_RESTORE_OBJECTS:.o=.d) $(CORE_RESTORE_SAN_OBJECTS:.o=.d)

VERSION_READ_SOURCES = $(CORE) src/platform/directory.c src/platform/file_lease.c src/workbook/core_payload.c src/workbook/version_sql.c src/workbook/version_stage.c src/workbook/version_read.c tests/test_version_read.c
VERSION_READ_OBJECTS = $(addprefix build/obj/release/app/,$(addsuffix .o,$(basename $(VERSION_READ_SOURCES))))
VERSION_READ_SAN_OBJECTS = $(addprefix build/obj/sanitize/app/,$(addsuffix .o,$(basename $(VERSION_READ_SOURCES))))
build/tests/test_version_read: $(VERSION_READ_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(VERSION_READ_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_version_read-debug: $(VERSION_READ_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(VERSION_READ_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(VERSION_READ_OBJECTS:.o=.d) $(VERSION_READ_SAN_OBJECTS:.o=.d)

VERSION_STAGE_SOURCES = $(CORE) src/platform/directory.c src/platform/file_lease.c src/workbook/core_payload.c src/workbook/version_sql.c src/workbook/version_stage.c tests/test_version_stage.c
VERSION_STAGE_OBJECTS = $(addprefix build/obj/release/app/,$(addsuffix .o,$(basename $(VERSION_STAGE_SOURCES))))
VERSION_STAGE_SAN_OBJECTS = $(addprefix build/obj/sanitize/app/,$(addsuffix .o,$(basename $(VERSION_STAGE_SOURCES))))
build/tests/test_version_stage: $(VERSION_STAGE_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(VERSION_STAGE_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_version_stage-debug: $(VERSION_STAGE_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(VERSION_STAGE_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(VERSION_STAGE_OBJECTS:.o=.d) $(VERSION_STAGE_SAN_OBJECTS:.o=.d)

CORE_COMPARE_SOURCES = $(CORE) src/platform/directory.c src/platform/file_lease.c src/workbook/core_payload.c src/workbook/core_compare.c tests/test_core_compare.c
CORE_COMPARE_OBJECTS = $(addprefix build/obj/release/app/,$(addsuffix .o,$(basename $(CORE_COMPARE_SOURCES))))
CORE_COMPARE_SAN_OBJECTS = $(addprefix build/obj/sanitize/app/,$(addsuffix .o,$(basename $(CORE_COMPARE_SOURCES))))
build/tests/test_core_compare: $(CORE_COMPARE_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(CORE_COMPARE_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_core_compare-debug: $(CORE_COMPARE_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(CORE_COMPARE_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(CORE_COMPARE_OBJECTS:.o=.d) $(CORE_COMPARE_SAN_OBJECTS:.o=.d)

CORE_PAYLOAD_SOURCES = $(CORE) src/platform/directory.c src/platform/file_lease.c src/workbook/core_payload.c tests/test_core_payload.c
CORE_PAYLOAD_OBJECTS = $(addprefix build/obj/release/app/,$(addsuffix .o,$(basename $(CORE_PAYLOAD_SOURCES))))
CORE_PAYLOAD_SAN_OBJECTS = $(addprefix build/obj/sanitize/app/,$(addsuffix .o,$(basename $(CORE_PAYLOAD_SOURCES))))
build/tests/test_core_payload: $(CORE_PAYLOAD_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(CORE_PAYLOAD_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_core_payload-debug: $(CORE_PAYLOAD_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(CORE_PAYLOAD_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(CORE_PAYLOAD_OBJECTS:.o=.d) $(CORE_PAYLOAD_SAN_OBJECTS:.o=.d)

WORKBOOK_PAYLOAD_OBJECTS = $(addprefix build/obj/release/app/,$(addsuffix .o,$(basename $(CORE)))) build/obj/release/app/tests/test_workbook_payload.o
WORKBOOK_PAYLOAD_SAN_OBJECTS = $(addprefix build/obj/sanitize/app/,$(addsuffix .o,$(basename $(CORE)))) build/obj/sanitize/app/tests/test_workbook_payload.o
build/tests/test_workbook_payload: $(WORKBOOK_PAYLOAD_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(WORKBOOK_PAYLOAD_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_workbook_payload-debug: $(WORKBOOK_PAYLOAD_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(WORKBOOK_PAYLOAD_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(WORKBOOK_PAYLOAD_OBJECTS:.o=.d) $(WORKBOOK_PAYLOAD_SAN_OBJECTS:.o=.d)

WORKBOOK_SAVE_OBJECTS = $(addprefix build/obj/release/app/,$(addsuffix .o,$(basename $(CORE)))) build/obj/release/app/tests/test_workbook_save.o
WORKBOOK_SAVE_SAN_OBJECTS = $(addprefix build/obj/sanitize/app/,$(addsuffix .o,$(basename $(CORE)))) build/obj/sanitize/app/tests/test_workbook_save.o
build/tests/test_workbook_save: $(WORKBOOK_SAVE_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(WORKBOOK_SAVE_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_workbook_save-debug: $(WORKBOOK_SAVE_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(WORKBOOK_SAVE_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(WORKBOOK_SAVE_OBJECTS:.o=.d) $(WORKBOOK_SAVE_SAN_OBJECTS:.o=.d)

# Only these test objects expose a deterministic saving-thread barrier.
build/obj/feature-workbook-save/worker.o: src/data/worker.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_WORKBOOK_SAVE)
build/obj/feature-workbook-save-sanitize/worker.o: src/data/worker.c build/obj/sanitize/app/compile.flags
	$(call compile_object,$(APP_SANITIZE_FLAGS) -DDUXCEL_FEATURE_WORKBOOK_SAVE)
WORKER_SAVE_OBJECTS = $(filter-out %/src/data/worker.o,$(addprefix build/obj/release/app/,$(addsuffix .o,$(basename $(CORE))))) build/obj/feature-workbook-save/worker.o build/obj/release/app/tests/test_worker_save.o
WORKER_SAVE_SAN_OBJECTS = $(filter-out %/src/data/worker.o,$(addprefix build/obj/sanitize/app/,$(addsuffix .o,$(basename $(CORE))))) build/obj/feature-workbook-save-sanitize/worker.o build/obj/sanitize/app/tests/test_worker_save.o
build/tests/test_worker_save: $(WORKER_SAVE_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(WORKER_SAVE_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_worker_save-debug: $(WORKER_SAVE_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(WORKER_SAVE_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(WORKER_SAVE_OBJECTS:.o=.d) $(WORKER_SAVE_SAN_OBJECTS:.o=.d)

build/obj/feature-workbook-save/app.o: src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_WORKBOOK_SAVE)
WORKBOOK_SAVE_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/data/worker.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) build/obj/feature-workbook-save/app.o build/obj/feature-workbook-save/worker.o
build/tests/test_workbook_save-gui: tests/test_workbook_save_gui.c $(WORKBOOK_SAVE_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_workbook_save_gui.c $(WORKBOOK_SAVE_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include build/obj/feature-workbook-save/app.d

DEFINITION_STATE_OBJECTS = build/obj/release/source-scope/src/workbook/definition_state.o build/obj/release/source-scope/tests/test_definition_state.o
DEFINITION_STATE_SAN_OBJECTS = build/obj/sanitize/source-scope/src/workbook/definition_state.o build/obj/sanitize/source-scope/tests/test_definition_state.o
build/tests/test_definition_state: $(DEFINITION_STATE_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(DEFINITION_STATE_OBJECTS),)
build/tests/test_definition_state-debug: $(DEFINITION_STATE_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(DEFINITION_STATE_SAN_OBJECTS),$(SANFLAGS))
-include $(DEFINITION_STATE_OBJECTS:.o=.d) $(DEFINITION_STATE_SAN_OBJECTS:.o=.d)

OBJECTS_ASSET_SRC = src/common.c src/transforms/analysis.c src/transforms/number.c src/transforms/pivot.c src/transforms/marks.c src/workbook/cells_tsv_scan.c src/workbook/cells.c src/workbook/objects.c src/workbook/definition_wire.c src/workbook/analysis_asset.c src/workbook/pivot_asset.c src/workbook/marks_asset.c src/workbook/objects_asset.c tests/test_objects_asset.c
OBJECTS_ASSET_OBJECTS = $(addprefix build/obj/release/source-scope/,$(addsuffix .o,$(basename $(OBJECTS_ASSET_SRC))))
OBJECTS_ASSET_SAN_OBJECTS = $(addprefix build/obj/sanitize/source-scope/,$(addsuffix .o,$(basename $(OBJECTS_ASSET_SRC))))
build/tests/test_objects_asset: $(OBJECTS_ASSET_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(OBJECTS_ASSET_OBJECTS),-lm)
build/tests/test_objects_asset-debug: $(OBJECTS_ASSET_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(OBJECTS_ASSET_SAN_OBJECTS),-lm $(SANFLAGS))
-include $(OBJECTS_ASSET_OBJECTS:.o=.d) $(OBJECTS_ASSET_SAN_OBJECTS:.o=.d)

DEFINITION_DEPENDENCIES_SRC = $(filter-out tests/test_objects_asset.c,$(OBJECTS_ASSET_SRC)) src/data/source_digest.c src/workbook/source_scope.c src/workbook/source_versions_codec.c src/workbook/definition_dependencies.c tests/test_definition_dependencies.c
DEFINITION_DEPENDENCIES_OBJECTS = $(addprefix build/obj/release/source-scope/,$(addsuffix .o,$(basename $(DEFINITION_DEPENDENCIES_SRC))))
DEFINITION_DEPENDENCIES_SAN_OBJECTS = $(addprefix build/obj/sanitize/source-scope/,$(addsuffix .o,$(basename $(DEFINITION_DEPENDENCIES_SRC))))
build/tests/test_definition_dependencies: $(DEFINITION_DEPENDENCIES_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(DEFINITION_DEPENDENCIES_OBJECTS),-lm)
build/tests/test_definition_dependencies-debug: $(DEFINITION_DEPENDENCIES_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(DEFINITION_DEPENDENCIES_SAN_OBJECTS),-lm $(SANFLAGS))
-include $(DEFINITION_DEPENDENCIES_OBJECTS:.o=.d) $(DEFINITION_DEPENDENCIES_SAN_OBJECTS:.o=.d)

DEFINITION_EVENT_SRC = $(filter-out tests/test_definition_dependencies.c,$(DEFINITION_DEPENDENCIES_SRC)) src/workbook/definition_state.c src/workbook/format.c src/workbook/format_delta_codec.c src/workbook/format_dependencies.c src/workbook/definition_event.c tests/test_definition_event.c
DEFINITION_EVENT_OBJECTS = $(addprefix build/obj/release/source-scope/,$(addsuffix .o,$(basename $(DEFINITION_EVENT_SRC))))
DEFINITION_EVENT_SAN_OBJECTS = $(addprefix build/obj/sanitize/source-scope/,$(addsuffix .o,$(basename $(DEFINITION_EVENT_SRC))))
build/tests/test_definition_event: $(DEFINITION_EVENT_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(DEFINITION_EVENT_OBJECTS),-lm)
build/tests/test_definition_event-debug: $(DEFINITION_EVENT_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(DEFINITION_EVENT_SAN_OBJECTS),-lm $(SANFLAGS))
-include $(DEFINITION_EVENT_OBJECTS:.o=.d) $(DEFINITION_EVENT_SAN_OBJECTS:.o=.d)

SOURCE_FORMAT_COMMAND_SRC = $(filter-out tests/test_cell_command.c,$(CELL_COMMAND_TEST_SRC)) tests/test_source_format_command.c
SOURCE_FORMAT_COMMAND_OBJECTS = $(addprefix build/obj/release/cell-command/,$(addsuffix .o,$(basename $(SOURCE_FORMAT_COMMAND_SRC))))
SOURCE_FORMAT_COMMAND_SAN_OBJECTS = $(addprefix build/obj/sanitize/cell-command/,$(addsuffix .o,$(basename $(SOURCE_FORMAT_COMMAND_SRC))))
build/tests/test_source_format_command: $(SOURCE_FORMAT_COMMAND_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(SOURCE_FORMAT_COMMAND_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_source_format_command-debug: $(SOURCE_FORMAT_COMMAND_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(SOURCE_FORMAT_COMMAND_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(SOURCE_FORMAT_COMMAND_OBJECTS:.o=.d) $(SOURCE_FORMAT_COMMAND_SAN_OBJECTS:.o=.d)

PRIMARY_SOURCE_DRAFT_SRC = $(filter-out tests/test_cell_command.c,$(CELL_COMMAND_TEST_SRC)) src/workbook/primary_source_draft.c tests/test_primary_source_draft.c
PRIMARY_SOURCE_DRAFT_OBJECTS = $(addprefix build/obj/release/cell-command/,$(addsuffix .o,$(basename $(PRIMARY_SOURCE_DRAFT_SRC))))
PRIMARY_SOURCE_DRAFT_SAN_OBJECTS = $(addprefix build/obj/sanitize/cell-command/,$(addsuffix .o,$(basename $(PRIMARY_SOURCE_DRAFT_SRC))))
build/tests/test_primary_source_draft: $(PRIMARY_SOURCE_DRAFT_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(PRIMARY_SOURCE_DRAFT_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_primary_source_draft-debug: $(PRIMARY_SOURCE_DRAFT_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(PRIMARY_SOURCE_DRAFT_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(PRIMARY_SOURCE_DRAFT_OBJECTS:.o=.d) $(PRIMARY_SOURCE_DRAFT_SAN_OBJECTS:.o=.d)

DEFINITION_ORIGIN_SRC = $(filter-out tests/test_source_format_command.c src/workbook/definition_origin.c,$(SOURCE_FORMAT_COMMAND_SRC)) src/workbook/definition_origin.c tests/test_definition_origin.c
DEFINITION_ORIGIN_OBJECTS = $(addprefix build/obj/release/cell-command/,$(addsuffix .o,$(basename $(DEFINITION_ORIGIN_SRC))))
DEFINITION_ORIGIN_SAN_OBJECTS = $(addprefix build/obj/sanitize/cell-command/,$(addsuffix .o,$(basename $(DEFINITION_ORIGIN_SRC))))
build/tests/test_definition_origin: $(DEFINITION_ORIGIN_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(DEFINITION_ORIGIN_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_definition_origin-debug: $(DEFINITION_ORIGIN_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(DEFINITION_ORIGIN_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(DEFINITION_ORIGIN_OBJECTS:.o=.d) $(DEFINITION_ORIGIN_SAN_OBJECTS:.o=.d)

DEFINITION_BASELINE_SRC = src/workbook/definition_baseline.c tests/test_definition_baseline.c
DEFINITION_BASELINE_OBJECTS = $(addprefix build/obj/release/source-scope/,$(addsuffix .o,$(basename $(DEFINITION_BASELINE_SRC))))
DEFINITION_BASELINE_SAN_OBJECTS = $(addprefix build/obj/sanitize/source-scope/,$(addsuffix .o,$(basename $(DEFINITION_BASELINE_SRC))))
build/tests/test_definition_baseline: $(DEFINITION_BASELINE_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(DEFINITION_BASELINE_OBJECTS),-lm)
build/tests/test_definition_baseline-debug: $(DEFINITION_BASELINE_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(DEFINITION_BASELINE_SAN_OBJECTS),-lm $(SANFLAGS))
-include $(DEFINITION_BASELINE_OBJECTS:.o=.d) $(DEFINITION_BASELINE_SAN_OBJECTS:.o=.d)

OBJECT_SOURCE_PLAN_SRC = $(filter-out tests/test_objects_asset.c,$(OBJECTS_ASSET_SRC)) src/workbook/definition_state.c src/workbook/definition_baseline.c src/workbook/object_source_plan.c tests/test_object_source_plan.c
OBJECT_SOURCE_PLAN_OBJECTS = $(addprefix build/obj/release/source-scope/,$(addsuffix .o,$(basename $(OBJECT_SOURCE_PLAN_SRC))))
OBJECT_SOURCE_PLAN_SAN_OBJECTS = $(addprefix build/obj/sanitize/source-scope/,$(addsuffix .o,$(basename $(OBJECT_SOURCE_PLAN_SRC))))
build/tests/test_object_source_plan: $(OBJECT_SOURCE_PLAN_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(OBJECT_SOURCE_PLAN_OBJECTS),-lm)
build/tests/test_object_source_plan-debug: $(OBJECT_SOURCE_PLAN_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(OBJECT_SOURCE_PLAN_SAN_OBJECTS),-lm $(SANFLAGS))
-include $(OBJECT_SOURCE_PLAN_OBJECTS:.o=.d) $(OBJECT_SOURCE_PLAN_SAN_OBJECTS:.o=.d)

HISTORY_DEFINITION_SRC = $(filter-out tests/test_definition_event.c,$(DEFINITION_EVENT_SRC)) src/workbook/history_stream.c src/workbook/cell_delta.c src/workbook/cell_delta_codec.c src/workbook/history_format.c src/workbook/history_definition.c src/workbook/definition_baseline.c tests/test_history_definition.c
HISTORY_DEFINITION_OBJECTS = $(addprefix build/obj/release/source-scope/,$(addsuffix .o,$(basename $(HISTORY_DEFINITION_SRC))))
HISTORY_DEFINITION_SAN_OBJECTS = $(addprefix build/obj/sanitize/source-scope/,$(addsuffix .o,$(basename $(HISTORY_DEFINITION_SRC))))
build/tests/test_history_definition: $(HISTORY_DEFINITION_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(HISTORY_DEFINITION_OBJECTS),-lm)
build/tests/test_history_definition-debug: $(HISTORY_DEFINITION_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(HISTORY_DEFINITION_SAN_OBJECTS),-lm $(SANFLAGS))
-include $(HISTORY_DEFINITION_OBJECTS:.o=.d) $(HISTORY_DEFINITION_SAN_OBJECTS:.o=.d)

MARKS_ASSET_SRC = src/common.c src/transforms/marks.c src/workbook/definition_wire.c src/workbook/marks_asset.c tests/test_marks_asset.c
MARKS_ASSET_OBJECTS = $(addprefix build/obj/release/source-scope/,$(addsuffix .o,$(basename $(MARKS_ASSET_SRC))))
MARKS_ASSET_SAN_OBJECTS = $(addprefix build/obj/sanitize/source-scope/,$(addsuffix .o,$(basename $(MARKS_ASSET_SRC))))
build/tests/test_marks_asset: $(MARKS_ASSET_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(MARKS_ASSET_OBJECTS),-lm)
build/tests/test_marks_asset-debug: $(MARKS_ASSET_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(MARKS_ASSET_SAN_OBJECTS),-lm $(SANFLAGS))
-include $(MARKS_ASSET_OBJECTS:.o=.d) $(MARKS_ASSET_SAN_OBJECTS:.o=.d)

PIVOT_ASSET_SRC = src/common.c src/transforms/analysis.c src/transforms/number.c src/transforms/pivot.c src/workbook/definition_wire.c src/workbook/pivot_asset.c tests/test_pivot_asset.c
PIVOT_ASSET_OBJECTS = $(addprefix build/obj/release/source-scope/,$(addsuffix .o,$(basename $(PIVOT_ASSET_SRC))))
PIVOT_ASSET_SAN_OBJECTS = $(addprefix build/obj/sanitize/source-scope/,$(addsuffix .o,$(basename $(PIVOT_ASSET_SRC))))
build/tests/test_pivot_asset: $(PIVOT_ASSET_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(PIVOT_ASSET_OBJECTS),-lm)
build/tests/test_pivot_asset-debug: $(PIVOT_ASSET_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(PIVOT_ASSET_SAN_OBJECTS),-lm $(SANFLAGS))
-include $(PIVOT_ASSET_OBJECTS:.o=.d) $(PIVOT_ASSET_SAN_OBJECTS:.o=.d)

ANALYSIS_ASSET_SRC = src/common.c src/transforms/analysis.c src/transforms/number.c src/workbook/definition_wire.c src/workbook/analysis_asset.c tests/test_analysis_asset.c
ANALYSIS_ASSET_OBJECTS = $(addprefix build/obj/release/source-scope/,$(addsuffix .o,$(basename $(ANALYSIS_ASSET_SRC))))
ANALYSIS_ASSET_SAN_OBJECTS = $(addprefix build/obj/sanitize/source-scope/,$(addsuffix .o,$(basename $(ANALYSIS_ASSET_SRC))))
build/tests/test_analysis_asset: $(ANALYSIS_ASSET_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(ANALYSIS_ASSET_OBJECTS),-lm)
build/tests/test_analysis_asset-debug: $(ANALYSIS_ASSET_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(ANALYSIS_ASSET_SAN_OBJECTS),-lm $(SANFLAGS))
-include $(ANALYSIS_ASSET_OBJECTS:.o=.d) $(ANALYSIS_ASSET_SAN_OBJECTS:.o=.d)

FORMAT_DEPENDENCIES_OBJECTS = $(filter-out %/tests/test_source_scope.o,$(SOURCE_SCOPE_OBJECTS)) build/obj/release/source-scope/src/workbook/source_versions_codec.o build/obj/release/app/src/workbook/format.o build/obj/release/app/src/workbook/format_delta_codec.o build/obj/release/app/src/workbook/format_dependencies.o build/obj/release/app/tests/test_format_dependencies.o
FORMAT_DEPENDENCIES_SAN_OBJECTS = $(filter-out %/tests/test_source_scope.o,$(SOURCE_SCOPE_SAN_OBJECTS)) build/obj/sanitize/source-scope/src/workbook/source_versions_codec.o build/obj/sanitize/app/src/workbook/format.o build/obj/sanitize/app/src/workbook/format_delta_codec.o build/obj/sanitize/app/src/workbook/format_dependencies.o build/obj/sanitize/app/tests/test_format_dependencies.o
build/tests/test_format_dependencies: $(FORMAT_DEPENDENCIES_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(FORMAT_DEPENDENCIES_OBJECTS),-lm)
build/tests/test_format_dependencies-debug: $(FORMAT_DEPENDENCIES_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(FORMAT_DEPENDENCIES_SAN_OBJECTS),-lm $(SANFLAGS))
-include $(FORMAT_DEPENDENCIES_OBJECTS:.o=.d) $(FORMAT_DEPENDENCIES_SAN_OBJECTS:.o=.d)

HISTORY_FORMAT_OBJECTS = $(filter-out %/tests/test_format_dependencies.o,$(FORMAT_DEPENDENCIES_OBJECTS)) build/obj/release/app/src/workbook/history_stream.o build/obj/release/app/src/workbook/cell_delta_codec.o build/obj/release/app/src/workbook/cell_delta.o build/obj/release/app/src/workbook/history_format.o build/obj/release/app/tests/test_history_format.o
HISTORY_FORMAT_SAN_OBJECTS = $(filter-out %/tests/test_format_dependencies.o,$(FORMAT_DEPENDENCIES_SAN_OBJECTS)) build/obj/sanitize/app/src/workbook/history_stream.o build/obj/sanitize/app/src/workbook/cell_delta_codec.o build/obj/sanitize/app/src/workbook/cell_delta.o build/obj/sanitize/app/src/workbook/history_format.o build/obj/sanitize/app/tests/test_history_format.o
build/tests/test_history_format: $(HISTORY_FORMAT_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(HISTORY_FORMAT_OBJECTS),-lm)
build/tests/test_history_format-debug: $(HISTORY_FORMAT_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(HISTORY_FORMAT_SAN_OBJECTS),-lm $(SANFLAGS))
-include $(HISTORY_FORMAT_OBJECTS:.o=.d) $(HISTORY_FORMAT_SAN_OBJECTS:.o=.d)

SOURCE_VERSIONS_CODEC_OBJECTS = $(filter-out %/tests/test_source_scope.o,$(SOURCE_SCOPE_OBJECTS)) build/obj/release/source-scope/src/workbook/source_versions_codec.o build/obj/release/source-scope/tests/test_source_versions_codec.o
SOURCE_VERSIONS_CODEC_SAN_OBJECTS = $(filter-out %/tests/test_source_scope.o,$(SOURCE_SCOPE_SAN_OBJECTS)) build/obj/sanitize/source-scope/src/workbook/source_versions_codec.o build/obj/sanitize/source-scope/tests/test_source_versions_codec.o
build/tests/test_source_versions_codec: $(SOURCE_VERSIONS_CODEC_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(SOURCE_VERSIONS_CODEC_OBJECTS),-lm)
build/tests/test_source_versions_codec-debug: $(SOURCE_VERSIONS_CODEC_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(SOURCE_VERSIONS_CODEC_SAN_OBJECTS),-lm $(SANFLAGS))
-include $(SOURCE_VERSIONS_CODEC_OBJECTS:.o=.d) $(SOURCE_VERSIONS_CODEC_SAN_OBJECTS:.o=.d)

FORMAT_DELTA_CODEC_OBJECTS = build/obj/release/app/src/common.o build/obj/release/app/src/workbook/format.o build/obj/release/app/src/workbook/format_delta_codec.o build/obj/release/app/tests/test_format_delta_codec.o
FORMAT_DELTA_CODEC_SAN_OBJECTS = build/obj/sanitize/app/src/common.o build/obj/sanitize/app/src/workbook/format.o build/obj/sanitize/app/src/workbook/format_delta_codec.o build/obj/sanitize/app/tests/test_format_delta_codec.o
build/tests/test_format_delta_codec: $(FORMAT_DELTA_CODEC_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(FORMAT_DELTA_CODEC_OBJECTS),)
build/tests/test_format_delta_codec-debug: $(FORMAT_DELTA_CODEC_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(FORMAT_DELTA_CODEC_SAN_OBJECTS),$(SANFLAGS))
-include $(FORMAT_DELTA_CODEC_OBJECTS:.o=.d) $(FORMAT_DELTA_CODEC_SAN_OBJECTS:.o=.d)

HISTORY_STREAM_OBJECTS = build/obj/release/app/src/common.o build/obj/release/app/src/workbook/history_stream.o build/obj/release/app/src/workbook/cell_delta_codec.o build/obj/release/app/src/workbook/cell_delta.o build/obj/release/app/tests/test_history_stream.o
HISTORY_STREAM_SAN_OBJECTS = build/obj/sanitize/app/src/common.o build/obj/sanitize/app/src/workbook/history_stream.o build/obj/sanitize/app/src/workbook/cell_delta_codec.o build/obj/sanitize/app/src/workbook/cell_delta.o build/obj/sanitize/app/tests/test_history_stream.o
build/tests/test_history_stream: $(HISTORY_STREAM_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(HISTORY_STREAM_OBJECTS),)
build/tests/test_history_stream-debug: $(HISTORY_STREAM_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(HISTORY_STREAM_SAN_OBJECTS),$(SANFLAGS))
-include $(HISTORY_STREAM_OBJECTS:.o=.d) $(HISTORY_STREAM_SAN_OBJECTS:.o=.d)

CELL_DELTA_CODEC_OBJECTS = build/obj/release/app/src/workbook/cell_delta_codec.o build/obj/release/app/src/workbook/cell_delta.o build/obj/release/app/tests/test_cell_delta_codec.o
CELL_DELTA_CODEC_SAN_OBJECTS = build/obj/sanitize/app/src/workbook/cell_delta_codec.o build/obj/sanitize/app/src/workbook/cell_delta.o build/obj/sanitize/app/tests/test_cell_delta_codec.o
build/tests/test_cell_delta_codec: $(CELL_DELTA_CODEC_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(CELL_DELTA_CODEC_OBJECTS),)
build/tests/test_cell_delta_codec-debug: $(CELL_DELTA_CODEC_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(CELL_DELTA_CODEC_SAN_OBJECTS),$(SANFLAGS))
-include $(CELL_DELTA_CODEC_OBJECTS:.o=.d) $(CELL_DELTA_CODEC_SAN_OBJECTS:.o=.d)

CELL_DELTA_OBJECTS = build/obj/release/app/src/workbook/cell_delta.o build/obj/release/app/tests/test_cell_delta.o
CELL_DELTA_SAN_OBJECTS = build/obj/sanitize/app/src/workbook/cell_delta.o build/obj/sanitize/app/tests/test_cell_delta.o
build/tests/test_cell_delta: $(CELL_DELTA_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(CELL_DELTA_OBJECTS),)
build/tests/test_cell_delta-debug: $(CELL_DELTA_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(CELL_DELTA_SAN_OBJECTS),$(SANFLAGS))
-include $(CELL_DELTA_OBJECTS:.o=.d) $(CELL_DELTA_SAN_OBJECTS:.o=.d)

HISTORY_PATH_OBJECTS = build/obj/release/app/src/workbook/history_path.o build/obj/release/app/src/workbook/history_anchors.o build/obj/release/app/src/workbook/history_window.o build/obj/release/app/src/workbook/version_index.o build/obj/release/app/tests/test_history_path.o
HISTORY_PATH_SAN_OBJECTS = build/obj/sanitize/app/src/workbook/history_path.o build/obj/sanitize/app/src/workbook/history_anchors.o build/obj/sanitize/app/src/workbook/history_window.o build/obj/sanitize/app/src/workbook/version_index.o build/obj/sanitize/app/tests/test_history_path.o
build/tests/test_history_path: $(HISTORY_PATH_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(HISTORY_PATH_OBJECTS),)
build/tests/test_history_path-debug: $(HISTORY_PATH_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(HISTORY_PATH_SAN_OBJECTS),$(SANFLAGS))
-include $(HISTORY_PATH_OBJECTS:.o=.d) $(HISTORY_PATH_SAN_OBJECTS:.o=.d)

HISTORY_ANCHORS_OBJECTS = build/obj/release/app/src/workbook/history_anchors.o build/obj/release/app/src/workbook/version_index.o build/obj/release/app/tests/test_history_anchors.o
HISTORY_ANCHORS_SAN_OBJECTS = build/obj/sanitize/app/src/workbook/history_anchors.o build/obj/sanitize/app/src/workbook/version_index.o build/obj/sanitize/app/tests/test_history_anchors.o
build/tests/test_history_anchors: $(HISTORY_ANCHORS_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(HISTORY_ANCHORS_OBJECTS),)
build/tests/test_history_anchors-debug: $(HISTORY_ANCHORS_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(HISTORY_ANCHORS_SAN_OBJECTS),$(SANFLAGS))
-include $(HISTORY_ANCHORS_OBJECTS:.o=.d) $(HISTORY_ANCHORS_SAN_OBJECTS:.o=.d)

HISTORY_WINDOW_OBJECTS = build/obj/release/app/src/workbook/history_window.o build/obj/release/app/tests/test_history_window.o
HISTORY_WINDOW_SAN_OBJECTS = build/obj/sanitize/app/src/workbook/history_window.o build/obj/sanitize/app/tests/test_history_window.o
build/tests/test_history_window: $(HISTORY_WINDOW_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(HISTORY_WINDOW_OBJECTS),)
build/tests/test_history_window-debug: $(HISTORY_WINDOW_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(HISTORY_WINDOW_SAN_OBJECTS),$(SANFLAGS))
-include $(HISTORY_WINDOW_OBJECTS:.o=.d) $(HISTORY_WINDOW_SAN_OBJECTS:.o=.d)

build/obj/feature-history-preview/app.o: src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_HISTORY_PREVIEW)
HISTORY_PREVIEW_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/data/worker_archive.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) build/obj/feature-history-preview/app.o build/obj/feature-preview-worker/adapter.o
build/tests/test_history_preview-gui: tests/test_history_preview_gui.c $(HISTORY_PREVIEW_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_history_preview_gui.c $(HISTORY_PREVIEW_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include build/obj/feature-history-preview/app.d

# Explicit milestone interaction: the same historical UI with full-App ASan/UBSan.
# Ordinary history-preview feature runs keep their small release GUI path.
build/obj/feature-history-preview-sanitize/app.o: src/app/app.c build/obj/sanitize/app/compile.flags
	$(call compile_object,$(APP_SANITIZE_FLAGS) -DDUXCEL_FEATURE_HISTORY_PREVIEW)
HISTORY_PREVIEW_SAN_GUI_LINK = $(filter-out build/obj/sanitize/app/src/app/app.o build/obj/sanitize/app/src/data/worker_archive.o build/obj/sanitize/app/src/main.o,$(APP_SANITIZE_OBJECTS)) build/obj/feature-history-preview-sanitize/app.o build/obj/feature-preview-worker-sanitize/adapter.o
build/tests/test_history_preview-gui-debug: tests/test_history_preview_gui.c $(HISTORY_PREVIEW_SAN_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_SANITIZE_FLAGS) tests/test_history_preview_gui.c $(HISTORY_PREVIEW_SAN_GUI_LINK),$(LDFLAGS) $(LDLIBS) $(SANFLAGS))
-include build/obj/feature-history-preview-sanitize/app.d

build/obj/feature-version-controls/app.o: src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_VERSION_CONTROLS)
VERSION_CONTROLS_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/data/worker.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) build/obj/feature-version-controls/app.o build/obj/feature-workbook-save/worker.o
ifeq ($(shell uname -s),Darwin)
VERSION_CONTROLS_GUI_LINK += build/obj/release/app/tests/test_app_lifecycle_native.o
endif
build/tests/test_version_controls-gui: tests/test_version_controls_gui.c $(VERSION_CONTROLS_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_version_controls_gui.c $(VERSION_CONTROLS_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include build/obj/feature-version-controls/app.d


FILE_LEASE_OBJECTS = build/obj/release/app/src/common.o build/obj/release/app/src/platform/file_lease.o build/obj/release/app/tests/test_file_lease.o
FILE_LEASE_SAN_OBJECTS = build/obj/sanitize/app/src/common.o build/obj/sanitize/app/src/platform/file_lease.o build/obj/sanitize/app/tests/test_file_lease.o
build/tests/test_file_lease: $(FILE_LEASE_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(FILE_LEASE_OBJECTS),)
build/tests/test_file_lease-debug: $(FILE_LEASE_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(FILE_LEASE_SAN_OBJECTS),$(SANFLAGS))
-include $(FILE_LEASE_OBJECTS:.o=.d) $(FILE_LEASE_SAN_OBJECTS:.o=.d)

RECOVERY_OBJECTS = $(filter-out %/tests/test_journal_session.o,$(JOURNAL_SESSION_OBJECTS)) build/obj/release/app/tests/test_workbook_recovery.o
RECOVERY_SAN_OBJECTS = $(filter-out %/tests/test_journal_session.o,$(JOURNAL_SESSION_SAN_OBJECTS)) build/obj/sanitize/app/tests/test_workbook_recovery.o
build/tests/test_workbook_recovery: $(RECOVERY_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(RECOVERY_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_workbook_recovery-debug: $(RECOVERY_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(RECOVERY_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(RECOVERY_OBJECTS:.o=.d) $(RECOVERY_SAN_OBJECTS:.o=.d)

DIRECTORY_OBJECTS = build/obj/release/app/src/common.o build/obj/release/app/src/platform/directory.o build/obj/release/app/tests/test_directory_cursor.o
DIRECTORY_SAN_OBJECTS = build/obj/sanitize/app/src/common.o build/obj/sanitize/app/src/platform/directory.o build/obj/sanitize/app/tests/test_directory_cursor.o
build/tests/test_directory_cursor: $(DIRECTORY_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(DIRECTORY_OBJECTS),)
build/tests/test_directory_cursor-debug: $(DIRECTORY_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(DIRECTORY_SAN_OBJECTS),$(SANFLAGS))
-include $(DIRECTORY_OBJECTS:.o=.d) $(DIRECTORY_SAN_OBJECTS:.o=.d)
RECOVERY_CATALOG_OBJECTS = $(filter-out %/tests/test_journal_session.o,$(JOURNAL_SESSION_OBJECTS)) build/obj/release/app/tests/test_recovery_catalog.o
RECOVERY_CATALOG_SAN_OBJECTS = $(filter-out %/tests/test_journal_session.o,$(JOURNAL_SESSION_SAN_OBJECTS)) build/obj/sanitize/app/tests/test_recovery_catalog.o
build/tests/test_recovery_catalog: $(RECOVERY_CATALOG_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(RECOVERY_CATALOG_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_recovery_catalog-debug: $(RECOVERY_CATALOG_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(RECOVERY_CATALOG_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(RECOVERY_CATALOG_OBJECTS:.o=.d) $(RECOVERY_CATALOG_SAN_OBJECTS:.o=.d)

build/obj/feature-recovery-worker/recovery_worker.o: src/workbook/recovery_worker.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_TEST_RECOVERY_WORKER)
build/obj/feature-recovery-worker-sanitize/recovery_worker.o: src/workbook/recovery_worker.c build/obj/sanitize/app/compile.flags
	$(call compile_object,$(APP_SANITIZE_FLAGS) -DDUXCEL_TEST_RECOVERY_WORKER)
RECOVERY_WORKER_OBJECTS = $(filter-out %/tests/test_journal_session.o %/src/workbook/recovery_worker.o,$(JOURNAL_SESSION_OBJECTS)) build/obj/feature-recovery-worker/recovery_worker.o build/obj/release/app/tests/test_recovery_worker.o
RECOVERY_WORKER_SAN_OBJECTS = $(filter-out %/tests/test_journal_session.o %/src/workbook/recovery_worker.o,$(JOURNAL_SESSION_SAN_OBJECTS)) build/obj/feature-recovery-worker-sanitize/recovery_worker.o build/obj/sanitize/app/tests/test_recovery_worker.o
build/tests/test_recovery_worker: $(RECOVERY_WORKER_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(RECOVERY_WORKER_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
build/tests/test_recovery_worker-debug: $(RECOVERY_WORKER_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(RECOVERY_WORKER_SAN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS))
-include $(RECOVERY_WORKER_OBJECTS:.o=.d) $(RECOVERY_WORKER_SAN_OBJECTS:.o=.d)

RECOVERY_GUI_OBJECT = build/obj/feature-recovery/app.o
$(RECOVERY_GUI_OBJECT): src/app/app.c build/obj/release/app/compile.flags
	$(call compile_object,$(APP_RELEASE_FLAGS) -DDUXCEL_FEATURE_RECOVERY)
RECOVERY_GUI_LINK = $(filter-out build/obj/release/app/src/app/app.o build/obj/release/app/src/main.o,$(APP_RELEASE_OBJECTS)) $(RECOVERY_GUI_OBJECT)
build/tests/test_recovery-gui: tests/test_recovery_gui.c $(RECOVERY_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_recovery_gui.c $(RECOVERY_GUI_LINK),$(LDFLAGS) $(LDLIBS))
-include $(RECOVERY_GUI_OBJECT:.o=.d)

build/tests/test_recovery_process: tests/test_recovery_gui.c $(RECOVERY_GUI_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_RELEASE_FLAGS) tests/test_recovery_gui.c $(RECOVERY_GUI_LINK),$(LDFLAGS) $(LDLIBS))
RECOVERY_PROCESS_SAN_OBJECT = build/obj/feature-recovery-sanitize/app.o
$(RECOVERY_PROCESS_SAN_OBJECT): src/app/app.c build/obj/sanitize/app/compile.flags
	$(call compile_object,$(APP_SANITIZE_FLAGS) -DDUXCEL_FEATURE_RECOVERY)
RECOVERY_PROCESS_SAN_LINK = $(filter-out build/obj/sanitize/app/src/app/app.o build/obj/sanitize/app/src/main.o,$(APP_SANITIZE_OBJECTS)) $(RECOVERY_PROCESS_SAN_OBJECT)
build/tests/test_recovery_process-debug: tests/test_recovery_gui.c $(RECOVERY_PROCESS_SAN_LINK) | build/tests build/tests/assets
	$(call link_binary,$(APP_SANITIZE_FLAGS) tests/test_recovery_gui.c $(RECOVERY_PROCESS_SAN_LINK),$(LDFLAGS) $(LDLIBS) $(SANFLAGS))
-include $(RECOVERY_PROCESS_SAN_OBJECT:.o=.d)

SCHEMA_TEST_OBJECTS = build/obj/release/app/src/workbook/schema.o build/obj/release/app/src/workbook/workbook_codec.o build/obj/release/app/tests/test_workbook_schema.o
SCHEMA_TEST_SAN_OBJECTS = build/obj/sanitize/app/src/workbook/schema.o build/obj/sanitize/app/src/workbook/workbook_codec.o build/obj/sanitize/app/tests/test_workbook_schema.o
build/tests/test_workbook_schema: $(SCHEMA_TEST_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(SCHEMA_TEST_OBJECTS),)
build/tests/test_workbook_schema-debug: $(SCHEMA_TEST_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(SCHEMA_TEST_SAN_OBJECTS),$(SANFLAGS))
-include $(SCHEMA_TEST_OBJECTS:.o=.d) $(SCHEMA_TEST_SAN_OBJECTS:.o=.d)

VERSION_CHUNKS_OBJECTS = build/obj/release/app/src/workbook/version_chunks.o build/obj/release/app/tests/test_version_chunks.o
VERSION_CHUNKS_SAN_OBJECTS = build/obj/sanitize/app/src/workbook/version_chunks.o build/obj/sanitize/app/tests/test_version_chunks.o
build/tests/test_version_chunks: $(VERSION_CHUNKS_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(VERSION_CHUNKS_OBJECTS),)
build/tests/test_version_chunks-debug: $(VERSION_CHUNKS_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(VERSION_CHUNKS_SAN_OBJECTS),$(SANFLAGS))
-include $(VERSION_CHUNKS_OBJECTS:.o=.d) $(VERSION_CHUNKS_SAN_OBJECTS:.o=.d)

VERSION_INDEX_TEST_OBJECTS = build/obj/release/app/src/workbook/version_index.o build/obj/release/app/tests/test_version_index.o
VERSION_INDEX_TEST_SAN_OBJECTS = build/obj/sanitize/app/src/workbook/version_index.o build/obj/sanitize/app/tests/test_version_index.o
build/tests/test_version_index: $(VERSION_INDEX_TEST_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(VERSION_INDEX_TEST_OBJECTS),)
build/tests/test_version_index-debug: $(VERSION_INDEX_TEST_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(VERSION_INDEX_TEST_SAN_OBJECTS),$(SANFLAGS))
-include $(VERSION_INDEX_TEST_OBJECTS:.o=.d) $(VERSION_INDEX_TEST_SAN_OBJECTS:.o=.d)
VERSION_TAG_TEST_OBJECTS = build/obj/release/app/src/workbook/version_index.o build/obj/release/app/src/workbook/version_tag.o build/obj/release/app/tests/test_version_tag.o
VERSION_TAG_TEST_SAN_OBJECTS = build/obj/sanitize/app/src/workbook/version_index.o build/obj/sanitize/app/src/workbook/version_tag.o build/obj/sanitize/app/tests/test_version_tag.o
build/tests/test_version_tag: $(VERSION_TAG_TEST_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(VERSION_TAG_TEST_OBJECTS),)
build/tests/test_version_tag-debug: $(VERSION_TAG_TEST_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(VERSION_TAG_TEST_SAN_OBJECTS),$(SANFLAGS))
-include $(VERSION_TAG_TEST_OBJECTS:.o=.d) $(VERSION_TAG_TEST_SAN_OBJECTS:.o=.d)

# Pure metadata codec: actual module dependencies, no App or native engine.
SHEET_LIFECYCLE_SRC = src/common.c src/workbook/definition_wire.c src/workbook/sheet_lifecycle.c tests/test_sheet_lifecycle.c
SHEET_LIFECYCLE_OBJECTS = $(addprefix build/obj/release/app/,$(addsuffix .o,$(basename $(SHEET_LIFECYCLE_SRC))))
SHEET_LIFECYCLE_SAN_OBJECTS = $(addprefix build/obj/sanitize/app/,$(addsuffix .o,$(basename $(SHEET_LIFECYCLE_SRC))))
build/tests/test_sheet_lifecycle: $(SHEET_LIFECYCLE_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(SHEET_LIFECYCLE_OBJECTS),)
build/tests/test_sheet_lifecycle-debug: $(SHEET_LIFECYCLE_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(SHEET_LIFECYCLE_SAN_OBJECTS),$(SANFLAGS))
-include $(SHEET_LIFECYCLE_OBJECTS:.o=.d) $(SHEET_LIFECYCLE_SAN_OBJECTS:.o=.d)

CELL_RANGE_PLAN_TEST_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_OBJECTS)) build/obj/release/cell-command/tests/test_cell_range_plan.o
CELL_RANGE_PLAN_TEST_SAN_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_SAN_OBJECTS)) build/obj/sanitize/cell-command/tests/test_cell_range_plan.o
build/tests/test_cell_range_plan: $(CELL_RANGE_PLAN_TEST_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(CELL_RANGE_PLAN_TEST_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_cell_range_plan-debug: $(CELL_RANGE_PLAN_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(CELL_RANGE_PLAN_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(CELL_RANGE_PLAN_TEST_OBJECTS:.o=.d) $(CELL_RANGE_PLAN_TEST_SAN_OBJECTS:.o=.d)

RANGE_EVENT_TEST_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_OBJECTS)) build/obj/release/cell-command/tests/test_range_event.o
RANGE_EVENT_TEST_SAN_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_SAN_OBJECTS)) build/obj/sanitize/cell-command/tests/test_range_event.o
build/tests/test_range_event: $(RANGE_EVENT_TEST_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(RANGE_EVENT_TEST_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_range_event-debug: $(RANGE_EVENT_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(RANGE_EVENT_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(RANGE_EVENT_TEST_OBJECTS:.o=.d) $(RANGE_EVENT_TEST_SAN_OBJECTS:.o=.d)

RANGE_COMMAND_TEST_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_OBJECTS)) build/obj/release/cell-command/tests/test_range_command.o
RANGE_COMMAND_TEST_SAN_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_SAN_OBJECTS)) build/obj/sanitize/cell-command/tests/test_range_command.o
build/tests/test_range_command: $(RANGE_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(RANGE_COMMAND_TEST_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_range_command-debug: $(RANGE_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(RANGE_COMMAND_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(RANGE_COMMAND_TEST_OBJECTS:.o=.d) $(RANGE_COMMAND_TEST_SAN_OBJECTS:.o=.d)

HISTORY_RANGE_TEST_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_OBJECTS)) build/obj/release/cell-command/tests/test_history_range.o
HISTORY_RANGE_TEST_SAN_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_SAN_OBJECTS)) build/obj/sanitize/cell-command/tests/test_history_range.o
build/tests/test_history_range: $(HISTORY_RANGE_TEST_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(HISTORY_RANGE_TEST_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_history_range-debug: $(HISTORY_RANGE_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(HISTORY_RANGE_TEST_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(HISTORY_RANGE_TEST_OBJECTS:.o=.d) $(HISTORY_RANGE_TEST_SAN_OBJECTS:.o=.d)

SHEET_LIFECYCLE_COMMAND_SRC = src/common.c src/transforms/analysis.c src/transforms/distinct.c src/transforms/number.c src/transforms/pivot.c src/transforms/marks.c src/workbook/cells_tsv_scan.c src/workbook/cells.c src/workbook/format.c src/workbook/source_scope.c src/workbook/objects.c src/workbook/workbook.c src/workbook/column_widths.c src/workbook/column_width_history.c src/workbook/column_width_command.c src/workbook/identity.c src/workbook/history.c src/workbook/lifecycle.c src/workbook/history_stream.c src/workbook/cell_delta.c src/workbook/cell_delta_codec.c src/workbook/definition_wire.c src/workbook/sheet_lifecycle.c src/workbook/history_sheet_lifecycle.c src/workbook/sheet_lifecycle_command.c tests/test_sheet_lifecycle_command.c
SHEET_LIFECYCLE_COMMAND_OBJECTS = $(addprefix build/obj/release/cell-command/,$(addsuffix .o,$(basename $(SHEET_LIFECYCLE_COMMAND_SRC))))
SHEET_LIFECYCLE_COMMAND_SAN_OBJECTS = $(addprefix build/obj/sanitize/cell-command/,$(addsuffix .o,$(basename $(SHEET_LIFECYCLE_COMMAND_SRC))))
build/tests/test_sheet_lifecycle_command: $(SHEET_LIFECYCLE_COMMAND_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(SHEET_LIFECYCLE_COMMAND_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_sheet_lifecycle_command-debug: $(SHEET_LIFECYCLE_COMMAND_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(SHEET_LIFECYCLE_COMMAND_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(SHEET_LIFECYCLE_COMMAND_OBJECTS:.o=.d) $(SHEET_LIFECYCLE_COMMAND_SAN_OBJECTS:.o=.d)

# Pure canonical column-width assets/journal/SheetView bridge: no renderer or engine.
COLUMN_WIDTH_SRC = src/common.c src/workbook/column_widths.c
COLUMN_WIDTH_HISTORY_SRC = $(COLUMN_WIDTH_SRC) src/workbook/column_width_history.c src/workbook/column_width_command.c
COLUMN_WIDTH_OBJECTS = $(addprefix build/obj/release/column-widths/,$(addsuffix .o,$(basename $(COLUMN_WIDTH_SRC))))
COLUMN_WIDTH_SAN_OBJECTS = $(addprefix build/obj/sanitize/column-widths/,$(addsuffix .o,$(basename $(COLUMN_WIDTH_SRC))))
COLUMN_WIDTH_HISTORY_OBJECTS = $(addprefix build/obj/release/column-widths/,$(addsuffix .o,$(basename $(COLUMN_WIDTH_HISTORY_SRC))))
COLUMN_WIDTH_HISTORY_SAN_OBJECTS = $(addprefix build/obj/sanitize/column-widths/,$(addsuffix .o,$(basename $(COLUMN_WIDTH_HISTORY_SRC))))
build/obj/release/column-widths/compile.flags: force-build-signature
	$(call build_signature,$(CC) -Isrc $(DUCKDB_CFLAGS) -DDUXCEL_COLUMN_WIDTH_HISTORY_TEST $(CFLAGS) $(WARN) -MMD -MP -c)
build/obj/sanitize/column-widths/compile.flags: force-build-signature
	$(call build_signature,$(CC) -Isrc $(DUCKDB_CFLAGS) -DDUXCEL_COLUMN_WIDTH_HISTORY_TEST -O1 -g $(WARN) $(SANFLAGS) -MMD -MP -c)
$(COLUMN_WIDTH_HISTORY_OBJECTS) $(COLUMN_WIDTH_HISTORY_SAN_OBJECTS) build/obj/release/column-widths/tests/test_column_widths.o build/obj/sanitize/column-widths/tests/test_column_widths.o build/obj/release/column-widths/tests/test_column_width_history.o build/obj/sanitize/column-widths/tests/test_column_width_history.o: src/common.h src/workbook/column_widths.h
build/obj/release/column-widths/src/workbook/column_width_history.o build/obj/sanitize/column-widths/src/workbook/column_width_history.o build/obj/release/column-widths/tests/test_column_width_history.o build/obj/sanitize/column-widths/tests/test_column_width_history.o: src/workbook/column_width_history.h
build/obj/release/column-widths/%.o: %.c build/obj/release/column-widths/compile.flags
	$(call compile_object,-Isrc $(DUCKDB_CFLAGS) -DDUXCEL_COLUMN_WIDTH_HISTORY_TEST $(CFLAGS) $(WARN))
build/obj/sanitize/column-widths/%.o: %.c build/obj/sanitize/column-widths/compile.flags
	$(call compile_object,-Isrc $(DUCKDB_CFLAGS) -DDUXCEL_COLUMN_WIDTH_HISTORY_TEST -O1 -g $(WARN) $(SANFLAGS))
build/tests/test_column_widths: $(COLUMN_WIDTH_OBJECTS) build/obj/release/column-widths/tests/test_column_widths.o | build/tests
	$(call link_binary,$(CFLAGS) $^,-lm)
build/tests/test_column_widths-debug: $(COLUMN_WIDTH_SAN_OBJECTS) build/obj/sanitize/column-widths/tests/test_column_widths.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $^,-lm $(SANFLAGS))
build/tests/test_column_width_history: $(COLUMN_WIDTH_HISTORY_OBJECTS) build/obj/release/column-widths/tests/test_column_width_history.o | build/tests
	$(call link_binary,$(CFLAGS) $^,-lm)
build/tests/test_column_width_history-debug: $(COLUMN_WIDTH_HISTORY_SAN_OBJECTS) build/obj/sanitize/column-widths/tests/test_column_width_history.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $^,-lm $(SANFLAGS))
-include $(COLUMN_WIDTH_OBJECTS:.o=.d) $(COLUMN_WIDTH_SAN_OBJECTS:.o=.d) $(COLUMN_WIDTH_HISTORY_OBJECTS:.o=.d) $(COLUMN_WIDTH_HISTORY_SAN_OBJECTS:.o=.d)
-include build/obj/release/column-widths/tests/test_column_widths.d build/obj/sanitize/column-widths/tests/test_column_widths.d build/obj/release/column-widths/tests/test_column_width_history.d build/obj/sanitize/column-widths/tests/test_column_width_history.d

CELL_TSV_SRC = src/common.c src/workbook/cells.c src/workbook/cells_tsv_scan.c tests/test_cell_tsv.c
CELL_TSV_OBJECTS = $(addprefix build/obj/release/app/,$(CELL_TSV_SRC:.c=.o))
CELL_TSV_SAN_OBJECTS = $(addprefix build/obj/sanitize/app/,$(CELL_TSV_SRC:.c=.o))
build/tests/test_cell_tsv: $(CELL_TSV_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(CELL_TSV_OBJECTS),)
build/tests/test_cell_tsv-debug: $(CELL_TSV_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(CELL_TSV_SAN_OBJECTS),$(SANFLAGS))
-include $(CELL_TSV_OBJECTS:.o=.d) $(CELL_TSV_SAN_OBJECTS:.o=.d)

CELL_PASTE_PLAN_SRC = $(filter-out tests/test_cell_tsv.c,$(CELL_TSV_SRC)) src/workbook/cell_paste_plan.c tests/test_cell_paste_plan.c
CELL_PASTE_PLAN_OBJECTS = $(addprefix build/obj/release/app/,$(CELL_PASTE_PLAN_SRC:.c=.o))
CELL_PASTE_PLAN_SAN_OBJECTS = $(addprefix build/obj/sanitize/app/,$(CELL_PASTE_PLAN_SRC:.c=.o))
build/tests/test_cell_paste_plan: $(CELL_PASTE_PLAN_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $(CELL_PASTE_PLAN_OBJECTS),)
build/tests/test_cell_paste_plan-debug: $(CELL_PASTE_PLAN_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(CELL_PASTE_PLAN_SAN_OBJECTS),$(SANFLAGS))
-include $(CELL_PASTE_PLAN_OBJECTS:.o=.d) $(CELL_PASTE_PLAN_SAN_OBJECTS:.o=.d)
# Complete current sheet capture uses only its model/asset dependencies.
SHEET_PRESENCE_SRC = src/common.c src/transforms/analysis.c src/transforms/distinct.c src/transforms/number.c src/transforms/pivot.c src/transforms/marks.c src/workbook/cells_tsv_scan.c src/workbook/cells.c src/workbook/format.c src/workbook/source_scope.c src/workbook/source_versions_codec.c src/workbook/objects.c src/workbook/workbook.c src/workbook/identity.c src/workbook/history.c src/workbook/lifecycle.c src/workbook/column_widths.c src/workbook/column_width_history.c src/workbook/column_width_command.c src/workbook/analysis_asset.c src/workbook/pivot_asset.c src/workbook/marks_asset.c src/workbook/objects_asset.c src/workbook/definition_wire.c src/workbook/definition_dependencies.c src/workbook/format_delta_codec.c src/workbook/format_dependencies.c src/workbook/sheet_presence_asset.c tests/test_sheet_presence_asset.c
SHEET_PRESENCE_OBJECTS = $(addprefix build/obj/release/cell-command/,$(addsuffix .o,$(basename $(SHEET_PRESENCE_SRC))))
SHEET_PRESENCE_SAN_OBJECTS = $(addprefix build/obj/sanitize/cell-command/,$(addsuffix .o,$(basename $(SHEET_PRESENCE_SRC))))
build/tests/test_sheet_presence_asset: $(SHEET_PRESENCE_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $^,$(THREAD_LIBS) -lm)
build/tests/test_sheet_presence_asset-debug: $(SHEET_PRESENCE_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $^,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(SHEET_PRESENCE_OBJECTS:.o=.d) $(SHEET_PRESENCE_SAN_OBJECTS:.o=.d)

MIXED_RANGE_PLAN_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_OBJECTS)) build/obj/release/cell-command/tests/test_mixed_range_plan.o
MIXED_RANGE_PLAN_SAN_OBJECTS = $(filter-out %/tests/test_cell_command.o,$(CELL_COMMAND_TEST_SAN_OBJECTS)) build/obj/sanitize/cell-command/tests/test_mixed_range_plan.o
build/tests/test_mixed_range_plan: $(MIXED_RANGE_PLAN_OBJECTS) build/web-tests/platform-model.o | build/tests
	$(call link_binary,$(CFLAGS) $(MIXED_RANGE_PLAN_OBJECTS) build/web-tests/platform-model.o,$(THREAD_LIBS) -lm)
build/tests/test_mixed_range_plan-debug: $(MIXED_RANGE_PLAN_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $(MIXED_RANGE_PLAN_SAN_OBJECTS) build/web-tests/platform-model-sanitize.o,$(THREAD_LIBS) -lm $(SANFLAGS))
-include $(MIXED_RANGE_PLAN_OBJECTS:.o=.d) $(MIXED_RANGE_PLAN_SAN_OBJECTS:.o=.d)

SOURCE_ADMISSION_OBJECTS = $(filter-out %/tests/test_source_versions_codec.o,$(SOURCE_VERSIONS_CODEC_OBJECTS)) build/obj/release/source-scope/src/workbook/source_versions_admission.o build/obj/release/source-scope/tests/test_source_versions_admission.o
SOURCE_ADMISSION_SAN_OBJECTS = $(filter-out %/tests/test_source_versions_codec.o,$(SOURCE_VERSIONS_CODEC_SAN_OBJECTS)) build/obj/sanitize/source-scope/src/workbook/source_versions_admission.o build/obj/sanitize/source-scope/tests/test_source_versions_admission.o
build/tests/test_source_versions_admission: $(SOURCE_ADMISSION_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $^,-lm)
build/tests/test_source_versions_admission-debug: $(SOURCE_ADMISSION_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $^,-lm $(SANFLAGS))
-include $(SOURCE_ADMISSION_OBJECTS:.o=.d) $(SOURCE_ADMISSION_SAN_OBJECTS:.o=.d)

SOURCE_DEPENDENCY_ADMISSION_SRC = $(filter-out tests/test_definition_dependencies.c,$(DEFINITION_DEPENDENCIES_SRC)) src/workbook/format.c src/workbook/format_delta_codec.c src/workbook/format_dependencies.c src/workbook/source_versions_admission.c src/workbook/source_dependency_admission.c tests/test_source_dependency_admission.c
SOURCE_DEPENDENCY_ADMISSION_OBJECTS = $(addprefix build/obj/release/source-scope/,$(addsuffix .o,$(basename $(SOURCE_DEPENDENCY_ADMISSION_SRC))))
SOURCE_DEPENDENCY_ADMISSION_SAN_OBJECTS = $(addprefix build/obj/sanitize/source-scope/,$(addsuffix .o,$(basename $(SOURCE_DEPENDENCY_ADMISSION_SRC))))
build/tests/test_source_dependency_admission: $(SOURCE_DEPENDENCY_ADMISSION_OBJECTS) | build/tests
	$(call link_binary,$(CFLAGS) $^,-lm)
build/tests/test_source_dependency_admission-debug: $(SOURCE_DEPENDENCY_ADMISSION_SAN_OBJECTS) | build/tests
	$(call link_binary,-O1 -g $(SANFLAGS) $^,-lm $(SANFLAGS))
-include $(SOURCE_DEPENDENCY_ADMISSION_OBJECTS:.o=.d) $(SOURCE_DEPENDENCY_ADMISSION_SAN_OBJECTS:.o=.d)
