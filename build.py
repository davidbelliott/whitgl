import sys
import os
import platform

sys.path.insert(0, 'input')
import ninja_syntax

plat = platform.system()
bit64 = platform.architecture()[0] == '64bit'
joinp = os.path.join

if(bit64):
  fmodlib = 'fmodex64'
else:
  fmodlib = 'fmodex'
fmodso = 'lib%s.so' % fmodlib

cflags = '-Iinc -Wall -Wextra -Werror -g'
ldflags = ''
if(plat == 'Windows'):
  cflags += ' -Iinput/glfw/include -Iinput/libpng/include -Iinput/zlib/include -Iinput/glew/include -Iinput/fmod/win/inc '
  ldflags += ' -Linput/glfw/lib-mingw -Linput/glew/lib -Linput/libpng/lib -Linput/zlib/lib -Linput/fmod/win/lib input/glfw/lib-mingw/glfw3dll.a -lglu32 -lopengl32 -lglew32 -lfmodex -lpng -lz '
else:
  cflags += ' -isystem input/fmod/api/inc -Iinput/glfw/include '
  ldflags += ' -Wl,-rpath=.,--enable-new-dtags -Linput/fmod/api/lib -Linput/glfw/build/src -lglfw3 -lGLU -lGL -lGLEW -lSOIL -lm -l%s -lX11 -lXxf86vm -lpthread -lXrandr -lXi -lpng' % fmodlib

def rules(n, cflags, ldflags):
  n.variable('cflags', cflags)
  n.variable('ldflags', ldflags)
  n.newline()

  n.rule('cxx',
    command='gcc -MMD -MF $out.d $cflags -c $in -o $out',
    depfile='$out.d',
    description='CXX $out')
  n.rule('static',
    command='ar rcs $out $in')
  n.rule('link',
    command='gcc $in $libs $ldflags -o $out',
    description='LINK $out')
  n.rule('cp',
    command='cp $in $out',
    description='COPY $in $out')
  n.newline()

def walk_src(n, path, objdir):
  obj = []
  for (dirpath, dirnames, filenames) in os.walk(path):
    for f in filenames:
      _, ext = os.path.splitext(f)
      if ext == '.c':
        s = os.path.relpath(joinp(dirpath, f), path)
        o = s.replace('.c', '.o')
        obj += n.build(joinp(objdir, o), 'cxx', joinp(path, s))
  n.newline()
  return obj
  
def walk_data(n, data_in, data_out, validext=['png','ogg']):
  data = []
  for (dirpath, dirnames, filenames) in os.walk(data_in):
    for f in filenames:
      ext = f[-3:]
      valid = False
      for e in validext:
        if ext == e:
          valid = True
      if not valid:
        continue
      s = os.path.relpath(joinp(dirpath, f), data_in)
      data += n.build(joinp(data_out, s), 'cp', joinp(data_in, s))
  n.newline()
  return data

def copy_libs(n, inputs, outdir):
  targets = []
  if plat == 'Windows':
    targets += n.build(joinp(outdir, 'fmodex.dll'), 'cp', joinp(inputs, 'fmod', 'fmodex.dll'))
    targets += n.build(joinp(outdir, 'glew32.dll'), 'cp', joinp(inputs, 'glew', 'lib', 'glew32.dll'))
    targets += n.build(joinp(outdir, 'glfw3.dll'), 'cp', joinp(inputs, 'glfw', 'lib-mingw', 'glfw3.dll'))
    targets += n.build(joinp(outdir, 'libpng3.dll'), 'cp', joinp(inputs, 'libpng', 'bin', 'libpng3.dll'))
  elif plat == 'Darwin':
    targets += n.build(joinp(outdir, 'libfmod.dylib'), 'cp', joinp(inputs, 'fmod', 'lib', 'libfmod.dylib'))
  else:
    targets += n.build(joinp(outdir, fmodso), 'cp', joinp(inputs, 'fmod', 'api', 'lib', fmodso))
  n.newline()
  return targets

def main():
  target = 'whitgl.a'
  srcdir = 'src'
  examplesrcdir = 'example'
  inputdir = 'input'
  builddir = 'build'
  exampledir = joinp(builddir, 'example')
  objdir = joinp(builddir, 'obj')
  libdir = joinp(builddir, 'lib')
  data_in =  joinp(examplesrcdir, 'data')
  data_out = joinp(exampledir, 'data')
  BUILD_FILENAME = 'build.ninja'
  buildfile = open(BUILD_FILENAME, 'w')
  n = ninja_syntax.Writer(buildfile)
  rules(n, cflags, ldflags)
  # Library
  obj = walk_src(n, srcdir, objdir)
  staticlib = n.build(joinp(libdir, target), 'static', obj)
  targets = []
  targets += staticlib
  n.newline()

  # Example
  obj = walk_src(n, examplesrcdir, objdir)
  targets += n.build(joinp(exampledir, 'example'), 'link', obj+staticlib)
  n.newline()

  data = walk_data(n, data_in, data_out)

  targets += n.build('data', 'phony', data)
  n.newline()

  targets += copy_libs(n, inputdir, exampledir)

  n.build('all', 'phony', targets)
  n.default('all')

if __name__ == '__main__':
  main()