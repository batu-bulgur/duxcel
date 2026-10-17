/** Review-only browser transport. C owns widgets, picking, notes and frame pixels.
 * This module never reads the live canvas or computes workbook data. */
export const REVIEW_LIMITS=Object.freeze({json:4*1024*1024,pixels:32*1024*1024,png:128*1024*1024,build:256*1024,sidecar:1024*1024});
const textEncoder=new TextEncoder(),textDecoder=new TextDecoder('utf-8',{fatal:true});
const now=()=>globalThis.performance?.now?.()??Date.now();
const identifier=value=>typeof value==='string' && /^[A-Za-z0-9_-]{1,95}$/.test(value);
const uint64=value=>typeof value==='string' && /^(0|[1-9][0-9]{0,19})$/.test(value) && BigInt(value)<=18446744073709551615n;
function check(ok,message){if(!ok)throw new Error(message);}
function dimensions(width,height){
    check(Number.isSafeInteger(width)&&Number.isSafeInteger(height)&&width>0&&height>0&&width<=REVIEW_LIMITS.pixels/height,'Invalid or oversized review dimensions.');
    return {width,height};
}
function finite(value,name){check(typeof value==='number'&&Number.isFinite(value),`Invalid browser ${name}.`);return value;}
function positive(value,name){finite(value,name);check(value>0,`Invalid browser ${name}.`);return value;}
function cloneJSON(value,limit,name){
    const json=JSON.stringify(value);check(typeof json==='string'&&textEncoder.encode(json).length<=limit,`${name} exceeds its metadata limit.`);
    return JSON.parse(json);
}
function defaultGeometry(canvas){
    const rect=canvas.getBoundingClientRect(),view=globalThis.visualViewport;
    return {canvasCSS:{x:rect.x??rect.left,y:rect.y??rect.top,width:rect.width,height:rect.height},
        canvasBacking:{width:canvas.width,height:canvas.height},devicePixelRatio:globalThis.devicePixelRatio??1,
        visualViewport:view?{scale:view.scale,offsetLeft:view.offsetLeft,offsetTop:view.offsetTop,width:view.width,height:view.height}:null,
        viewport:{width:globalThis.innerWidth??rect.width,height:globalThis.innerHeight??rect.height,scrollX:globalThis.scrollX??0,scrollY:globalThis.scrollY??0},
        userAgent:globalThis.navigator?.userAgent??'',language:globalThis.navigator?.language??''};
}
function frozenGeometry(value,logical){
    check(value&&value.canvasCSS&&value.canvasBacking,'Browser canvas geometry is unavailable.');
    const css={x:finite(value.canvasCSS.x,'canvas x'),y:finite(value.canvasCSS.y,'canvas y'),
        width:positive(value.canvasCSS.width,'canvas width'),height:positive(value.canvasCSS.height,'canvas height')};
    const backing=dimensions(value.canvasBacking.width,value.canvasBacking.height);
    const result={canvas_css:css,canvas_backing:backing,device_pixel_ratio:positive(value.devicePixelRatio,'DPR'),visual_viewport:null};
    if(value.visualViewport){
        const view=value.visualViewport;
        result.visual_viewport={scale:positive(view.scale,'viewport scale'),offset_left:finite(view.offsetLeft,'viewport offset x'),offset_top:finite(view.offsetTop,'viewport offset y'),width:positive(view.width,'viewport width'),height:positive(view.height,'viewport height')};
    }
    if(value.viewport){
        const view=value.viewport;
        result.viewport={width:positive(view.width,'window width'),height:positive(view.height,'window height'),scroll_x:finite(view.scrollX,'scroll x'),scroll_y:finite(view.scrollY,'scroll y')};
    }
    for(const [input,output,limit] of [['userAgent','user_agent',2048],['language','language',128]]){
        const text=value[input]??'';check(typeof text==='string'&&textEncoder.encode(text).length<=limit,'Browser identity text exceeds its limit.');result[output]=text;
    }
    result.transforms={logical_to_css:{scale_x:css.width/logical.width,scale_y:css.height/logical.height,translate_x:css.x,translate_y:css.y},
        logical_to_framebuffer:{scale_x:backing.width/logical.width,scale_y:backing.height/logical.height}};
    return result;
}
async function defaultEncodePNG({rgba,width,height}){
    check(typeof OffscreenCanvas==='function'&&typeof ImageData==='function','PNG export is unavailable in this browser.');
    const image=new OffscreenCanvas(width,height),context=image.getContext('2d');
    check(context,'Cannot prepare review image encoding.');
    context.putImageData(new ImageData(new Uint8ClampedArray(rgba.buffer,rgba.byteOffset,rgba.byteLength),width,height),0,0);
    return image.convertToBlob({type:'image/png'});
}
function octal(header,offset,width,value){
    const text=value.toString(8);check(text.length<width,'Review archive member is too large.');
    header.set(textEncoder.encode(text.padStart(width-1,'0')+'\0'),offset);
}
function tarHeader(reportId,name,size){
    const header=new Uint8Array(512);
    header.set(textEncoder.encode(name),0);octal(header,100,8,0o600);octal(header,108,8,0);octal(header,116,8,0);
    octal(header,124,12,size);octal(header,136,12,0);header.fill(32,148,156);header[156]=48;
    header.set(textEncoder.encode('ustar\0'),257);header.set(textEncoder.encode('00'),263);header.set(textEncoder.encode(reportId),345);
    const sum=header.reduce((total,byte)=>total+byte,0);header.set(textEncoder.encode(sum.toString(8).padStart(6,'0')+'\0 '),148);
    return header;
}
function archive(reportId,files){
    const parts=[];
    for(const [name,bytes] of files){parts.push(tarHeader(reportId,name,bytes.byteLength),bytes);const padding=(512-bytes.byteLength%512)%512;if(padding)parts.push(new Uint8Array(padding));}
    parts.push(new Uint8Array(1024));return new Blob(parts,{type:'application/x-tar'});
}
async function pngBytes(value,width,height){
    let bytes;
    if(value instanceof Blob){check(value.size<=REVIEW_LIMITS.png,'Review PNG exceeds 128 MiB.');bytes=new Uint8Array(await value.arrayBuffer());}
    else{check(value instanceof Uint8Array&&value.byteLength<=REVIEW_LIMITS.png,'Review encoder did not return a bounded PNG.');bytes=new Uint8Array(value);}
    const signature=[137,80,78,71,13,10,26,10];
    check(bytes.length>=33&&signature.every((byte,index)=>bytes[index]===byte),'Review encoder did not return a PNG.');
    const view=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength);
    check(view.getUint32(8)===13&&textDecoder.decode(bytes.subarray(12,16))==='IHDR'&&view.getUint32(16)===width&&view.getUint32(20)===height,'Encoded review PNG dimensions do not match the frozen frame.');
    return bytes;
}
async function sha256(crypto,bytes){
    const hash=new Uint8Array(await crypto.subtle.digest('SHA-256',bytes));
    check(hash.length===32,'Browser SHA-256 returned an invalid digest.');
    return Array.from(hash,byte=>byte.toString(16).padStart(2,'0')).join('');
}

/** Dependencies encodePNG/submit/download/geometry/crypto are injectable for pure tests.
 * encodePNG({rgba,width,height}) receives an owned RGBA copy, returns PNG bytes or
 * Blob. submit(blob,reportId,{projectId}) returns a durable local or queued receipt.
 * download(blob,filename) remains an explicit legacy/export mode, not durability.
 * geometry() returns canvasCSS,canvasBacking,devicePixelRatio, optional
 * visualViewport/viewport/userAgent/language as in defaultGeometry above.
 * beginFrame throws synchronously on invalid capture metadata. exportReport copies
 * both buffers before returning its handled Promise; complete is always deferred. */
export function createReviewHost({canvas,build,crypto=globalThis.crypto,encodePNG=defaultEncodePNG,submit,download,geometry=()=>defaultGeometry(canvas)}={}){
    check(canvas&&typeof canvas.addEventListener==='function'&&typeof canvas.removeEventListener==='function','A review canvas is required.');
    check(crypto&&typeof crypto.getRandomValues==='function'&&typeof crypto.subtle?.digest==='function','Review export requires browser cryptography.');
    check(typeof encodePNG==='function'&&typeof geometry==='function'&&(!download||typeof download==='function')&&(!submit||typeof submit==='function'),'Invalid review transport dependencies.');
    const buildSnapshot=cloneJSON(build,REVIEW_LIMITS.build,'Build metadata'),binarySHA256=buildSnapshot?.artifacts?.['duxcel.wasm'];
    check(typeof binarySHA256==='string'&&/^[a-f0-9]{64}$/.test(binarySHA256),'Actual review Wasm SHA-256 is unavailable.');
    const random=new Uint8Array(16);crypto.getRandomValues(random);
    const sessionId='S-'+Array.from(random,byte=>byte.toString(16).padStart(2,'0')).join('');
    let epoch=0,disposed=false,requested=false,frozen=null;
    const urls=new Map();
    function revoke(url){const timer=urls.get(url);if(timer!==undefined){clearTimeout(timer);urls.delete(url);}URL.revokeObjectURL(url);}
    const requestDownload=download??((blob,filename)=>{
        const document=canvas.ownerDocument??globalThis.document;check(document&&typeof URL.createObjectURL==='function','Browser downloads are unavailable.');
        const url=URL.createObjectURL(blob);let anchor;
        try{anchor=document.createElement('a');anchor.href=url;anchor.download=filename;anchor.hidden=true;document.body.append(anchor);anchor.click();}
        catch(error){URL.revokeObjectURL(url);throw error;}
        finally{anchor?.remove();}
        urls.set(url,setTimeout(()=>revoke(url),1000));
    });
    const listener=event=>{
        const document=canvas.ownerDocument??globalThis.document;
        if(disposed||event.target!==canvas||(document&&document.activeElement!==canvas)||event.isComposing||event.altKey||
            !(event.ctrlKey||event.metaKey)||!event.shiftKey||!(event.code==='Period'||(!event.code&&(event.key==='.'||event.key==='>'))))return;
        event.preventDefault();event.stopImmediatePropagation();if(!event.repeat)requested=true;
    };
    // GLFW captures keys on window. Register this host before module/GLFW init;
    // a canvas listener would run after that window capture handler.
    const keyboardTarget=canvas.ownerDocument?.defaultView??canvas;
    keyboardTarget.addEventListener('keydown',listener,true);
    const active=token=>!disposed&&epoch===token;
    function cancel(){epoch++;frozen=null;requested=false;}
    return {
        sessionId,binarySHA256,
        get closed(){return disposed;},
        takeRequest(){const value=requested;requested=false;return value;},
        beginFrame({reportId,frameId,logicalWidth,logicalHeight}={}){
            check(!disposed,'Review transport is closed.');check(identifier(reportId)&&uint64(frameId),'Invalid review report or frame identity.');
            const logical=dimensions(logicalWidth,logicalHeight),browser=frozenGeometry(geometry(),logical);
            epoch++;frozen={reportId,frameId,logical,browser};return true;
        },
        exportReport({reportId,frameId,json,rgba,width,height,complete}={}){
            const token=++epoch,started=now();let ownedJSON,ownedRGBA,capture,validation,copyMS,projectId;
            try{
                check(!disposed,'Review transport is closed.');check(typeof complete==='function','Review completion callback is required.');
                check(identifier(reportId)&&uint64(frameId)&&frozen&&frozen.reportId===reportId&&frozen.frameId===frameId,'Review export does not match its frozen frame.');
                dimensions(width,height);check(width===frozen.browser.canvas_backing.width&&height===frozen.browser.canvas_backing.height,'Review pixels do not match the captured canvas backing size.');
                check(json instanceof Uint8Array&&json.byteLength>0&&json.byteLength<=REVIEW_LIMITS.json,'Review JSON exceeds 4 MiB or is missing.');
                check(rgba instanceof Uint8Array&&rgba.byteLength===width*height*4,'Invalid review RGBA buffer.');
                const copyStarted=now();ownedJSON=new Uint8Array(json);ownedRGBA=new Uint8Array(rgba);copyMS=Math.max(0,now()-copyStarted);
                capture=cloneJSON(frozen,REVIEW_LIMITS.sidecar,'Frozen metadata');
                const manifest=JSON.parse(textDecoder.decode(ownedJSON));
                if(submit){projectId=manifest.project_id;check(identifier(projectId),'Review manifest project identity is unavailable.');}
                check(manifest.schema==='duxcel.native-review'&&manifest.version===1&&manifest.report_id===reportId&&manifest.session_id===sessionId&&manifest.frame?.frame_id===frameId,'Review manifest identity does not match the frozen C frame.');
                check(manifest.frame.binary_sha256===binarySHA256&&manifest.frame.window?.width===capture.logical.width&&manifest.frame.window?.height===capture.logical.height&&manifest.frame.framebuffer?.width===width&&manifest.frame.framebuffer?.height===height,'Review manifest dimensions or Wasm hash do not match capture metadata.');
            }catch(error){validation=error;}
            let completed=false;
            const finish=(ok,error)=>{if(!active(token)||completed)return false;completed=true;try{if(typeof complete==='function')complete(ok,error);}catch{return false;}return ok;};
            return Promise.resolve().then(async()=>{
                if(!active(token))return false;if(validation)throw validation;
                const encoded=await encodePNG({rgba:ownedRGBA,width,height});if(!active(token))return false;
                const png=await pngBytes(encoded,width,height);if(!active(token))return false;
                const [manifestHash,pngHash]=await Promise.all([sha256(crypto,ownedJSON),sha256(crypto,png)]);if(!active(token))return false;
                const sidecar={schema:'duxcel.browser-review',version:1,report_id:reportId,session_id:sessionId,frame_id:frameId,binary_sha256:binarySHA256,
                    logical:capture.logical,...capture.browser,build:buildSnapshot,transport:{kind:submit?'unified-review-queue':'browser-download',durable:false},
                    timing:{owned_copy_ms:copyMS,export_preparation_ms:Math.max(0,now()-started),scope:'Export invocation through PNG/hash preparation, before archive construction/transport; not input latency.'},
                    files:{'manifest.json':{sha256:manifestHash,bytes:ownedJSON.byteLength},'frame.png':{sha256:pngHash,bytes:png.byteLength,width,height}}};
                const browser=textEncoder.encode(JSON.stringify(sidecar));check(browser.byteLength<=REVIEW_LIMITS.sidecar,'Browser review metadata exceeds its limit.');
                if(!active(token))return false;
                const report=archive(reportId,[['manifest.json',ownedJSON],['frame.png',png],['browser.json',browser]]);
                if(submit){
                    const receipt=await submit(report,reportId,{projectId});
                    check(receipt?.report_id===reportId&&receipt.durable===true&&(receipt.queued===true||receipt.queued===false&&receipt.local===true),'Review transport did not confirm durable storage for this report.');
                    return finish(true,receipt.queued?'Saved to the shared review queue.':'Saved in this browser; awaiting the review service.');
                }
                await requestDownload(report,reportId+'.dxreview.tar');
                return finish(true,'');
            }).catch(error=>finish(false,String(error?.message??error).slice(0,512)));
        },
        cancel,
        dispose(){if(disposed)return;disposed=true;cancel();keyboardTarget.removeEventListener('keydown',listener,true);for(const url of [...urls.keys()])revoke(url);}
    };
}
