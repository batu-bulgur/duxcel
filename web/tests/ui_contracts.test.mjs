// Behavioral fingerprints were retained before retiring session metadata.
import test from 'node:test';
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {paneRailExpandedReference,inspectorScrollReference,inspectorSQLReference} from './support/ui_contracts.mjs';
const excluded=["file","sha256","original","original_sha256","original_state","manifest_sha256","sourceHashes","provenance","scope","capture","derivation"];
function semantic(value){if(Array.isArray(value))return value.map(semantic);if(value&&typeof value==='object')return Object.fromEntries(Object.entries(value).filter(([k])=>!excluded.includes(k)).sort(([a],[b])=>a.localeCompare(b)).map(([k,v])=>[k,semantic(v)]));return value;}
const hashes=["1d6724fbe19dba45c4ee0c5640ae6b0cc8cdc500a79fcb40734edfebc76863be","709ea98c31d46febc4992bd5a9327c5090f326aa969f61521b52b1136e566ad7","ebbbc59ebe28cb63930dd9625ce72e0487c96fc81d70036c10d8198d4924ae59"];
for(const [index,load] of [paneRailExpandedReference,inspectorScrollReference,inspectorSQLReference].entries())test('accepted UI contract '+load.name+' retains geometry/scroll/colors',async()=>{
 const value=semantic(await load());
 assert.equal(createHash('sha256').update(JSON.stringify(value)).digest('hex'),hashes[index]);
});
