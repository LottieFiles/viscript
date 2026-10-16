-- Ported from gitpage/static/tmath/thorvg/gl-engine/stack-fillrule.lua.
-- Trace data and render colors are preserved; structural colors follow the site theme.
local W,H=800,720
local LOOP=true
local TRACE={back={fillValid=1,strokeValid=0,convex=1,stencil=0,optPath={cmds=10,pts=17},optStrokePath={cmds=0,pts=0},optPts={{184,36},{184,124},{184,130.627},{178.627,136},{172,136},{36,136},{29.373,136},{24,130.627},{24,124},{24,36},{24,29.373},{29.373,24},{36,24},{172,24},{178.627,24},{184,29.373},{184,36}},strokePts={},fillBBox={24,24,184,136},strokeBBox={0,0,0,0},strokeRenderWidth=0,fill={vertex={184,36,184,124,183.756,126.418,183.057,128.671,181.951,130.709,180.485,132.485,178.709,133.951,176.671,135.057,174.418,135.756,172,136,36,136,33.582,135.756,31.329,135.057,29.291,133.951,27.515,132.485,26.049,130.709,24.943,128.671,24.244,126.418,24,124,24,36,24.244,33.582,24.943,31.329,26.049,29.291,27.515,27.515,29.291,26.049,31.329,24.943,33.582,24.244,36,24,172,24,174.418,24.244,176.671,24.943,178.709,26.049,180.485,27.515,181.951,29.291,183.057,31.329,183.756,33.582,184,36},index={0,1,2,0,2,3,0,3,4,0,4,5,0,5,6,0,6,7,0,7,8,0,8,9,0,9,10,0,10,11,0,11,12,0,12,13,0,13,14,0,14,15,0,15,16,0,16,17,0,17,18,0,18,19,0,19,20,0,20,21,0,21,22,0,22,23,0,23,24,0,24,25,0,25,26,0,26,27,0,27,28,0,28,29,0,29,30,0,30,31,0,31,32,0,32,33,0,33,34,0,34,35,0,35,36}},stroke={vertex={},index={}}},front={fillValid=1,strokeValid=0,convex=1,stencil=0,optPath={cmds=10,pts=17},optStrokePath={cmds=0,pts=0},optPts={{264,84},{264,172},{264,178.627},{258.627,184},{252,184},{116,184},{109.373,184},{104,178.627},{104,172},{104,84},{104,77.373},{109.373,72},{116,72},{252,72},{258.627,72},{264,77.373},{264,84}},strokePts={},fillBBox={104,72,264,184},strokeBBox={0,0,0,0},strokeRenderWidth=0,fill={vertex={264,84,264,172,263.756,174.418,263.057,176.671,261.951,178.709,260.485,180.485,258.709,181.951,256.671,183.057,254.418,183.756,252,184,116,184,113.582,183.756,111.329,183.057,109.291,181.951,107.515,180.485,106.049,178.709,104.943,176.671,104.244,174.418,104,172,104,84,104.244,81.582,104.943,79.329,106.049,77.291,107.515,75.515,109.291,74.049,111.329,72.943,113.582,72.244,116,72,252,72,254.418,72.244,256.671,72.943,258.709,74.049,260.485,75.515,261.951,77.291,263.057,79.329,263.756,81.582,264,84},index={0,1,2,0,2,3,0,3,4,0,4,5,0,5,6,0,6,7,0,7,8,0,8,9,0,9,10,0,10,11,0,11,12,0,12,13,0,13,14,0,14,15,0,15,16,0,16,17,0,17,18,0,18,19,0,19,20,0,20,21,0,21,22,0,22,23,0,23,24,0,24,25,0,25,26,0,26,27,0,27,28,0,28,29,0,29,30,0,30,31,0,31,32,0,32,33,0,33,34,0,34,35,0,35,36}},stroke={vertex={},index={}}},star={fillValid=1,strokeValid=1,convex=0,stencil=1,optPath={cmds=11,pts=10},optStrokePath={cmds=11,pts=10},optPts={{330,44},{345,83},{387,85},{355,112},{365,153},{330,130},{295,153},{305,112},{273,85},{315,83}},strokePts={{0,-60},{15,-21},{57,-19},{25,8},{35,49},{0,26},{-35,49},{-25,8},{-57,-19},{-15,-21}},fillBBox={273,44,387,153},strokeBBox={-65,-69,65,56},strokeRenderWidth=6,fill={vertex={330,44,345,83,387,85,355,112,365,153,330,130,295,153,305,112,273,85,315,83},index={0,1,2,0,2,3,0,3,4,0,4,5,0,5,6,0,6,7,0,7,8,0,8,9}},stroke={vertex={-2.8,-58.923,2.8,-61.077,12.2,-19.923,17.8,-22.077,14.857,-18.003,15.143,-23.997,56.857,-16.003,57.143,-21.997,15,-21,12.2,-19.923,14.857,-18.003,12.902,-18.096,55.065,-21.293,58.935,-16.707,23.065,5.707,26.935,10.293,57,-19,57.143,-21.997,58.935,-16.707,64.773,-21.633,22.085,8.711,27.915,7.289,32.085,49.711,37.915,48.289,25,8,23.065,5.707,22.085,8.711,21.645,6.905,36.648,46.493,33.352,51.507,1.648,23.493,-1.648,28.507,35,49,37.915,48.289,33.352,51.507,39.72,55.692,-1.648,23.493,1.648,28.507,-36.648,46.493,-33.352,51.507,0,26,1.648,23.493,-1.648,23.493,0,22.41,-32.085,49.711,-37.915,48.289,-22.085,8.711,-27.915,7.289,-35,49,-33.352,51.507,-37.915,48.289,-39.72,55.692,-23.065,5.707,-26.935,10.293,-55.065,-21.293,-58.935,-16.707,-25,8,-22.085,8.711,-23.065,5.707,-21.645,6.905,-56.857,-16.003,-57.143,-21.997,-14.857,-18.003,-15.143,-23.997,-57,-19,-58.935,-16.707,-57.143,-21.997,-64.773,-21.633,-12.2,-19.923,-17.8,-22.077,2.8,-58.923,-2.8,-61.077,-15,-21,-14.857,-18.003,-12.2,-19.923,-12.902,-18.096,0,-60,-2.8,-61.077,2.8,-61.077,0,-68.357},index={0,1,2,1,3,2,4,5,6,5,7,6,8,9,11,11,10,8,12,13,14,13,15,14,16,17,19,19,18,16,20,21,22,21,23,22,24,25,27,27,26,24,28,29,30,29,31,30,32,33,35,35,34,32,36,37,38,37,39,38,40,41,43,43,42,40,44,45,46,45,47,46,48,49,51,51,50,48,52,53,54,53,55,54,56,57,59,59,58,56,60,61,62,61,63,62,64,65,67,67,66,64,68,69,70,69,71,70,72,73,75,75,74,72,76,77,79,79,78,76}}}}
local scene=vis.scene{width=W,height=H,fps=30,loop=LOOP,
  theme={preset="pro_black",text={h1={font="CMUSerif-Roman"},h2={font="CMUSerif-Roman"},h3={font="CMUSerif-Roman"},
    text={font="CMUSerif-Roman"},code={font="CMUTypewriter-Regular"}}},
  camera={mode="fixed",view="2d",target={0,0},height=H}}

-- Layout is written in top-left pixels (y down); P maps it to the y-up world.
local function P(x,y) return {x-W/2,H/2-y} end
local function M(x,y) local p=P(x,y) return {1,0,0,p[1],0,1,0,p[2],0,0,1,0,0,0,0,1} end
local function D(dx,dy) return {dx,-dy} end
local I={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1}
-- A group whose children are already placed with P(); move the group to move them all.
local function layer(id,o) o=o or {} return scene:group{id=id,matrix=I,opacity=o.opacity} end

local function text(parent,id,s,x,y,o)
  o=o or {}
  return parent:text{id=id,text=s,point=P(x,y),size=o.size or 19,role=o.role or "code",
    fill=o.color or "foreground",align=o.align or {0.5,0.5},layer=o.layer or 40,opacity=o.opacity}
end
local function title(s)
  return scene:text{id="title",text=s,point=P(28,34),size=30,role="h2",fill="foreground",align={0,0.5},layer=40}
end
local function box(parent,id,x,y,w,h,o)
  o=o or {}
  return parent:rectangle{id=id,center=P(x+w/2,y+h/2),size={w,h},stroke=o.stroke or "foreground",
    fill=o.fill or "background",width=o.width or 1.5,layer=o.layer or 20,opacity=o.opacity,dash=o.dash}
end
-- A node: a square box with its label centred inside. Returns box, label.
local function node(parent,id,label,x,y,w,h,o)
  o=o or {}
  local b=box(parent,id,x,y,w,h,o)
  local t=text(parent,id..":label",label,x+w/2,y+h/2,{size=o.size,color=o.color,layer=(o.layer or 20)+20,opacity=o.opacity})
  return b,t
end
local function route(parent,id,pts,o)
  o=o or {}
  local q={}
  for i,p in ipairs(pts) do q[i]=P(p[1],p[2]) end
  return parent:route{id=id,points=q,stroke=o.stroke or "border",width=o.width or 1.5,
    tip=o.tip or 9,tail=o.tail or 0,layer=o.layer or 15,dash=o.dash,dash_offset=o.dash_offset,opacity=o.opacity}
end
local function line(parent,id,a,b,o)
  o=o or {}
  return parent:line{id=id,from=P(a[1],a[2]),to=P(b[1],b[2]),stroke=o.stroke or "border",
    width=o.width or 1.5,layer=o.layer or 15,dash=o.dash,opacity=o.opacity}
end
local function poly(parent,id,pts,o)
  o=o or {}
  local q={}
  for i,p in ipairs(pts) do q[i]=P(p[1],p[2]) end
  return parent:polygon{id=id,points=q,stroke=o.stroke or "foreground",fill=o.fill or "#00000000",
    width=o.width or 1.5,layer=o.layer or 25,opacity=o.opacity}
end
local function dot(parent,id,x,y,r,o)
  o=o or {}
  return parent:circle{id=id,center=P(x,y),radius=r,fill=o.fill or "foreground",stroke=o.stroke or "#00000000",
    width=o.width or 0,layer=o.layer or 30,opacity=o.opacity}
end
-- A token: a group placed at (x,y) carrying a filled box and its label; move the group.
local function token(id,label,x,y,w,h,o)
  o=o or {}
  local g=scene:group{id=id,matrix=M(x,y),opacity=o.opacity}
  g:rectangle{id=id..":box",center={0,0},size={w,h},fill=o.fill or "accent",stroke=o.stroke or "#00000000",
    width=o.width or 0,layer=o.layer or 50}
  if label then
    g:text{id=id..":label",text=label,point={0,0},size=o.size or 16,role="code",
      fill=o.color or "background",align={0.5,0.5},layer=(o.layer or 50)+5}
  end
  return g
end
local function shift(target,dx,dy,dur,curve) scene:shift(target,D(dx,dy),dur,curve or "ease_in_out") end
-- A token that remembers where it is, so moves are written as destinations.
local function mover(g,x,y) return {g=g,x=x,y=y} end
local function goTo(m,x,y,dur,curve)
  scene:shift(m.g,D(x-m.x,y-m.y),dur,curve or "ease_in_out")
  m.x,m.y=x,y
end
local function to(m,x,y) local d={target=m.g,shift=D(x-m.x,y-m.y)} m.x,m.y=x,y return d end
local function S(target,dx,dy) return {target=target,shift=D(dx,dy)} end
local function F(target,role) return {target=target,fill=role} end
local function K(target,role) return {target=target,stroke=role} end
local function O(target,v) return {target=target,opacity=v} end
local function FK(target,fill,stroke) return {target=target,fill=fill,stroke=stroke} end

-- The example: back, front (alpha 192), star with a 6 px miter stroke, on a 400x208 canvas.
-- Outlines come from TRACE (GlGeometry fill vertices are emitted in outline order).
local EX={back="#326edc",front="#f08c3c",frontA=192/255,star="#28a06e",stroke="foreground"}
local function pairsOf(v) local q={} for i=1,#v,2 do q[#q+1]={v[i],v[i+1]} end return q end
local function mapPts(pts,x,y,s,ox,oy)
  local q={}
  for i,p in ipairs(pts) do q[i]={x+(p[1]-(ox or 0))*s,y+(p[2]-(oy or 0))*s} end
  return q
end
local function example(parent,id,x,y,s,o)
  o=o or {}
  local L=o.layer or 20
  local r={}
  local h=o.hidden and 0 or nil
  r.frame=box(parent,id..":frame",x,y,400*s,208*s,{stroke=o.frameStroke or "border",fill=o.bg or "background",layer=L})
  r.back=poly(parent,id..":back",mapPts(pairsOf(TRACE.back.fill.vertex),x,y,s),{fill=EX.back,stroke="#00000000",width=0,layer=L+2,opacity=h})
  r.front=poly(parent,id..":front",mapPts(pairsOf(TRACE.front.fill.vertex),x,y,s),{fill=EX.front,stroke="#00000000",width=0,layer=L+3,opacity=h or EX.frontA})
  r.star=poly(parent,id..":star",mapPts(TRACE.star.optPts,x,y,s),{fill=EX.star,stroke="#00000000",width=0,layer=L+4,opacity=h})
  r.stroke=poly(parent,id..":stroke",mapPts(TRACE.star.optPts,x,y,s),{stroke=EX.stroke,width=6*s,layer=L+5,opacity=h})
  return r
end
-- Signed area of a triangle in y-down pixels; ThorVG's star fan has front faces where this is negative.
local function cross3(a,b,c) return (b[1]-a[1])*(c[2]-a[2])-(b[2]-a[2])*(c[1]-a[1]) end

-- Shared closed five-point path for tessellation and FillRule comparison.
local function fillRuleStarPoints(radius)
  radius=radius or 1
  local points={}
  for k=0,4 do
    local a=math.rad(-90-k*144)
    points[k+1]={radius*math.cos(a),radius*math.sin(a)}
  end
  return points
end

-- Tracks of beats played on one clock. Each track is a list of beats (descriptor lists); the
-- longest track sets the length and shorter ones repeat inside it. Tracks must end as they start.
local function runTracks(tracks,beat)
  local len=0
  for _,t in ipairs(tracks) do if #t>len then len=#t end end
  for b=1,len do
    local q={}
    for _,t in ipairs(tracks) do
      local k=b-1
      if k<math.floor(len/#t)*#t then for _,x in ipairs(t[k%#t+1]) do q[#q+1]=x end end
    end
    if #q>0 then scene:play(q,beat) else scene:wait(beat) end
  end
end

-- Call tree on the left: {depth, call, file:line, section of the post}; act[i] lists the beats run at row i.
local NX,IND,RH=52,22,40
local function callTree(R,y0)
  local rows,parents={},{}
  local y=y0
  for i,r in ipairs(R) do
    local d=r[1]
    local x=NX+d*IND
    local row={depth=d,y=y}
    row.name=text(scene,"r:"..i,r[2],x,y,{size=d==0 and 16 or 13,color=d==0 and "foreground" or "muted",align={0,0.5}})
    text(scene,"r:"..i..":file",r[3],x,y+(d==0 and 19 or 17),{size=10,color="muted",align={0,0.5}})
    if d>0 then
      local p=parents[d-1]
      local px=NX+(d-1)*IND+6
      line(scene,"r:"..i..":v",{px,p.y+22},{px,y},{stroke="border",width=1})
      line(scene,"r:"..i..":h",{px,y},{x-4,y},{stroke="border",width=1})
    end
    parents[d]=row
    rows[i]=row
    y=y+RH
  end
  -- dashed boundary around each run of rows that share the same r[4]
  local i=1
  while i<=#R do
    local g=R[i][4]
    if g then
      local j,dmin=i,R[i][1]
      while j<#R and R[j+1][4]==g do j=j+1 dmin=math.min(dmin,R[j][1]) end
      local x=NX+dmin*IND-10
      local top,bot=rows[i].y-12,rows[j].y+25
      box(scene,"r:grp"..i,x,top,424-x,bot-top,{stroke="accent",width=1.2,dash={5,4},opacity=0.7,layer=10})
      i=j+1
    else i=i+1 end
  end
  return rows
end
local function cat(...) local r={} for _,l in ipairs({...}) do for _,x in ipairs(l) do r[#r+1]=x end end return r end
-- One cursor walks the rows; the first beat of a row moves the cursor, its other beats play the panel.
local function walk(rows,act,reset,beat)
  local cursor=line(scene,"cursor",{46,rows[1].y-10},{46,rows[1].y+10},{stroke="accent",width=3,opacity=0,layer=45})
  local T={}
  for i,row in ipairs(rows) do
    local beats=act[i] or {{}}
    for b,q in ipairs(beats) do
      local d=cat(q)
      if b==1 then
        if i==1 then d[#d+1]=O(cursor,1) else d[#d+1]=S(cursor,0,row.y-rows[i-1].y) d[#d+1]=F(rows[i-1].name,"foreground") end
        d[#d+1]=F(row.name,"accent")
      end
      T[#T+1]=d
    end
  end
  T[#T+1]={F(rows[#rows].name,"foreground")}
  T[#T+1]={}
  T[#T+1]={}
  local r=cat(reset,{{target=cursor,shift=D(0,-(rows[#rows].y-rows[1].y)),opacity=0}})
  for _,row in ipairs(rows) do r[#r+1]=F(row.name,row.depth==0 and "foreground" or "muted") end
  T[#T+1]=r
  T[#T+1]={}
  runTracks({T},beat or 0.4)
end
-- panel frame on the right
local function panel(id,label,x,y,w,h)
  box(scene,id,x,y,w,h,{stroke="border"})
  text(scene,id..":name",label,x+10,y+14,{size=12,color="muted",align={0,0.5}})
end
local function lit(t) return {target=t,fill="foreground",opacity=1} end

-- height: 720
-- loop: true
title("FillRule")
text(scene,"head:file","tvgGlRenderTask.cpp:155",780,34,{size=14,color="muted",align={1,0.5}})
line(scene,"head:rule",{20,64},{780,64},{stroke="foreground",width=1.5})

-- Left: stencil pass over a self-crossing pentagram, one cell = one pixel sample ------------
panel("L","stencil pass  INCR_WRAP / DECR_WRAP",20,84,390,616)
local CS,GX,GY,N=34,35,150,11
for j=0,N-1 do for i=0,N-1 do box(scene,"G:c"..i..":"..j,GX+i*CS,GY+j*CS,CS,CS,{stroke="border",width=0.6,layer=21}) end end
-- moveTo + 4 lineTo, each vertex two points ahead on the circle: edges cross
local P={}
for k,p in ipairs(fillRuleStarPoints()) do
  P[k]={GX+N*CS/2+4.9*CS*p[1],GY+N*CS/2+0.3*CS+4.9*CS*p[2]}
end
poly(scene,"G:out",P,{stroke="foreground",width=1.4,layer=34})
for k=1,5 do text(scene,"G:pl"..k,tostring(k-1),P[k][1]+(k==1 and 0 or (P[k][1]>GX+N*CS/2 and 12 or -12)),P[k][2]+(k==1 and -12 or 6),{size=11,color="muted"}) end
local pathT=text(scene,"L:path","moveTo 0, lineTo 1 2 3 4, close",36,118,{size=12,color="muted",align={0,0.5}})

local tris={{P[1],P[2],P[3]},{P[1],P[3],P[4]},{P[1],P[4],P[5]}}
local function inTri(x,y,q)
  local function s(p,r) return (r[1]-p[1])*(y-p[2])-(r[2]-p[2])*(x-p[1]) end
  local a,b,c=s(q[1],q[2]),s(q[2],q[3]),s(q[3],q[1])
  return (a>=0 and b>=0 and c>=0) or (a<=0 and b<=0 and c<=0)
end
local triP,val,cur={},{},{}
for t=1,3 do
  -- The GL view matrix reverses Y; GL_CCW makes a negative screen-space area front-facing.
  local back=cross3(tris[t][1],tris[t][2],tris[t][3])>0
  triP[t]=poly(scene,"G:t"..t,tris[t],{fill=back and "accent" or "muted",stroke="foreground",width=1,layer=26,opacity=0})
  val[t]={}
  for j=0,N-1 do for i=0,N-1 do
    local cx,cy=GX+(i+0.5)*CS,GY+(j+0.5)*CS
    if inTri(cx,cy,tris[t]) then
      local k=i..":"..j
      local old=cur[k] or {v=0}
      local v=old.v+(back and -1 or 1)
      local o={v=v,i=i,j=j,prev=old.obj}
      if v~=0 then o.obj=text(scene,"G:v"..t..":"..k,tostring(v),cx,cy,{size=14,color=v==2 and "accent" or "foreground",layer=44,opacity=0}) end
      cur[k]=o
      val[t][#val[t]+1]=o
    end
  end end
end
-- vertex order of each fan triangle as seen on screen: CCW is front (glFrontFace(GL_CCW)), CW is back
local wind,tcnt={},{}
for t=1,3 do
  local q=tris[t]
  local back=cross3(q[1],q[2],q[3])>0
  local cx,cy=(q[1][1]+q[2][1]+q[3][1])/3,(q[1][2]+q[2][2]+q[3][2])/3
  local function inset(p) return {p[1]+(cx-p[1])*0.28,p[2]+(cy-p[2])*0.28} end
  local c=back and "accent" or "foreground"
  wind[t]=layer("L:w"..t,{opacity=0})
  for e=1,3 do
    local a,b=inset(q[e]),inset(q[e%3+1])
    local m={a[1]+(b[1]-a[1])*0.12,a[2]+(b[2]-a[2])*0.12}
    local n={a[1]+(b[1]-a[1])*0.88,a[2]+(b[2]-a[2])*0.88}
    route(wind[t],"L:w"..t..e,{m,n},{stroke=c,width=2,tip=9,layer=48})
  end
  -- Winding is named in the caption below; keep the sample values clear inside the triangle.
  tcnt[t]=text(scene,"L:n"..t,string.format("triangle %d/3  %s -> %s",t,back and "CW" or "CCW",back and "back -> DECR_WRAP -1" or "front -> INCR_WRAP +1"),36,560,{size=12,color=c,align={0,0.5},opacity=0})
end
text(scene,"L:ff","glFrontFace(GL_CCW)",36,590,{size=12,color="muted",align={0,0.5}})

-- Right: two cover passes over the same stencil values ---------------------------------------
local PX,PW=430,350
local MC=14
local MX=PX+PW/2-N*MC/2
local function mini(id,y0,keep,label,f1,f2)
  panel(id,label,PX,y0,PW,300)
  text(scene,id..":a",f1,PX+14,y0+40,{size=12,align={0,0.5}})
  text(scene,id..":b",f2,PX+14,y0+62,{size=12,align={0,0.5}})
  for j=0,N-1 do for i=0,N-1 do box(scene,id..":c"..i..":"..j,MX+i*MC,y0+86+j*MC,MC,MC,{stroke="border",width=0.5,layer=21}) end end
  local mp={}
  for k=1,5 do mp[k]={MX+(P[k][1]-GX)/CS*MC,y0+86+(P[k][2]-GY)/CS*MC} end
  poly(scene,id..":out",mp,{stroke="foreground",width=1,layer=34})
  local g=layer(id..":fill",{opacity=0})
  for j=0,N-1 do for i=0,N-1 do
    local o=cur[i..":"..j]
    if o and keep(o.v) then box(g,id..":f"..i..":"..j,MX+i*MC,y0+86+j*MC,MC,MC,{fill=EX.star,stroke="background",width=0.5,layer=22}) end
  end end
  return g
end
local nz=mini("NZ",84,function(v) return v~=0 end,"FillNonZero","stencilFunc(GL_NOTEQUAL, 0x0, 0xFF)","stencil != 0  ->  1 and 2 drawn")
local eo=mini("EO",400,function(v) return v%2==1 end,"FillEvenOdd","stencilFunc(GL_NOTEQUAL, 0x00, 0x01)","stencil & 1 != 0  ->  2 skipped")

for t=1,3 do
  -- Clear the outgoing caption before the next one appears in the same position.
  if t>1 then scene:play({O(tcnt[t-1],0)},0.12) end
  local w={O(triP[t],0.4),O(tcnt[t],1),O(wind[t],1)}
  if t>1 then w[#w+1]=O(triP[t-1],0) w[#w+1]=O(tcnt[t-1],0) w[#w+1]=O(wind[t-1],0) end
  scene:play(w,t>1 and 0.38 or 0.5)
  scene:wait(0.5)
  local q={}
  local leave={}
  for _,o in ipairs(val[t]) do
    if o.obj then q[#q+1]=O(o.obj,1) end
    if o.prev then leave[#leave+1]=O(o.prev,0) end
  end
  if #leave>0 then scene:play(leave,0.12) end
  scene:play(q,#leave>0 and 0.48 or 0.6)
  scene:wait(0.6)
end
local T={}
T[#T+1]={O(triP[3],0),O(tcnt[3],0),O(wind[3],0)}
T[#T+1]={O(nz,1)}
T[#T+1]={}
T[#T+1]={O(eo,1)}
T[#T+1]={}
T[#T+1]={}
local r={O(nz,0),O(eo,0)}
for t=1,3 do for _,o in ipairs(val[t]) do if o.obj then r[#r+1]=O(o.obj,0) end end end
T[#T+1]=r
T[#T+1]={}
runTracks({T},0.6)

return scene
