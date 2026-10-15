#!/usr/bin/env python3
"""Deterministic CC0 glTF fixtures, no third-party Python modules. Outputs checked in."""
import base64,copy,json,math,pathlib,struct,zlib
ROOT=pathlib.Path(__file__).resolve().parents[2]
OUT=ROOT/'tests/data/gltf';LAB=ROOT/'Projects/EngineTest/Resource/gltf/MaterialLab'

def png(pixels,w=2,h=2):
 def chunk(t,b):return struct.pack('>I',len(b))+t+b+struct.pack('>I',zlib.crc32(t+b))
 raw=b''.join(b'\0'+bytes(pixels[y*w*4:(y+1)*w*4]) for y in range(h))
 return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')
class Asset:
 def __init__(self):
  self.data=bytearray();self.j={'asset':{'version':'2.0','generator':'ICE deterministic fixtures'},'buffers':[{'uri':'mesh data.bin','byteLength':0}],'bufferViews':[],'accessors':[],'meshes':[],'nodes':[],'scenes':[{'nodes':[]}],'scene':0,'materials':[]}
 def view(self,data,stride=None):
  while len(self.data)%4:self.data.append(0)
  v={'buffer':0,'byteOffset':len(self.data),'byteLength':len(data)}
  if stride:v['byteStride']=stride
  self.j['bufferViews'].append(v);self.data.extend(data);return len(self.j['bufferViews'])-1
 def accessor(self,data,typ,count,component=5126,**kwargs):
  a={'bufferView':self.view(data),'componentType':component,'count':count,'type':typ};a.update(kwargs);self.j['accessors'].append(a);return len(self.j['accessors'])-1
 def mesh(self,positions,normals=None,uv=None,uv1=None,indices=None,material=0,colors=None):
  pack=lambda vs:b''.join(struct.pack('<%df'%len(v),*v) for v in vs)
  xyz=list(zip(*positions));attrs={'POSITION':self.accessor(pack(positions),'VEC3',len(positions),min=[min(x) for x in xyz],max=[max(x) for x in xyz])}
  for name,values,typ in [('NORMAL',normals,'VEC3'),('TEXCOORD_0',uv,'VEC2'),('TEXCOORD_1',uv1,'VEC2')]:
   if values is not None:attrs[name]=self.accessor(pack(values),typ,len(values))
  if colors is not None:attrs['COLOR_0']=self.accessor(bytes(sum((list(x) for x in colors),[])),'VEC4',len(colors),5121,normalized=True)
  primitive={'attributes':attrs,'material':material}
  if indices is not None:primitive['indices']=self.accessor(struct.pack('<%dH'%len(indices),*indices),'SCALAR',len(indices),5123)
  self.j['meshes'].append({'primitives':[primitive]});return len(self.j['meshes'])-1
 def node(self,mesh,position,scale=None,parent=None):
  n={'mesh':mesh,'translation':position,'name':'Instance '+str(len(self.j['nodes']))}
  if scale:n['scale']=scale
  index=len(self.j['nodes']);self.j['nodes'].append(n)
  if parent is None:self.j['scenes'][0]['nodes'].append(index)
  else:self.j['nodes'][parent].setdefault('children',[]).append(index)
  return index
 def save(self,directory,name,images=None):
  directory.mkdir(parents=True,exist_ok=True);j=copy.deepcopy(self.j);j['buffers'][0]['byteLength']=len(self.data);(directory/'mesh data.bin').write_bytes(self.data);j['buffers'][0]['uri']='mesh%20data.bin'
  (directory/(name+'.gltf')).write_text(json.dumps(j,indent=2)+'\n')
  if images:
   for path,data in images.items():(directory/path).write_bytes(data)
  embedded=copy.deepcopy(j);embedded['buffers'][0]['uri']='data:application/octet-stream;base64,'+base64.b64encode(self.data).decode()
  for image in embedded.get('images',[]):image['uri']='data:image/png;base64,'+base64.b64encode(images[image['uri']]).decode()
  (directory/(name+'-embedded.gltf')).write_text(json.dumps(embedded,separators=(',',':'))+'\n')
  binary=bytearray(self.data);glb=copy.deepcopy(j);glb['buffers'][0].pop('uri')
  for image in glb.get('images',[]):
   while len(binary)%4:binary.append(0)
   data=images[image.pop('uri')];image['bufferView']=len(glb['bufferViews']);image['mimeType']='image/png';glb['bufferViews'].append({'buffer':0,'byteOffset':len(binary),'byteLength':len(data)});binary.extend(data)
  glb['buffers'][0]['byteLength']=len(binary)
  while len(binary)%4:binary.append(0)
  text=json.dumps(glb,separators=(',',':')).encode()
  while len(text)%4:text+=b' '
  payload=struct.pack('<2I',len(text),0x4e4f534a)+text+struct.pack('<2I',len(binary),0x004e4942)+binary
  (directory/(name+'.glb')).write_bytes(struct.pack('<3I',0x46546c67,2,len(payload)+12)+payload)
  (directory/'LICENSE.txt').write_text('ICE test fixture assets, dedicated to the public domain under CC0-1.0.\nhttps://creativecommons.org/publicdomain/zero/1.0/\n')
  return j

def sphere():
 positions=[];normals=[];uv=[];indices=[];columns=32;rows=16
 for y in range(rows+1):
  phi=y*math.pi/rows
  for x in range(columns+1):
   theta=x*2*math.pi/columns;n=(math.sin(phi)*math.cos(theta),math.cos(phi),math.sin(phi)*math.sin(theta));positions.append(n);normals.append(n);uv.append((x/columns,y/rows))
 for y in range(rows):
  for x in range(columns):
   a=y*(columns+1)+x;b=a+columns+1;indices.extend((a,a+1,b,a+1,b+1,b))
 return positions,normals,uv,indices

def material_lab():
 a=Asset();p,n,uv,ind=sphere()
 for row,metal in enumerate([0,1]):
  for col,rough in enumerate([.08,.3,.6,1]):
   mat={'name':('Metal' if metal else 'Dielectric')+' roughness '+str(rough),'pbrMetallicRoughness':{'baseColorFactor':[.75,.32,.09,1] if row else [.06,.5,.42,1],'metallicFactor':metal,'roughnessFactor':rough}}
   a.j['materials'].append(mat);mesh=a.mesh(p,n,uv,indices=ind,material=len(a.j['materials'])-1);a.node(mesh,[(col-1.5)*2.6,(.5-row)*2.6,0],scale=[-1,1,1] if col==3 else None)
 a.save(LAB,'MaterialLab');return a

def render_probe():
 a=Asset();a.j['extensionsUsed']=['KHR_materials_unlit'];a.j['images']=[{'uri':'color.png'},{'uri':'mr.png'},{'uri':'normal.png'}];a.j['samplers']=[{'magFilter':9728,'minFilter':9728,'wrapS':33071,'wrapT':33071}];a.j['textures']=[{'source':i,'sampler':0} for i in range(3)]
 material=lambda name,color,alpha='OPAQUE':{'name':name,'pbrMetallicRoughness':{'baseColorFactor':color,'metallicFactor':0,'roughnessFactor':1},'extensions':{'KHR_materials_unlit':{}},'alphaMode':alpha}
 a.j['materials']=[material('UV0 red',[1,1,1,1]),material('Mirrored UV1 blue',[1,1,1,1]),material('Masked out',[1,0,0,.2],'MASK'),material('Blue opaque',[0,0,1,1]),material('Red front blend',[1,0,0,.5],'BLEND'),material('Green middle blend',[0,1,0,.5],'BLEND')]
 a.j['materials'][0]['pbrMetallicRoughness'].update({'baseColorTexture':{'index':0},'metallicRoughnessTexture':{'index':1},'metallicFactor':.5,'roughnessFactor':.5});a.j['materials'][0]['normalTexture']={'index':2}
 a.j['materials'][1]['pbrMetallicRoughness']['baseColorTexture']={'index':0,'texCoord':1}
 p=[(-.7,-.55,0),(.7,-.55,0),(.7,.55,0),(-.7,.55,0)];n=[(0,0,1)]*4;uv=[(0,0),(.5,0),(.5,.5),(0,.5)];uv1=[(.25,.75)]*4;indices=[0,1,2,0,2,3]
 for material_id,position,scale in [(0,[-2,1,0],None),(1,[0,1,0],[-1,1,1]),(2,[2,1,0],None),(4,[0,-1,.3],None),(3,[0,-1,0],None),(5,[0,-1,.2],None)]:
  mesh=a.mesh(p,n,uv,uv1,indices,material_id);a.node(mesh,position,scale)
 images={'color.png':png([255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255]),'mr.png':png([51,102,204,255]*4),'normal.png':png([255,128,255,255]*4)}
 a.save(OUT/'RenderProbe','RenderProbe',images)

def sparse_and_interleaved():
 a=Asset();a.j['materials']=[{'pbrMetallicRoughness':{'baseColorFactor':[1,1,1,1],'metallicFactor':0}}]
 vertices=[(0,0,0,0,0,1,0,0),(1,0,0,0,0,1,1,0),(0,1,0,0,0,1,0,1)]
 view=a.view(b''.join(struct.pack('<8f4B',*v,128,64,255,255) for v in vertices),36)
 for typ,offset,component,shape in [('POSITION',0,5126,'VEC3'),('NORMAL',12,5126,'VEC3'),('TEXCOORD_0',24,5126,'VEC2'),('COLOR_0',32,5121,'VEC4')]:
  entry={'bufferView':view,'byteOffset':offset,'componentType':component,'count':3,'type':shape}
  if typ=='POSITION':entry.update(min=[0,0,0],max=[1,1,0])
  if typ=='COLOR_0':entry['normalized']=True
  a.j['accessors'].append(entry)
 a.j['meshes']=[{'primitives':[{'attributes':{'POSITION':0,'NORMAL':1,'TEXCOORD_0':2,'COLOR_0':3},'material':0}]}]
 a.j['nodes']=[{'translation':[2,3,4],'children':[1,2]},{'mesh':0,'scale':[2,1,1]},{'mesh':0,'translation':[-2,0,0],'scale':[-1,1,1]}];a.j['scenes'][0]['nodes']=[0];j=a.save(OUT/'Interleaved','Interleaved')
 # Sparse POSITION-only triangle: no base buffer view and no indices/normals.
 b=Asset();iv=b.view(bytes([0,1,2]));values=b.view(struct.pack('<9f',0,0,0,1,0,0,0,1,0));b.j['accessors']=[{'componentType':5126,'count':3,'type':'VEC3','min':[0,0,0],'max':[1,1,0],'sparse':{'count':3,'indices':{'bufferView':iv,'componentType':5121},'values':{'bufferView':values}}}];b.j['meshes']=[{'primitives':[{'attributes':{'POSITION':0}}]}];b.node(0,[0,0,0]);b.save(OUT/'Sparse','Sparse')
 # Failed loads must preserve the prior asset, including GPU hot reload.
 bad=copy.deepcopy(j);bad['extensionsRequired']=['KHR_draco_mesh_compression'];(OUT/'Interleaved/unsupported.gltf').write_text(json.dumps(bad))
 bad=copy.deepcopy(j);bad['accessors'][0]['count']=1000;(OUT/'Interleaved/truncated.gltf').write_text(json.dumps(bad))
 bad=copy.deepcopy(j);bad['nodes'][1]['children']=[0];(OUT/'Interleaved/cycle.gltf').write_text(json.dumps(bad))
 bad=copy.deepcopy(j);bad['animations']=[{'channels':[],'samplers':[]}];(OUT/'Interleaved/animation.gltf').write_text(json.dumps(bad))

if __name__=='__main__':material_lab();render_probe();sparse_and_interleaved()
