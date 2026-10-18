common.o: src/common.c src/common.h
analysis.o: src/transforms/analysis.c src/transforms/analysis.h \
  src/transforms/../common.h src/transforms/number.h
number.o: src/transforms/number.c src/transforms/number.h \
  src/transforms/../common.h
pivot.o: src/transforms/pivot.c src/transforms/pivot.h \
  src/transforms/analysis.h src/transforms/../common.h \
  src/transforms/number.h
marks.o: src/transforms/marks.c src/transforms/marks.h \
  src/transforms/pivot.h src/transforms/analysis.h \
  src/transforms/../common.h src/transforms/number.h
source_digest.o: src/data/source_digest.c src/data/source_digest.h
source_scope.o: src/workbook/source_scope.c src/workbook/source_scope.h \
  src/workbook/../data/page.h /opt/homebrew/include/duckdb.h \
  src/workbook/../data/source_digest.h \
  src/workbook/../data/../transforms/analysis.h \
  src/workbook/../data/../transforms/../common.h \
  src/workbook/../data/../transforms/number.h \
  src/workbook/../data/../transforms/pivot.h
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
test_workbook_codec_read.o: tests/test_workbook_codec_read.c \
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
