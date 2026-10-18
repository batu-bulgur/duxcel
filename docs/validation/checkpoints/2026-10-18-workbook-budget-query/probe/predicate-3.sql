SELECT EXISTS(SELECT 1 FROM _duxcel_workspace.table_sources WHERE path IS NULL OR octet_length(encode(path))>65536 OR contains(path,chr(0)))
