common.o: src/common.c src/common.h
platform.o: src/platform/platform.c src/platform/platform.h \
  src/platform/../common.h
distinct.o: src/transforms/distinct.c src/transforms/distinct.h \
  src/transforms/pivot.h src/transforms/analysis.h \
  src/transforms/../common.h src/transforms/number.h
analysis.o: src/transforms/analysis.c src/transforms/analysis.h \
  src/transforms/../common.h src/transforms/number.h
record_sql.o: src/transforms/record_sql.c src/transforms/record_sql.h \
  src/transforms/analysis.h src/transforms/../common.h \
  src/transforms/number.h
number.o: src/transforms/number.c src/transforms/number.h \
  src/transforms/../common.h
pivot.o: src/transforms/pivot.c src/transforms/pivot.h \
  src/transforms/analysis.h src/transforms/../common.h \
  src/transforms/number.h
chart_other.o: src/transforms/chart_other.c src/transforms/chart_other.h \
  src/transforms/../data/page.h /opt/homebrew/include/duckdb.h \
  src/transforms/../data/source_digest.h \
  src/transforms/../data/../transforms/analysis.h \
  src/transforms/../data/../transforms/../common.h \
  src/transforms/../data/../transforms/number.h \
  src/transforms/../data/../transforms/pivot.h
marks.o: src/transforms/marks.c src/transforms/marks.h \
  src/transforms/pivot.h src/transforms/analysis.h \
  src/transforms/../common.h src/transforms/number.h
database.o: src/data/database.c src/data/database.h src/data/page.h \
  /opt/homebrew/include/duckdb.h src/data/source_digest.h \
  src/data/../transforms/analysis.h src/data/../transforms/../common.h \
  src/data/../transforms/number.h src/data/../transforms/pivot.h \
  src/data/../transforms/chart_other.h \
  src/data/../transforms/../data/page.h \
  src/data/../transforms/distinct.h src/data/../transforms/record_sql.h \
  src/data/../platform/platform.h
page.o: src/data/page.c src/data/page.h /opt/homebrew/include/duckdb.h \
  src/data/source_digest.h src/data/../transforms/analysis.h \
  src/data/../transforms/../common.h src/data/../transforms/number.h \
  src/data/../transforms/pivot.h
source_digest.o: src/data/source_digest.c src/data/source_digest.h
clipboard.o: src/workbook/clipboard.c src/workbook/clipboard.h \
  src/workbook/../common.h src/workbook/cells.h src/workbook/objects.h \
  src/workbook/../transforms/pivot.h \
  src/workbook/../transforms/analysis.h \
  src/workbook/../transforms/../common.h \
  src/workbook/../transforms/number.h src/workbook/../transforms/marks.h
cells.o: src/workbook/cells.c src/workbook/cells.h \
  src/workbook/../common.h
format.o: src/workbook/format.c src/workbook/format.h \
  src/workbook/../common.h
source_scope.o: src/workbook/source_scope.c src/workbook/source_scope.h \
  src/workbook/../data/page.h /opt/homebrew/include/duckdb.h \
  src/workbook/../data/source_digest.h \
  src/workbook/../data/../transforms/analysis.h \
  src/workbook/../data/../transforms/../common.h \
  src/workbook/../data/../transforms/number.h \
  src/workbook/../data/../transforms/pivot.h
objects.o: src/workbook/objects.c src/workbook/objects.h \
  src/workbook/../transforms/pivot.h \
  src/workbook/../transforms/analysis.h \
  src/workbook/../transforms/../common.h \
  src/workbook/../transforms/number.h src/workbook/../transforms/marks.h \
  src/workbook/cells.h src/workbook/../common.h
workbook.o: src/workbook/workbook.c src/workbook/workbook.h \
  src/workbook/../transforms/analysis.h \
  src/workbook/../transforms/../common.h \
  src/workbook/../transforms/number.h src/workbook/cells.h \
  src/workbook/../common.h src/workbook/objects.h \
  src/workbook/../transforms/pivot.h src/workbook/../transforms/marks.h \
  src/workbook/format.h src/workbook/source_scope.h \
  src/workbook/../data/page.h /opt/homebrew/include/duckdb.h \
  src/workbook/../data/source_digest.h \
  src/workbook/../data/../transforms/analysis.h \
  src/workbook/../data/../transforms/pivot.h \
  src/workbook/workbook_codec.h src/workbook/../platform/platform.h
workbook_codec.o: src/workbook/workbook_codec.c \
  src/workbook/workbook_codec.h src/workbook/workbook.h \
  src/workbook/../transforms/analysis.h \
  src/workbook/../transforms/../common.h \
  src/workbook/../transforms/number.h src/workbook/cells.h \
  src/workbook/../common.h src/workbook/objects.h \
  src/workbook/../transforms/pivot.h src/workbook/../transforms/marks.h \
  src/workbook/format.h src/workbook/source_scope.h \
  src/workbook/../data/page.h /opt/homebrew/include/duckdb.h \
  src/workbook/../data/source_digest.h \
  src/workbook/../data/../transforms/analysis.h \
  src/workbook/../data/../transforms/pivot.h
worker.o: src/data/worker.c src/data/worker.h src/data/database.h \
  src/data/page.h /opt/homebrew/include/duckdb.h \
  src/data/source_digest.h src/data/../transforms/analysis.h \
  src/data/../transforms/../common.h src/data/../transforms/number.h \
  src/data/../transforms/pivot.h src/data/../platform/platform.h \
  src/data/../workbook/workbook.h \
  src/data/../workbook/../transforms/analysis.h \
  src/data/../workbook/cells.h src/data/../workbook/../common.h \
  src/data/../workbook/objects.h \
  src/data/../workbook/../transforms/pivot.h \
  src/data/../workbook/../transforms/marks.h \
  src/data/../workbook/format.h src/data/../workbook/source_scope.h \
  src/data/../workbook/../data/page.h \
  src/data/../workbook/../data/source_digest.h \
  src/data/../workbook/clipboard.h
workbook_codec_read.o: src/workbook/workbook_codec_read.c \
  src/workbook/workbook_codec_read.h src/workbook/workbook_codec.h \
  src/workbook/workbook.h src/workbook/../transforms/analysis.h \
  src/workbook/../transforms/../common.h \
  src/workbook/../transforms/number.h src/workbook/cells.h \
  src/workbook/../common.h src/workbook/objects.h \
  src/workbook/../transforms/pivot.h src/workbook/../transforms/marks.h \
  src/workbook/format.h src/workbook/source_scope.h \
  src/workbook/../data/page.h /opt/homebrew/include/duckdb.h \
  src/workbook/../data/source_digest.h \
  src/workbook/../data/../transforms/analysis.h \
  src/workbook/../data/../transforms/pivot.h
test_workbook_codec_native.o: tests/test_workbook_codec_native.c \
  src/workbook/workbook_codec_read.h src/workbook/workbook_codec.h \
  src/workbook/workbook.h src/workbook/../transforms/analysis.h \
  src/workbook/../transforms/../common.h \
  src/workbook/../transforms/number.h src/workbook/cells.h \
  src/workbook/../common.h src/workbook/objects.h \
  src/workbook/../transforms/pivot.h src/workbook/../transforms/marks.h \
  src/workbook/format.h src/workbook/source_scope.h \
  src/workbook/../data/page.h /opt/homebrew/include/duckdb.h \
  src/workbook/../data/source_digest.h \
  src/workbook/../data/../transforms/analysis.h \
  src/workbook/../data/../transforms/pivot.h src/platform/platform.h \
  tests/test_workbook_codec_objects_native.inc \
  tests/test_workbook_codec_queries_native.inc \
  tests/test_workbook_codec_options_native.inc \
  tests/test_workbook_codec_tables_native.inc \
  tests/test_workbook_codec_pivot_marks_native.inc
