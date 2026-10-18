SELECT EXISTS(SELECT sheet_id FROM _duxcel_workspace.source_versions GROUP BY sheet_id HAVING count(*)>1024)
