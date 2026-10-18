SELECT EXISTS(SELECT sheet_id FROM _duxcel_workspace.format_scopes GROUP BY sheet_id HAVING count(*)>512)
