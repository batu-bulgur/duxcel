SELECT EXISTS(SELECT 1 FROM _duxcel_workspace.format_scopes WHERE definition IS NULL OR octet_length(definition)>=262144)
