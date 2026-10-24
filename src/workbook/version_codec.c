#include "version_codec.h"
static const WorkbookVersionCodecTable tables[]={
    {"meta","codec,document_high,document_low,generation,saved_generation,saved_present,saved_key","IUUUUBX",
        "codec INTEGER,document_high UBIGINT,document_low UBIGINT,generation UBIGINT,saved_generation UBIGINT,saved_present BOOLEAN,saved_key BLOB"},
    {"roots","position,key,bytes,encoding","IXUI","position INTEGER,key BLOB,bytes UBIGINT,encoding INTEGER,PRIMARY KEY(position),UNIQUE(key)"},
    {"tags","position,label,root,created","ITXU","position INTEGER,label VARCHAR,root BLOB,created UBIGINT,PRIMARY KEY(position),UNIQUE(label)"},
    {"chunks","root,position,data","XUX","root BLOB,position UBIGINT,data BLOB,PRIMARY KEY(root,position)"}
};
const WorkbookVersionCodecTable *workbook_version_codec_table(size_t position) { return position<DX_VERSION_CODEC_TABLES ? tables+position : NULL; }
