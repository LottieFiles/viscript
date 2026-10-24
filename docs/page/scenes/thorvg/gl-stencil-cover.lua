-- Adapted from gitpage/static/tmath/thorvg/gl-engine/init-stencil-cover.lua.
-- The ten-point outline matches CPU: SwShapeTask (outer radius 66, inner radius 28).
-- Fan triangles, stencil values, bounds and covered samples are derived from that path.
local W,H=800,720
local LOOP=false
local TRACE={back={fillValid=1,strokeValid=0,convex=1,stencil=0,optPath={cmds=10,pts=17},optStrokePath={cmds=0,pts=0},optPts={{184,36},{184,124},{184,130.627},{178.627,136},{172,136},{36,136},{29.373,136},{24,130.627},{24,124},{24,36},{24,29.373},{29.373,24},{36,24},{172,24},{178.627,24},{184,29.373},{184,36}},strokePts={},fillBBox={24,24,184,136},strokeBBox={0,0,0,0},strokeRenderWidth=0,fill={vertex={184,36,184,124,183.756,126.418,183.057,128.671,181.951,130.709,180.485,132.485,178.709,133.951,176.671,135.057,174.418,135.756,172,136,36,136,33.582,135.756,31.329,135.057,29.291,133.951,27.515,132.485,26.049,130.709,24.943,128.671,24.244,126.418,24,124,24,36,24.244,33.582,24.943,31.329,26.049,29.291,27.515,27.515,29.291,26.049,31.329,24.943,33.582,24.244,36,24,172,24,174.418,24.244,176.671,24.943,178.709,26.049,180.485,27.515,181.951,29.291,183.057,31.329,183.756,33.582,184,36},index={0,1,2,0,2,3,0,3,4,0,4,5,0,5,6,0,6,7,0,7,8,0,8,9,0,9,10,0,10,11,0,11,12,0,12,13,0,13,14,0,14,15,0,15,16,0,16,17,0,17,18,0,18,19,0,19,20,0,20,21,0,21,22,0,22,23,0,23,24,0,24,25,0,25,26,0,26,27,0,27,28,0,28,29,0,29,30,0,30,31,0,31,32,0,32,33,0,33,34,0,34,35,0,35,36}},stroke={vertex={},index={}}},front={fillValid=1,strokeValid=0,convex=1,stencil=0,optPath={cmds=10,pts=17},optStrokePath={cmds=0,pts=0},optPts={{264,84},{264,172},{264,178.627},{258.627,184},{252,184},{116,184},{109.373,184},{104,178.627},{104,172},{104,84},{104,77.373},{109.373,72},{116,72},{252,72},{258.627,72},{264,77.373},{264,84}},strokePts={},fillBBox={104,72,264,184},strokeBBox={0,0,0,0},strokeRenderWidth=0,fill={vertex={264,84,264,172,263.756,174.418,263.057,176.671,261.951,178.709,260.485,180.485,258.709,181.951,256.671,183.057,254.418,183.756,252,184,116,184,113.582,183.756,111.329,183.057,109.291,181.951,107.515,180.485,106.049,178.709,104.943,176.671,104.244,174.418,104,172,104,84,104.244,81.582,104.943,79.329,106.049,77.291,107.515,75.515,109.291,74.049,111.329,72.943,113.582,72.244,116,72,252,72,254.418,72.244,256.671,72.943,258.709,74.049,260.485,75.515,261.951,77.291,263.057,79.329,263.756,81.582,264,84},index={0,1,2,0,2,3,0,3,4,0,4,5,0,5,6,0,6,7,0,7,8,0,8,9,0,9,10,0,10,11,0,11,12,0,12,13,0,13,14,0,14,15,0,15,16,0,16,17,0,17,18,0,18,19,0,19,20,0,20,21,0,21,22,0,22,23,0,23,24,0,24,25,0,25,26,0,26,27,0,27,28,0,28,29,0,29,30,0,30,31,0,31,32,0,32,33,0,33,34,0,34,35,0,35,36}},stroke={vertex={},index={}}},star={fillValid=1,strokeValid=1,convex=0,stencil=1,optPath={cmds=11,pts=10},optStrokePath={cmds=11,pts=10},optPts={{330,44},{345,83},{387,85},{355,112},{365,153},{330,130},{295,153},{305,112},{273,85},{315,83}},strokePts={{0,-60},{15,-21},{57,-19},{25,8},{35,49},{0,26},{-35,49},{-25,8},{-57,-19},{-15,-21}},fillBBox={273,44,387,153},strokeBBox={-65,-69,65,56},strokeRenderWidth=6,fill={vertex={330,44,345,83,387,85,355,112,365,153,330,130,295,153,305,112,273,85,315,83},index={0,1,2,0,2,3,0,3,4,0,4,5,0,5,6,0,6,7,0,7,8,0,8,9}},stroke={vertex={-2.8,-58.923,2.8,-61.077,12.2,-19.923,17.8,-22.077,14.857,-18.003,15.143,-23.997,56.857,-16.003,57.143,-21.997,15,-21,12.2,-19.923,14.857,-18.003,12.902,-18.096,55.065,-21.293,58.935,-16.707,23.065,5.707,26.935,10.293,57,-19,57.143,-21.997,58.935,-16.707,64.773,-21.633,22.085,8.711,27.915,7.289,32.085,49.711,37.915,48.289,25,8,23.065,5.707,22.085,8.711,21.645,6.905,36.648,46.493,33.352,51.507,1.648,23.493,-1.648,28.507,35,49,37.915,48.289,33.352,51.507,39.72,55.692,-1.648,23.493,1.648,28.507,-36.648,46.493,-33.352,51.507,0,26,1.648,23.493,-1.648,23.493,0,22.41,-32.085,49.711,-37.915,48.289,-22.085,8.711,-27.915,7.289,-35,49,-33.352,51.507,-37.915,48.289,-39.72,55.692,-23.065,5.707,-26.935,10.293,-55.065,-21.293,-58.935,-16.707,-25,8,-22.085,8.711,-23.065,5.707,-21.645,6.905,-56.857,-16.003,-57.143,-21.997,-14.857,-18.003,-15.143,-23.997,-57,-19,-58.935,-16.707,-57.143,-21.997,-64.773,-21.633,-12.2,-19.923,-17.8,-22.077,2.8,-58.923,-2.8,-61.077,-15,-21,-14.857,-18.003,-12.2,-19.923,-12.902,-18.096,0,-60,-2.8,-61.077,2.8,-61.077,0,-68.357},index={0,1,2,1,3,2,4,5,6,5,7,6,8,9,11,11,10,8,12,13,14,13,15,14,16,17,19,19,18,16,20,21,22,21,23,22,24,25,27,27,26,24,28,29,30,29,31,30,32,33,35,35,34,32,36,37,38,37,39,38,40,41,43,43,42,40,44,45,46,45,47,46,48,49,51,51,50,48,52,53,54,53,55,54,56,57,59,59,58,56,60,61,62,61,63,62,64,65,67,67,66,64,68,69,70,69,71,70,72,73,75,75,74,72,76,77,79,79,78,76}}}}
local scene=vis.scene{width=W,height=H,loop=LOOP,
  theme={text={h1={font="CMUSerif-Roman"},h2={font="CMUSerif-Roman"},h3={font="CMUSerif-Roman"},
    text={font="CMUSerif-Roman"},code={font="CMUTypewriter-Regular"}}},
  camera={height=H}}

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
local function shift(target,dx,dy,dur,curve) scene:shift(target,D(dx,dy),{duration = dur, curve = curve or "ease_in_out"}) end
-- A token that remembers where it is, so moves are written as destinations.
local function mover(g,x,y) return {g=g,x=x,y=y} end
local function goTo(m,x,y,dur,curve)
  scene:shift(m.g,D(x-m.x,y-m.y),{duration = dur, curve = curve or "ease_in_out"})
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
    if #q>0 then scene:tween(q,{duration = beat}) else scene:wait(beat) end
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
title("GlStencilCoverTask")
text(scene,"head:file","tvgGlRenderTask.cpp:155",780,34,{size=14,color="muted",align={1,0.5}})
line(scene,"head:rule",{20,64},{780,64},{stroke="foreground",width=1.5})
local viewport,N,domain,GY=160,32,158,150
local S=domain/N
local BANDS=8
local xs={20,213,406,599}
local function gx(c) return xs[c]+(181-domain)/2 end
local labels={"RT_Stencil","depthStencilBuffer","RT_Color cover","colorBuffer"}
local tabs,heads={},{}
for i,x in ipairs(xs) do
  box(scene,"diagram:"..i,x,84,181,302,{stroke="border"})
  tabs[i]=box(scene,"tab:"..i,x,84,181,46,{stroke=i==1 and "accent" or "border",fill=i==1 and "surface" or "background",layer=23})
  heads[i]=text(scene,"stage:"..i,labels[i],x+90.5,105,{size=i==2 and 12 or 15,color=i==1 and "accent" or "foreground"})
  box(scene,"domain:"..i,gx(i),GY,N*S,N*S,{stroke="border",layer=21})
  if i<4 then route(scene,"to:"..i,{{x+181,234},{xs[i+1],234}},{stroke="foreground",tip=4,width=1}) end
end
-- The CPU example's alternating outer/inner radii, traversed in reverse winding.
-- This gives the same silhouette and a completed nonzero stencil value of one.
local points={}
local minX,minY,maxX,maxY=math.huge,math.huge,-math.huge,-math.huge
for k=0,9 do
  local a=math.rad(-90-k*36)
  local r=k%2==0 and 66 or 28
  local x,y=viewport/2+r*math.cos(a),viewport/2+r*math.sin(a)
  points[#points+1]={x,y}
  minX,minY,maxX,maxY=math.min(minX,x),math.min(minY,y),math.max(maxX,x),math.max(maxY,y)
end
local bbox={math.floor(minX),math.floor(minY),math.ceil(maxX),math.ceil(maxY)}
local function project(p,c) return {gx(c)+N*S*p[1]/viewport,GY+N*S*p[2]/viewport} end
local function inTri(x,y,a,b,c)
  local p={x,y}
  local u,v,w=cross3(a,b,p),cross3(b,c,p),cross3(c,a,p)
  return (u>=0 and v>=0 and w>=0) or (u<=0 and v<=0 and w<=0)
end
local outline={}
for i,p in ipairs(points) do outline[i]=project(p,1) end
poly(scene,"stencil:geometry",outline,{stroke="foreground",width=1.6,layer=25})
for i,p in ipairs(outline) do dot(scene,"stencil:vertex"..i,p[1],p[2],1.7,{fill="foreground",layer=30}) end
local fans={}
for t=2,#points-1 do fans[#fans+1]=poly(scene,"stencil:triangle"..t,{project(points[1],1),project(points[t],1),project(points[t+1],1)},{fill="accent",stroke="accent",width=0.7,opacity=0,layer=24}) end
local q={}
for i,p in ipairs({{bbox[1],bbox[2]},{bbox[3],bbox[2]},{bbox[3],bbox[4]},{bbox[1],bbox[4]}}) do q[i]=project(p,3) end
local quad=poly(scene,"cover:quad",q,{stroke="accent",fill="surface",width=1.2,layer=23,opacity=0})
local diagonal=line(scene,"cover:diagonal",q[1],q[3],{stroke="accent",width=0.8,layer=24,opacity=0})
local rows,cells={},{}
-- Sweep eight bands so the denser samples retain the existing selectable clip times.
for j=0,BANDS-1 do rows[j+1]={tests=layer("test:row"..j,{opacity=0}),writes=layer("write:row"..j,{opacity=0}),mask=layer("mask:row"..j,{opacity=0}),read=layer("read:row"..j,{opacity=0}),focus=layer("result:row"..j,{opacity=0}),changed={},covered=0} end
for j=0,N-1 do for i=0,N-1 do
  local x,y=(i+0.5)*viewport/N,(j+0.5)*viewport/N
  local v=0
  for t=2,#points-1 do if inTri(x,y,points[1],points[t],points[t+1]) then v=v+(cross3(points[1],points[t],points[t+1])<0 and 1 or -1) end end
  v=v%256
  local row=rows[math.floor(j*BANDS/N)+1]
  -- Show representative values at readable spacing; the mask keeps every sample.
  local old,zero
  if i%4==2 and j%4==2 then
    old=text(scene,"buffer:value:"..i..":"..j,tostring(v),gx(2)+(i+0.5)*S,GY+(j+0.5)*S,{size=10,color=v~=0 and "accent" or "muted",layer=44,opacity=v==0 and 1 or 0})
    if v~=0 then
      zero=text(scene,"buffer:zero:"..i..":"..j,"0",gx(2)+(i+0.5)*S,GY+(j+0.5)*S,{size=10,color="muted",layer=44})
      row.changed[#row.changed+1]={old=old,zero=zero}
    end
  end
  if v~=0 then
    row.covered=row.covered+1
    box(row.mask,"buffer:mask:"..i..":"..j,gx(2)+i*S,GY+j*S,S,S,{stroke="#00000000",fill="#0a66c233",width=0,layer=24})
    box(row.read,"buffer:read:"..i..":"..j,gx(2)+i*S,GY+j*S,S,S,{stroke="accent",fill="#00000000",width=0.45,layer=27})
    box(row.focus,"color:focus:"..i..":"..j,gx(4)+i*S,GY+j*S,S,S,{stroke="accent",fill="#00000000",width=0.6,layer=27})
  end
  cells[#cells+1]={old=old,zero=zero,value=v}
  if x>=bbox[1] and x<bbox[3] and y>=bbox[2] and y<bbox[4] then
    local px,py=gx(3)+i*S,GY+j*S
    box(row.tests,"test:sample:"..i..":"..j,px,py,S,S,{stroke=v~=0 and "accent" or "border",fill=v~=0 and "#0a66c233" or "#00000000",width=v~=0 and 0.5 or 0.25,layer=26})
    if v~=0 then box(row.writes,"color:sample:"..i..":"..j,gx(4)+i*S,GY+j*S,S,S,{fill=EX.star,stroke=EX.star,width=0,layer=24})
    elseif i%4==2 and j%4==2 then
      local inset=S*0.25
      line(row.tests,"test:failA:"..i..":"..j,{px+inset,py+inset},{px+S-inset,py+S-inset},{stroke="muted",width=0.65,layer=28})
      line(row.tests,"test:failB:"..i..":"..j,{px+S-inset,py+inset},{px+inset,py+S-inset},{stroke="muted",width=0.65,layer=28})
    end
  end
end end
for c=2,4 do for k=0,N do
  line(scene,"grid:v:"..c..":"..k,{gx(c)+k*S,GY},{gx(c)+k*S,GY+N*S},{stroke="border",width=k%4==0 and 0.5 or 0.25,layer=21})
  line(scene,"grid:h:"..c..":"..k,{gx(c),GY+k*S},{gx(c)+N*S,GY+k*S},{stroke="border",width=k%4==0 and 0.5 or 0.25,layer=21})
end end
text(scene,"resource:1",string.format("%d triangles",#points-2),xs[1]+12,334,{size=12,color="muted",align={0,0.5}})
text(scene,"resource:2","FillNonZero",xs[2]+12,334,{size=13,color="foreground",align={0,0.5}})
text(scene,"resource:3","fillBBox",xs[3]+12,334,{size=13,color="muted",align={0,0.5}})
text(scene,"resource:1:count",string.format("%d indices",(#points-2)*3),xs[1]+12,358,{size=12,color="muted",align={0,0.5}})
text(scene,"resource:3:count",string.format("%d vertices",(#q-2)*3),xs[3]+12,358,{size=12,color="muted",align={0,0.5}})
text(scene,"resource:4","RGBA",xs[4]+12,334,{size=13,color="foreground",align={0,0.5}})
box(scene,"details:frame",20,404,760,296,{stroke="border"})
line(scene,"details:rule",{32,454},{768,454},{stroke="border",width=1})
line(scene,"details:columns",{516,466},{516,684},{stroke="border",width=1})
local groups={}
for i=1,4 do groups[i]=layer("detail:"..i,{opacity=i==1 and 1 or 0}) end
-- The same parent function remains visible for all four selected stages.
local runCode={
  "void GlStencilCoverTask::run(...) {",
  "  state.enable(GL_STENCIL_TEST);",
  "  // ... stencil ops; colorMask(false)",
  "  ARRAY_FOREACH(p, stencilTasks)",
  "    (*p)->run(state);",
  "  // ... cover test; colorMask(true)",
  "  ARRAY_FOREACH(p, coverTasks)",
  "    (*p)->run(state);",
  "  state.disable(GL_STENCIL_TEST);",
  "}",
}
local activeLines={{2,3,4,5},{6},{7,8},{9}}
local right={
  {"stencilTasks[0]","GlRenderTask* / RT_Stencil","GlRenderTask::run(state)","{","  state.useProgram(...);","  // ... uniforms, inputs","  glDrawElements(...);","}","GL_FRONT: INCR_WRAP","GL_BACK:  DECR_WRAP"},
  {"state.stencilFunc(","  GL_NOTEQUAL, 0x0, 0xFF);","state.stencilOp(","  GL_KEEP, GL_KEEP,","  GL_REPLACE);","1 != 0: pass","0 != 0: fail","depth test: pass","GL_REPLACE: ref 0"},
  {"coverTasks[0]","GlRenderTask* / RT_Color","GlRenderTask::run(state)","{","  state.useProgram(...);","  // ... uniforms, inputs","  glDrawArrays(...);","}","fillBBox: 6 vertices","colorMask(true)"},
  {"colorBuffer: written","stencil values: 0","GL_STENCIL_TEST: off","aColor (RGBA)",string.format("(%.2f, %.2f, %.2f, %.2f)",40/255,160/255,110/255,1)},
}
for stage=1,4 do
  text(groups[stage],"detail:"..stage..":name","GlStencilCoverTask::run()",36,430,{size=17,color="foreground",align={0,0.5}})
  text(groups[stage],"detail:"..stage..":data",labels[stage],536,430,{size=stage==2 and 14 or 16,color="foreground",align={0,0.5}})
  for i,s in ipairs(runCode) do
    local active=false
    for _,n in ipairs(activeLines[stage]) do if n==i then active=true end end
    text(groups[stage],"detail:"..stage..":code"..i,s,36,470+(i-1)*23,{size=15,color=active and "accent" or "muted",align={0,0.5}})
  end
  for i,s in ipairs(right[stage]) do text(groups[stage],"detail:"..stage..":value"..i,s,536,480+(i-1)*21,{size=stage==4 and 14 or 12,color="accent",align={0,0.5}}) end
end
-- Four independently selectable stages; each case occupies 6.4 seconds.
for stage=1,4 do
  local base=(stage-1)*6.4
  local reset={O(quad,stage==4 and 1 or 0),O(diagonal,stage==4 and 1 or 0)}
  for i=1,4 do reset[#reset+1]=O(groups[i],0) end
  for _,h in ipairs(fans) do reset[#reset+1]=O(h,stage==1 and 0 or 0.12) end
  for _,c in ipairs(cells) do
    if c.zero then reset[#reset+1]=O(c.old,(stage==1 or stage==4) and 0 or 1) reset[#reset+1]=O(c.zero,(stage==1 or stage==4) and 1 or 0) end
  end
  for _,row in ipairs(rows) do
    reset[#reset+1]=O(row.mask,(stage==2 or stage==3) and 1 or 0)
    reset[#reset+1]=O(row.tests,stage==4 and 1 or 0)
    reset[#reset+1]=O(row.writes,stage==4 and 1 or 0)
    reset[#reset+1]=O(row.read,0)
    reset[#reset+1]=O(row.focus,0)
  end
  scene:tween(reset,{duration = 0.10})
  local select={O(groups[stage],1)}
  for i=1,4 do select[#select+1]=FK(tabs[i],i==stage and "surface" or "background",i==stage and "accent" or "border") select[#select+1]=F(heads[i],i==stage and "accent" or "foreground") end
  scene:tween(select,{duration = 0.10})
  if stage==1 then
    for _,h in ipairs(fans) do scene:tween(O(h,0.12),{duration = 0.28}) end
    for _,row in ipairs(rows) do
      if row.covered>0 then
        local hide,write={},{O(row.mask,1)}
        for _,c in ipairs(row.changed) do hide[#hide+1]=O(c.zero,0) write[#write+1]=O(c.old,1) end
        if #hide>0 then scene:tween(hide,{duration = 0.10}) else scene:wait(0.10) end
        scene:tween(write,{duration = 0.24})
      else scene:wait(0.34) end
    end
  elseif stage==2 then
    scene:wait(0.40)
    for _,row in ipairs(rows) do scene:tween(O(row.read,1),{duration = 0.28}) end
  elseif stage==3 then
    scene:tween({O(quad,1),O(diagonal,1)},{duration = 0.45})
    for _,row in ipairs(rows) do
      scene:tween(O(row.tests,1),{duration = 0.28})
      if row.covered>0 then
        local hide,write={O(row.mask,0)},{O(row.writes,1)}
        for _,c in ipairs(row.changed) do hide[#hide+1]=O(c.old,0) write[#write+1]=O(c.zero,1) end
        scene:tween(hide,{duration = 0.10}) scene:tween(write,{duration = 0.22})
      else scene:wait(0.32) end
    end
  else
    for _,row in ipairs(rows) do
      scene:tween(O(row.focus,1),{duration = 0.14})
      scene:tween(O(row.focus,0),{duration = 0.14})
    end
    scene:wait(0.40)
  end
  local remain=base+6.2-scene:duration()
  if remain>0 then scene:wait(remain) end
  scene:wait(0.20)
end

return scene
