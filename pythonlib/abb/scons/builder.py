"""
This environment builds on SCons.Script.SConscript.SConsEnvironment

* library repository
* custom printout
* wrapper functions

Inspired/Copied from chrome and blender 

"""


import platform
import subprocess
import re
import os
import sys
import shutil
import tempfile
import types
import logging

import SCons
from SCons.Script.SConscript import SConsEnvironment

if sys.platform != 'win32':
    class bcolors:
        HEADER = '\033[95m'
        OKBLUE = '\033[94m'
        OKGREEN = '\033[92m'
        WARNING = '\033[93m'
        FAIL = '\033[91m'
        ENDC = '\033[0m'
else:
    class bcolors:
        HEADER = ''
        OKBLUE = ''
        OKGREEN = ''
        WARNING = ''
        FAIL = ''
        ENDC = ''
    
bc = bcolors()

def _exec_script(dest, src, dir):
    try:
        src = src.split(" ")
        p = subprocess.Popen(src, cwd=dir)
        p.wait()
    except:
        logging.error("FAILED", exc_info=True)

    if p.returncode != 0:
        raise Exception('"%s" call failed' % src)
    
    
def _exec_script_str(dest, src, dir):
    dest = os.path.basename(dest)
    return bc.HEADER + 'Executing "%s" ' % src +bc.ENDC + "for " + bc.OKGREEN + dest + bc.ENDC


def get_cc_version(CC):
    p = subprocess.Popen([CC, "--version"], stdout=subprocess.PIPE)
    exp = re.compile(r"^.*(\d+)\.(\d+)\.(\d+).*$")
    for l in p.stdout.readlines():
        reg = exp.match(l)
        if reg:
            return int(reg.group(1)), int(reg.group(2)), int(reg.group(3))
    raise "Unexpected version"


def GetProcessorCount():
    '''
      Detects the number of CPUs on the system. Adapted form:
      http://codeliberates.blogspot.com/2008/05/detecting-cpuscores-in-python.html
    '''
    # Windows:
    if os.environ.has_key('NUMBER_OF_PROCESSORS'):
        return max(int(os.environ.get('NUMBER_OF_PROCESSORS', '1')), 1)
    # Linux, Unix and Mac OS X:
    if hasattr(os, 'sysconf'):
        if os.sysconf_names.has_key('SC_NPROCESSORS_ONLN'):
            # Linux and Unix or Mac OS X with python >= 2.5:
            return os.sysconf('SC_NPROCESSORS_ONLN')
    else:  # Mac OS X with Python < 2.5:
        return int(os.popen2("sysctl -n hw.ncpu")[1].read())
    return 1  # Default


def _copy_files_or_dirs_or_symlinks(dest, src):
    SCons.Node.FS.invalidate_node_memos(dest)
    if SCons.Util.is_List(src) and os.path.isdir(dest):
        for file in src:
            shutil.copy2(file, dest)
        return 0
    elif os.path.islink(src):
        linkto = os.readlink(src)
        os.symlink(linkto, dest)
        return 0
    elif os.path.isfile(src):
        return shutil.copy2(src, dest)
    else:
        return shutil.copytree(src, dest, 1)

def _copy_files_or_dirs_or_symlinks_str(dest, src):
    return 'Copying %s to %s ...' % (src, dest)


def _symlink(target, source):
    try:
        lk = os.path.relpath(source, os.path.dirname(target))
    except AttributeError:
        # < python 2.6
        lk = relpath(source, os.path.dirname(target))
    if os.path.lexists(target):
        os.unlink(target)
    os.symlink(lk, target)

def _symlink_str(dest, src):
    return 'Symlink %s to %s ...' % (src, dest)


class AbbEnvironment(SConsEnvironment):

    AbbCopy = SCons.Action.ActionFactory(_copy_files_or_dirs_or_symlinks,
                                            _copy_files_or_dirs_or_symlinks_str,
                                            convert=str)
    AbbSymLink = SCons.Action.ActionFactory(_symlink, _symlink_str, convert=str)
    
    AbbExec = SCons.Action.ActionFactory(_exec_script, _exec_script_str, convert=str)

    def boost_version(self):
        BOOST_ROOT = self["BOOST_ROOT"]
        
        version = file(os.path.join(BOOST_ROOT, "boost", "version.hpp"), "r")
        exp = re.compile(r"^.*BOOST_VERSION (\d+)\s*$")
        for l in version.readlines():
            reg = exp.match(l)
            if reg:
                v = int(reg.group(1))
                return (v // 100000, (v // 100) % 1000, v % 100)
        raise "Unexpected unknown boost version"


    def __AbbObjFiles(self, lenv, component_name, sources, pch=None):
        objs = []
        for cpp_list in sources:
            if type(cpp_list) == type([]):
                for cpp in cpp_list:
                    base, ext = os.path.splitext(os.path.normpath(cpp.abspath))
                    object = '${BUILD_DIR}/%s_build/%s' % (component_name, os.path.basename(base))
                    o = lenv.SharedObject(object, cpp)
                    objs.append(o[0])
            else:
                cpp = cpp_list
                if type(cpp) == type(""):
                    cpp = self.File(cpp)
                base, ext = os.path.splitext(os.path.normpath(cpp.abspath))
                object = '${BUILD_DIR}/%s_build/%s' % (component_name, os.path.basename(base))
                o = lenv.SharedObject(object, cpp)
                objs.append(o[0])

        if pch:
            lenv['GchSh'] = lenv.GchSh('${BUILD_DIR}/%s_build/' % component_name + os.path.basename(pch) + ".gch", pch)[0]
            lenv.Prepend(CPPPATH=["${BUILD_DIR}/%s_build/" % component_name])
            for o in objs:
                lenv.Depends(o, lenv['GchSh'])

        return objs


    @staticmethod
    def __filter__source_abs_path(sources):
        newsources = []
        for s in sources:
            if hasattr(s, "abspath"):
                if s.sources:
                    s = s.sources[0].abspath # object file
                else:
                    s = s.abspath
            newsources.append(s)
        return newsources


    def __add_MSVS(self, lenv, target, name, sources=None, includes=None):
        if sys.platform == 'win32':
            variant_plat = 'Win32' if lenv['OS32'] else 'x64'
            variant=['Debug|' + variant_plat,] if lenv['DEBUG'] else ['Release|' + variant_plat,]
            
            sources = self.__filter__source_abs_path(sources)
            includes = self.__filter__source_abs_path(includes)
            
            targetname = "${TOP_DIR}/build/${BUILD_PLATFORM}/" + name + "_" + lenv['BUILD_BITS'] + "_" + lenv['BUILD_TARGET']
            prj = lenv.MSVSProject(target=targetname + lenv['MSVSPROJECTSUFFIX'],
                                             srcs=sources,
                                             incs=includes,
                                             misc=[],
                                             resources=[],
                                             buildtarget=target[0],
                                             auto_build_solution=0,
                                             variant=variant)
            
            
            sln = lenv.MSVSSolution(target=targetname + lenv['MSVSSOLUTIONSUFFIX'],
                         projects=[prj],
                         variant=variant)
            
            lenv.Requires(target, [lenv.Alias(prj), lenv.Alias(sln)])


    @staticmethod
    def _apply_use(lenv, use, sources=[], includes=[], defines={}, linkflags=[],
                        libs=[], libpath=[], deps=[],
                        cflags=[], cxxflags=[], isProgram=False):
        for u in use:
            u(lenv, sources=sources, includes=includes, defines=defines, linkflags=linkflags,
                libs=libs, libpath=libpath, deps=deps)

        lenv.Append(LIBPATH=[" ${BUILD_DIR}"])
        if libs:
            lenv.Append(LIBS=libs)
        if deps:
            lenv.Append(LIBS=deps)
        if libpath:
            lenv.Append(LIBPATH=libpath)
        if includes:
            lenv.Append(CPPPATH=includes)
        if defines:
            lenv.Append(CPPDEFINES=defines)
        if linkflags:
            lenv.Append(LINKFLAGS=linkflags)
        if cflags:
            lenv.Append(CFLAGS=cflags)
        if cxxflags:
            lenv.Append(CXXFLAGS=cxxflags)

        if sys.platform != 'win32':
            if not isProgram:
                lenv.Append(LINKFLAGS=["-Wl,-shared,-Bsymbolic"])
                
            lenv.Append(LINKFLAGS=["-Wl,--rpath=\\$$ORIGIN/", "-Wl,--rpath-link=\\$$ORIGIN/", "-Wl,--no-undefined"])


    def __post_setup(self, lenv, target, deps):
        SConsEnvironment.Default(self, target)
        
        if deps:
            dependencies = map(lambda x: lenv.Alias(x), deps)
            lenv.Requires(target, dependencies)

    
    def AbbSharedLib(self=None, shlibname=None, sources=[], includes=[], defines={}, linkflags=[],
                        libs=[], libpath=[], deps=[], shlibprefix=True, use=[]):
        print bc.HEADER+'Configuring shared library '+bc.ENDC+bc.OKGREEN+shlibname+bc.ENDC
        
        lenv = self.Clone()
        self._apply_use(lenv, use, sources=sources, includes=includes, defines=defines, linkflags=linkflags,
                        libs=libs, libpath=libpath, deps=deps)

        
        if libs:
            lenv.Append(LIBS=libs)

        objs = self.__AbbObjFiles(lenv, shlibname, sources)

        if not shlibprefix:
            shlibfile = lenv.File('${BUILD_DIR}/%s${SHLIBSUFFIX}' % shlibname)
        else:
            shlibfile = lenv.File('${BUILD_DIR}/${SHLIBPREFIX}%s${SHLIBSUFFIX}' % shlibname)
        
        shlib = lenv.SharedLibrary(shlibfile, source=objs)
        
        self.__post_setup(lenv, shlib, deps)
        self.__add_MSVS(lenv, shlib, shlibname, sources=sources, includes=includes)
        return shlib

    
    def AbbProg(self=None, progname=None, sources=[], includes=[], libs=[], libpath=[], linkflags=[],
                   defines={}, deps=[], use=[]):
        print bc.HEADER+'Configuring program '+bc.ENDC+bc.OKGREEN+progname+bc.ENDC

        lenv = self.Clone()
        self._apply_use(lenv, use, sources=sources, includes=includes, defines=defines, linkflags=linkflags,
                        libs=libs, libpath=libpath, deps=deps, isProgram=True)

        objs = self.__AbbObjFiles(lenv, progname, sources)

        progfile = lenv.File('${BUILD_DIR}/%s${PROGSUFFIX}' % progname)
        prog = lenv.Program(progfile, source=objs)

        self.__post_setup(lenv, prog, deps)
        self.__add_MSVS(lenv, prog, progname, sources=sources, includes=includes)
        return prog
    
    
    def _AbbLib(self=None, libname=None, sources=[]):
        if not self or not libname or not sources:
            print bc.FAIL+'Cannot continue. Missing argument for AbbLib '+libname+bc.ENDC
            self.Exit()

        lenv = self.Clone()        
        
        libfile = lenv.File('${BUILD_DIR}/${LIBPREFIX}%s${LIBSUFFIX}' % libname)
        lib = lenv.Library(libfile, source=sources)
        SConsEnvironment.Default(self, lib) # we add to default target, because this way we get some kind of progress info during build
        
        return lib

    
    def AbbSharedObj(self, component_name, sources=[], includes=[], defines={}, use=[]):
        print bc.HEADER+'Configuring shared objects '+bc.ENDC+bc.OKGREEN+component_name+bc.ENDC

        lenv = self.Clone()
        self._apply_use(lenv, use, sources=sources, includes=includes, defines=defines)
        
        objs = self.__AbbObjFiles(lenv, component_name, sources)
        
        lib = lenv._AbbLib(component_name, sources=objs)
        self.__add_MSVS(lenv, lib, component_name, sources=sources, includes=includes)
        lib[0].attributes.shared = 1
        
        target = lenv.Alias(component_name, lib)
        target[0].attributes.shared = 1
        
        self.__post_setup(lenv, target, component_name)
        return target


def CreateAbbEnvironment(rootdir, ARGUMENTS):
    SRC_ROOT = rootdir + os.sep
    TOP_DIR = rootdir
    
    DEBUG=int(ARGUMENTS.get('debug', 0))
    OS32=int(ARGUMENTS.get('os32', 0))
    OS64=int(ARGUMENTS.get('os64', 0))
    GCC=int(ARGUMENTS.get('gcc', 0))
    
    OSBITS = platform.architecture()[0]
    if not OS32 and not OS64:
        OS32 = OSBITS == "32bit"
        OS64 = OSBITS == "64bit"
    
    BUILD_BITS = "64" if OS64 else "32"
    BUILD_TARGET = "debug" if DEBUG else "release"
    
    if sys.platform != 'win32':
        BUILD_PLATFORM = sys.platform
        if os.path.exists('/etc/debian_version'):
            BUILD_PLATFORM = 'deb-' + BUILD_PLATFORM
        elif os.path.exists('/etc/redhat-release'):
            BUILD_PLATFORM = 'rh-' + BUILD_PLATFORM
        else:
            BUILD_PLATFORM = 'unk-' + BUILD_PLATFORM
    else:
        BUILD_PLATFORM = 'vc'
    
    BUILD_DIR="${TOP_DIR}/targets/${BUILD_PLATFORM}/${BUILD_TARGET}/${BUILD_BITS}"
    
    if DEBUG:
        CPPDEFINES = {'DEBUG':"1",'_DEBUG':"1",}
    else:
        CPPDEFINES = {'NDEBUG':None,}
        
    if sys.platform == 'win32':
        CFLAGS = "${DIALECTFLAGS} ${WARNINGFLAGS}"
        CXXFLAGS = "${CFLAGS} /GR /Gy /EHsc"
        LINKFLAGS=['/INCREMENTAL:NO', '/NXCOMPAT']

        CPPDEFINES.update({
             'WIN32':None,
         })
    else:
        CFLAGS = "-g" if DEBUG else "-O3"
        CXXFLAGS = "${CFLAGS} -Wno-reorder"
        LINKFLAGS = ["-Wl,--no-undefined"]
    
        if OS32:
            CFLAGS += " -m32"
            LINKFLAGS.append("-m32")
        if OS64:
            CFLAGS += " -m64"
            LINKFLAGS.append("-m64")

        CPPDEFINES.update({
             'LINUX_OS':None,
             '_FILE_OFFSET_BITS':64,
         })
            
    env = AbbEnvironment(CFLAGS=CFLAGS, CXXFLAGS=CXXFLAGS,
                  LINKFLAGS=LINKFLAGS, CPPDEFINES=CPPDEFINES,
                  SRC_ROOT=SRC_ROOT, OS64=OS64, OS32=OS32,
                  TOP_DIR=TOP_DIR,
                  DEBUG=DEBUG,
                  BUILD_TARGET=BUILD_TARGET,
                  BUILD_PLATFORM=BUILD_PLATFORM,
                  BUILD_BITS=BUILD_BITS,
                  BUILD_DIR=BUILD_DIR,
                  TARGET_ARCH=("x86" if OS32 else "amd64"))

    if sys.platform != 'win32':
        CC = os.environ.get("CC", "gcc")
        CXX = os.environ.get("CXX", "g++")
        
        if GCC:
            if GCC == 46:
                CC="gcc-4.6"
                CXX="g++-4.6"
            elif GCC == 45:
                CC="gcc-4.5"
                CXX="g++-4.5"
            elif GCC == 44:
                CC="gcc-4.4"
                CXX="g++-4.4"
            elif GCC == 43:
                CC="gcc-4.3"
                CXX="g++-4.3"
            elif GCC == 42:
                CC="gcc-4.2"
                CXX="g++-4.2"
            elif GCC == 41:
                CC="gcc-4.1"
                CXX="g++-4.1"
            elif GCC == 40:
                CC="gcc-4.0"
                CXX="g++-4.0"

        gcc_version = get_cc_version(CC)
        cpp_version = get_cc_version(CXX)
        
        env.Replace(CC=CC)
        env.Replace(CXX=CXX)
    
        BUILD_PLATFORM = BUILD_PLATFORM + "-g++-%d.%d.%d" % cpp_version
    else:
        env['ENV']['TMP'] = os.path.dirname(tempfile.NamedTemporaryFile().name) # cl.exe
        
        env['MSVSSCONS'] = sys.executable
        env['MSVSSCONSFLAGS'] = rootdir + '/scons.py -f ${MSVSSCONSCRIPT.name} debug=${DEBUG}'
        if OS32:
            env['MSVSSCONSFLAGS'] = env['MSVSSCONSFLAGS'] + ' os32=1'
        else:
            env['MSVSSCONSFLAGS'] = env['MSVSSCONSFLAGS'] + ' os64=1'
        #env['BUILD_MSVS'] = '${TOP_DIR}/build/${BUILD_PLATFORM}'
            

        env.Append(CCPDBFLAGS=['/Zi'])
        env.Append(CCPDBFLAGS=['/nologo'])
        env.Append(LINKFLAGS=['/nologo'])

        if OS32:
            env.Append(LINKFLAGS=['/MACHINE:X86'])
            env.Append(ARFLAGS=['/MACHINE:X86'])
        if OS64:
            env.Append(LINKFLAGS=['/MACHINE:X64'])
            env.Append(ARFLAGS=['/MACHINE:X64'])
    
        if DEBUG:
            env.Append(CCFLAGS=['/Od', '/MDd']) # '/MTd'
            env.Append(LINKFLAGS=['/DEBUG'])
        else:
            env.Append(CCFLAGS=['/O2', '/MD']) # '/MT'
            env.Append(LINKFLAGS=['/OPT:REF', '/OPT:ICF'])

        BUILD_PLATFORM = 'vc' + str(int(float(env['MSVS_VERSION'])))
        
    env.Replace(BUILD_PLATFORM=BUILD_PLATFORM)
    env.Append(LIBPATH=["${BUILD_DIR}"])
    
    env.SConsignFile(env.File('$BUILD_DIR/.sconsign').abspath)
    env.CacheDir(env.Dir('$BUILD_DIR/scons_cache').abspath)

    print bc.HEADER+'#'*75 + bc.ENDC
    print bc.HEADER+'This is a SCons build system for lin/win/mac' + bc.ENDC
    print bc.HEADER+'Valid build options:' + bc.ENDC
    build_options = {
        "debug={0,1}" : "build release/debug",
        "os32={0,1}" : "force i386 build",
        "os64={0,1}" : "force x64 build",
        "gcc={46,45,44,43,42,41,40}" : "use specific gcc version",
        "-c" : "clean",
    }
    option_keys = build_options.keys()
    option_keys.sort()
    for k in option_keys:
        print bc.HEADER + '  ' + k + ': ' + build_options[k] + bc.ENDC
    print bc.HEADER+'#'*75 + bc.ENDC
    if sys.platform != 'win32':
        print bc.OKBLUE+'Using CC version ' + str(gcc_version) + bc.ENDC
        print bc.OKBLUE+'Using CXX version ' + str(cpp_version) + bc.ENDC
    else:
        print bc.OKBLUE+'Using VS version ' + env['MSVS_VERSION'] + bc.ENDC

    return env
