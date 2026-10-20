// Memory-only URL/DOM/timer injection: no browser, filesystem or OS download.
import test from 'node:test';
import assert from 'node:assert/strict';
import {createBrowserDownload} from '../download.mjs';
const encoder=new TextEncoder(),decoder=new TextDecoder('utf-8',{fatal:true});
function parquet(size=20){
    const bytes=new Uint8Array(size);bytes.set([80,65,82,49]);bytes.fill(23,4,size-8);bytes[size-8]=size-12;
    bytes.set([80,65,82,49],size-4);return bytes;
}
function fixture(overrides={}){
    const blobs=[],created=[],revoked=[],dispatched=[],cancelled=[],timers=new Map();let next=0;
    const functions={
        createBlob(bytes){const blob=new Blob([bytes],{type:'application/vnd.apache.parquet'});blobs.push(blob);return blob;},
        createURL(blob){assert.ok(blob instanceof Blob);const url=`memory:download-${created.length+1}`;created.push(url);return url;},
        revokeURL(url){revoked.push(url);},dispatch(url,name){dispatched.push({url,name});},
        schedule(callback,delay){const id=next++;timers.set(id,{callback,delay});return id;},
        cancel(id){cancelled.push(id);timers.delete(id);},...overrides
    };
    const owner=createBrowserDownload(functions);
    return {owner,functions,blobs,created,revoked,dispatched,cancelled,timers,fire(id=timers.keys().next().value){const entry=timers.get(id);assert.ok(entry);timers.delete(id);entry.callback();}};
}
function failed(result){assert.equal(result.ok,false);assert.equal(typeof result.error,'string');assert.ok(result.error.length);assert.ok(Object.isFrozen(result));assert.ok(encoder.encode(result.error).length<=512);}

test('real Blob snapshots the exact borrowed subview before heap mutation or detach, without an extra byte clone',async()=>{
    const f=fixture(),storage=new Uint8Array(64),bytes=storage.subarray(11,31),expected=parquet();bytes.set(expected);
    let seen;const original=f.functions.createBlob;
    const owner=createBrowserDownload({...f.functions,createBlob(view){seen=view;return original(view);}});
    const name='O\'Reilly "report" İstanbul 🦆.parquet',result=owner.request(bytes,name);
    assert.deepEqual(result,{ok:true,error:null});assert.ok(Object.isFrozen(result));assert.strictEqual(seen,bytes);
    assert.deepEqual(f.dispatched,[{url:'memory:download-1',name}]);assert.equal(f.blobs[0].type,'application/vnd.apache.parquet');
    bytes.fill(0);structuredClone(storage.buffer,{transfer:[storage.buffer]});
    assert.deepEqual(new Uint8Array(await f.blobs[0].arrayBuffer()),expected);
    assert.equal(f.timers.size,1);assert.equal([...f.timers.values()][0].delay,2000);
    owner.dispose();assert.deepEqual(f.cancelled,[0]);assert.deepEqual(f.revoked,['memory:download-1']);assert.ok(Object.isFrozen(owner));
});
test('one retained URL stays busy until its grace timer releases it; retries and stale timers keep ownership separate',()=>{
    const f=fixture();assert.equal(f.owner.ready(),true);assert.equal(f.owner.ready(),true);assert.equal(f.owner.request(parquet(),'one.parquet').ok,true);assert.equal(f.owner.ready(),false);
    const first=[...f.timers.values()][0].callback;failed(f.owner.request(parquet(),'two.parquet'));
    assert.equal(f.created.length,1);f.fire(0);assert.equal(f.owner.ready(),true);assert.deepEqual(f.revoked,['memory:download-1']);assert.deepEqual(f.cancelled,[]);
    assert.equal(f.owner.request(parquet(),'two.parquet').ok,true);assert.equal(f.owner.ready(),false);first();assert.equal(f.owner.ready(),false);assert.equal(f.revoked.length,1);assert.equal(f.timers.size,1);
    f.owner.dispose();f.owner.dispose();assert.equal(f.owner.ready(),false);assert.deepEqual(f.cancelled,[1]);assert.deepEqual(f.revoked,['memory:download-1','memory:download-2']);
    first();failed(f.owner.request(parquet(),'disposed.parquet'));assert.equal(f.created.length,2);
});
test('Parquet bytes enforce exact bounds, bounded footer and ordinary attached nonshared Uint8Array views',()=>{
    const f=fixture(),invalid=[undefined,null,{},[],new ArrayBuffer(20),new DataView(new ArrayBuffer(20)),new Uint16Array(20),parquet(12),parquet().subarray(0,11)];
    for(const offset of [0,1,2,3,16,17,18,19]){const bad=parquet();bad[offset]=0;invalid.push(bad);}
    const zero=parquet();zero[12]=0;invalid.push(zero);const overflow=parquet();overflow.fill(255,12,16);invalid.push(overflow);
    const detached=parquet();structuredClone(detached.buffer,{transfer:[detached.buffer]});invalid.push(detached);
    if(typeof SharedArrayBuffer!=='undefined'){const shared=new Uint8Array(new SharedArrayBuffer(20));shared.set(parquet());invalid.push(shared);}
    invalid.push(new Proxy(parquet(),{}));
    for(const value of invalid){failed(f.owner.request(value,'invalid.parquet'));assert.equal(f.owner.ready(),true);}
    assert.equal(f.created.length,0);assert.equal(f.blobs.length,0);
    const boundary=parquet(64*1024*1024);new DataView(boundary.buffer).setUint32(boundary.length-8,1,true);
    let length=0;const bounded=fixture({createBlob(view){length=view.byteLength;return new Blob([]);}});
    assert.equal(bounded.owner.request(boundary,'maximum.parquet').ok,true);assert.equal(length,64*1024*1024);bounded.owner.dispose();
    failed(bounded.owner.request(parquet(),'disposed.parquet'));
    const oversized=new Uint8Array(64*1024*1024+1);failed(f.owner.request(oversized,'large.parquet'));assert.equal(f.created.length,0);
});
test('suggested filenames require valid UTF8 scalars and exactly bounded safe Parquet basenames',()=>{
    const f=fixture(),valid=['a.parquet','.parquet','Report.PARQUET','a'.repeat(247)+'.parquet','é'.repeat(123)+'a.parquet','🦆'.repeat(61)+'abc.parquet'];
    for(const name of valid){assert.equal(encoder.encode(name).length<=255,true);assert.equal(f.owner.request(parquet(),name).ok,true);f.fire();}
    for(const name of [undefined,null,1,{},new String('a.parquet'),'','a','a.csv','a.parquet.more','/a.parquet','a/b.parquet','a\\b.parquet','a\0.parquet','a\n.parquet','a\r.parquet','a\t.parquet','a\x7f.parquet','a\ud800.parquet','a\udc00.parquet','a'.repeat(248)+'.parquet','é'.repeat(124)+'.parquet','🦆'.repeat(62)+'.parquet'])failed(f.owner.request(parquet(),name));
    assert.equal(f.created.length,valid.length);f.owner.dispose();
});
test('unavailable or throwing factories, URL creation, dispatch and timers fail honestly with exact cleanup and recovery',()=>{
    for(const name of ['createBlob','createURL','revokeURL','dispatch','schedule','cancel']){
        const f=fixture({[name]:null});failed(f.owner.request(parquet(),'unavailable.parquet'));assert.equal(f.created.length,0);assert.equal(f.blobs.length,0);f.owner.dispose();
    }
    for(const failure of ['createBlob','createURL','dispatch','schedule']){
        let failing=true;const errors=[],f=fixture();const original=f.functions[failure];
        const owner=createBrowserDownload({...f.functions,[failure](...args){if(failing){errors.push(failure);throw new Error(`${failure} rejected`);}return original(...args);}});
        assert.equal(owner.ready(),true);const result=owner.request(parquet(),'first.parquet');failed(result);assert.equal(owner.ready(),true);assert.equal(result.error,`${failure} rejected`);
        assert.equal(f.timers.size,0);assert.equal(f.revoked.length,failure==='dispatch'||failure==='schedule'?1:0);
        failing=false;assert.equal(owner.request(parquet(),'retry.parquet').ok,true);owner.dispose();assert.deepEqual(errors,[failure]);
    }
    for(const bad of [undefined,null,0,{},'']){
        const f=fixture({createURL(){return bad;}});failed(f.owner.request(parquet(),'bad.parquet'));assert.equal(f.dispatched.length,0);assert.equal(f.timers.size,0);f.owner.dispose();
    }
});
test('cleanup exceptions retain failed URL ownership, block a second URL and permit explicit cleanup retry',()=>{
    let attempts=0;const f=fixture({revokeURL(url){assert.equal(url,'memory:download-1');if(++attempts<3)throw new Error('Temporary cleanup failure');}});
    assert.equal(f.owner.ready(),true);assert.equal(f.owner.request(parquet(),'one.parquet').ok,true);assert.equal(f.owner.ready(),false);f.fire();assert.equal(f.owner.ready(),false);assert.equal(attempts,1);
    failed(f.owner.request(parquet(),'must-stay-busy.parquet'));assert.equal(f.created.length,1);
    f.owner.dispose();assert.equal(f.owner.ready(),false);assert.equal(attempts,2);f.owner.dispose();assert.equal(f.owner.ready(),false);assert.equal(attempts,3);f.owner.dispose();assert.equal(attempts,3);
    let cancelled=0;const c=fixture({cancel(){cancelled++;throw new Error('cancel failed');}});
    assert.equal(c.owner.request(parquet(),'cancel.parquet').ok,true);const late=[...c.timers.values()][0].callback;
    c.owner.dispose();assert.equal(cancelled,1);assert.deepEqual(c.revoked,['memory:download-1']);late();assert.equal(c.revoked.length,1);
    const b=fixture({dispatch(){throw new Error('Dispatch failed');},revokeURL(){throw new Error('Revoke failed');}});
    failed(b.owner.request(parquet(),'failed.parquet'));failed(b.owner.request(parquet(),'busy.parquet'));assert.equal(b.created.length,1);b.owner.dispose();
});
test('reentrant platform functions cannot replace the reserved operation, publish after disposal or retain stale timers',()=>{
    for(const step of ['createBlob','createURL','dispatch','schedule']){
        const f=fixture(),original=f.functions[step];let owner,nested=0;
        owner=createBrowserDownload({...f.functions,[step](...args){nested++;failed(owner.request(parquet(),'nested.parquet'));return original(...args);}});
        assert.equal(owner.request(parquet(),'outer.parquet').ok,true);assert.equal(nested,1);assert.equal(f.created.length,1);owner.dispose();
    }
    for(const step of ['createBlob','createURL','dispatch','schedule']){
        const f=fixture(),original=f.functions[step];let owner,late;
        owner=createBrowserDownload({...f.functions,[step](...args){const value=original(...args);if(step==='schedule')late=args[0];owner.dispose();return value;}});
        failed(owner.request(parquet(),'disposed.parquet'));assert.equal(f.revoked.length,step==='createBlob'?0:1);assert.equal(f.timers.size,0);late?.();assert.equal(f.revoked.length,step==='createBlob'?0:1);
    }
    const sync=fixture({schedule(callback){callback();return 71;}});failed(sync.owner.request(parquet(),'synchronous.parquet'));assert.deepEqual(sync.revoked,['memory:download-1']);assert.deepEqual(sync.cancelled,[71]);sync.owner.dispose();
    let owner;const reentrant=fixture({revokeURL(){owner.dispose();}});owner=reentrant.owner;
    assert.equal(owner.request(parquet(),'revoke.parquet').ok,true);owner.dispose();
});
test('hostile diagnostics are bounded scalar UTF8 and configuration never invokes defaults',()=>{
    const errors=['🦆'.repeat(1000),'\ud800\0'+ 'é'.repeat(1000),{get message(){throw new Error('Unsafe getter');}},{message:'',name:'Denied'},Symbol('denied'),null];
    for(const error of errors){const f=fixture({dispatch(){throw error;}}),result=f.owner.request(parquet(),'errors.parquet');failed(result);assert.equal(decoder.decode(encoder.encode(result.error)),result.error);assert.equal(result.error.includes('\0'),false);assert.equal(f.revoked.length,1);f.owner.dispose();}
    for(const name of ['createBlob','createURL','revokeURL','dispatch','schedule','cancel'])for(const value of [0,false,{},'function'])assert.throws(()=>createBrowserDownload({[name]:value}),TypeError);
    const inert=createBrowserDownload();assert.ok(Object.isFrozen(inert));assert.equal(inert.ready(),true);inert.dispose();assert.equal(inert.ready(),false);
});
