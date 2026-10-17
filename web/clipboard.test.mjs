// Pure memory writers only: never call navigator or any OS clipboard API.
import test from 'node:test';
import assert from 'node:assert/strict';
import {createBrowserClipboard} from './clipboard.mjs';
const encoder=new TextEncoder(),decoder=new TextDecoder('utf-8',{fatal:true}),flush=()=>new Promise(resolve=>setImmediate(resolve));
function deferred(){let resolve,reject;const promise=new Promise((yes,no)=>{resolve=yes;reject=no;});return {promise,resolve,reject};}

test('one owned pending write and one terminal outcome preserve exact text without early success',async()=>{
    const pending=deferred(),writes=[];
    const clipboard=createBrowserClipboard({writeText(text){writes.push(text);return pending.promise;}});
    const text='O\'Reilly\t"quoted"\nNULL\r\n\u0000İstanbul 🦆';
    const id=clipboard.begin(text);assert.ok(Number.isInteger(id)&&id>0&&id<=0x7fffffff);
    assert.deepEqual(writes,[text]);assert.equal(clipboard.take(),null);
    assert.equal(clipboard.begin('must not replace pending text'),0);
    pending.resolve();await flush();assert.equal(clipboard.begin('must not replace unread outcome'),0);
    const result=clipboard.take();assert.deepEqual(result,{id,ok:true,error:null});assert.ok(Object.isFrozen(result));
    assert.equal(clipboard.take(),null);
    const next=clipboard.begin('');assert.equal(next,id+1);await flush();assert.deepEqual(writes,[text,'']);
    assert.deepEqual(clipboard.take(),{id:next,ok:true,error:null});clipboard.dispose();
});
test('exact UTF8 byte limits preserve Unicode and reject invalid text without issuing IDs or writes',async()=>{
    const writes=[],clipboard=createBrowserClipboard({maxBytes:4,writeText:async text=>{writes.push(text);}});
    for(const text of ['abcd','éé','🦆','']){
        const id=clipboard.begin(text);assert.ok(id);await flush();assert.equal(clipboard.take().ok,true);
    }
    for(const text of ['abcde','ééa','🦆a','\ud800','\udc00','a\ud800',null,undefined,1,{},new String('a')])assert.equal(clipboard.begin(text),0);
    assert.deepEqual(writes,['abcd','éé','🦆','']);
    assert.equal(clipboard.begin('a'),5);await flush();clipboard.take();clipboard.dispose();
    const empty=createBrowserClipboard({maxBytes:0,writeText:async text=>{assert.equal(text,'');}});
    assert.equal(empty.begin('a'),0);assert.equal(empty.begin(''),1);await flush();assert.equal(empty.take().ok,true);
});
test('the actual 8MiB limit accepts its exact boundary and rejects oversized input before the writer',async()=>{
    let written=0;const clipboard=createBrowserClipboard({writeText:async text=>{written=text.length;}});
    assert.equal(clipboard.begin('x'.repeat(8*1024*1024+1)),0);assert.equal(written,0);
    assert.equal(clipboard.begin('x'.repeat(8*1024*1024)),1);assert.equal(written,8*1024*1024);
    await flush();assert.equal(clipboard.take().ok,true);clipboard.dispose();
});
test('unavailable APIs, synchronous throws and rejected promises yield one honest failure then recover',async()=>{
    for(const writeText of [undefined,null,()=>{throw new Error('Permission denied.');},async()=>{throw new Error('Write rejected.');},()=>undefined]){
        const clipboard=createBrowserClipboard({writeText});const id=clipboard.begin('kept text');assert.equal(id,1);
        await flush();const result=clipboard.take();assert.equal(result.id,id);assert.equal(result.ok,false);
        assert.equal(typeof result.error,'string');assert.ok(result.error.length);assert.equal(clipboard.take(),null);
        assert.equal(clipboard.begin('retry'),2);await flush();assert.equal(clipboard.take().ok,false);clipboard.dispose();
    }
    let fail=true;const clipboard=createBrowserClipboard({writeText:async()=>{if(fail)throw new Error('Denied once');}});
    clipboard.begin('first');await flush();assert.equal(clipboard.take().ok,false);fail=false;
    clipboard.begin('retry');await flush();assert.equal(clipboard.take().ok,true);
});
test('diagnostics are scalar UTF8 bounded by bytes even for hostile rejection values',async()=>{
    const failures=['🦆'.repeat(1000),'\ud800\0'+ 'é'.repeat(1000),{get message(){throw new Error('getter must not escape');}},Symbol('denied'),null];
    for(const maximum of [1,3,4,511,512])for(const error of failures){
        const clipboard=createBrowserClipboard({maxErrorBytes:maximum,writeText:()=>Promise.reject(error)});
        clipboard.begin('value');await flush();const result=clipboard.take();
        assert.equal(result.ok,false);assert.ok(encoder.encode(result.error).length<=maximum);assert.ok(result.error.length);
        assert.equal(result.error.includes('\0'),false);assert.equal(decoder.decode(encoder.encode(result.error)),result.error);clipboard.dispose();
    }
});
test('thenables cannot settle twice, throw past success or publish an old ID over a later operation',async()=>{
    let oldResolve,oldReject,reads=0;const current=deferred();let calls=0;
    const clipboard=createBrowserClipboard({writeText(){
        if(calls++)return current.promise;
        return {get then(){reads++;return function(resolve,reject){oldResolve=resolve;oldReject=reject;};}};
    }});
    const first=clipboard.begin('first');assert.equal(reads,1);oldResolve();oldReject(new Error('late reject'));await flush();
    assert.deepEqual(clipboard.take(),{id:first,ok:true,error:null});const second=clipboard.begin('second');
    oldResolve();oldReject(new Error('older ID'));await flush();assert.equal(clipboard.take(),null);
    current.reject(new Error('current failure'));await flush();assert.deepEqual(clipboard.take(),{id:second,ok:false,error:'current failure'});
    const throws=createBrowserClipboard({writeText:()=>({get then(){throw new Error('then getter failure');}})});
    throws.begin('value');assert.equal(throws.take().error,'then getter failure');
    const both=createBrowserClipboard({writeText:()=>({then(resolve){resolve();throw new Error('after success');}})});
    both.begin('value');await flush();assert.equal(both.take().ok,true);
});
test('reentrant writers see the reserved busy slot; disposal ignores resolved and rejected late completions',async()=>{
    let clipboard;clipboard=createBrowserClipboard({writeText(){assert.equal(clipboard.begin('nested'),0);assert.equal(clipboard.take(),null);return Promise.resolve();}});
    clipboard.begin('outer');await flush();assert.equal(clipboard.take().ok,true);
    for(const reject of [false,true]){
        const pending=deferred(),owner=createBrowserClipboard({writeText:()=>pending.promise});
        assert.equal(owner.begin('pending'),1);owner.dispose();owner.dispose();assert.equal(owner.begin('after dispose'),0);
        if(reject)pending.reject(new Error('late error'));else pending.resolve();
        await flush();assert.equal(owner.take(),null);
    }
    clipboard.begin('settled but unread');await flush();clipboard.dispose();assert.equal(clipboard.take(),null);
});
test('configuration fails closed without a writer call or platform access',()=>{
    for(const maxBytes of [-1,1.5,Infinity,NaN,'4',8*1024*1024+1])assert.throws(()=>createBrowserClipboard({maxBytes}),TypeError);
    for(const maxErrorBytes of [0,-1,1.5,Infinity,NaN,'4',513])assert.throws(()=>createBrowserClipboard({maxErrorBytes}),TypeError);
    for(const writeText of [0,false,{},'writer'])assert.throws(()=>createBrowserClipboard({writeText}),TypeError);
    assert.ok(Object.isFrozen(createBrowserClipboard()));
});
