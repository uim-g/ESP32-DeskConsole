const canvas = document.querySelector("canvas");
const context = canvas.getContext("2d", {alpha:false});
const image = context.createImageData(320,240);
const bytes = image.data;
const badge = document.querySelector("#led");
const buttons = [...document.querySelectorAll("[data-key]")];
const inputs = buttons.map(()=>new Set());
const weather = {data:null,version:0,stale:false};
const storagePrefix = "deskconsole:"+location.pathname+":";
const desk = {
  keys:0,
  weather,
  pollInput(ready){ samplePointerButtons(ready); },
  render(ptr,on,heap){
    const start=ptr>>1;
    for(let i=0,j=0;i<320*240;i++,j+=4){
      const pixel=on?heap[start+i]:0;
      const r=pixel>>11,g=(pixel>>5)&63,b=pixel&31;
      bytes[j]=(r<<3)|(r>>2);
      bytes[j+1]=(g<<2)|(g>>4);
      bytes[j+2]=(b<<3)|(b>>2);
      bytes[j+3]=255;
    }
    context.putImageData(image,0,0);
  },
  led(r,g,b){
    badge.hidden=false;
    badge.style.backgroundColor=r||g||b?`rgb(${r*30},${g*30},${b*30})`:"#353e46";
  },
  read(group,key,ptr,size,heap){
    try {
      const raw=localStorage.getItem(storagePrefix+group+":"+key);
      if(!raw) return 0;
      const data=atob(raw);
      if(ptr){
        if(data.length>size) return 0;
        for(let i=0;i<data.length;i++) heap[ptr+i]=data.charCodeAt(i);
      }
      return data.length;
    } catch { return 0; }
  },
  write(group,key,data){
    try {
      localStorage.setItem(storagePrefix+group+":"+key,btoa(String.fromCharCode(...data)));
      return data.length;
    } catch { return 0; }
  },
  weatherValue(field,index){
    return index<0?weather.data?.current?.[field]:weather.data?.daily?.[field]?.[index];
  },
  async startWeather(){
    const status=document.querySelector("#weather");
    try {
      const query=new URLSearchParams({
        latitude:"2.945",longitude:"101.874",timezone:"Asia/Kuala_Lumpur",forecast_days:"3",
        current:"temperature_2m,apparent_temperature,relative_humidity_2m,surface_pressure,wind_speed_10m,precipitation,weather_code",
        daily:"weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,sunrise,sunset"
      });
      const response=await fetch("https://api.open-meteo.com/v1/forecast?"+query,{signal:AbortSignal.timeout(9000)});
      if(!response.ok) throw Error("Weather HTTP "+response.status);
      const data=await response.json();
      if(!data.current || !Array.isArray(data.daily?.time)) throw Error("Weather response incomplete");
      weather.data=data;
      weather.version++;
      weather.stale=false;
      status.textContent="士毛月天气 / Semenyih weather · Open-Meteo";
    } catch {
      weather.stale=true;
      status.textContent=weather.data?"天气更新失败，显示缓存 / Weather update failed. Showing cached data.":"天气暂不可用，30 秒后重试 / Weather unavailable. Retrying in 30 seconds.";
    }
    setTimeout(()=>this.startWeather(),weather.stale?30000:900000);
  }
};
function input(i,source,down){
  if(down) inputs[i].add(source);
  else inputs[i].delete(source);
  buttons[i].classList.toggle("pressed",inputs[i].size>0);
  desk.keys=inputs.reduce((mask,keys,index)=>keys.size?mask|(1<<index):mask,0);
}
const samplePointerButtons=createPointerButtons(buttons,input);
buttons.forEach((button,i)=>{
  button.addEventListener("click",e=>{
    if(e.detail!==0) return;
    input(i,"activation",true);
    setTimeout(()=>input(i,"activation",false),80);
  });
});
const keyMap={KeyQ:0,KeyA:1,ArrowUp:2,ArrowDown:3};
for(const type of ["keydown","keyup"]) addEventListener(type,event=>{
  if(!Object.hasOwn(keyMap,event.code)) return;
  if(type==="keydown" && (event.ctrlKey||event.altKey||event.metaKey)) return;
  event.preventDefault();
  input(keyMap[event.code],"keyboard",type==="keydown");
});
addEventListener("blur",()=>inputs.forEach((keys,i)=>{
  for(const source of keys) input(i,source,false);
}));
