#!/usr/bin/python

import struct
import argparse
import sys
import os.path

def process_mtl(filename):
        materials = []
        file = open(filename)
        for line in file:
                tokens = line.split()
                if(len(tokens) == 0):
                        continue
                ident = tokens.pop(0)
                if(ident == 'newmtl'):
                        m = {}
                        m['name'] = tokens[0]
                        m['color'] = (1,1,1)
                        materials.append(m)
                if(ident == 'Kd'):
                        materials[-1]['color'] = (float(tokens[0]),float(tokens[1]),float(tokens[2]))
        return materials

def process_obj(filename):
        file = open(filename)
        vertices = []
        normals = []
        texcoords = []
        faces = []
        materials = []
        default_material = {}
        default_material['name'] = 'default'
        default_material['color'] = (1,0.1,1)
        materials.append(default_material)
        current_material = 0
        for line in file:
                tokens = line.split()
                if(len(tokens) == 0):
                        continue
                ident = tokens.pop(0)
                if(ident == 'v'):
                        vertex = (float(tokens[0]),float(tokens[1]),float(tokens[2]));
                        vertices.append(vertex)
                if(ident == 'vn'):
                        normal = (float(tokens[0]),float(tokens[1]),float(tokens[2]));
                        normals.append(normal)
                if(ident == 'vt'):
                        texcoord = (float(tokens[0]),float(tokens[1]));
                        texcoords.append(texcoord)
                if(ident == 'f'):
                        face = {}
                        face['vertices'] = (int(tokens[0].split('/')[0]),int(tokens[1].split('/')[0]),int(tokens[2].split('/')[0]))
                        face['texcoords'] = (int(tokens[0].split('/')[1]),int(tokens[1].split('/')[1]),int(tokens[2].split('/')[1]))
                        face['normals'] = (int(tokens[0].split('/')[2]),int(tokens[1].split('/')[2]),int(tokens[2].split('/')[2]))
                        face['material'] = current_material
                        faces.append(face)
                        if len(tokens) == 4:
                                face['vertices'] = (int(tokens[2].split('/')[0]),int(tokens[3].split('/')[0]),int(tokens[0].split('/')[0]))
                                face['normals'] = (int(tokens[2].split('/')[2]),int(tokens[3].split('/')[2]),int(tokens[0].split('/')[2]))
                                face['material'] = current_material
                                faces.append(face)
                if(ident == 'mtllib'):
                        path = os.path.join(os.path.dirname(filename), tokens[0])
                        materials += process_mtl(path)
                if(ident == 'usemtl'):
                        current_material = 0
                        for i in range(len(materials)):
                                if materials[i]['name'] == tokens[0]:
                                        current_material = i
        return vertices, normals, texcoords, faces, materials


def main():
        parser = argparse.ArgumentParser(description='Convert a wavefront obj file to use in slicer.')
        parser.add_argument('src', help='obj file name')
        parser.add_argument('dst', help='wmd file name')

        args = parser.parse_args()
        print("Converting %s to %s" % (args.src, args.dst))

        vertices, normals, texcoords, faces, materials = process_obj(args.src)

        n_vertices = len(faces)*3
        vertices_size = n_vertices*3*4
        colours_size = vertices_size * 2
        texcoord_size = n_vertices*2*4
        size = vertices_size + colours_size + texcoord_size
        print ("Vertices %d size %d" % (n_vertices, size))

        out = open(args.dst, 'wb')
        out.write(struct.pack('i', size))
        out.write(struct.pack('i', n_vertices))
        for face in faces:
                m = materials[face['material']]
                for i in range(3):
                        vertex = vertices[face['vertices'][i]-1]
                        normal = normals[face['normals'][i]-1]
                        texcoord = texcoords[face['texcoords'][i]-1]
                        for f in vertex:
                                out.write(struct.pack('f', f))
                        for t in texcoord:
                                out.write(struct.pack('f', t))
                        for c in m['color']:
                                out.write(struct.pack('f', c))
                        for n in normal:
                                out.write(struct.pack('f', n))

if __name__ == "__main__":
    main()
