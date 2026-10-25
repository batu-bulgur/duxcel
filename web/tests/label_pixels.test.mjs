import test from 'node:test';
import assert from 'node:assert/strict';
import {measureLabelPixels} from './support/label_pixels.mjs';
const foreground=[99,113,125];
function region(background=[244,246,248],width=24,height=20){
 const data=new Uint8ClampedArray(width*height*4);for(let p=0;p<data.length;p+=4)data.set([...background,255],p);
 return {data,width,height,background};
}
function pixel(image,x,y,color){image.data.set([...color,255],(y*image.width+x)*4);}
function glyph(image,{x=7,y=5,width=6,height=9,coverage=.1}={}){
 for(let j=0;j<height;j++)for(let i=0;i<width;i++)if(i===0||j===0||i===width-1||j===height-1)pixel(image,x+i,y+j,image.background.map((v,c)=>Math.round(v+(foreground[c]-v)*coverage)));
 return image;
}
function unchanged(a,b,ratio=1){const tolerance=1.25/ratio+.02;return Math.abs(a.width-b.width)<=tolerance&&Math.abs(a.height-b.height)<=tolerance;}
test('faint antialiasing and different backgrounds retain the same glyph support',()=>{
 const a=measureLabelPixels(glyph(region(),{coverage:.07})),b=measureLabelPixels(glyph(region([228,238,246]),{coverage:.8}));
 assert.deepEqual([a.width,a.height],[6,9]);assert.deepEqual([b.width,b.height],[6,9]);assert.ok(unchanged(a,b));
 assert.deepEqual(a.background,[244,246,248]);assert.deepEqual(b.background,[228,238,246]);
});
test('real glyph resizing fails the unchanged DPR tolerance',()=>{
 const before=measureLabelPixels(glyph(region())),after=measureLabelPixels(glyph(region(),{width:9,height:13}));
 assert.equal(unchanged(before,after),false);
 const dpr2=measureLabelPixels(glyph(region(undefined,40,32),{width:12,height:18}),2);
 assert.deepEqual([dpr2.width,dpr2.height],[6,9]);assert.equal(unchanged(dpr2,measureLabelPixels(glyph(region(undefined,40,32),{width:16,height:24}),2),2),false);
});
test('grid borders, selection tints, accents and transparent pixels are not glyphs',()=>{
 const image=region();for(let x=0;x<image.width;x++)pixel(image,x,1,[228,232,236]);
 for(let y=2;y<18;y++)pixel(image,1,y,[47,102,143]);
 pixel(image,3,3,[228,238,246]);pixel(image,5,3,[202,210,218]);pixel(image,6,3,[143,162,179]);pixel(image,4,4,[0,0,0]);image.data.set([99,113,125,0],(6*image.width+6)*4);
 const result=measureLabelPixels(image);assert.equal(result.ink,0);assert.equal(result.bounds,null);
});
test('empty and background-only regions have no support; malformed regions fail',()=>{
 assert.equal(measureLabelPixels(region()).ink,0);assert.equal(measureLabelPixels({data:new Uint8Array(),width:0,height:0}).ink,0);
 assert.throws(()=>measureLabelPixels({data:new Uint8Array(4),width:2,height:2}),TypeError);
 assert.throws(()=>measureLabelPixels(region(),0),TypeError);
});
