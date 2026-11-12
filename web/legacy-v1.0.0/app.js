"use strict";
const canvas = document.querySelector("canvas");
const ctx = canvas.getContext("2d", {alpha:false});
const white = 0xffff, black = 0, muted = 0xa69a, accent = 0x5eff;
const rgb = c => {
  const r = c >> 11, g = (c >> 5) & 63, b = c & 31;
  return "rgb(" + ((r << 3) | (r >> 2)) + "," + ((g << 2) | (g >> 4)) + "," + ((b << 3) | (b >> 2)) + ")";
};
function rect(x,y,w,h,c) { ctx.fillStyle = rgb(c); ctx.fillRect(x,y,w,h); }
function line(x,y,a,b,c) {
  const dx=Math.abs(a-x), dy=-Math.abs(b-y), sx=x<a?1:-1, sy=y<b?1:-1;
  let error=dx+dy;
  for (;;) {
    rect(x,y,1,1,c);
    if(x===a && y===b) break;
    const e=2*error;
    if(e>=dy) { error+=dy; x+=sx; }
    if(e<=dx) { error+=dx; y+=sy; }
  }
}
// Integer scan conversion follows Adafruit_GFX (see bundled license).
function arcSteps(r, visit) {
  let x=0,y=r,error=1-r;
  while(x<y) {
    if(error>=0) { y--; error-=2*y; }
    x++; error+=2*x+1;
    visit(x,y);
  }
}
function quarter(x,y,r,sx,sy,c) {
  arcSteps(r,(a,b)=>{
    rect(x+sx*a,y+sy*b,1,1,c);
    rect(x+sx*b,y+sy*a,1,1,c);
  });
}
function halfFill(x,y,r,side,delta,c) {
  let previousX=0,previousY=r;
  arcSteps(r,(a,b)=>{
    if(a<=b) rect(x+side*a,y-b,1,2*b+delta+1,c);
    if(b!==previousY) rect(x+side*previousY,y-previousX,1,2*previousX+delta+1,c);
    previousX=a; previousY=b;
  });
}
function round(x,y,w,h,r,c,filled=true) {
  r=Math.min(r,Math.floor(Math.min(w,h)/2));
  if(filled) {
    rect(x+r,y,w-2*r,h,c);
    halfFill(x+w-r-1,y+r,r,1,h-2*r-1,c);
    halfFill(x+r,y+r,r,-1,h-2*r-1,c);
  } else {
    rect(x+r,y,w-2*r,1,c); rect(x+r,y+h-1,w-2*r,1,c);
    rect(x,y+r,1,h-2*r,c); rect(x+w-1,y+r,1,h-2*r,c);
    quarter(x+r,y+r,r,-1,-1,c);
    quarter(x+w-r-1,y+r,r,1,-1,c);
    quarter(x+w-r-1,y+h-r-1,r,1,1,c);
    quarter(x+r,y+h-r-1,r,-1,1,c);
  }
}
function circle(x,y,r,c,filled=true) {
  if(filled) {
    rect(x,y-r,1,2*r+1,c);
    halfFill(x,y,r,1,0,c); halfFill(x,y,r,-1,0,c);
  } else {
    for(const d of [-r,r]) { rect(x+d,y,1,1,c); rect(x,y+d,1,1,c); }
    for(const sx of [-1,1]) for(const sy of [-1,1]) quarter(x,y,r,sx,sy,c);
  }
}
function triangle(points,c) {
  const [a,b,d]=points.sort((p,q)=>p[1]-q[1]);
  const crossing=(p,q,y)=>p[0]+Math.trunc((q[0]-p[0])*(y-p[1])/(q[1]-p[1]));
  for(let y=a[1];y<=d[1];y++) {
    const p=y<b[1]||b[1]===d[1]?crossing(a,b,y):crossing(b,d,y);
    const q=crossing(a,d,y);
    rect(Math.min(p,q),y,Math.abs(p-q)+1,1,c);
  }
}
function glyphs(value,font) {
  return [...value].map(ch=>font.glyphs[(ch==="°"?111:ch.charCodeAt(0))-32] || font.glyphs[31]);
}
function bounds(value,font) {
  let left=Infinity,right=-Infinity,top=Infinity,bottom=-Infinity,x=0;
  for(const [,w,h,advance,dx,dy] of glyphs(value,font)) {
    if(w && h) {
      left=Math.min(left,x+dx); right=Math.max(right,x+dx+w);
      top=Math.min(top,dy); bottom=Math.max(bottom,dy+h);
    }
    x+=advance;
  }
  return left===Infinity?[0,0,0,0]:[left,top,right-left,bottom-top];
}
function print(value,x,y,size,color) {
  const font=fonts[size];
  ctx.fillStyle=rgb(color);
  glyphs(value,font).forEach(([offset,w,h,advance,dx,dy],i)=>{
    if(value[i]==="°") {
      circle(x+Math.trunc(advance/2),y+font.glyphs[35][5]+2,2,color,false);
    } else for(let row=0;row<h;row++) for(let col=0;col<w;col++) {
      const bit=row*w+col;
      if(font.bits[offset+(bit>>3)] & (128>>(bit&7))) ctx.fillRect(x+dx+col,y+dy+row,1,1);
    }
    x+=advance;
  });
}
let background, highlight;
const fields=new Map();
function field(x,y,w,h,value,size=0,color=white,center=false) {
  value=String(value);
  if(fields.get(y*320+x)===value) return;
  fields.set(y*320+x,value);
  const end=Math.min(x+w,288);
  x=Math.max(x,32); w=end-x;
  let [dx,dy,tw,th]=bounds(value,fonts[size]);
  while(tw>w && value.length) {
    value=value.slice(0,-1); [dx,dy,tw,th]=bounds(value,fonts[size]);
  }
  rect(x,y,w,h,background);
  print(value,x-dx+(center?Math.trunc((w-tw)/2):0),y-dy+Math.trunc((h-th)/2),size,color);
}
const views = {
  Home:["",0x1845,0xfe23,["APPS"]],
  Apps:["Apps",0x10a4,0xfe28,["BACK","OPEN","^","v"]],
  F1:["F1 REACTION",black,0xfa69,["BACK","MODE","STATS","PLAY"]],
  Now:["Weather / Now",0x08a7,0x65ff,["BACK",null,"^","v"]],
  Forecast:["Weather / Forecast",0x08a7,0x65ff,["BACK",null,"^","v"]],
  WiFi:["Wi-Fi",0x0842,accent,["BACK","RETRY"]],
  Device:["Device",0x18e3,0xb6b1,["BACK"]],
  Stats:["F1 Statistics",black,0xfb0c,["BACK"]]
};
const apps=["F1","Now","WiFi","Device"];
let view="Home", selection=0, previousSelection=-1, moving=false;
let lastLights=-1, weatherCode=-100, dailyIcons=[];
const boot=performance.now();
const sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));
function tabs(offset) {
  views[view][3].forEach((label,i)=>{
    if(!label) return;
    const right=i>=2, arrow=label==="^"||label==="v";
    const icon=label==="APPS"||label==="BACK", length=label.length*6;
    const h=arrow?22:length+(icon?20:8), patch=right?296:0;
    const x=right?299+offset:-offset, cx=x+10;
    const y=(i%2?179:58)-(h>>1), center=y+(h>>1);
    ctx.save(); ctx.beginPath(); ctx.rect(patch,y,24,h); ctx.clip();
    rect(patch,y,24,h,background);
    if(offset<21) {
      round(right?x:x-7,y,28,h,7,highlight,false);
      if(arrow) {
        const d=label==="^"?-3:3;
        line(cx-5,center-d,cx,center+d,white);
        line(cx,center+d,cx+5,center-d,white);
      } else {
        if(label==="APPS") for(const dy of [-4,1]) for(const dx of [-4,1])
          round(cx+dx,y+9+dy,3,3,0,highlight,false);
        if(label==="BACK") {
          rect(cx-4,y+9,9,1,highlight);
          line(cx-4,y+9,cx,y+6,highlight);
          line(cx-4,y+9,cx,y+12,highlight);
        }
        for(let u=0;u<length;u++) for(let v=0;v<8;v++) {
          const byte=classic[label.charCodeAt(Math.floor(u/6))*5+u%6];
          if(u%6<5 && (byte & (1<<v))) {
            rect(cx+(right?3-v:v-3),center+(icon?6:0)+(right?u-length/2:length/2-u),1,1,white);
          }
        }
      }
    }
    ctx.restore();
  });
}
function beginPage() {
  const [title,base,color]=views[view];
  background=base; highlight=color;
  fields.clear(); previousSelection=-1; lastLights=-1; weatherCode=-100; dailyIcons=[];
  rect(0,0,320,240,base);
  if(title) {
    field(32,7,256,29,title,1,color,true);
    rect(32,40,256,1,muted);
  } else rect(84,160,152,1,muted);
  draw();
}
async function open(next) {
  moving=true;
  for(const offset of [0,1,3,7,12,18,21]) { tabs(offset); if(offset!==21) await sleep(12); }
  view=next; beginPage();
  for(const offset of [21,15,8,2,-2,-1,0]) { tabs(offset); if(offset!==0) await sleep(12); }
  moving=false;
}
function launcher() {
  ["F1 Reaction","Weather","Wi-Fi","Device"].forEach((label,i)=>{
    if(previousSelection>=0 && i!==selection && i!==previousSelection) return;
    const y=49+i*43, x=67, cy=y+18, active=i===selection;
    const ink=active?black:white;
    round(46,y,228,36,8,active?highlight:0x2167);
    if(!active) round(46,y,228,36,8,0x426c,false);
    if(i===0) {
      rect(x-6,cy-7,1,15,ink); round(x-5,cy-7,13,9,0,ink,false);
      rect(x-4,cy-6,4,3,ink); rect(x,cy-3,4,3,ink);
    } else if(i===1) {
      circle(x-5,cy,5,ink); circle(x+2,cy-3,6,ink); round(x-8,cy,19,6,3,ink);
    } else if(i===2) {
      for(const r of [5,9,13]) {
        const end=cy+3-(r>>1);
        line(x-r,end,x,cy-r,ink); line(x,cy-r,x+r,end,ink);
      }
      circle(x,cy+5,2,ink);
    } else {
      round(x-6,cy-6,13,13,0,ink,false);
      for(const n of [-4,0,4]) { rect(x-9,cy+n,3,1,ink); rect(x+7,cy+n,3,1,ink); }
    }
    print(label,93,y+25,1,ink);
  });
  previousSelection=selection;
}
function weatherIcon(x,y,code) {
  if(code<0) return;
  const sun=0xfdc0;
  if(code<=2) {
    const sx=code===0?x:x-8;
    circle(sx,y-6,8,sun);
    for(const d of [-1,1]) {
      rect(sx+(d<0?-14:11),y-6,4,1,sun);
      rect(sx,y-6+(d<0?-14:11),1,4,sun);
      line(sx+d*8,y-14,sx+d*11,y-17,sun);
    }
  }
  if(code===0) return;
  if(code===45||code===48) {
    for(const i of [-1,0,1]) round(x-14+(i+1)*2,y+i*6,24,3,1,0xce79);
    return;
  }
  circle(x-9,y,7,white); circle(x+1,y-4,9,white); circle(x+10,y+1,6,white);
  round(x-15,y,30,8,4,white);
  if((code>=71&&code<=77)||code===85||code===86) {
    for(const dx of [-8,0,8]) circle(x+dx,y+13,2,white);
  } else if(code>=95||(code>=51&&code<=67)||(code>=80&&code<=82)) {
    for(const dx of [-10,9]) line(x+dx,y+10,x+dx-3,y+17,0x54ff);
  }
  if(code>=95) {
    triangle([[x+2,y+5],[x-4,y+13],[x+4,y+13]],sun);
    triangle([[x,y+11],[x+6,y+11],[x-1,y+19]],sun);
  }
}
function condition(code) {
  if(code<0) return "Weather pending";
  if(code===0) return "Clear";
  if(code<=2) return "Partly cloudy";
  if(code===3) return "Overcast";
  if(code<=48) return "Fog";
  if(code<=57) return "Drizzle";
  if(code<=67) return "Rain";
  if(code<=77||code===85||code===86) return "Snow";
  if(code<=82) return "Showers";
  if(code>=95) return "Thunderstorm";
  return "Weather unknown";
}
const zone="Asia/Kuala_Lumpur";
const timeFormat=new Intl.DateTimeFormat("en-GB",{timeZone:zone,hour:"2-digit",minute:"2-digit",second:"2-digit",hourCycle:"h23"});
const dateFormat=new Intl.DateTimeFormat("en-GB",{timeZone:zone,weekday:"short",day:"2-digit",month:"short",year:"numeric"});
const timeOf=value=>value?.slice(11,16)||"--:--";
const number=(n,d=0)=>Number.isFinite(n)?n.toFixed(d):"--";
const temp=n=>number(n,1)+"°C";
let weather=null, stale=false, connected=navigator.onLine, weatherTimer, fetching=false;
const status=document.querySelector("#weather");
async function fetchWeather() {
  if(fetching) return;
  clearTimeout(weatherTimer);
  fetching=true;
  try {
    const url="https://api.open-meteo.com/v1/forecast?latitude=2.945&longitude=101.874&timezone=Asia%2FKuala_Lumpur&forecast_days=3"
      +"&current=temperature_2m,apparent_temperature,relative_humidity_2m,surface_pressure,wind_speed_10m,precipitation,weather_code"
      +"&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,sunrise,sunset";
    const response=await fetch(url,{signal:AbortSignal.timeout(9000)});
    if(!response.ok) throw Error("Weather HTTP "+response.status);
    const data=await response.json();
    if(!data.current || !Array.isArray(data.daily?.time)) throw Error("Weather payload");
    weather=data; stale=false;
    status.textContent="士毛月天气 / Semenyih weather · Open-Meteo";
  } catch {
    stale=true;
    status.textContent=weather?"天气更新失败，显示缓存 / Weather update failed. Showing cached data.":"天气暂不可用，30 秒后重试 / Weather unavailable. Retrying in 30 seconds.";
  } finally {
    fetching=false;
    weatherTimer=setTimeout(fetchWeather,stale?30000:900000);
  }
}
function home() {
  const now=new Date(), time=timeFormat.format(now), code=weather?.current.weather_code??-1;
  const parts=[time.slice(0,2),":",time.slice(3,5),":",time.slice(6,8)];
  [44,110,129,192,214].forEach((x,i)=>field(x,45,i%2?18:62,71,parts[i],2,white,true));
  field(18,123,284,31,dateFormat.format(now),1,white,true);
  if(weatherCode!==code) {
    rect(46,171,41,40,background); weatherIcon(68,191,code); weatherCode=code;
  }
  field(89,172,176,35,condition(code)+"  |  "+temp(weather?.current.temperature_2m),0,highlight,true);
}
function weatherPage() {
  if(!weather) { field(82,49,200,37,"Fetching weather...",1,muted); return; }
  const {current:c,daily:d}=weather, code=c.weather_code??-1;
  if(view==="Forecast") {
    for(let i=0;i<3;i++) {
      const y=48+i*43, dailyCode=d.weather_code[i]??-1;
      if(dailyIcons[i]!==dailyCode) {
        rect(35,y,43,41,background); weatherIcon(57,y+20,dailyCode); dailyIcons[i]=dailyCode;
      }
      field(83,y,199,21,d.time[i]+"  "+condition(dailyCode),0,highlight);
      field(83,y+21,199,21,number(d.temperature_2m_max[i])+"/"+temp(d.temperature_2m_min[i])+"   Rain "+number(d.precipitation_probability_max[i])+"%");
    }
    field(16,179,288,22,"Sunrise "+timeOf(d.sunrise[0])+"   Sunset "+timeOf(d.sunset[0]));
  } else {
    if(weatherCode!==code) {
      rect(34,47,44,44,background); weatherIcon(56,68,code); weatherCode=code;
    }
    field(82,49,200,37,condition(code)+"  "+temp(c.temperature_2m),1,highlight);
    ["Feels like "+temp(c.apparent_temperature),"Humidity "+number(c.relative_humidity_2m)+"%",
      "Wind "+number(c.wind_speed_10m)+" km/h","Pressure "+number(c.surface_pressure)+" hPa",
      "Rain "+number(c.precipitation,1)+" mm"].forEach((s,i)=>field(16,90+i*24,288,i===4?22:23,s));
  }
  field(16,211,288,22,"Open-Meteo "+timeOf(c.time)+(stale?"  (cached)":""),0,muted);
}
function network() {
  field(16,52,288,27,connected?"Connected":"Disconnected",1,accent);
  ["SSID: "+(connected?"YOUR_WIFI_NAME":"--"),"IP: "+(connected?"192.0.2.2":"--"),
    "RSSI: "+(connected?"-55 dBm":"--"),"Clock: synchronized"].forEach((s,i)=>field(16,86+i*26,288,24,s));
  field(16,202,288,24,"Action: reconnect",0,muted);
}
function device() {
  field(16,52,288,29,"ESP32-S3",1,highlight);
  ["Chip temp: "+temp(40),"Uptime: "+Math.floor((performance.now()-boot)/1000)+" s",
    "Free heap: 220 KiB","Free PSRAM: 8190 KiB","Flash: 16 MiB"].forEach((s,i)=>field(16,86+i*26,288,24,s));
}


let saved=true;
let scores={best:0,total:0,last:0,outcome:"Idle"};
const storageKey="deskconsole-v1.0.0-stats";
try { scores={...scores,...JSON.parse(localStorage.getItem(storageKey))}; }
catch { saved=false; }
const game={state:"Idle",lights:0,result:0,debug:false,started:0,wait:0,go:0};
const duration=ms=>ms?ms.toFixed(1)+" ms":"--";
const canChangeMode=()=>["Idle","Result","FalseStart","Timeout"].includes(game.state);
function resetGame() { Object.assign(game,{state:"Idle",lights:0,result:0}); }
function finish(outcome) {
  game.state=outcome; game.lights=0;
  scores.last=outcome==="Result"?game.result:0;
  scores.outcome=outcome;
  scores.total=Math.min(0xffffffff,scores.total+1);
  if(scores.last>0&&(!scores.best||scores.last<scores.best)) scores.best=scores.last;
  try { localStorage.setItem(storageKey,JSON.stringify(scores)); saved=true; }
  catch { saved=false; }
}
function play(pressed) {
  if(game.state==="Idle") { game.started=performance.now(); game.state="Lighting"; }
  else if(["Lighting","Waiting","GoPending"].includes(game.state)) finish("FalseStart");
  else if(game.state==="Go") {
    const elapsed=pressed-game.go;
    if(elapsed<0) finish("FalseStart");
    else if(elapsed>=10000) finish("Timeout");
    else { game.result=elapsed; finish("Result"); }
  } else resetGame();
  draw();
}
function gamePage() {
  if(lastLights!==game.lights) {
    for(let i=0;i<5;i++) {
      const x=42+i*48;
      if(lastLights<0) {
        round(x,68,44,78,9,0x2104); round(x,68,44,78,9,0x39e7,false);
        circle(x+22,88,13,0x4a49);
      }
      if(lastLights<0||(i<game.lights)!==(i<lastLights))
        circle(x+22,126,13,i<game.lights?0xf800:0x4a49);
    }
    lastLights=game.lights;
  }
  const messages={Idle:"Press to start",Lighting:"Wait for lights out",Waiting:"Wait for lights out",
    GoPending:"GO!",Go:game.debug?"RAW "+duration(performance.now()-game.go):"GO!",
    Result:duration(game.result),FalseStart:"FALSE START",Timeout:"TIMEOUT"};
  field(38,151,244,38,messages[game.state],1,white,true);
  field(88,43,144,21,game.debug?"DEBUG":"NORMAL",0,highlight,true);
  if(!saved) field(16,210,288,21,"Flash save unavailable",0,muted,true);
}
function statsPage() {
  const last={FalseStart:"FALSE START",Timeout:"TIMEOUT"}[scores.outcome]||duration(scores.last);
  [last,duration(scores.best),scores.total].forEach((value,i)=>{
    const y=52+i*56;
    round(42,y,236,46,8,0x39e7,false);
    field(51,y+4,86,38,["LAST","BEST","ROUNDS"][i],0,highlight);
    field(142,y+4,127,38,value);
  });
}
const pages={Home:home,Apps:launcher,F1:gamePage,Now:weatherPage,Forecast:weatherPage,
  WiFi:network,Device:device,Stats:statsPage};
function draw() { pages[view](); }
function handle(key,pressed) {
  if(key===0) {
    if(view==="F1") resetGame();
    open({Stats:"F1",Home:"Apps",Apps:"Home"}[view]||"Apps");
  } else if(view==="Apps") {
    if(key===1) open(apps[selection]);
    else { selection=(selection+(key===2?3:1))%4; draw(); }
  } else if(view==="F1") {
    if(key===3) play(pressed);
    else if(canChangeMode()) {
      if(key===1) { game.debug=!game.debug; draw(); }
      else open("Stats");
    }
  } else if((view==="Now"||view==="Forecast")&&key>=2) {
    open(view==="Now"?"Forecast":"Now");
  } else if(view==="WiFi"&&key===1) {
    connected=false; draw();
    setTimeout(()=>{ connected=navigator.onLine; if(view==="WiFi") draw(); },450);
    fetchWeather();
  }
}
const buttons=[...document.querySelectorAll("[data-key]")];
const input=buttons.map(()=>({sources:new Set(),stable:false,changed:0}));
function updateInput(i,source,down) {
  const key=input[i], wasDown=key.sources.size>0;
  if(down) key.sources.add(source);
  else key.sources.delete(source);
  const isDown=key.sources.size>0;
  if(wasDown!==isDown) key.changed=performance.now();
  buttons[i].classList.toggle("pressed",isDown);
}
buttons.forEach((button,i)=>{
  const pointers=new Map();
  function release(id,press){
    if(pointers.get(id)!==press) return;
    clearTimeout(press.timer);
    pointers.delete(id);
    updateInput(i,press.source,false);
  }
  button.addEventListener("pointerdown",event=>{
    if(event.button!==0) return;
    const previous=pointers.get(event.pointerId);
    if(previous) release(event.pointerId,previous);
    button.setPointerCapture(event.pointerId);
    const press={source:Symbol(),start:performance.now(),released:false};
    pointers.set(event.pointerId,press);
    updateInput(i,press.source,true);
  });
  button.addEventListener("pointerup",event=>{
    const press=pointers.get(event.pointerId);
    if(!press||press.released) return;
    press.released=true;
    // Bridge short taps across the 30 ms firmware debounce and browser polling.
    const remaining=60-(performance.now()-press.start);
    if(remaining>0) press.timer=setTimeout(()=>release(event.pointerId,press),remaining);
    else release(event.pointerId,press);
  });
  for(const type of ["pointercancel","lostpointercapture"]){
    button.addEventListener(type,event=>{
      const press=pointers.get(event.pointerId);
      if(press&&!press.released) release(event.pointerId,press);
    });
  }
  button.addEventListener("click",e=>{ if(e.detail===0&&!moving) handle(i,performance.now()); });
});
const keyMap={KeyQ:0,KeyA:1,ArrowUp:2,ArrowDown:3};
for(const type of ["keydown","keyup"]) addEventListener(type,e=>{
  if(!Object.hasOwn(keyMap,e.code)||e.altKey||e.ctrlKey||e.metaKey) return;
  e.preventDefault();
  updateInput(keyMap[e.code],"keyboard",type==="keydown");
});
addEventListener("blur",()=>input.forEach((key,i)=>{
  for(const source of key.sources) updateInput(i,source,false);
}));
for(const event of ["online","offline"]) addEventListener(event,()=>{
  connected=navigator.onLine;
  if(connected) fetchWeather();
});
let lastRefresh=0;
function tick() {
  const now=performance.now();
  if(!moving) {
    let event=null;
    input.forEach((key,i)=>{
      const down=key.sources.size>0;
      if(key.stable!==down && now-key.changed>=30) {
        key.stable=down;
        if(down&&!event) event=[i,key.changed];
      }
    });
    if(event) handle(...event);
    if(!moving && view==="F1") {
      if(game.state==="Lighting"&&now-game.started>=800) {
        game.started=now;
        if(++game.lights===5) {
          game.state="Waiting";
          game.wait=1000+Math.floor(Math.random()*2001);
        }
        draw();
      } else if(game.state==="Waiting"&&now-game.started>=game.wait) {
        game.lights=0; game.state="GoPending";
        if(input[3].sources.size) {
          input[3].stable=true;
          finish("FalseStart");
        }
        draw();
        if(game.state==="GoPending") { game.go=performance.now(); game.state="Go"; }
      } else if(game.state==="Go"&&now-game.go>=10030) { finish("Timeout"); draw(); }
      else if(game.debug&&game.state==="Go"&&now-lastRefresh>=100) { lastRefresh=now; draw(); }
    } else if(!moving && view!=="Apps" && now-lastRefresh>=1000) { lastRefresh=now; draw(); }
  }
  requestAnimationFrame(tick);
}
beginPage();
tabs(0);
requestAnimationFrame(tick);
fetchWeather();
