CC = clang
.DEFAULT_GOAL := all
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
CORE = src/common.c src/platform/platform.c src/transforms/distinct.c src/transforms/analysis.c src/transforms/record_sql.c src/transforms/number.c src/transforms/pivot.c src/transforms/marks.c src/data/database.c src/data/page.c src/data/source_digest.c src/workbook/clipboard.c src/workbook/cells.c src/workbook/format.c src/workbook/source_scope.c src/workbook/objects.c src/workbook/workbook.c src/data/worker.c
SRC = $(CORE) src/ui/cellreadout.c src/ui/choiceview.c src/ui/popup.c src/ui/motion.c src/ui/fontmetrics.c src/ui/paintlayer.c src/ui/paintlayer_backend.c src/ui/ui.c src/ui/ribbon.c src/ui/ribbon_spec.c src/ui/keytips.c src/ui/flyout.c src/ui/statusview.c src/ui/dockview.c src/ui/marksview.c src/ui/workspace.c src/ui/inspector.c src/ui/statsview.c src/ui/guideview.c src/ui/stepview.c src/ui/preview.c src/ui/chartview.c src/ui/chartencoding.c src/ui/textview.c src/ui/sortview.c src/ui/brand.c src/platform/icon.c src/grid/grid.c src/grid/selection_ladder.c src/app/step_preview.c src/app/app.c src/main.c
ifeq ($(shell uname -s),Darwin)
SRC += src/platform/macos.m
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
SRC += src/ui/filterlabel.c src/ui/selectview.c
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
icon-smoke: duxcel-debug icons-check
	mkdir -p tests/results/icons
	ASAN_OPTIONS=halt_on_error=1 ./duxcel-debug --icon-smoke --frames 360 --screenshot tests/results/icons/final.png
unique-smoke: duxcel-debug
	mkdir -p tests/generated tests/results/unique-rows
	ASAN_OPTIONS=halt_on_error=1 ./duxcel-debug --unique-smoke --frames 1200 --screenshot tests/results/unique-rows/final.png
query-smoke: duxcel-debug
	mkdir -p tests/generated tests/results/source-query
	ASAN_OPTIONS=halt_on_error=1 ./duxcel-debug --query-smoke --frames 3600 --screenshot tests/results/source-query/final.png
ifeq ($(shell uname -s),Darwin)
build/check-macos-icon: tools/check_macos_icon.m
	mkdir -p build
	$(CC) -O2 $(WARN) $< -framework AppKit -o $@
mac-icon-check: bundle build/check-macos-icon
	./build/check-macos-icon build/Duxcel.app
endif

all: duxcel
design-fixture: build/create-design-workbook
	node tools/export_design_fixture.mjs
	./build/create-design-workbook
design-large-fixture: build/create-design-workbook
	node tools/export_design_fixture.mjs --large
	./build/create-design-workbook --large
design-smoke: duxcel-debug design-fixture
	mkdir -p tests/results/design-definition
	ASAN_OPTIONS=halt_on_error=1 ./duxcel-debug tests/generated/design-definition.dxls --frames 240 --screenshot tests/results/design-definition/authoring.png
# App and fixture-helper compilation are incremental and isolated by exact
# configuration. Module tests retain their original independent commands below;
# in particular, renderer stubs must never reuse the application's UI objects.
APP_SOURCES = $(filter %.c %.m,$(SRC))
APP_EXTRA_OBJECTS = $(filter %.o,$(SRC))
APP_RELEASE_OBJECTS = $(addprefix build/obj/release/app/,$(addsuffix .o,$(basename $(APP_SOURCES))))
APP_SANITIZE_OBJECTS = $(addprefix build/obj/sanitize/app/,$(addsuffix .o,$(basename $(APP_SOURCES))))
APP_EDIT_OBJECTS = $(addprefix build/obj/edit-sanitize/app/,$(addsuffix .o,$(basename $(APP_SOURCES))))
DESIGN_SOURCES = $(CORE) tools/create_design_workbook.c
DESIGN_OBJECTS = $(addprefix build/obj/release/design/,$(addsuffix .o,$(basename $(DESIGN_SOURCES))))
INCREMENTAL_OBJECTS = $(APP_RELEASE_OBJECTS) $(APP_SANITIZE_OBJECTS) $(APP_EDIT_OBJECTS) $(DESIGN_OBJECTS)
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
DESIGN_FLAGS = -Isrc $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN)

# A no-op signature recipe preserves its timestamp. Changed compiler, includes,
# defines or options therefore invalidate the appropriate objects, not every
# other build configuration. Single make invocations remain serialized below.
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
build/obj/release/design/compile.flags: force-build-signature
	$(call build_signature,$(CC) $(DESIGN_FLAGS) -MMD -MP -c)
build/obj/release/app/link.flags: force-build-signature
	$(call build_signature,$(CC) $(CFLAGS) $(WARN) $(APP_RELEASE_OBJECTS) $(APP_EXTRA_OBJECTS) $(LDFLAGS) $(LDLIBS))
build/obj/sanitize/app/link.flags: force-build-signature
	$(call build_signature,$(CC) -O1 -g $(WARN) $(SANFLAGS) $(APP_SANITIZE_OBJECTS) $(APP_EXTRA_OBJECTS) $(LDFLAGS) $(LDLIBS) $(SANFLAGS))
build/obj/edit-sanitize/app/link.flags: force-build-signature
	$(call build_signature,$(CC) -O0 -g $(WARN) $(SANFLAGS) $(APP_EDIT_OBJECTS) $(APP_EXTRA_OBJECTS) $(LDFLAGS) $(LDLIBS) $(SANFLAGS))
build/obj/release/design/link.flags: force-build-signature
	$(call build_signature,$(CC) $(CFLAGS) $(WARN) $(DESIGN_OBJECTS) $(DUCKDB_LIBS) $(THREAD_LIBS) -lm)
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

# Only a successful compiler/linker invocation replaces an accepted artifact.
# Separate object and dependency temporary files preserve the previous pair on
# a compilation failure. -MT records the final path, not its temporary sibling.
define compile_object
	@mkdir -p "$(@D)"
	@set -e; trap 'rm -f "$@.new" "$(@:.o=.d).new"' 0; \
	$(CC) $(1) -MMD -MP -MF "$(@:.o=.d).new" -MT "$@" -c "$<" -o "$@.new"; \
	mv -f "$(@:.o=.d).new" "$(@:.o=.d)"; mv -f "$@.new" "$@"
endef
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
build/obj/release/design/%.o: %.c build/obj/release/design/compile.flags
	$(call compile_object,$(DESIGN_FLAGS))
$(APP_RELEASE_OBJECTS) $(APP_SANITIZE_OBJECTS) $(APP_EDIT_OBJECTS): | $(ICON_OUTPUTS)

define link_binary
	@mkdir -p "$(@D)"
	@set -e; trap 'rm -f "$@.new"' 0; \
	$(CC) $(1) -o "$@.new" $(2); mv -f "$@.new" "$@"
endef
duxcel: $(APP_RELEASE_OBJECTS) $(APP_EXTRA_OBJECTS) build/obj/release/app/link.flags
	$(call link_binary,$(CFLAGS) $(WARN) $(APP_RELEASE_OBJECTS) $(APP_EXTRA_OBJECTS),$(LDFLAGS) $(LDLIBS))
duxcel-debug: $(APP_SANITIZE_OBJECTS) $(APP_EXTRA_OBJECTS) build/obj/sanitize/app/link.flags
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

build/create-design-workbook: $(DESIGN_OBJECTS) build/obj/release/design/link.flags
	$(call link_binary,$(CFLAGS) $(WARN) $(DESIGN_OBJECTS),$(DUCKDB_LIBS) $(THREAD_LIBS) -lm)

# Losing a sidecar must not silently discard its header tracking. An absent
# empty-rule dependency forces its object to recreate the compiler's sidecar.
$(INCREMENTAL_OBJECTS): %.o: %.d
$(INCREMENTAL_OBJECTS:.o=.d):
-include $(INCREMENTAL_OBJECTS:.o=.d)
run: duxcel
	./duxcel $(ARGS)
debug: duxcel-debug
run-debug: duxcel-debug
	./duxcel-debug $(ARGS)
tests/test_brand: tests/test_brand.c src/ui/brand_geometry.h
	$(CC) -Isrc $(CFLAGS) $(WARN) tests/test_brand.c -o $@ -lm
tests/test_brand-debug: tests/test_brand.c src/ui/brand_geometry.h
	$(CC) -Isrc -O1 -g $(WARN) $(SANFLAGS) tests/test_brand.c -o $@ -lm $(SANFLAGS)
tests/test_format: src/common.c src/workbook/format.c src/transforms/number.c tests/test_format.c $(HEADERS)
	$(CC) -Isrc $(CFLAGS) $(WARN) src/common.c src/workbook/format.c src/transforms/number.c tests/test_format.c -o $@ -lm
tests/test_format-debug: src/common.c src/workbook/format.c src/transforms/number.c tests/test_format.c $(HEADERS)
	$(CC) -Isrc -O1 -g $(WARN) $(SANFLAGS) src/common.c src/workbook/format.c src/transforms/number.c tests/test_format.c -o $@ -lm $(SANFLAGS)
tests/test_chartview: src/ui/chartview.c tests/test_chartview.c $(HEADERS)
	$(CC) -Isrc $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN) src/ui/chartview.c tests/test_chartview.c -o $@ -lm
tests/test_chartview-debug: src/ui/chartview.c tests/test_chartview.c $(HEADERS)
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) $(SANFLAGS) src/ui/chartview.c tests/test_chartview.c -o $@ -lm $(SANFLAGS)
ENCODING_SRC = src/common.c src/transforms/analysis.c src/transforms/number.c src/transforms/pivot.c src/transforms/marks.c src/ui/chartview.c src/ui/chartencoding.c tests/test_chartencoding.c
tests/test_chartencoding: $(ENCODING_SRC) $(HEADERS)
	$(CC) -Isrc -Isrc/ui $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN) $(ENCODING_SRC) -o $@ -lm
tests/test_chartencoding-debug: $(ENCODING_SRC) $(HEADERS)
	$(CC) -Isrc -Isrc/ui $(DUCKDB_CFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(ENCODING_SRC) -o $@ -lm $(SANFLAGS)
GRID_PAGE_SRC = src/transforms/analysis.c src/transforms/record_sql.c src/transforms/pivot.c src/data/page.c src/data/source_digest.c
tests/test_grid: src/common.c $(GRID_PAGE_SRC) src/workbook/format.c src/transforms/number.c src/ui/motion.c src/ui/fontmetrics.c src/ui/paintlayer.c src/ui/paintlayer_backend.c src/ui/ui.c src/ui/ribbon.c src/ui/ribbon_spec.c src/ui/keytips.c src/ui/flyout.c src/ui/statusview.c src/ui/dockview.c src/ui/workspace.c src/ui/chartview.c src/ui/textview.c src/grid/grid.c tests/test_grid.c $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) src/common.c $(GRID_PAGE_SRC) src/workbook/format.c src/transforms/number.c src/ui/motion.c src/ui/fontmetrics.c src/ui/paintlayer.c src/ui/paintlayer_backend.c src/ui/ui.c src/ui/ribbon.c src/ui/ribbon_spec.c src/ui/keytips.c src/ui/flyout.c src/ui/statusview.c src/ui/dockview.c src/ui/workspace.c src/ui/chartview.c src/ui/textview.c src/grid/grid.c tests/test_grid.c -o $@ $(RAYLIB_LIBS) -lm
tests/test_grid-debug: src/common.c $(GRID_PAGE_SRC) src/workbook/format.c src/transforms/number.c src/ui/motion.c src/ui/fontmetrics.c src/ui/paintlayer.c src/ui/paintlayer_backend.c src/ui/ui.c src/ui/ribbon.c src/ui/ribbon_spec.c src/ui/keytips.c src/ui/flyout.c src/ui/statusview.c src/ui/dockview.c src/ui/workspace.c src/ui/chartview.c src/ui/textview.c src/grid/grid.c tests/test_grid.c $(HEADERS)
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) src/common.c $(GRID_PAGE_SRC) src/workbook/format.c src/transforms/number.c src/ui/motion.c src/ui/fontmetrics.c src/ui/paintlayer.c src/ui/paintlayer_backend.c src/ui/ui.c src/ui/ribbon.c src/ui/ribbon_spec.c src/ui/keytips.c src/ui/flyout.c src/ui/statusview.c src/ui/dockview.c src/ui/workspace.c src/ui/chartview.c src/ui/textview.c src/grid/grid.c tests/test_grid.c -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
SOURCE_SCOPE_TEST_SRC = src/common.c src/transforms/analysis.c src/transforms/number.c src/data/source_digest.c src/workbook/source_scope.c tests/test_source_scope.c
OBJECT_TEST_SRC = src/common.c src/transforms/analysis.c src/transforms/number.c src/transforms/pivot.c src/transforms/marks.c src/workbook/cells.c src/workbook/objects.c tests/test_objects.c
tests/test_objects: $(OBJECT_TEST_SRC) $(HEADERS)
	$(CC) -Isrc $(CFLAGS) $(WARN) -Werror $(OBJECT_TEST_SRC) -o $@ -lm
tests/test_objects-debug: $(OBJECT_TEST_SRC) $(HEADERS)
	$(CC) -Isrc -O1 -g $(WARN) -Werror $(SANFLAGS) $(OBJECT_TEST_SRC) -o $@ -lm $(SANFLAGS)
tests/test_source_scope: $(SOURCE_SCOPE_TEST_SRC) $(HEADERS)
	$(CC) -Isrc $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN) -Werror $(SOURCE_SCOPE_TEST_SRC) -o $@ -lm
tests/test_source_scope-debug: $(SOURCE_SCOPE_TEST_SRC) $(HEADERS)
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) -Werror $(SANFLAGS) $(SOURCE_SCOPE_TEST_SRC) -o $@ -lm $(SANFLAGS)
SELECTION_LADDER_SRC = src/grid/selection_ladder.c tests/test_selection_ladder.c
tests/test_selection_ladder: $(SELECTION_LADDER_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(SELECTION_LADDER_SRC) -o $@
tests/test_selection_ladder-debug: $(SELECTION_LADDER_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(SELECTION_LADDER_SRC) -o $@ $(SANFLAGS)
PAGE_SRC = src/common.c src/transforms/analysis.c src/transforms/record_sql.c src/transforms/number.c src/transforms/pivot.c src/data/page.c src/data/source_digest.c tests/test_page.c
tests/test_page: $(PAGE_SRC) $(HEADERS)
	$(CC) -Isrc $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN) $(PAGE_SRC) -o $@ -lm
tests/test_page-debug: $(PAGE_SRC) $(HEADERS)
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(PAGE_SRC) -o $@ -lm $(SANFLAGS)
PAGE_ENGINE_SRC = $(PAGE_SRC) src/platform/platform.c src/transforms/distinct.c src/data/database.c
tests/test_page-engine: $(PAGE_ENGINE_SRC) $(HEADERS)
	$(CC) -Isrc $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN) -DDUXCEL_PAGE_ENGINE_TEST $(PAGE_ENGINE_SRC) -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm
tests/test_page-engine-debug: $(PAGE_ENGINE_SRC) $(HEADERS)
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) $(SANFLAGS) -DDUXCEL_PAGE_ENGINE_TEST $(PAGE_ENGINE_SRC) -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS)
RECORD_SQL_SRC = src/common.c src/transforms/analysis.c src/transforms/number.c src/transforms/record_sql.c
RECORD_PAGE_SRC = $(RECORD_SQL_SRC) src/platform/platform.c src/transforms/pivot.c src/transforms/distinct.c src/data/page.c src/data/source_digest.c src/data/database.c
tests/test_record_sql: $(RECORD_SQL_SRC) tests/test_record_sql.c $(HEADERS)
	$(CC) -Isrc $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN) $(RECORD_SQL_SRC) tests/test_record_sql.c -o $@ $(DUCKDB_LIBS) -lm
tests/test_record_sql-debug: $(RECORD_SQL_SRC) tests/test_record_sql.c $(HEADERS)
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(RECORD_SQL_SRC) tests/test_record_sql.c -o $@ $(DUCKDB_LIBS) -lm $(SANFLAGS)
tests/test_record_pages: $(RECORD_PAGE_SRC) tests/test_record_pages.c $(HEADERS)
	$(CC) -Isrc $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN) $(RECORD_PAGE_SRC) tests/test_record_pages.c -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm
tests/test_record_pages-debug: $(RECORD_PAGE_SRC) tests/test_record_pages.c $(HEADERS)
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(RECORD_PAGE_SRC) tests/test_record_pages.c -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS)
test-records:
	python3 tools/test.py records $(TEST_ARGS)
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
tests/test_workbook: $(CORE) tests/test_workbook.c $(HEADERS)
	$(CC) -Isrc $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN) $(CORE) tests/test_workbook.c -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm
tests/test_workbook-debug: $(CORE) tests/test_workbook.c $(HEADERS)
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(CORE) tests/test_workbook.c -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS)
test-analysis-journal: tests/test_workbook tests/test_workbook-debug
	./tests/test_workbook --journal-only
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_workbook-debug --journal-only
.PHONY: test-analysis-journal
test-format-budget: tests/test_workbook tests/test_workbook-debug
	./tests/test_workbook --format-budget-only
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_workbook-debug --format-budget-only
.PHONY: test-format-budget
tests/test_core: $(CORE) tests/test_core.c $(HEADERS)
	$(CC) -Isrc $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN) $(CORE) tests/test_core.c -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm
tests/test_core-debug: $(CORE) tests/test_core.c $(HEADERS)
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(CORE) tests/test_core.c -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS)
INSPECTOR_SRC = src/ui/inspector.c tests/test_inspector.c
STEPVIEW_SRC = src/common.c src/platform/platform.c src/transforms/number.c src/transforms/analysis.c src/ui/stepview.c tests/test_stepview.c
PREVIEW_SRC = $(CORE) src/ui/motion.c src/ui/fontmetrics.c src/ui/paintlayer.c src/ui/paintlayer_backend.c src/ui/ui.c src/ui/preview.c src/ui/stepview.c src/grid/grid.c src/app/step_preview.c tests/test_preview.c
STATSVIEW_SRC = src/common.c src/transforms/number.c src/workbook/format.c src/ui/motion.c src/ui/fontmetrics.c src/ui/paintlayer.c src/ui/paintlayer_backend.c src/ui/ui.c src/ui/statsview.c tests/test_statsview.c
GUIDEVIEW_SRC = src/common.c src/transforms/number.c src/workbook/format.c src/ui/motion.c src/ui/fontmetrics.c src/ui/paintlayer.c src/ui/paintlayer_backend.c src/ui/ui.c src/ui/textview.c src/ui/guideview.c tests/test_guideview.c
FONTMETRICS_SRC = src/ui/fontmetrics.c tests/test_fontmetrics.c
CHOICEVIEW_SRC = src/ui/choiceview.c src/ui/selectview.c tests/test_choiceview.c
tests/test_choiceview: $(CHOICEVIEW_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(CHOICEVIEW_SRC) -o $@ $(RAYLIB_LIBS) -lm
tests/test_choiceview-debug: $(CHOICEVIEW_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(CHOICEVIEW_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
# Small actual-module input/paint recorders; never linked into App binaries.
PREVIEW_INPUT_SRC = src/ui/preview.c tests/test_preview_input.c
TEXTVIEW_INPUT_SRC = src/common.c src/ui/textview.c tests/test_textview.c
tests/test_preview_input: $(PREVIEW_INPUT_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(PREVIEW_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm
tests/test_preview_input-debug: $(PREVIEW_INPUT_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(PREVIEW_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
tests/test_preview_input_browser: $(PREVIEW_INPUT_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -DDUXCEL_BROWSER_HOST $(CFLAGS) $(WARN) $(PREVIEW_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm
tests/test_preview_input_browser-debug: $(PREVIEW_INPUT_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -DDUXCEL_BROWSER_HOST -O1 -g $(WARN) $(SANFLAGS) $(PREVIEW_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
tests/test_textview: $(TEXTVIEW_INPUT_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(TEXTVIEW_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm
tests/test_textview-debug: $(TEXTVIEW_INPUT_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(TEXTVIEW_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
tests/test_textview_review: $(TEXTVIEW_INPUT_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -DDUXCEL_DEV_REVIEW $(CFLAGS) $(WARN) $(TEXTVIEW_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm
tests/test_textview_review-debug: $(TEXTVIEW_INPUT_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -DDUXCEL_DEV_REVIEW -O1 -g $(WARN) $(SANFLAGS) $(TEXTVIEW_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
# Actual shared UI input, with section collection only in these test binaries.
UI_INPUT_LINK = -ffunction-sections -Wl,--gc-sections
ifeq ($(shell uname -s),Darwin)
UI_INPUT_LINK = -ffunction-sections -Wl,-dead_strip
endif
STATS_INPUT_SRC = src/ui/ui.c src/ui/statsview.c tests/test_statsview_input.c
UI_UPDATE_INPUT_SRC = src/common.c src/ui/fontmetrics.c src/ui/ui.c tests/test_ui_update_input.c
GUIDE_INPUT_SRC = src/common.c src/ui/ui.c src/ui/textview.c src/ui/motion.c src/ui/guideview.c tests/test_guideview_input.c
tests/test_guideview_input: $(GUIDE_INPUT_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(UI_INPUT_LINK) $(GUIDE_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm
tests/test_guideview_input-debug: $(GUIDE_INPUT_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(UI_INPUT_LINK) $(GUIDE_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
tests/test_guideview_input_review: $(GUIDE_INPUT_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -DDUXCEL_DEV_REVIEW -DDUXCEL_REVIEW_INPUT_IMPLEMENTATION -include src/dev/review_input.h $(CFLAGS) $(WARN) $(UI_INPUT_LINK) $(GUIDE_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm
tests/test_guideview_input_review-debug: $(GUIDE_INPUT_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -DDUXCEL_DEV_REVIEW -DDUXCEL_REVIEW_INPUT_IMPLEMENTATION -include src/dev/review_input.h -O1 -g $(WARN) $(SANFLAGS) $(UI_INPUT_LINK) $(GUIDE_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
tests/test_statsview_input: $(STATS_INPUT_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(UI_INPUT_LINK) $(STATS_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm
tests/test_statsview_input-debug: $(STATS_INPUT_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(UI_INPUT_LINK) $(STATS_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
tests/test_statsview_input_review: $(STATS_INPUT_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -DDUXCEL_DEV_REVIEW -DDUXCEL_REVIEW_INPUT_IMPLEMENTATION -include src/dev/review_input.h $(CFLAGS) $(WARN) $(UI_INPUT_LINK) $(STATS_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm
tests/test_statsview_input_review-debug: $(STATS_INPUT_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -DDUXCEL_DEV_REVIEW -DDUXCEL_REVIEW_INPUT_IMPLEMENTATION -include src/dev/review_input.h -O1 -g $(WARN) $(SANFLAGS) $(UI_INPUT_LINK) $(STATS_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
tests/test_ui_update_input: $(UI_UPDATE_INPUT_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(UI_INPUT_LINK) $(UI_UPDATE_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm
tests/test_ui_update_input-debug: $(UI_UPDATE_INPUT_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(UI_INPUT_LINK) $(UI_UPDATE_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
tests/test_ui_update_input_browser: $(UI_UPDATE_INPUT_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -DDUXCEL_BROWSER_HOST -U__APPLE__ $(CFLAGS) $(WARN) $(UI_INPUT_LINK) $(UI_UPDATE_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm
tests/test_ui_update_input_browser-debug: $(UI_UPDATE_INPUT_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -DDUXCEL_BROWSER_HOST -U__APPLE__ -O1 -g $(WARN) $(SANFLAGS) $(UI_INPUT_LINK) $(UI_UPDATE_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
tests/test_ui_update_input_control: $(UI_UPDATE_INPUT_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -U__APPLE__ $(CFLAGS) $(WARN) $(UI_INPUT_LINK) $(UI_UPDATE_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm
tests/test_ui_update_input_control-debug: $(UI_UPDATE_INPUT_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -U__APPLE__ -O1 -g $(WARN) $(SANFLAGS) $(UI_INPUT_LINK) $(UI_UPDATE_INPUT_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
MOTION_SRC = src/ui/motion.c tests/test_motion.c
tests/test_motion: $(MOTION_SRC) src/ui/motion.h
	$(CC) -Isrc $(CFLAGS) $(WARN) $(MOTION_SRC) -o $@ -lm
tests/test_motion-debug: $(MOTION_SRC) src/ui/motion.h
	$(CC) -Isrc -O1 -g $(WARN) $(SANFLAGS) $(MOTION_SRC) -o $@ -lm $(SANFLAGS)
tests/test_fontmetrics: $(FONTMETRICS_SRC) $(HEADERS)
	$(CC) -Isrc $(CFLAGS) $(WARN) $(FONTMETRICS_SRC) -o $@ -lm
tests/test_fontmetrics-debug: $(FONTMETRICS_SRC) $(HEADERS)
	$(CC) -Isrc -O1 -g $(WARN) $(SANFLAGS) $(FONTMETRICS_SRC) -o $@ -lm $(SANFLAGS)
tests/test_guideview: $(GUIDEVIEW_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(GUIDEVIEW_SRC) -o $@ $(RAYLIB_LIBS) -lm
tests/test_guideview-debug: $(GUIDEVIEW_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(GUIDEVIEW_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
tests/test_statsview: $(STATSVIEW_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(STATSVIEW_SRC) -o $@ $(RAYLIB_LIBS) -lm
tests/test_statsview-debug: $(STATSVIEW_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(STATSVIEW_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
tests/test_preview: $(PREVIEW_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(PREVIEW_SRC) -o $@ $(LDLIBS)
tests/test_preview-debug: $(PREVIEW_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(PREVIEW_SRC) -o $@ $(LDLIBS) $(SANFLAGS)
tests/test_inspector: $(INSPECTOR_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(INSPECTOR_SRC) -o $@ -lm
tests/test_inspector-debug: $(INSPECTOR_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(INSPECTOR_SRC) -o $@ -lm $(SANFLAGS)
tests/test_stepview: $(STEPVIEW_SRC) $(HEADERS)
	$(CC) -Isrc $(CFLAGS) $(WARN) $(STEPVIEW_SRC) -o $@ $(THREAD_LIBS) -lm
tests/test_stepview-debug: $(STEPVIEW_SRC) $(HEADERS)
	$(CC) -Isrc -O1 -g $(WARN) $(SANFLAGS) $(STEPVIEW_SRC) -o $@ $(THREAD_LIBS) -lm $(SANFLAGS)
tests/test_paintlayer: src/ui/paintlayer.c tests/test_paintlayer.c $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) src/ui/paintlayer.c tests/test_paintlayer.c -o $@ -lm
tests/test_paintlayer-debug: src/ui/paintlayer.c tests/test_paintlayer.c $(HEADERS)
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) src/ui/paintlayer.c tests/test_paintlayer.c -o $@ -lm $(SANFLAGS)
PAINT_NATIVE_SRC = src/common.c src/transforms/number.c src/workbook/format.c src/ui/motion.c src/ui/fontmetrics.c src/ui/paintlayer.c src/ui/paintlayer_backend.c src/ui/ui.c tests/test_paint_native.c
tests/test_paint-native-debug: $(PAINT_NATIVE_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(PAINT_NATIVE_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
paint-smoke: tests/test_paint-native-debug
	mkdir -p tests/results/native-authoring
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_paint-native-debug
test: tests/test_keytips tests/test_cellreadout tests/test_core tests/test_grid tests/test_workbook tests/test_brand tests/test_format tests/test_chartview tests/test_chartencoding tests/test_inspector tests/test_stepview tests/test_preview tests/test_statsview tests/test_guideview tests/test_fontmetrics tests/test_motion tests/test_choiceview icons-check tests/test_paintlayer tests/test_popup tests/test_marksview tests/test_preview_input tests/test_preview_input_browser tests/test_textview tests/test_textview_review tests/test_statsview_input tests/test_statsview_input_review tests/test_ui_update_input tests/test_ui_update_input_browser tests/test_ui_update_input_control tests/test_guideview_input tests/test_guideview_input_review
	mkdir -p tests/generated/workspace-old tests/generated/workspace-new
	./tests/test_filterlabel
	./tests/test_cellreadout
	./tests/test_core
	./tests/test_grid
	./tests/test_keytips
	./tests/test_workbook
	./tests/test_brand
	./tests/test_format
	./tests/test_chartview
	./tests/test_chartencoding
	./tests/test_inspector
	./tests/test_stepview
	./tests/test_preview
	./tests/test_statsview
	./tests/test_guideview
	./tests/test_fontmetrics
	./tests/test_motion
	./tests/test_choiceview
	./tests/test_paintlayer
	./tests/test_popup
	./tests/test_marksview
	./tests/test_preview_input
	./tests/test_preview_input_browser
	./tests/test_textview
	./tests/test_textview_review
	./tests/test_statsview_input
	./tests/test_statsview_input_review
	./tests/test_ui_update_input
	./tests/test_ui_update_input_browser
	./tests/test_ui_update_input_control
	./tests/test_guideview_input
	./tests/test_guideview_input_review
test-debug: tests/test_keytips-debug tests/test_cellreadout-debug tests/test_core-debug tests/test_grid-debug tests/test_workbook-debug tests/test_brand-debug tests/test_format-debug tests/test_chartview-debug tests/test_chartencoding-debug tests/test_inspector-debug tests/test_stepview-debug tests/test_preview-debug tests/test_statsview-debug tests/test_guideview-debug tests/test_fontmetrics-debug tests/test_motion-debug tests/test_choiceview-debug icons-check tests/test_paintlayer-debug tests/test_popup-debug tests/test_marksview-debug tests/test_preview_input-debug tests/test_preview_input_browser-debug tests/test_textview-debug tests/test_textview_review-debug tests/test_statsview_input-debug tests/test_statsview_input_review-debug tests/test_ui_update_input-debug tests/test_ui_update_input_browser-debug tests/test_ui_update_input_control-debug tests/test_guideview_input-debug tests/test_guideview_input_review-debug
	mkdir -p tests/generated/workspace-old tests/generated/workspace-new
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_filterlabel-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_cellreadout-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_core-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_grid-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_keytips-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_workbook-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_brand-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_format-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_chartview-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_chartencoding-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_inspector-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_stepview-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_preview-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_statsview-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_guideview-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_fontmetrics-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_motion-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_choiceview-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_paintlayer-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_popup-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_marksview-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_preview_input-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_preview_input_browser-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_textview-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_textview_review-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_statsview_input-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_statsview_input_review-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_ui_update_input-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_ui_update_input_browser-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_ui_update_input_control-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_guideview_input-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_guideview_input_review-debug
spill-test: tests/test_cellreadout tests/test_core tests/test_core-debug
	./tests/test_core --spill-probe
	TMPDIR=/ ASAN_OPTIONS=halt_on_error=1 ./tests/test_core-debug --spill-probe
unique-large-test: tests/test_cellreadout tests/test_core tests/test_core-debug
	test -f tests/downloads/yellow_tripdata_2025-01.parquet
	./tests/test_core --unique-large tests/downloads/yellow_tripdata_2025-01.parquet
	TMPDIR=/ ASAN_OPTIONS=halt_on_error=1 ./tests/test_core-debug --unique-large tests/downloads/yellow_tripdata_2025-01.parquet
smoke: duxcel-debug
	./duxcel-debug --smoke --frames 240 --screenshot /tmp/duxcel-smoke.png
workspace-smoke: duxcel-debug
	mkdir -p tests/generated
	ASAN_OPTIONS=halt_on_error=1 ./duxcel-debug --workspace-smoke --frames 240 --screenshot /tmp/duxcel-workspace-smoke.png
filter-smoke: duxcel-debug
	mkdir -p tests/results/filter-keyboard
	ASAN_OPTIONS=halt_on_error=1 ./duxcel-debug --filter-smoke --frames 6200 --screenshot /tmp/duxcel-filter-smoke.png
pivot-smoke: duxcel-debug
	ASAN_OPTIONS=halt_on_error=1 ./duxcel-debug --pivot-smoke --frames 1560 --screenshot /tmp/duxcel-pivot-smoke.png
export-smoke: duxcel-debug
	mkdir -p tests/generated
	ASAN_OPTIONS=halt_on_error=1 ./duxcel-debug --export-smoke --frames 840 --screenshot /tmp/duxcel-export-smoke.png
count-smoke: duxcel-debug
	mkdir -p tests/generated
	ASAN_OPTIONS=halt_on_error=1 ./duxcel-debug --count-smoke --frames 720 --screenshot /tmp/duxcel-count-smoke.png
clipboard-smoke: duxcel-debug
	mkdir -p tests/generated
	ASAN_OPTIONS=halt_on_error=1 ./duxcel-debug --clipboard-smoke --frames 720 --screenshot /tmp/duxcel-clipboard-smoke.png
sort-smoke: duxcel-debug
	mkdir -p tests/generated tests/results/multisort
	ASAN_OPTIONS=halt_on_error=1 ./duxcel-debug --sort-smoke --frames 720 --screenshot /tmp/duxcel-sort-smoke.png
layout-smoke: duxcel-debug design-fixture
	mkdir -p tests/generated tests/results/workspace-layout
	ASAN_OPTIONS=halt_on_error=1 ./duxcel-debug --layout-smoke --frames 20000 --screenshot /tmp/duxcel-layout-smoke.png
navigation-smoke: duxcel-debug
	mkdir -p tests/generated tests/results/sheet-navigation
	ASAN_OPTIONS=halt_on_error=1 ./duxcel-debug --navigation-smoke --frames 720 --screenshot /tmp/duxcel-navigation-smoke.png
startup-smoke: navigation-smoke
	ASAN_OPTIONS=halt_on_error=1 ./duxcel-debug tests/generated/navigation.dxls --frames 180
	cp tests/fixtures/null-text.csv tests/generated/invalid-workspace.dxls
	ASAN_OPTIONS=halt_on_error=1 ./duxcel-debug tests/generated/invalid-workspace.dxls --frames 60
nyc-smoke: duxcel-debug
	test -f tests/downloads/yellow_tripdata_2025-01.parquet
	mkdir -p tests/generated tests/results/current-workspace
	ASAN_OPTIONS=halt_on_error=1 ./duxcel-debug --nyc-smoke --frames 720 --screenshot tests/results/current-workspace/nyc-analysis.png
document-smoke: duxcel-debug
	mkdir -p tests/generated tests/results/current-workspace
	ASAN_OPTIONS=halt_on_error=1 ./duxcel-debug --document-smoke --frames 480 --screenshot tests/results/current-workspace/document-guard.png
chart-smoke: duxcel-debug
	mkdir -p tests/generated tests/results/chart-marks
	ASAN_OPTIONS=halt_on_error=1 ./duxcel-debug --chart-smoke --frames 1080 --screenshot tests/results/chart-marks/final.png
statistics-smoke: duxcel-debug
	mkdir -p tests/generated tests/results/selection-statistics
	ASAN_OPTIONS=halt_on_error=1 ./duxcel-debug --statistics-smoke --frames 2500 --screenshot tests/results/selection-statistics/final.png
table-smoke: duxcel-debug tests/test_core
	mkdir -p tests/generated tests/results/table-sources
	if test ! -f tests/generated/million.parquet || test ! -f "tests/generated/odd'name.parquet" || test ! -f tests/generated/fixture.duckdb; then ./tests/test_core; fi
	ASAN_OPTIONS=halt_on_error=1 ./duxcel-debug --table-smoke --frames 2520 --screenshot tests/results/table-sources/final.png
bundle: duxcel
	mkdir -p build/Duxcel.app/Contents/MacOS build/Duxcel.app/Contents/Resources
	cp duxcel build/Duxcel.app/Contents/MacOS/duxcel.new
	mv build/Duxcel.app/Contents/MacOS/duxcel.new build/Duxcel.app/Contents/MacOS/duxcel
	cp -R assets build/Duxcel.app/Contents/Resources/
	cp packaging/Info.plist build/Duxcel.app/Contents/
	cp assets/icons/duxcel.icns build/Duxcel.app/Contents/Resources/
ifeq ($(shell uname -s),Darwin)
	codesign --force --sign - build/Duxcel.app
endif
clean:
	rm -f tests/test_guideview_input tests/test_guideview_input-debug tests/test_guideview_input_review tests/test_guideview_input_review-debug
	rm -rf tests/test_guideview_input.dSYM tests/test_guideview_input-debug.dSYM tests/test_guideview_input_review.dSYM tests/test_guideview_input_review-debug.dSYM
	rm -f tests/test_statsview_input tests/test_statsview_input-debug tests/test_statsview_input_review tests/test_statsview_input_review-debug tests/test_ui_update_input tests/test_ui_update_input-debug tests/test_ui_update_input_browser tests/test_ui_update_input_browser-debug tests/test_ui_update_input_control tests/test_ui_update_input_control-debug
	rm -rf tests/test_statsview_input.dSYM tests/test_statsview_input-debug.dSYM tests/test_statsview_input_review.dSYM tests/test_statsview_input_review-debug.dSYM tests/test_ui_update_input.dSYM tests/test_ui_update_input-debug.dSYM tests/test_ui_update_input_browser.dSYM tests/test_ui_update_input_browser-debug.dSYM tests/test_ui_update_input_control.dSYM tests/test_ui_update_input_control-debug.dSYM
	rm -rf build/edit-sanitize
	rm -f tests/test_objects tests/test_objects-debug
	rm -rf tests/test_objects.dSYM tests/test_objects-debug.dSYM
	rm -rf build/obj
	rm -f $(ICON_STAMP) build/create-design-workbook
	rm -f duxcel.new duxcel-debug.new build/create-design-workbook.new
	rm -f tests/test_filterlabel tests/test_filterlabel-debug tests/test_background-native-debug
	rm -rf tests/test_filterlabel.dSYM tests/test_filterlabel-debug.dSYM tests/test_background-native-debug.dSYM
	rm -f tests/test_cellreadout tests/test_cellreadout-debug
	rm -f tests/test_marksview tests/test_marksview-debug tests/test_popup tests/test_popup-debug tests/test_paintlayer tests/test_paintlayer-debug tests/test_paint-native-debug
	rm -f tests/test_fontmetrics tests/test_fontmetrics-debug
	rm -rf tests/test_fontmetrics.dSYM tests/test_fontmetrics-debug.dSYM
	rm -f tests/test_guideview tests/test_guideview-debug
	rm -rf tests/test_guideview.dSYM tests/test_guideview-debug.dSYM
	rm -f tests/test_statsview tests/test_statsview-debug
	rm -rf tests/test_statsview.dSYM tests/test_statsview-debug.dSYM
	rm -f tests/test_preview tests/test_preview-debug
	rm -rf tests/test_preview.dSYM tests/test_preview-debug.dSYM
	rm -f tests/test_inspector tests/test_inspector-debug tests/test_stepview tests/test_stepview-debug
	rm -rf tests/test_inspector.dSYM tests/test_inspector-debug.dSYM tests/test_stepview.dSYM tests/test_stepview-debug.dSYM
	rm -f tests/test_preview_input tests/test_preview_input-debug tests/test_preview_input_browser tests/test_preview_input_browser-debug tests/test_textview tests/test_textview-debug tests/test_textview_review tests/test_textview_review-debug
	rm -f tests/test_choiceview tests/test_choiceview-debug tests/test_motion tests/test_motion-debug duxcel duxcel-debug tests/test_core tests/test_core-debug tests/test_grid tests/test_grid-debug tests/test_workbook tests/test_workbook-debug tests/test_brand tests/test_brand-debug tests/test_chartencoding tests/test_chartencoding-debug
	rm -rf duxcel.dSYM duxcel-debug.dSYM tests/test_core.dSYM tests/test_core-debug.dSYM tests/test_grid.dSYM tests/test_grid-debug.dSYM tests/test_workbook.dSYM tests/test_workbook-debug.dSYM tests/test_brand.dSYM tests/test_brand-debug.dSYM tests/test_chartencoding.dSYM tests/test_chartencoding-debug.dSYM
.PHONY: all run debug run-debug test test-debug spill-test unique-large-test smoke workspace-smoke filter-smoke pivot-smoke export-smoke count-smoke clipboard-smoke sort-smoke layout-smoke navigation-smoke startup-smoke nyc-smoke document-smoke chart-smoke statistics-smoke table-smoke icon-smoke query-smoke unique-smoke mac-icon-check bundle clean icons icons-check icons-verify paint-smoke
.NOTPARALLEL:

# Short edit loop, a named native slice, or the recorded full integration tier.
# Builds stay serialized by the existing rules; TEST_ARGS may set --jobs 2 for
# independent already-built headless checks. See python3 tools/test.py --list.
test-fast:
	python3 tools/test.py fast $(TEST_ARGS)
test-affected:
	python3 tools/test.py affected --feature $(FEATURE) $(TEST_ARGS)
test-native:
	python3 tools/test.py native --scenario $(SCENARIO) $(TEST_ARGS)

tests/test_background-native-debug: tests/test_background_native.m
	$(CC) $(RAYLIB_CFLAGS) -O1 -g $(WARN) $(SANFLAGS) $< -o $@ $(RAYLIB_LIBS) -framework AppKit $(SANFLAGS)
test-full:
	python3 tools/test.py full $(TEST_ARGS)
test-runner:
	python3 tools/test_runner_test.py
test-build:
	python3 tools/test_build.py
.PHONY: test-fast test-affected test-native test-full test-runner test-build


tests/test_popup: src/ui/motion.c src/ui/popup.c tests/test_popup.c $(HEADERS)
	$(CC) -Isrc $(CFLAGS) $(WARN) src/ui/motion.c src/ui/popup.c tests/test_popup.c -o $@ -lm

tests/test_popup-debug: src/ui/motion.c src/ui/popup.c tests/test_popup.c $(HEADERS)
	$(CC) -Isrc -O1 -g $(WARN) $(SANFLAGS) src/ui/motion.c src/ui/popup.c tests/test_popup.c -o $@ -lm $(SANFLAGS)


tests/test_marksview: src/ui/marksview.c tests/test_marksview.c $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) src/ui/marksview.c tests/test_marksview.c -o $@ -lm

tests/test_marksview-debug: src/ui/marksview.c tests/test_marksview.c $(HEADERS)
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) src/ui/marksview.c tests/test_marksview.c -o $@ -lm $(SANFLAGS)

CELLREADOUT_SRC = src/ui/cellreadout.c tests/test_cellreadout.c
tests/test_cellreadout: $(CELLREADOUT_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(CELLREADOUT_SRC) -o $@
tests/test_cellreadout-debug: $(CELLREADOUT_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(CELLREADOUT_SRC) -o $@ $(SANFLAGS)

FILTERLABEL_SRC = src/common.c src/transforms/number.c src/transforms/analysis.c src/ui/filterlabel.c tests/test_filterlabel.c
tests/test_filterlabel: $(FILTERLABEL_SRC) src/ui/filterlabel.h src/transforms/analysis.h src/transforms/number.h src/common.h
	$(CC) -Isrc $(CFLAGS) $(WARN) $(FILTERLABEL_SRC) -o $@ -lm
tests/test_filterlabel-debug: $(FILTERLABEL_SRC) src/ui/filterlabel.h src/transforms/analysis.h src/transforms/number.h src/common.h
	$(CC) -Isrc -O1 -g $(WARN) $(SANFLAGS) $(FILTERLABEL_SRC) -o $@ -lm $(SANFLAGS)
test: tests/test_filterlabel tests/test_statsview_input tests/test_statsview_input_review tests/test_ui_update_input tests/test_ui_update_input_browser tests/test_ui_update_input_control
test-debug: tests/test_filterlabel-debug tests/test_statsview_input-debug tests/test_statsview_input_review-debug tests/test_ui_update_input-debug tests/test_ui_update_input_browser-debug tests/test_ui_update_input_control-debug

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
	python3 tools/native_review_test.py
	python3 tools/test_review_build.py --self-test
test-review:
	python3 tools/test_native_review.py $(TEST_ARGS)
.PHONY: test-review-core test-review

# External development tooling only; neither application links this service.
review-serve:
	python3 tools/review_service.py serve --agent codex
test-review-queue:
	python3 tools/review_queue_test.py
	python3 tools/review_service_test.py
	node --test web/review_host.test.mjs web/review_queue.test.mjs
.PHONY: review-serve test-review-queue

# Browser proof uses the same C renderer/controllers and a separate backend.
# The compiler/dependencies live only in the ignored project-local web cache.
WEB_SOURCES = $(filter-out src/data/database.c,$(filter %.c,$(APP_SOURCES))) src/workbook/workbook_browser.c src/data/page_wire.c src/data/browser_query.c src/data/distinct_page.c
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
BROWSER_MODEL_TEST_SRC = $(filter-out src/data/database.c src/data/worker.c,$(CORE)) src/workbook/workbook_browser.c tests/test_workbook_model.c
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
build/web-tests/test-model: $(BROWSER_MODEL_TEST_SRC) $(HEADERS)
	mkdir -p $(@D)
	$(CC) -Isrc $(DUCKDB_CFLAGS) -DDUXCEL_BROWSER_HOST -O2 -g $(WARN) $(BROWSER_MODEL_TEST_SRC) -o $@ -lm
build/web-tests/test-model-sanitize: $(BROWSER_MODEL_TEST_SRC) $(HEADERS)
	mkdir -p $(@D)
	$(CC) -Isrc $(DUCKDB_CFLAGS) -DDUXCEL_BROWSER_HOST -O1 -g $(WARN) $(SANFLAGS) $(BROWSER_MODEL_TEST_SRC) -o $@ -lm $(SANFLAGS)
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
	node --test web/official_worker.test.mjs web/page_wire.test.mjs web/engine_host.test.mjs web/official_worker_probe.test.mjs
	python3 tools/package_web_vendor_test.py
.PHONY: test-browser-core

KEYTIPS_SRC = src/ui/keytips.c src/ui/ribbon_spec.c src/ui/ribbon.c tests/test_keytips.c
tests/test_keytips: $(KEYTIPS_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(KEYTIPS_SRC) -o $@ -lm
tests/test_keytips-debug: $(KEYTIPS_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(KEYTIPS_SRC) -o $@ -lm $(SANFLAGS)
