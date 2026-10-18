SELECT EXISTS(SELECT sheet_id,position FROM _duxcel_workspace.format_records GROUP BY sheet_id,position HAVING count(*)>20000)
