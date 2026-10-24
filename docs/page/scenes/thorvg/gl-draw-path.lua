-- Ported from gitpage/static/tmath/thorvg/gl-engine/draw-branch.lua.
-- Trace data and render colors are preserved; structural colors follow the site theme.
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
title("GPU Draw Path Selection")
text(scene,"head:file","tvgGlRenderer.cpp:309",780,34,{size=14,color="muted",align={1,0.5}})
line(scene,"head:rule",{20,64},{780,64},{stroke="foreground",width=1.5})

-- Left: four inputs --------------------------------------------------------------------------
local function starPts(cx,cy,s)
  local q={}
  for i,p in ipairs(TRACE.star.optPts) do q[i]={cx+(p[1]-330)*s,cy+(p[2]-98)*s} end
  return q
end
local cards,play={},{}
local defs={{"back  fill  Color","rect"},{"star  fill  Color","fill"},{"star  stroke  a=255","stroke"},{"star  LinearGradient","grad"}}
for k,d in ipairs(defs) do
  local y=84+(k-1)*154
  cards[k]=box(scene,"C:"..k,20,y,210,142,{stroke="border"})
  text(scene,"C:"..k..":t",d[1],32,y+16,{size=11,color="muted",align={0,0.5}})
  play[k]=poly(scene,"C:"..k..":p",{{206,y+10},{218,y+17},{206,y+24}},{fill="muted",stroke="muted",width=0.5,layer=30})
  local cx,cy=125,y+82
  if d[2]=="rect" then
    box(scene,"C:"..k..":s",cx-60,cy-36,120,80,{fill=EX.back,stroke="foreground",width=1,layer=24})
  elseif d[2]=="fill" then
    poly(scene,"C:"..k..":s",starPts(cx,cy,0.9),{fill=EX.star,stroke="foreground",width=1,layer=24})
  elseif d[2]=="stroke" then
    poly(scene,"C:"..k..":s",starPts(cx,cy,0.9),{stroke=EX.stroke,width=4,layer=24})
  else
    local p=starPts(cx,cy,0.9)
    poly(scene,"C:"..k..":s",p,{fill=EX.star,stroke="foreground",width=1,layer=24})
    line(scene,"C:"..k..":g",{cx-50,cy},{cx+50,cy},{stroke="accent",width=2,layer=30})
  end
end

-- Right: the branches ------------------------------------------------------------------------
local groups,centers={},{}
local function N(id,label,x,y,w,h,sz)
  h=h or 30
  local cx,cy=x+w/2,y+h/2
  local g=scene:group{id="N:"..id..":group",matrix=M(cx,cy)}
  groups[id],centers[id]=g,{cx,cy}
  local b=g:rectangle{id="N:"..id,center={0,0},size={w,h},fill="background",stroke="border",width=1.5,layer=20}
  g:text{id="N:"..id..":label",text=label,point={0,0},size=sz or 11,role="code",fill="foreground",align={0.5,0.5},layer=40}
  return b
end
local n={}
n.rs=N("rs","renderShape()",420,84,200)
n.q=N("q","rshape->fill ?",420,140,200)
n.fa=N("fa","drawPrimitive(Fill*)",265,206,190)
n.ga=N("ga","RT_LinGradient task",265,266,190)
n.fb=N("fb","drawPrimitive(Color)",480,206,240)
n.sb=N("sb","!stroke && Normal && None && !clips",470,266,260,30,10)
n.pt=N("pt","createPrimitiveTask(RT_Color)",480,332,240)
n.sk=N("sk","stroke && a == 255  ->  None",480,392,240)
n.md=N("md","stencilMode",340,462,280)
n.o3=N("o3","GlStencilCoverTask",265,566,170,36)
n.o2=N("o2","GlRenderTask",455,566,150,36)
n.o1=N("o1","GlSolidBatch",625,566,150,36)
local routes={}
local function R(id,pts) routes[tonumber(id)]=route(scene,"R:"..id,pts,{stroke="border",width=1.2,layer=16}) end
R("1",{{520,114},{520,140}})
R("2",{{470,170},{470,188},{360,188},{360,206}})
R("3",{{580,170},{580,188},{600,188},{600,206}})
R("4",{{360,236},{360,266}})
R("5",{{600,236},{600,266}})
R("6",{{600,296},{600,332}})
R("7",{{600,362},{600,392}})
R("8",{{360,296},{360,440},{430,440},{430,462}})
R("9",{{600,422},{600,440},{560,440},{560,462}})
R("10",{{730,281},{755,281},{755,566}})
R("11",{{420,492},{420,530},{350,530},{350,566}})
R("12",{{540,492},{540,530},{530,530},{530,566}})
text(scene,"L:y","yes",750,300,{size=10,color="muted",align={1,0.5}})
text(scene,"L:n","no",608,314,{size=10,color="muted",align={0,0.5}})
text(scene,"L:g","gradient",404,180,{size=10,color="muted",align={1,0.5}})
text(scene,"L:c","color",620,180,{size=10,color="muted",align={0,0.5}})
text(scene,"L:f","Fill*",344,548,{size=10,color="muted",align={1,0.5}})
text(scene,"L:z","None",536,548,{size=10,color="muted",align={0,0.5}})
local paths={
  {"rs","q","fb","sb","o1"},
  {"rs","q","fb","sb","pt","sk","md","o3"},
  {"rs","q","fb","sb","pt","sk","md","o2"},
  {"rs","q","fa","ga","md","o3"},
}
local pathRoutes={
  {1,3,5,10},
  {1,3,5,6,7,9,11},
  {1,3,5,6,7,9,12},
  {1,2,4,8,11},
}
-- Buttons sample the completed selection, then play only the 0.36-second scale clip.
-- Each case occupies 0.60 seconds; index.ts starts playback after its 0.20-second setup.
for k,path in ipairs(paths) do
  local selected,selectedRoutes={},{}
  for _,id in ipairs(path) do selected[id]=true end
  for _,id in ipairs(pathRoutes[k]) do selectedRoutes[id]=true end
  local setup={}
  for j=1,#cards do
    setup[#setup+1]=FK(cards[j],j==k and "surface" or "background",j==k and "accent" or "border")
    setup[#setup+1]=F(play[j],j==k and "accent" or "muted")
  end
  for _,id in ipairs({"rs","q","fa","ga","fb","sb","pt","sk","md","o3","o2","o1"}) do
    setup[#setup+1]=FK(n[id],selected[id] and "surface" or "background",selected[id] and "accent" or "border")
  end
  for id,r in ipairs(routes) do setup[#setup+1]=K(r,selectedRoutes[id] and "accent" or "border") end
  scene:tween(setup,{duration = 0.20})
  local grow,settle={},{}
  for _,id in ipairs(path) do
    local cx,cy=centers[id][1],centers[id][2]
    local x,y=cx-W/2,H/2-cy
    grow[#grow+1]={target=groups[id],transform={1.035,0,0,x,0,1.035,0,y,0,0,1,0,0,0,0,1}}
    settle[#settle+1]={target=groups[id],transform=M(cx,cy)}
  end
  scene:tween(grow,{duration = 0.18, curve = "ease_out"})
  scene:tween(settle,{duration = 0.18, curve = "ease_in_out"})
  scene:wait(0.04)
end

return scene
