function report_options(ok,checks,fixtures,headers,requests,rejections,stale,peak,normal,meta,sheets,objects,source_options,definitions,describe,object256,option256,option64,query256,query64,object_groups,option_groups,query_groups,rejected,schema_rejections,proofs,pod_poisoned,query_poisoned,first_rows,error){
    const at=normal>>>2,rows=first_rows>>>2;
    Module['dxCodecReadTestDone']({ok:Boolean(ok),mode:'source-options',checks,fixtures,headers,queries:requests,rejections,stale,
        peakOwnedInput:peak,headerOwnedLimit:2097152,inputOwnedLimit:4194304,
        normalFixtureQueries:Array.from(HEAPU32.subarray(at,at+4)),familyExecutions:{meta,sheets,objects,sourceOptions:source_options,queries:definitions,describe},
        acceptedFeeds:{objects256:object256,sourceOptions256:option256,sourceOptions64:option64,queries256:query256,queries64:query64},
        objectSplitGroups:object_groups,sourceOptionsSplitGroups:option_groups,validSplitGroups:query_groups,
        rejectedFeeds:rejected,schemaRejections:schema_rejections,rejectedFirstRows:Array.from(HEAPU32.subarray(rows,rows+6)),
        podPoisonedResults:pod_poisoned,poisonedQueryResults:query_poisoned,
        requiredNullRejected:Boolean(proofs&1),duplicateFalseRejected:Boolean(proofs&2),wrongKindRejected:Boolean(proofs&4),foreignOwnerRejected:Boolean(proofs&8),
        sentinelRejected:Boolean(proofs&16),budgetRejected:Boolean(proofs&32),originalTypeRejected:Boolean(proofs&64),
        error:error?UTF8ToString(error):null,boundary:'official-worker/shared-C-meta-sheets-object-source-options-query Header; owned POD/raw definitions only, no Table Analysis, source execution, Workbook publication or browser Save/Open'});
}
