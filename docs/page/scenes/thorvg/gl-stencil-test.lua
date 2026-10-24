-- Ported from gitpage/static/tmath/thorvg/gl-engine/stack-stencilmode.lua.
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
title("stencil test")
text(scene,"head:file","glStencilFunc  glStencilOp  glColorMask",780,34,{size=13,color="muted",align={1,0.5}})
line(scene,"head:rule",{20,64},{780,64},{stroke="foreground",width=1.5})

-- Left: the draw on top, then the stencil and color attachments at the same pixel coordinates --
panel("L","framebuffer  stencil + color attachments",20,84,390,616)
local NX,NY,CW,SK,CH,LX=10,8,27,9,16,40
local LV={geo=142,st=332,col=522}
local function iso(u,v,Y) return {LX+u*CW+v*SK,Y+v*CH} end
local function rect(u0,v0,u1,v1,Y) return {iso(u0,v0,Y),iso(u1,v0,Y),iso(u1,v1,Y),iso(u0,v1,Y)} end
local function plate(id,Y,label)
  poly(scene,id..":plate",rect(0,0,NX,NY,Y),{fill="surface",stroke="border",width=1,layer=20})
  text(scene,id..":name",label,LX-8,Y-14,{size=12,color="muted",align={0,0.5}})
end
plate("GEO",LV.geo,"draw call")
plate("ST",LV.st,"stencil buffer  8 bit / pixel")
plate("COL",LV.col,"color buffer  RGBA")
for j=0,NY-1 do for i=0,NX-1 do
  for _,k in ipairs({"st","col"}) do poly(scene,"G:"..k..i..":"..j,rect(i,j,i+1,j+1,LV[k]),{stroke="border",width=0.5,layer=21}) end
end end
local clr=text(scene,"L:clr","glClear  ->  0",396,LV.st+NY*CH+14,{size=11,color="accent",align={1,0.5},opacity=0})

-- pass 1 circle, pass 2 triangle, pass 3 rectangle; each in its own color
local function circ(cx,cy,r) local q={} for k=0,39 do local a=k*math.pi/20 q[#q+1]={cx+r*math.cos(a),cy+r*math.sin(a)} end return q end
local function inPoly(x,y,P)
  local c=false
  for k=1,#P do local a,b=P[k],P[k%#P+1]
    if (a[2]>y)~=(b[2]>y) and x<(b[1]-a[1])*(y-a[2])/(b[2]-a[2])+a[1] then c=not c end end
  return c
end
local SH={
  {pts=circ(3.6,4,2.8),fill=EX.back},
  {pts={{6,0.4},{9.6,7.6},{2.4,7.6}},fill=EX.star},
  {pts={{5.2,1.2},{9.6,1.2},{9.6,6.8},{5.2,6.8}},fill=EX.front},
}
local function at(P,Y) local q={} for i,p in ipairs(P) do q[i]=iso(p[1],p[2],Y) end return q end
for k,sh in ipairs(SH) do
  sh.g=poly(scene,"G:s"..k,at(sh.pts,LV.geo),{fill=sh.fill,stroke="foreground",width=1.2,layer=26,opacity=0})
  sh.s=poly(scene,"G:ss"..k,at(sh.pts,LV.st),{stroke=sh.fill,width=1.4,dash={4,3},layer=34,opacity=0})
  local lo,hi={99,99},{-99,-99}
  for _,p in ipairs(sh.pts) do lo={math.min(lo[1],p[1]),math.min(lo[2],p[2])} hi={math.max(hi[1],p[1]),math.max(hi[2],p[2])} end
  sh.d1=layer("G:da"..k,{opacity=0})
  sh.d2=layer("G:db"..k,{opacity=0})
  for m,p in ipairs({{lo[1],lo[2]},{hi[1],lo[2]},{hi[1],hi[2]},{lo[1],hi[2]}}) do
    line(sh.d1,"G:da"..k..m,iso(p[1],p[2],LV.geo),iso(p[1],p[2],LV.st),{stroke=sh.fill,width=1,dash={4,3},layer=40})
    line(sh.d2,"G:db"..k..m,iso(p[1],p[2],LV.st),iso(p[1],p[2],LV.col),{stroke=sh.fill,width=1,dash={4,3},layer=40})
  end
end

local cell={}
for j=0,NY-1 do for i=0,NX-1 do
  local q=iso(i+0.5,j+0.5,LV.st)
  local c={i=i,j=j}
  for k,sh in ipairs(SH) do c[k]=inPoly(i+0.5,j+0.5,sh.pts) end
  c.t0=text(scene,"G:z"..i..":"..j,"0",q[1],q[2],{size=9,color="muted",layer=44,opacity=0})
  if c[1] then c.t1=text(scene,"G:o"..i..":"..j,"1",q[1],q[2],{size=10,color=SH[1].fill,layer=44,opacity=0}) end
  cell[#cell+1]=c
end end
-- stencil test result per pass on the stencil plane: pass = shape color, fail = crossed
local pass,fail={},{}
for k=1,3 do pass[k]=layer("S:p"..k,{opacity=0}) fail[k]=layer("S:f"..k,{opacity=0}) end
for _,c in ipairs(cell) do
  local ok={c[1],c[2] and c[1],c[3] and not c[1]}
  for k=1,3 do
    if c[k] then
      if ok[k] then poly(pass[k],"S:p"..k..c.i..":"..c.j,rect(c.i,c.j,c.i+1,c.j+1,LV.st),{fill=SH[k].fill,stroke="#00000000",width=0,layer=30,opacity=0.4})
      else
                line(fail[k],"S:xa"..k..c.i..":"..c.j,iso(c.i+0.25,c.j+0.2,LV.st),iso(c.i+0.75,c.j+0.8,LV.st),{stroke="foreground",width=1.4,layer=46})
        line(fail[k],"S:xb"..k..c.i..":"..c.j,iso(c.i+0.75,c.j+0.2,LV.st),iso(c.i+0.25,c.j+0.8,LV.st),{stroke="foreground",width=1.4,layer=46})
      end
    end
  end
end
-- color writes per pass: circle everywhere it covers, triangle where stencil == 1, rectangle where stencil != 1
local wr={}
for k=1,3 do wr[k]=layer("C:w"..k,{opacity=0}) end
for _,c in ipairs(cell) do
  local hit={c[1],c[2] and c[1],c[3] and not c[1]}
  for k=1,3 do if hit[k] then poly(wr[k],"C:f"..k..c.i..":"..c.j,rect(c.i,c.j,c.i+1,c.j+1,LV.col),{fill=SH[k].fill,stroke="background",width=0.5,layer=22+k}) end end
end

-- Right top: the stencil test ----------------------------------------------------------
local PX,PW=430,350
panel("A","glStencilFunc(func, ref, mask)",PX,84,PW,214)
text(scene,"A:eq","(ref & mask)  func  (stencil & mask)",PX+14,118,{size=12,align={0,0.5}})
local funcs={"GL_NEVER","GL_LESS","GL_LEQUAL","GL_GREATER","GL_GEQUAL","GL_EQUAL","GL_NOTEQUAL","GL_ALWAYS"}
local fn={}
for k,f in ipairs(funcs) do
  fn[f]=text(scene,"A:f"..k,f,PX+14+((k-1)%2)*170,150+math.floor((k-1)/2)*26,{size=12,color="muted",align={0,0.5}})
end
local cur={
  text(scene,"A:c1","glStencilFunc(GL_ALWAYS, 1, 0xFF)",PX+14,272,{size=12,color=SH[1].fill,align={0,0.5},opacity=0}),
  text(scene,"A:c2","glStencilFunc(GL_EQUAL, 1, 0xFF)",PX+14,272,{size=12,color=SH[2].fill,align={0,0.5},opacity=0}),
  text(scene,"A:c3","glStencilFunc(GL_NOTEQUAL, 1, 0xFF)",PX+14,272,{size=12,color=SH[3].fill,align={0,0.5},opacity=0}),
}

-- Right middle: what one fragment passes through -------------------------------------------
panel("B","per fragment",PX,314,PW,190)
local sb={}
for k,s in ipairs({"fragment shader","stencil test","stencil op","color write"}) do sb[k]=node(scene,"B:s"..k,s,PX+14,340+(k-1)*38,150,30,{size=12,stroke="border"}) end
local function col(k,list,c)
  local r={}
  for p,s in ipairs(list) do r[p]=text(scene,"B:"..k..p,s,PX+176,355+(p-1)*38,{size=11,color=p==1 and "muted" or c,align={0,0.5},opacity=0}) end
  return r
end
local P1=col("a",{"circle","ALWAYS  ->  pass","REPLACE  ->  1","every covered pixel"},SH[1].fill)
local P2=col("b",{"triangle","stencil == 1","KEEP","where test passed"},SH[2].fill)
local P3=col("c",{"rectangle","stencil != 1","KEEP","where test passed"},SH[3].fill)

-- Right bottom: the stencil write -------------------------------------------------------
panel("C","glStencilOp(sfail, dpfail, dppass)",PX,520,PW,180)
local ops={"GL_KEEP","GL_ZERO","GL_REPLACE","GL_INVERT","GL_INCR","GL_DECR","GL_INCR_WRAP","GL_DECR_WRAP"}
local op={}
for k,o in ipairs(ops) do op[o]=text(scene,"C:o"..k,o,PX+14+((k-1)%2)*170,552+math.floor((k-1)/2)*26,{size=12,color="muted",align={0,0.5}}) end
local ocur={
  text(scene,"C:c1","glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE)",PX+14,680,{size=11,color=SH[1].fill,align={0,0.5},opacity=0}),
  text(scene,"C:c2","glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP)",PX+14,680,{size=11,color=SH[2].fill,align={0,0.5},opacity=0}),
  text(scene,"C:c3","glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP)",PX+14,680,{size=11,color=SH[3].fill,align={0,0.5},opacity=0}),
}

-- moving highlight over the selected func / op, frames around the two panels
local function fpos(k) return PX+8+((k-1)%2)*170,150+math.floor((k-1)/2)*26-12 end
local function opos(k) return PX+8+((k-1)%2)*170,552+math.floor((k-1)/2)*26-12 end
local fx,fy=fpos(8)
local hlF=box(scene,"A:hl",fx,fy,150,24,{fill="accent",stroke="accent",width=1.5,layer=30,opacity=0})
local ox,oy=opos(3)
local hlO=box(scene,"C:hl",ox,oy,150,24,{fill="accent",stroke="accent",width=1.5,layer=30,opacity=0})
local frA,frC={},{}
for k=1,3 do
  frA[k]=box(scene,"A:fr"..k,PX,84,PW,214,{fill="#00000000",stroke=SH[k].fill,width=3,layer=50,opacity=0})
  frC[k]=box(scene,"C:fr"..k,PX,520,PW,180,{fill="#00000000",stroke=SH[k].fill,width=3,layer=50,opacity=0})
end
-- shape marks next to the func / op each shape used; they stay until the loop resets
local function mark(id,k,x,y)
  local c=SH[k].fill
  if k==1 then return dot(scene,id,x,y,5,{fill=c,opacity=0,layer=48}) end
  if k==2 then return poly(scene,id,{{x,y-6},{x+6,y+5},{x-6,y+5}},{fill=c,stroke=c,width=0.5,opacity=0,layer=48}) end
  return box(scene,id,x-5,y-5,10,10,{fill=c,stroke=c,width=0.5,opacity=0,layer=48})
end
local fAt,oAt={fx,fy},{ox,oy}
local function moveF(k,c) local x,y=fpos(k) local d={target=hlF,shift=D(x-fAt[1],y-fAt[2]),opacity=0.25,fill=c,stroke=c} fAt={x,y} return d end
local function moveO(k,c) local x,y=opos(k) local d={target=hlO,shift=D(x-oAt[1],y-oAt[2]),opacity=0.25,fill=c,stroke=c} oAt={x,y} return d end
-- Timeline: each pass = shape -> stencil state -> stencil plane -> color plane ---------------
local function cat(a,b) for _,x in ipairs(b) do a[#a+1]=x end return a end
local T={}
local q={O(clr,1)}
for _,c in ipairs(cell) do q[#q+1]=O(c.t0,1) end
T[#T+1]=q
T[#T+1]={}
local P={P1,P2,P3}
local FN={fn.GL_ALWAYS,fn.GL_EQUAL,fn.GL_NOTEQUAL}
local OP={op.GL_REPLACE,op.GL_KEEP,op.GL_KEEP}
local OC={ocur[1],ocur[2],ocur[3]}
local MF,MO={},{}
for k=1,3 do
  local fx2,fy2=fpos(({8,6,7})[k])
  MF[k]=mark("A:m"..k,k,fx2+138,fy2+12)
  local ox2,oy2=opos(({3,1,1})[k])
  MO[k]=mark("C:m"..k,k,ox2+138-(k==3 and 16 or 0),oy2+12)
end
local FI,OI={8,6,7},{3,1,1}
for k=1,3 do
  -- 1. the shape
  q={O(SH[k].g,1),K(sb[1],SH[k].fill),O(P[k][1],1),O(clr,0)}
  if k>1 then
    cat(q,{O(SH[k-1].g,0),O(SH[k-1].d2,0),O(pass[k-1],0),O(fail[k-1],0),O(cur[k-1],0),F(FN[k-1],"muted"),K(sb[4],"border"),O(OC[k-1],0),F(OP[k-1],"muted")})
    for m=1,4 do q[#q+1]=O(P[k-1][m],0) end
    if k>2 then q[#q+1]=O(SH[k-1].s,0) end
  end
  T[#T+1]=q
  T[#T+1]={}
  -- 2. glStencilFunc / glStencilOp
  T[#T+1]={O(frA[k],1),moveF(FI[k],SH[k].fill)}
  T[#T+1]={F(FN[k],SH[k].fill),O(cur[k],1),O(MF[k],1)}
  T[#T+1]={O(frA[k],0),O(frC[k],1),moveO(OI[k],SH[k].fill)}
  T[#T+1]={F(OP[k],SH[k].fill),O(OC[k],1),O(MO[k],1)}
  T[#T+1]={O(frC[k],0),K(sb[1],"border"),K(sb[2],SH[k].fill),K(sb[3],SH[k].fill),O(P[k][2],1),O(P[k][3],1)}
  -- 3. down to the stencil plane: test each covered pixel
  q={O(SH[k].d1,1),O(SH[k].s,1),O(pass[k],1),O(fail[k],1)}
  -- Clear replaced digits before showing their new value in the next beat.
  if k==1 then for _,c in ipairs(cell) do if c[1] then q[#q+1]=O(c.t0,0) end end end
  T[#T+1]=q
  if k==1 then
    q={}
    for _,c in ipairs(cell) do if c[1] then q[#q+1]=O(c.t1,1) end end
    T[#T+1]=q
  end
  T[#T+1]={}
  -- 4. passed pixels reach the color plane
  T[#T+1]={O(SH[k].d1,0),O(SH[k].d2,1),K(sb[2],"border"),K(sb[3],"border"),K(sb[4],SH[k].fill),O(P[k][4],1),O(wr[k],1)}
  T[#T+1]=k<3 and {O(P[k][1],0)} or {}
end
T[#T+1]={}
T[#T+1]={}
runTracks({T},0.55)

return scene
