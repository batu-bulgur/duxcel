// Measure UI_MUTED glyph support in an actual compositor ROI. Antialiasing
// changes coverage with raster phase; a hard darkness cutoff loses faint edges.
// Background mode and the foreground color ray exclude borders and accents.
export function measureLabelPixels(image,ratio=1,ratioY=ratio){
 const {data,width,height}=image,foreground=[99,113,125],counts=new Map();
 // Current opaque grid/border paint can be close to the antialias color ray.
 const nonGlyph=new Set([0xe4e8ec,0xcad2da,0x8fa2b3,0x285f86,0xe4eef6]);
 if(!Number.isInteger(width)||!Number.isInteger(height)||width<0||height<0||data.length!==width*height*4||!(ratio>0)||!(ratioY>0))throw new TypeError('Invalid label pixel region');
 let mode=0,frequency=0,min=255,max=0;
 for(let p=0;p<data.length;p+=4){
  min=Math.min(min,data[p],data[p+1],data[p+2]);max=Math.max(max,data[p],data[p+1],data[p+2]);
  if(data[p+3]!==255)continue;
  const key=(data[p]<<16)|(data[p+1]<<8)|data[p+2],n=(counts.get(key)??0)+1;counts.set(key,n);
  if(n>frequency){mode=key;frequency=n;}
 }
 const background=[mode>>>16,(mode>>>8)&255,mode&255],span=background.map((v,c)=>v-foreground[c]);
 let left=width,right=-1,top=height,bottom=-1,ink=0;
 for(let y=0;y<height;y++)for(let x=0;x<width;x++){
  const p=(y*width+x)*4;if(data[p+3]!==255||span.some(v=>v<=0)||nonGlyph.has((data[p]<<16)|(data[p+1]<<8)|data[p+2]))continue;
  const coverage=span.map((v,c)=>(background[c]-data[p+c])/v),low=Math.min(...coverage),high=Math.max(...coverage);
  if(low<=.05||high>1.035||high-low>=.035)continue;
  left=Math.min(left,x);right=Math.max(right,x);top=Math.min(top,y);bottom=Math.max(bottom,y);ink++;
 }
 return {width:ink?(right-left+1)/ratio:0,height:ink?(bottom-top+1)/ratioY:0,ink,min,max,background,bounds:ink?{left,right,top,bottom}:null};
}
