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
CORE = src/common.c src/platform/platform.c src/transforms/analysis.c src/transforms/number.c src/transforms/pivot.c src/transforms/marks.c src/data/database.c src/workbook/clipboard.c src/workbook/cells.c src/workbook/format.c src/workbook/objects.c src/workbook/workbook.c src/data/worker.c
SRC = $(CORE) src/ui/cellreadout.c src/ui/choiceview.c src/ui/popup.c src/ui/motion.c src/ui/fontmetrics.c src/ui/paintlayer.c src/ui/paintlayer_backend.c src/ui/ui.c src/ui/ribbon.c src/ui/ribbon_spec.c src/ui/flyout.c src/ui/statusview.c src/ui/dockview.c src/ui/marksview.c src/ui/workspace.c src/ui/inspector.c src/ui/statsview.c src/ui/guideview.c src/ui/stepview.c src/ui/preview.c src/ui/chartview.c src/ui/chartencoding.c src/ui/textview.c src/ui/sortview.c src/ui/brand.c src/platform/icon.c src/grid/grid.c src/app/step_preview.c src/app/app.c src/main.c
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
$(ICON_OUTPUTS): $(ICON_INPUTS)
	python3 tools/generate_icons.py
icons:
	python3 tools/generate_icons.py
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
build/create-design-workbook: $(CORE) tools/create_design_workbook.c $(HEADERS)
	mkdir -p build
	$(CC) -Isrc $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN) $(CORE) tools/create_design_workbook.c -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm
design-fixture: build/create-design-workbook
	node tools/export_design_fixture.mjs
	./build/create-design-workbook
design-large-fixture: build/create-design-workbook
	node tools/export_design_fixture.mjs --large
	./build/create-design-workbook --large
design-smoke: duxcel-debug design-fixture
	mkdir -p tests/results/design-definition
	ASAN_OPTIONS=halt_on_error=1 ./duxcel-debug tests/generated/design-definition.dxls --frames 240 --screenshot tests/results/design-definition/authoring.png
duxcel: $(SRC) $(HEADERS) $(ICON_OUTPUTS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(SRC) -o $@ $(LDFLAGS) $(LDLIBS)
duxcel-debug: $(SRC) $(HEADERS) $(ICON_OUTPUTS)
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(SRC) -o $@ $(LDFLAGS) $(LDLIBS) $(SANFLAGS)
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
tests/test_grid: src/common.c src/workbook/format.c src/transforms/number.c src/ui/motion.c src/ui/fontmetrics.c src/ui/paintlayer.c src/ui/paintlayer_backend.c src/ui/ui.c src/ui/ribbon.c src/ui/ribbon_spec.c src/ui/flyout.c src/ui/statusview.c src/ui/dockview.c src/ui/workspace.c src/ui/chartview.c src/ui/textview.c src/grid/grid.c tests/test_grid.c $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) src/common.c src/workbook/format.c src/transforms/number.c src/ui/motion.c src/ui/fontmetrics.c src/ui/paintlayer.c src/ui/paintlayer_backend.c src/ui/ui.c src/ui/ribbon.c src/ui/ribbon_spec.c src/ui/flyout.c src/ui/statusview.c src/ui/dockview.c src/ui/workspace.c src/ui/chartview.c src/ui/textview.c src/grid/grid.c tests/test_grid.c -o $@ $(RAYLIB_LIBS) -lm
tests/test_grid-debug: src/common.c src/workbook/format.c src/transforms/number.c src/ui/motion.c src/ui/fontmetrics.c src/ui/paintlayer.c src/ui/paintlayer_backend.c src/ui/ui.c src/ui/ribbon.c src/ui/ribbon_spec.c src/ui/flyout.c src/ui/statusview.c src/ui/dockview.c src/ui/workspace.c src/ui/chartview.c src/ui/textview.c src/grid/grid.c tests/test_grid.c $(HEADERS)
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) src/common.c src/workbook/format.c src/transforms/number.c src/ui/motion.c src/ui/fontmetrics.c src/ui/paintlayer.c src/ui/paintlayer_backend.c src/ui/ui.c src/ui/ribbon.c src/ui/ribbon_spec.c src/ui/flyout.c src/ui/statusview.c src/ui/dockview.c src/ui/workspace.c src/ui/chartview.c src/ui/textview.c src/grid/grid.c tests/test_grid.c -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
tests/test_workbook: $(CORE) tests/test_workbook.c $(HEADERS)
	$(CC) -Isrc $(DUCKDB_CFLAGS) $(CFLAGS) $(WARN) $(CORE) tests/test_workbook.c -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm
tests/test_workbook-debug: $(CORE) tests/test_workbook.c $(HEADERS)
	$(CC) -Isrc $(DUCKDB_CFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(CORE) tests/test_workbook.c -o $@ $(DUCKDB_LIBS) $(THREAD_LIBS) -lm $(SANFLAGS)
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
CHOICEVIEW_SRC = src/common.c src/transforms/number.c src/workbook/format.c src/ui/motion.c src/ui/fontmetrics.c src/ui/paintlayer.c src/ui/paintlayer_backend.c src/ui/ui.c src/ui/choiceview.c tests/test_choiceview.c
tests/test_choiceview: $(CHOICEVIEW_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(CHOICEVIEW_SRC) -o $@ $(RAYLIB_LIBS) -lm
tests/test_choiceview-debug: $(CHOICEVIEW_SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(SANFLAGS) $(CHOICEVIEW_SRC) -o $@ $(RAYLIB_LIBS) -lm $(SANFLAGS)
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
test: tests/test_cellreadout tests/test_core tests/test_grid tests/test_workbook tests/test_brand tests/test_format tests/test_chartview tests/test_chartencoding tests/test_inspector tests/test_stepview tests/test_preview tests/test_statsview tests/test_guideview tests/test_fontmetrics tests/test_motion tests/test_choiceview icons-check tests/test_paintlayer tests/test_popup tests/test_marksview
	mkdir -p tests/generated/workspace-old tests/generated/workspace-new
	./tests/test_cellreadout
	./tests/test_core
	./tests/test_grid
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
test-debug: tests/test_cellreadout-debug tests/test_core-debug tests/test_grid-debug tests/test_workbook-debug tests/test_brand-debug tests/test_format-debug tests/test_chartview-debug tests/test_chartencoding-debug tests/test_inspector-debug tests/test_stepview-debug tests/test_preview-debug tests/test_statsview-debug tests/test_guideview-debug tests/test_fontmetrics-debug tests/test_motion-debug tests/test_choiceview-debug icons-check tests/test_paintlayer-debug tests/test_popup-debug tests/test_marksview-debug
	mkdir -p tests/generated/workspace-old tests/generated/workspace-new
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_cellreadout-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_core-debug
	ASAN_OPTIONS=halt_on_error=1 ./tests/test_grid-debug
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
	ASAN_OPTIONS=halt_on_error=1 ./duxcel-debug --layout-smoke --frames 18000 --screenshot /tmp/duxcel-layout-smoke.png
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
test-full:
	python3 tools/test.py full $(TEST_ARGS)
test-runner:
	python3 tools/test_runner_test.py
.PHONY: test-fast test-affected test-full test-runner


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
