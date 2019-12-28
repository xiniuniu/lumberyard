#
# All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
# its licensors.
#
# For complete copyright and license terms please see the LICENSE at the root of this
# distribution (the "License"). All use of this software is governed by the License,
# or, if provided, by the license below or the license accompanying this file. Do not
# remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#
# Original file Copyright Crytek GMBH or its affiliates, used under license.
#
from waflib.Configure import conf, ConfigurationContext
from waflib.Build import BuildContext
from waflib.TaskGen import feature, before_method, after_method
from waflib.Options import OptionsContext
from waflib import Utils, Logs, Errors, Task, Node
from os import stat
from cry_utils import append_kw_entry, append_to_unique_list, sanitize_kw_input_lists, clean_duplicates_in_list, prepend_kw_entry, get_configuration
from copy_tasks import should_overwrite_file,fast_copy2
from collections import defaultdict
from third_party import is_third_party_uselib_configured, get_third_party_platform_name, get_third_party_configuration_name
from gems import Gem
from settings_manager import LUMBERYARD_SETTINGS
from build_configurations import ALIAS_TO_PLATFORMS_MAP
from utils import is_value_true
import os, stat, errno, json, re, threading, inspect, copy
from lumberyard import add_platform_root

COMMON_INPUTS = [
    'additional_settings',
    'export_definitions',
    'meta_includes',
    'file_list',
    'use',  # module dependency
    'defines',
    'export_defines',
    'includes',
    'export_includes',
    'cxxflags',
    'cflags',
    'lib',  # shared
    'libpath',  # shared
    'stlib',  # static
    'stlibpath',  # static
    'linkflags',
    'framework',
    'frameworkpath',
    'rpath',
    'features',
    'enable_rtti',
    'remove_release_define',
    'uselib',
    'mirror_artifacts_to_include',
    'mirror_artifacts_to_exclude',
    'output_folder',
    'source_artifacts_include',
    'source_artifacts_exclude',
    'mirror_artifacts_to_include',
    'mirror_artifacts_to_exclude',
    'copy_external',
    'copy_dependent_files',
    'copyright_org',  # optional keyword to tag the file's originator company
    'dx12_only',  # Option to build only if dx12 was detected on the host machine
    'additional_manifests'
]
SANITIZE_INPUTS = COMMON_INPUTS + ['output_file_name', 'files', 'winres_includes', 'winres_defines']


class BuildTargetType(object):
    """
    Supported WAF target types for build_ commands
    """
    SHARED_LIB = "shlib"
    STATIC_LIB = "stlib"
    APPLICATION = "program"
    FILE_CONTAINER = "container"

# Table that maps the the build pattern strings and waf features based on the type of build (BuildTargetType)
BUILD_PROCESS_TABLE = {
    BuildTargetType.SHARED_LIB: (['cshlib_PATTERN', 'cstlib_PATTERN'],
                               'shlib'
                               ),
    BuildTargetType.STATIC_LIB: (['cstlib_PATTERN'],
                               'stlib'
                               ),
    BuildTargetType.APPLICATION: (['cprogram_PATTERN'],
                                'program'
                                )
}

BUILD_FUNC_KW = '__build_func__'
BUILD_TYPE_KW = '__build_type__'

def build_func(f, build_type):
    """
    Custom decorator that is similar to the @conf decorator except that it is intended to mark
    build functions specifically. All build functions must be decorated with this decorator
    :param f: build method to bind
    :type f: function
    :parm build_type: The WAF build type (see BuildTargetType)
    :type build_type: string
    """
    def fun(*k, **kw):
        kw[BUILD_FUNC_KW] = f.__name__
        kw[BUILD_TYPE_KW] = build_type
        result = f(*k, **kw)
        return result

    setattr(OptionsContext, f.__name__, fun)
    setattr(ConfigurationContext, f.__name__, fun)
    setattr(BuildContext, f.__name__, fun)
    return f


def build_shlib(f):
    return build_func(f, BuildTargetType.SHARED_LIB)


def build_stlib(f):
    return build_func(f, BuildTargetType.STATIC_LIB)


def build_program(f):
    return build_func(f, BuildTargetType.APPLICATION)


def build_file_container(f):
    return build_func(f, BuildTargetType.FILE_CONTAINER)


def AddGlobalKeywords(ctx, kw):
    """
    Helper function to add the global keywords that are added for every single compile ever.
    Happens at the very start before expansion of these keywords.
    Keep this list as small as possible for maximum flexibility but if you find yourself adding something to every single
    WSCRIPT it might be far better to just add it here so you can remove it here in the future all in one place.
    """
    pass


CXX_EXT_LIST = ('.c', '.C', '.cc', '.CC', '.cpp', '.CPP')


@conf
def is_cxx_file(self, file_name):
    return file_name.endswith(CXX_EXT_LIST)


###############################################################################
# DX12 detection and configuration.  DX12_INCLUDES and DX12_LIBPATH are set in compile_rules_win_x64_host
@conf
def has_dx12(conf):
    
    result = conf.does_support_dx12()
    if result and any(result):
        return True

    return False


#############################################################################
def SanitizeInput(ctx, kw):

    # Sanitize the inputs kws that should be list inputs
    sanitize_kw_input_lists(SANITIZE_INPUTS, kw)

    # Recurse for additional settings
    if 'additional_settings' in kw:
        for setting in kw['additional_settings']:
            SanitizeInput(ctx, setting)


def RegisterVisualStudioFilter(ctx, kw):
    """
    Util-function to register each provided visual studio filter parameter in a central lookup map
    """
    if not 'vs_filter' in kw:
        ctx.fatal('Mandatory "vs_filter" task generater parameter missing in %s/wscript' % ctx.path.abspath())

    if not hasattr(ctx, 'vs_project_filters'):
        ctx.vs_project_filters = {}

    ctx.vs_project_filters[ kw['target' ] ] = kw['vs_filter']

def AssignTaskGeneratorIdx(ctx, kw):
    """
    Util-function to assing a unique idx to prevent concurrency issues when two task generator output the same file.
    """
    if not hasattr(ctx, 'index_counter'):
        ctx.index_counter = 0
    if not hasattr(ctx, 'index_map'):
        ctx.index_map = {}

    # Use a path to the wscript and the actual taskgenerator target as a unqiue key
    key = ctx.path.abspath() + '___' + kw['target']

    if key in ctx.index_map:
        kw['idx'] = ctx.index_map.get(key)
    else:
        ctx.index_counter += 1
        kw['idx'] = ctx.index_map[key] = ctx.index_counter

    append_kw_entry(kw,'features','parse_vcxproj')

def SetupRunTimeLibraries(ctx, kw, overwrite_settings = None):
    """
    Util-function to set the correct flags and defines for the runtime CRT (and to keep non windows defines in sync with windows defines)
    By default CryEngine uses the "Multithreaded, dynamic link" variant (/MD)
    """
    runtime_crt = 'dynamic'                     # Global Setting
    if overwrite_settings:                      # Setting per Task Generator Type
        runtime_crt = overwrite_settings
    if kw.get('force_static_crt', False):       # Setting per Task Generator
        runtime_crt = 'static'
    if kw.get('force_dynamic_crt', False):      # Setting per Task Generator
        runtime_crt = 'dynamic'

    if runtime_crt != 'static' and runtime_crt != 'dynamic':
        ctx.fatal('Invalid Settings: "%s" for runtime_crt' % runtime_crt )

    crt_flag = []
    link_flag = []

    is_debug = 'debug' in get_configuration(ctx, kw['target'])
    is_msvc = ctx.env['CC_NAME'] == 'msvc'
    is_clang_windows = 'win_x64_clang' == ctx.env['PLATFORM']

    if runtime_crt == 'static':
        append_kw_entry(kw, 'defines', ['_MT'])
        if is_msvc:
            if is_debug:
                crt_flag = ['/MTd']
            else:
                crt_flag = ['/MT']
        elif is_clang_windows:
            if is_debug:
                link_flag = ['/DEFAULTLIB:libcmtd']
            else:
                link_flag = ['/DEFAULTLIB:libcmt']
    else: # runtime_crt == 'dynamic':
        append_kw_entry(kw, 'defines', ['_MT', '_DLL'])
        if is_msvc:
            if is_debug:
                crt_flag = ['/MDd']
            else:
                crt_flag = ['/MD']
        elif is_clang_windows:
            if is_debug:
                link_flag = ['/DEFAULTLIB:msvcrtd']
            else:
                link_flag = ['/DEFAULTLIB:msvcrt']

    append_kw_entry(kw, 'cflags', crt_flag)
    append_kw_entry(kw, 'cxxflags', crt_flag)
    append_kw_entry(kw, 'linkflags', link_flag)


def TrackFileListChanges(ctx, kw):
    """
    Util function to ensure file lists are correctly tracked regardless of current target platform
    """
    def _to_list( value ):
        """ Helper function to ensure a value is always a list """
        if isinstance(value,list):
            return value
        return [ value ]

    files_to_track = []
    kw['waf_file_entries'] = []

    # Collect all file list entries
    for (key,value) in kw.items():
        if 'file_list' in key:
            files_to_track += _to_list(value)
        # Collect potential file lists from additional options
        if 'additional_settings' in key:
            for settings_container in kw['additional_settings']:
                for (key2,value2) in settings_container.items():
                    if 'file_list' in key2:
                        files_to_track += _to_list(value2)

    # Remove duplicates
    files_to_track = list(set(files_to_track))

    # Add results to global lists
    for file in files_to_track:
        file_node = ctx.path.make_node(file)
        append_kw_entry(kw,'waf_file_entries',[ file_node ])


def LoadFileLists(ctx, kw, file_lists):
    """
    Util function to extract a list of needed source files, based on uber files and current command
    It expects that kw['file_list'] points to a valid file, containing a JSON file with the following mapping:
    Dict[ <UberFile> -> Dict[ <Project Filter> -> List[Files] ] ]
    """
    def _MergeFileList(in_0, in_1):
        """ Merge two file lists """
        result = dict(in_0)

        for (uber_file,project_filter) in in_1.items():
            for (filter_name,file_list) in project_filter.items():
                for file in file_list:
                    if not uber_file in result:
                        result[uber_file] = {}
                    if not filter_name in result[uber_file]:
                        result[uber_file][filter_name] = []
                    result[uber_file][filter_name].append(file)
        return result

    def _DisableUberFile(ctx, project_filter_list, files_marked_for_exclusion):
        for (filter_name, file_list) in project_filter_list.items():
            if any(ctx.path.make_node(file).abspath().lower() in files_marked_for_exclusion for file in file_list): # if file in exclusion list
                return True
        return False

    task_generator_files        = set() # set of all files in this task generator (store as abspath to be case insenstive)

    file_to_project_filter  = {}
    uber_file_to_file_list  = {}
    file_list_to_source     = {}
    file_list_content       = {}

    source_files            = set()
    no_uber_file_files      = set()
    header_files            = set()
    objc_source_files       = set()
    qt_source_files         = set()
    resource_files          = set()
    plist_files             = set()
    uber_files              = set()
    other_files             = set()

    target                  = kw['target']
    found_pch               = False
    pch_file                = kw.get('pch', '')
    platform                = ctx.env['PLATFORM']
    uber_file_folder        = ctx.get_bintemp_folder_node().make_node('uber_files/{}'.format(target))

    pch_node = ctx.path.make_node(pch_file)

    # Keep track of files per waf_file spec and be ready to identify duplicate ones per file
    file_list_to_file_collection = dict()
    file_list_to_duplicate_file_collection = dict()
    has_duplicate_files = False

    # Apply project override
    disable_uber_files_for_project = ctx.get_project_overrides(target).get('exclude_from_uber_file', False)

    files_marked_for_uber_file_exclusion = []
    if not disable_uber_files_for_project:
        for key, value in ctx.get_file_overrides(target).iteritems():
            if value.get('exclude_from_uber_file', False):
                files_marked_for_uber_file_exclusion.append(key)

    # Load file lists   and build all needed lookup lists
    for file_list_file in file_lists:

        # Prevent processing the same file twice
        if file_list_file in file_list_to_file_collection:
            continue
        # Prepare to collect the files per waf_file spec
        file_list_to_file_collection[file_list_file] = set()
        file_list_to_duplicate_file_collection[file_list_file] = set()

        # Read *.waf_file from disc
        file_list = ctx.read_file_list(file_list_file)

        # Make the file list relative to the .waf_files file
        file_list_relative_dir = os.path.dirname(file_list_file)
        if file_list_relative_dir != '':
            for (uber_file, project_filter_list) in file_list.items():
                for (filter_name, file_entries) in project_filter_list.items():
                    relative_file_entries = []
                    for file in file_entries:
                        if file.startswith('@ENGINE@'):
                            relative_file_entries.append(ctx.engine_node.make_node(file[len('@ENGINE@'):]).abspath())
                        elif os.path.isabs(file):
                            relative_file_entries.append(file)
                        else:
                            relative_file_entries.append(os.path.join(file_list_relative_dir, file))
                    project_filter_list[filter_name] = relative_file_entries

        # configure uber files
        if not disable_uber_files_for_project:
            # if there's a wart on this filename, use it as the token for generating uber file names
            # e.g. AzFramework_win.waf_files -> _win
            file_list_token = re.sub(target, '', file_list_file, flags=re.IGNORECASE).replace('.waf_files', '')
            file_list = ctx.map_uber_files(file_list, file_list_token, target)

        file_list_content = _MergeFileList(file_list_content, file_list)

        # Build various mappings/lists based in file just
        for (uber_file, project_filter_list) in file_list.items():

            # Disable uber file usage if defined by override parameter
            disable_uber_file = disable_uber_files_for_project or _DisableUberFile(ctx, project_filter_list, files_marked_for_uber_file_exclusion)

            if disable_uber_file:
                Logs.debug('[Option Override] - %s - Disabled uber file "%s"' %(target, uber_file))

            generate_uber_file = uber_file != 'none' and uber_file != 'NoUberFile' and not disable_uber_file # TODO: Deprecate 'NoUberfile'
            if generate_uber_file:

                if uber_file in uber_files:
                    ctx.cry_file_error('[%s] UberFile "%s" was specifed twice. Please choose a different name' % (kw['target'], uber_file), file_list_file)

                # Collect Uber file related information
                uber_file_node = uber_file_folder.make_node(uber_file)
                uber_file_node_abs = uber_file_node.abspath()
                task_generator_files.add(uber_file_node_abs)
                uber_files.add(uber_file_node)

                file_to_project_filter[uber_file_node_abs] = 'Uber Files'

            for (filter_name, file_entries) in project_filter_list.items():
                for file_entry in file_entries:

                    if file_entry.startswith('@ENGINE@'):
                        file_node = ctx.engine_node.make_node(file_entry[len('@ENGINE@'):])
                        file = file_node.abspath()
                    else:
                        if os.path.isabs(file_entry):
                            file_node = ctx.root.make_node(file_entry)
                        else:
                            file_node = ctx.path.make_node(file_entry)
                        file = file_entry

                    filenode_abs_path = file_node.abspath()

                    # Keep track of files for file_list file and track which ones are duplicate
                    if (filenode_abs_path in file_list_to_file_collection[file_list_file]):
                        file_list_to_duplicate_file_collection[file_list_file].add(filenode_abs_path)
                        has_duplicate_files = True
                    else:
                        file_list_to_file_collection[file_list_file].add(filenode_abs_path)

                    task_generator_files.add(filenode_abs_path)

                    # Collect per file information
                    if file_node.abspath() == pch_node.abspath():
                        # PCHs are not compiled with the normal compilation, hence don't collect them
                        found_pch = True

                    elif ctx.is_cxx_file(file):
                        source_files.add(file_node)
                        if not generate_uber_file:
                            no_uber_file_files.add(file_node)
                    elif file.endswith(('.mm', '.m')):
                        objc_source_files.add(file_node)
                    elif file.endswith(('.ui', '.qrc', '.ts')):
                        qt_source_files.add(file_node)
                    elif file.endswith(('.h', '.H', '.hpp', '.HPP', '.hxx', '.HXX')):
                        header_files.add(file_node)
                    elif file.endswith(('.rc', '.r')):
                        resource_files.add(file_node)
                    elif file.endswith('.plist'):
                        plist_files.add(file_node)
                    else:
                        other_files.add(file_node)

                    # Build file name -> Visual Studio Filter mapping
                    file_to_project_filter[filenode_abs_path] = filter_name

                    # Build list of uber files to files
                    if generate_uber_file:
                        uber_file_abspath = uber_file_node.abspath()
                        if not uber_file_abspath in uber_file_to_file_list:
                            uber_file_to_file_list[uber_file_abspath]   = []
                        uber_file_to_file_list[uber_file_abspath]       += [ file_node ]

            # Remember which sources come from which file list (for later lookup)
            file_list_to_source[file_list_file] = list(source_files | qt_source_files)

    # Report any files that were duplicated within a file_list spec
    if has_duplicate_files:
        for (error_file_list,error_file_set) in file_list_to_duplicate_file_collection.items():
            if len(error_file_set) > 0:
                for error_file in error_file_set:
                    Logs.error('[ERROR] file "%s" was specifed more than once in file spec %s' % (str(error_file), error_file_list))
        ctx.fatal('[ERROR] One or more files errors detected for target %s.' % (kw['target']))

    # Compute final source list based on platform
    if platform == 'project_generator' or ctx.options.file_filter != "":
        # Collect all files plus uber files for project generators and when doing a single file compilation
        if ctx.cmd == 'info':
            kw['source'] = source_files | qt_source_files | objc_source_files | header_files | resource_files | other_files
        else:
            kw['source'] = uber_files | source_files | qt_source_files | objc_source_files | header_files | resource_files | other_files
        kw['mac_plist'] = list(plist_files)

        if platform == 'project_generator' and pch_file != '':
            kw['source'].add(pch_node) # Also collect PCH for project generators

    else:
        # Regular compilation path
        # Note: Always sort the file list so that it is stable between recompilation.  WAF uses the order of input files to generate the UID of the task,
        # and if the order changes, it will cause recompilation!

        kw['source'] = []

        if found_pch and not ctx.is_option_true('use_precompiled_header'):
            kw['source'].append(pch_node) # Also collect PCH for when not using PCH for intended task

        if ctx.is_option_true('use_uber_files'):
            # Only take uber files when uber files are enabled and files not using uber files
            kw['source'].extend(sorted(uber_files | no_uber_file_files | qt_source_files, key=lambda file: file.abspath()))
        else:
            # Fall back to pure list of source files
            kw['source'].extend(sorted(source_files | qt_source_files, key=lambda file: file.abspath()))

        # Append platform specific files
        if ctx.is_apple_platform(platform):
            kw['source'].extend(sorted(objc_source_files, key=lambda file: file.abspath()))
            kw['mac_plist'] = list(sorted(plist_files, key=lambda file:file.abspath()))

        elif ctx.is_windows_platform(platform):
            kw['source'].extend(sorted(resource_files, key=lambda file: file.abspath()))

    # Handle PCH files
    if pch_file != '' and found_pch == False:
        # PCH specified but not found
        ctx.cry_file_error('[%s] Could not find PCH file "%s" in provided file list (%s).\nPlease verify that the name of the pch is the same as provided in a WAF file and that the PCH is not stored in an UberFile.' % (kw['target'], pch_file, ', '.join(file_lists)), 'wscript' )

    # Try some heuristic when to use PCH files
    #if ctx.is_option_true('use_uber_files') and found_pch and len(uber_file_relative_list) > 0 and ctx.options.file_filter == "" and ctx.cmd != 'generate_uber_files':
        # Disable PCH files when having UberFiles as they  bring the same benefit in this case
        #kw['pch_name'] = kw['pch']
        #del kw['pch']

    # Store global lists in context
    kw['task_generator_files']  = task_generator_files
    kw['file_list_content']     = file_list_content
    kw['project_filter']        = file_to_project_filter
    kw['uber_file_lookup']      = uber_file_to_file_list
    kw['file_list_to_source']   = file_list_to_source
    kw['header_files']          = sorted(header_files, key=lambda file: file.abspath())

    # In the uber files, paths are relative to the current module path, so make sure the root of the project is included in the include search path
    # include it first to avoid wrong files being picked from other modules (e.g. picking stdafx.h from another module)
    includeList = kw.setdefault('includes', [])
    if '.' in includeList:
      includeList.remove('.')
    includeList.insert(0, '.')


def VerifyInput(ctx, kw):
    """
    Helper function to verify passed input values
    """

    # 'target' is required
    target_name = kw['target']

    wscript_file = ctx.path.make_node('wscript').abspath()
    if kw['file_list'] == []:
        ctx.cry_file_error('TaskGenerator "%s" is missing mandatory parameter "file_list"' % target_name, wscript_file )

    if 'source' in kw:
        ctx.cry_file_error('TaskGenerator "%s" is using unsupported parameter "source", please use "file_list"' % target_name, wscript_file )

    # Loop through and check the follow type of keys that represent paths and validate that they exist on the system.
    # If they do not exist, this does not mean its an error, but we want to be able to display a warning instead as
    # having unnecessary paths in the include paths affects build performance
    path_check_key_values = ['includes','libpath']

    # Validate the paths only during the build command execution and exist in the spec
    if ctx.cmd.startswith('build'):

        if ctx.is_target_enabled(target_name):

            # Validate the include paths and show warnings for ones that dont exist (only for the currently building platform
            # and only during the build command execution)
            current_platform, current_configuration = ctx.get_platform_and_configuration()
            
            platform_details = ctx.get_target_platform_detail(current_platform)
            # Special case: If the platform's alias include BOTH 'win' and 'msvc', reduce it to 'win'
            if {'win', 'msvc'}.issubset(platform_details.aliases):
                current_platform = 'win'

            azcg_build_path = os.path.normcase(ctx.bldnode.make_node('azcg').abspath())

            # Search for the keywords in 'path_check_key_values'
            for kw_check in kw.keys():
                for path_check_key in path_check_key_values:
                    if kw_check == path_check_key or (kw_check.endswith('_' + path_check_key) and kw_check.startswith(current_platform)):
                        path_check_values = kw[kw_check]
                        if path_check_values is not None:
                            # Make sure we are working with a list of strings, not a string
                            if isinstance(path_check_values,str):
                                path_check_values = [path_check_values]
                            for path_check in path_check_values:
                                if isinstance(path_check,str):
                                    # If the path input is a string, derive the absolute path for the input path
                                    path_to_validate = os.path.normcase(os.path.join(ctx.path.abspath(),path_check))
                                else:
                                    # If the path is a Node object, get its absolute path
                                    path_to_validate = os.path.normcase(path_check.abspath())

                                # Path validation can be skipped for azcg because it may not have been generated yet
                                is_azcg_path = path_to_validate.startswith(azcg_build_path)

                                if not os.path.exists(path_to_validate) and not is_azcg_path:
                                    Logs.warn('[WARNING] \'{}\' value \'{}\' defined in TaskGenerator "{}" does not exist'.format(kw_check,path_to_validate,target_name))

def InitializeTaskGenerator(ctx, kw):
    """
    Helper function to call all initialization routines required for a task generator
    """

    apply_default_keywords(ctx, kw)
    AddGlobalKeywords(ctx, kw)

    SanitizeInput(ctx, kw)
    VerifyInput(ctx, kw)
    AssignTaskGeneratorIdx(ctx, kw)
    RegisterVisualStudioFilter(ctx, kw)
    TrackFileListChanges(ctx, kw)

    return True


def apply_default_keywords(ctx, kw):
    """
    Apply default keywords if they are not provided in the keyword dictionary
    :param ctx:     Context
    :param kw:      Keyword Dictionary
    """
    if 'platforms' not in kw:
        kw['platforms'] = ['all']
    if 'configurations' not in kw:
        kw['configurations'] = ['all']


def apply_cryengine_module_defines(ctx, kw):

    additional_defines = []
    ctx.add_aws_native_sdk_platform_defines(additional_defines)
    #additional_defines.append('LY_BUILD={}'.format(ctx.get_lumberyard_build()))
    append_kw_entry(kw, 'defines', additional_defines)


# Append any common static modules to the configuration
def AppendCommonModules(ctx,kw):

    common_modules_dependencies = []

    if not 'use' in kw:
        kw['use'] = []
        
    if not isinstance(ctx, ConfigurationContext) and 'project_generator' != ctx.env['PLATFORM']:
        # Append common module's dependencies
        bcrypt_required_platforms = ALIAS_TO_PLATFORMS_MAP.get('win', set()) | ALIAS_TO_PLATFORMS_MAP.get('msvc', set())
        if any(p == ctx.env['PLATFORM'] for p in bcrypt_required_platforms):  # ACCEPTED_USE
            common_modules_dependencies = ['bcrypt']

    if 'test' in ctx.env['CONFIGURATION'] or 'project_generator' == ctx.env['PLATFORM'] or isinstance(ctx, ConfigurationContext):
        append_to_unique_list(kw['use'], 'AzTest')
        if 'uselib' not in kw:
            kw['uselib'] = []
        append_to_unique_list(kw['uselib'], 'GMOCK')

    append_kw_entry(kw,'lib',common_modules_dependencies)

def LoadAdditionalFileSettings(ctx, kw):
    """
    Load all settings from the additional_settings parameter, and store them in a lookup map
    """
    append_kw_entry(kw,'features',[ 'apply_additional_settings' ])
    kw['file_specifc_settings'] = {}

    for setting in kw['additional_settings']:

        setting['target'] = kw['target'] # reuse target name

        file_list = []

        if 'file_list' in setting:
            # Option A: The files are specifed as a *.waf_files (which is loaded already)
            for list in setting['file_list']:
                file_list += kw['file_list_to_source'][list]

        if 'files' in setting:
            # Option B: The files are already specified as an list
            file_list += setting['files']

        if 'regex' in setting:
            # Option C: A regex is specifed to match the files
            p = re.compile(setting['regex'])

            for file in kw['source']:
                if p.match(file):
                    file_list += [file]

        # insert files into lookup dictonary, but make sure no uber file and no file within an uber file is specified
        uber_file_folder = ctx.bldnode.make_node('..')
        uber_file_folder = uber_file_folder.make_node('uber_files')
        uber_file_folder = uber_file_folder.make_node(kw['target'])

        for file in file_list:
            if isinstance(file, Node.Node):
                file_abspath = file.abspath()
            elif os.path.isabs(file):
                file_abspath = file
            else:
                file_abspath = ctx.path.make_node(file).abspath()

            if 'uber_file_lookup' in kw:
                for uber_file in kw['uber_file_lookup']:

                    # Uber files are not allowed for additional settings
                    if file_abspath == uber_file:
                        ctx.cry_file_error("Additional File Settings are not supported for UberFiles (%s) to ensure a consistent behavior without UberFiles, please adjust your setup" % file, ctx.path.make_node('wscript').abspath())

                    for entry in kw['uber_file_lookup'][uber_file]:
                        if file_abspath == entry.abspath():
                            ctx.cry_file_error("Additional File Settings are not supported for file using UberFiles (%s) to ensure a consistent behavior without UberFiles, please adjust your setup" % file, ctx.path.make_node('wscript').abspath())

            # All fine, add file name to dictonary
            kw['file_specifc_settings'][file_abspath] = setting
            setting['source'] = []


def ConfigureTaskGenerator(ctx, kw):
    """
    Helper function to apply default configurations and to set platform/configuration dependent settings
    """

    target = kw['target']

    # Ensure we have a name for lookup purposes
    if 'name' not in kw:
        kw['name'] = target

    # Process any platform roots
    platform_roots = kw.get('platform_roots', [])
    if platform_roots:
        if not isinstance(platform_roots, list):
            platform_roots = [platform_roots]
        for platform_root_param in platform_roots:
            if not isinstance(platform_root_param,dict):
                raise Errors.WafError("Invalid keyword value for 'platform_roots' for target '{}'. Expecting a list of "
                                      "dictionary entries for 'root' (path) and 'export_includes' (boolean)".format(target))
            platform_root = platform_root_param['root']
            export_platform_includes = platform_root_param['export_includes']
            ctx.add_platform_root(kw=kw,
                                  root=platform_root,
                                  export=export_platform_includes)

    # Deal with restricted platforms
    ctx.process_restricted_settings(kw)

    # Determine if this is a build command (vs a platform generator)
    is_configure_cmd = isinstance(ctx, ConfigurationContext)
    is_build_platform_cmd = getattr(ctx, 'is_build_cmd', False)


    # Lookup the PlatformConfiguration for the current platform/configuration (if this is a build command)
    current_platform_configuration = None
    target_platform = []
    target_configuration = []

    if is_build_platform_cmd:

        target_platform = ctx.target_platform
        target_configuration = ctx.target_configuration
        current_platform_configuration = ctx.get_platform_configuration(target_platform, target_configuration)

        # Provide the module name to the test framework.
        if current_platform_configuration.is_test and not ctx.is_mac_platform(target_platform) and not ctx.is_linux_platform(target_platform):
            module_define = 'AZ_MODULE_NAME="{}"'.format(target.upper())
            append_kw_entry(kw, 'defines', module_define)

    # Special case:  Only non-android launchers can use required gems
    apply_required_gems = kw.get('use_required_gems', False)
    if kw.get('is_launcher', False):
        if apply_required_gems and not ctx.is_android_platform(target_platform):
            ctx.apply_required_gems_to_context(target, kw)
    else:
        if apply_required_gems:
            ctx.apply_required_gems_to_context(target, kw)

    # Apply all settings, based on current platform and configuration
    ApplyConfigOverwrite(ctx, kw)

    ApplyPlatformSpecificSettings(ctx, kw, target)
    ApplyBuildOptionSettings(ctx, kw)

    platform = ctx.env['PLATFORM']

    is_engine_project = ctx.path.is_child_of(ctx.engine_node)

    # Load all file lists (including additional settings)
    file_list = kw['file_list']
    for setting in kw['additional_settings']:
        file_list += setting.get('file_list', [])
        file_list += ctx.GetPlatformSpecificSettings(setting, 'file_list', platform, get_configuration(ctx, kw['target']) )
    # Load all configuration specific files when generating projects
    if platform == 'project_generator':
        for configuration in ctx.get_all_configuration_names():
            file_list += ctx.GetPlatformSpecificSettings(kw, 'file_list', platform, configuration)
        for alias in LUMBERYARD_SETTINGS.get_configuration_aliases():
            file_list += ctx.GetPlatformSpecificSettings(kw, 'file_list', platform, alias)

    LoadFileLists(ctx, kw, file_list)
    LoadAdditionalFileSettings(ctx, kw)

    # If uselib is set, validate them
    uselib_names = kw.get('uselib', None)
    if uselib_names is not None:
        processed_dependencies = []
        for uselib_name in uselib_names:
            if not is_third_party_uselib_configured(ctx, uselib_name):
                Logs.warn("[WARN] Invalid uselib '{}' declared in project {}.  This may cause compilation or linker errors".format(uselib_name,target))
            else:
                if not uselib_name in processed_dependencies and not uselib_name in kw.get('no_inherit_config', []):
                    ctx.append_dependency_configuration_settings(uselib_name, kw)
                    processed_dependencies.append(uselib_name)

    # Make sure we have a 'use' list
    if not kw.get('use', None):
        kw['use'] = []

    if platform != 'project_generator':
        # Check if we are applying external file copies
        if 'copy_external' in kw and len(kw['copy_external'])>0:
            for copy_external_key in kw['copy_external']:
                copy_external_env_key = 'COPY_EXTERNAL_FILES_{}'.format(copy_external_key)
                if 'COPY_EXTERNAL_FILES' not in ctx.env:
                    ctx.env['COPY_EXTERNAL_FILES'] = []
                append_kw_entry(kw,'features','copy_external_files')
                if copy_external_env_key in ctx.env:
                    for copy_external_value in ctx.env[copy_external_env_key]:
                        ctx.env['COPY_EXTERNAL_FILES'].append(copy_external_value)

        # Check if we are applying external file copies to specific files
        copy_dependent_files = kw.get('copy_dependent_files',[])
        if len(copy_dependent_files)>0:
            append_kw_entry(kw,'features','copy_module_dependent_files')
            copy_dependent_env_key = 'COPY_DEPENDENT_FILES_{}'.format(target.upper())
            if copy_dependent_env_key not in ctx.env:
                ctx.env[copy_dependent_env_key] = []
            for copy_dependent_file in copy_dependent_files:
                if is_engine_project:
                    copy_dependent_file_abs = os.path.normpath(os.path.join(ctx.engine_path,copy_dependent_file))
                else:
                    copy_dependent_file_abs = os.path.normpath(os.path.join(ctx.path.abspath(),copy_dependent_file))
                ctx.env[copy_dependent_env_key].append(copy_dependent_file_abs)

    if ctx.is_windows_platform(platform):
        # Handle meta includes for WinRT
        for meta_include in kw.get('meta_includes', []):
            append_kw_entry(kw,'cxxflags',[ '/AI' + meta_include ])

    # Handle export definitions file
    append_kw_entry(kw,'linkflags',[ '/DEF:' + ctx.path.make_node( export_file ).abspath() for export_file in kw['export_definitions']])

    # Handle Spec unique defines (if one is provided)
    if ctx.is_project_spec_specified():
        append_kw_entry(kw, 'defines', ctx.get_current_spec_defines())

    # Generate output file name (without file ending), use target as an default if nothing is specified
    if kw['output_file_name'] == []:
        kw['output_file_name'] = kw['target']
    elif isinstance(kw['output_file_name'],list):
        kw['output_file_name'] = kw['output_file_name'][0] # Change list into a single string

    # Handle force_disable_mfc (Small Hack for Perforce Plugin (no MFC, needs to be better defined))
    if kw.get('force_disable_mfc', False) and '_AFXDLL' in kw['defines']:
        kw['defines'].remove('_AFXDLL')

    # Clean out some duplicate kw values to reduce the size for the hash calculation
    kw['defines'] = clean_duplicates_in_list(kw['defines'],'{} : defines'.format(target))

    # Apply the default copyright_org if none is specified
    if len(kw['copyright_org'])==0:
        kw['copyright_org'] = ['Amazon']

    if ctx.is_build_monolithic():
        append_kw_entry(kw, 'defines', ['_LIB', 'AZ_MONOLITHIC_BUILD'])
        
    if kw.get('inject_copyright', True):
        append_kw_entry(kw,'features',[ 'generate_rc_file' ])


def RunTaskGenerator(ctx, *k, **kw ):

    COPY_TARGETS_INDEX = 0
    CTX_TYPE_INDEX = 1

    if BUILD_TYPE_KW not in kw or BUILD_FUNC_KW not in kw:
        raise Errors.WafError("'RunTaskGenerator is not called from a 'build_*' decorated function: {}.".format(inspect.stack()[1][3]))
    build_type = kw[BUILD_TYPE_KW]

    # At this point we dont need the special build function keywords, delete them so then dont get attached to the task
    del kw[BUILD_FUNC_KW]
    del kw[BUILD_TYPE_KW]

    if build_type not in BUILD_PROCESS_TABLE:
        ctx.fatal('[ERROR] Unsupported build target type: {}'.format(build_type))
        return None

    if build_type in ('program', 'shlib'):
        # Make it so that this kind of module can link even if the program is currently running.
        append_kw_entry(kw, 'features', ['link_running_program'])
    target = kw['target']
    if ctx.env['STUB_ST'] and build_type == 'shlib':
        kw['output_stub_name'] = kw.get('output_file_name', target)
    if ctx.env['ALT_STUB_ST'] and build_type == 'shlib':
        kw['alt_output_stub_name'] = kw.get('output_file_name', target)

    return getattr(ctx, BUILD_PROCESS_TABLE[build_type][CTX_TYPE_INDEX])(*k, **kw)


@feature('fake_extern_engine_lib')
@after_method('copy_module_dependent_files')
def process_extern_engine_lib(self):
    """
    Feature implementation that will replaces a normal shlib or stlib task definition that normally takes in
     source files but instead will create a 'fake' library definition from a prebuilt binary.  It also does
     an extra step by copying the prebuilt binary to its target destination

    :param self:    Context
    """
    def _copy_file(src, dst):
        if should_overwrite_file(src, dst):
            try:
                # In case the file is readonly, we'll remove the existing file first
                if os.path.exists(dst):
                    os.chmod(dst, stat.S_IWRITE)
                fast_copy2(src, dst)
            except:
                Logs.warn('[WARN] Unable to copy {} to destination {}.  '
                          'Check the file permissions or any process that may be locking it.'
                          .format(src, dst))

        pass

    def _copy_dir(src, dst_path_base):

        basename = os.path.basename(src)
        dst_path_abs = os.path.join(dst_path_base,basename)
        if not os.path.exists(dst_path_abs):
            os.mkdir(dst_path_abs)

        items = os.listdir(src)
        for item in items:
            src_item_abs = os.path.join(src, item)
            if os.path.isdir(src_item_abs):
                _copy_dir(src_item_abs, dst_path_abs)
            else:
                dst_item_abs = os.path.join(dst_path_abs, item)
                _copy_file(src_item_abs, dst_item_abs)

    # Perform the copy of files here if needed, without using WAF's task dependency
    base_target_path = self.target_folder
    target_paths = [base_target_path]
    output_subfolder_copies = getattr(self,'output_sub_folder_copy',[]) or []
    for output_subfolder_copy in output_subfolder_copies:
        output_subfolder_path = os.path.realpath(os.path.join(base_target_path,output_subfolder_copy))
        if not os.path.exists(output_subfolder_path):
            try:
                os.makedirs(output_subfolder_path)
                target_paths.append(output_subfolder_path)
            except:
                Logs.warn('[WARN] Target folder "{}" for engine output cannot be created.'.format(output_subfolder_path))

    for source_path in getattr(self,'source_paths',[]):
        if os.path.isfile(source_path):
            for target_path in target_paths:
                dest_path = os.path.join(target_path,os.path.basename(source_path))
                _copy_file(source_path, dest_path)
        elif os.path.isdir(source_path):
            for target_path in target_paths:
                _copy_dir(source_path, target_path)

    # Calculate the module target

    # Create a link task only for stlib or shlib
    if self.lib_type in ('stlib','shlib'):

        output_filename = self.env['c{}_PATTERN'.format(self.lib_type)] % self.output_name
        output_file = os.path.join(self.target_folder, output_filename)
        if not os.path.exists(output_file):
            raise Errors.WafError('could not find library %r' % self.output_name)

        output_file_node = self.bld.root.find_node(output_file)
        output_file_node.cache_sig = Utils.h_file(output_file_node.abspath())
        self.link_task = self.create_task('fake_%s' % self.lib_type, [], [output_file_node])
        if not getattr(self, 'target', None):
            self.target = self.name
        if not getattr(self, 'output_file_name', None):
            self.output_file_name = self.target


def MonolithicBuildModule(ctx, *k, **kw):
    """
    Util function to collect all libs and linker settings for monolithic builds
    (Which apply all of those only to the final link as no DLLs or libs are produced)
    """
    # Set up member for monolithic build settings
    if not hasattr(ctx, 'monolithic_build_settings'):
        ctx.monolithic_build_settings = defaultdict(lambda: [])

    # For game specific modules, store with a game unique prefix
    prefix = ''
    if kw.get('game_project', False):
        prefix = kw['game_project'] + '_'

    # Collect libs for later linking
    def _append(key, values):
        if not ctx.monolithic_build_settings.get(key):
            ctx.monolithic_build_settings[key] = []
        ctx.monolithic_build_settings[key] += values

    def _append_linker_options():
        for setting in ['stlib', 'stlibpath', 'lib', 'libpath', 'linkflags', 'framework']:
            _append(prefix + setting, kw[setting] )
            _append(prefix + setting, ctx.GetPlatformSpecificSettings(kw, setting, ctx.env['PLATFORM'], ctx.env['CONFIGURATION']))

    # If this is a cryengine module, then it is marked to be included in all monolithic applications implicitly
    is_cryengine_module = kw.get('is_cryengine_module', False)
    if is_cryengine_module:
        _append(prefix + 'use',         [ kw['target'] ] )
        _append_linker_options()
    # If this is a gem we need to be sure the linker options apply to the monolithic application, but do not want to reapply use
    # because it can duplicate symbols
    elif kw.get('is_gem', False):
        _append_linker_options()

    if 'uselib' in kw:
        _append(prefix + 'uselib',        kw['uselib'] )

    # Remove rc files from the sources for monolithic builds (only the rc of
    # the launcher will be used) and remove any duplicate files that may have
    # sneaked in as well (using the python idiom: list(set(...)) to do so
    kw['source'] = [file for file in kw['source'] if not file.abspath().endswith('.rc')]

    return ctx.objects(*k, **kw)

###############################################################################
def BuildTaskGenerator(ctx, kw):

    """
    Check if this task generator should be build at all in the current configuration
    """
    target = kw['target']
    if BUILD_FUNC_KW not in kw or BUILD_TYPE_KW not in kw:
        raise Errors.WafError("'BuildTaskGenerator is not called from a 'build_*' decorated function: {}.".format(inspect.stack()[1][3]))
    module_type = kw[BUILD_FUNC_KW]
    build_type = kw[BUILD_TYPE_KW]

    if isinstance(ctx, ConfigurationContext):
        ctx.update_module_definition(module_type, build_type, kw)
        return False

    # Assign a deterministic UID to this target
    ctx.assign_target_uid(kw)

    current_platform = ctx.env['PLATFORM']
    current_configuration = ctx.env['CONFIGURATION']
    if ctx.cmd == 'configure':

        # During the configure process, do an extra check on the module's declared 'use' or 'uselib' to see if it matches
        # any tagged invalid uselibs that was detected during the 3rd party initialization.
        if hasattr(ctx,'InvalidUselibs'):
            declared_uselibs = kw.get('uselib',[]) + kw.get('use',[])
            illegal_uselib = []
            for declared_uselib in declared_uselibs:
                if declared_uselib in ctx.InvalidUselibs:
                    illegal_uselib.append(declared_uselib)
            if len(illegal_uselib)>0:
                Logs.warn("[WARN] Module '{}' may fail to build due to invalid use or uselib dependencies ({})".format(target,','.join(illegal_uselib)))

        return False        # Dont build during configure

    if ctx.cmd == 'generate_uber_files':
        ctx(features='generate_uber_file', uber_file_list=kw['file_list_content'], target=target, pch=os.path.basename( kw.get('pch', '') ))
        return False        # Dont do the normal build when generating uber files

    if ctx.cmd == 'generate_module_def_files':
        ctx(features='generate_module_def_files',
            use_module_list=kw['use'],
            platform_list=kw.get('platforms', []),
            configuration_list=kw.get('configurations', []),
            export_internal_3p_libs=kw.get('export_internal_3rd_party_libs', False),
            target=target)
        return False

    if ctx.cmd == 'generate_module_dependency_files':  # Command used by Integration Toolkit (ly_integration_toolkit.py)
        ctx(features='generate_module_dependency_files',
            use_module_list=kw['use'],
            platform_list=kw.get('platforms', []),
            configuration_list=kw.get('configurations', []),
            target=target,
            waf_files=kw.get('waf_file_entries', []))
        return False

    if current_platform == 'project_generator':
        return True         # Always include all projects when generating project for IDEs

    if kw and kw.get('dx12_only',False):
        if not ctx.has_dx12():
            return False

    if not ctx.is_valid_platform_request(**kw):
        return False

    if not ctx.is_valid_configuration_request(**kw):
        return False
    
    # If this is a unit test module for a static library (see CryEngineStaticLibrary), then enable it if its unit test target
    # is enabled as well
    unit_test_target = kw.get('unit_test_target')
    if unit_test_target and ctx.is_target_enabled(unit_test_target):
        Logs.debug('lumberyard: module {} enabled for platform {}.'.format(target, current_platform))
        return True

    if ctx.is_target_enabled(target):
        Logs.debug('lumberyard: module {} enabled for platform {}.'.format(target, current_platform))
        return True     # Skip project is it is not part of the current spec

    Logs.debug('lumberyard: disabled module %s because it is not in the current list of spec modules' % target)
    return False



@feature('apply_additional_settings')
@before_method('extract_vcxproj_overrides')
def tg_apply_additional_settings(self):
    """
    Apply all settings found in the additional_settings parameter after all compile tasks are generated
    """
    if len(self.file_specifc_settings) == 0:
        return # no file specific settings found

    for t in getattr(self, 'compiled_tasks', []):
        input_file = t.inputs[0].abspath()

        file_specific_settings = self.file_specifc_settings.get(input_file, None)

        if not file_specific_settings:
            continue

        t.env['CFLAGS']     += file_specific_settings.get('cflags', [])
        t.env['CXXFLAGS'] += file_specific_settings.get('cxxflags', [])
        t.env['DEFINES']    += file_specific_settings.get('defines', [])

        for inc in file_specific_settings.get('defines', []):
            if os.path.isabs(inc):
                t.env['INCPATHS'] += [ inc ]
            else:
                t.env['INCPATHS'] += [ self.path.make_node(inc).abspath() ]

###############################################################################
def find_file_in_content_dict(content_dict, file_name):
    """
    Check if a file exists in the content dictionary
    """
    file_name_search_key = file_name.upper()
    for uber_file_name in iter(content_dict):
        vs_filter_dict = content_dict[uber_file_name]
        for vs_filter_name in iter(vs_filter_dict):
            source_files = vs_filter_dict[vs_filter_name]
            for source_file in source_files:
                if source_file.upper() == file_name_search_key:
                    return True
                # Handle the (posix) case if file_name is in a different folder than the context root
                if source_file.upper().endswith('/'+file_name_search_key):
                    return True
                # Handle the (dos) case if file_name is in a different folder than the context root
                if source_file.upper().endswith('\\'+file_name_search_key):
                    return True



    return False

###############################################################################
@conf
def ConfigureTarget(ctx, *k, **kw):
    if not isinstance(ctx, ConfigurationContext):
        return False
    return ctx.update_module_definition(kw)


@build_shlib
def CryEngineModule(ctx, *k, **kw):
    """
    CryEngine Modules are mostly compiled as dynamic libraries
    Except the build configuration requires a monolithic build
    """
    # Initialize the Task Generator
    if not InitializeTaskGenerator(ctx, kw):
        return

    # Append common modules
    AppendCommonModules(ctx,kw)

    # Setup TaskGenerator specific settings
    apply_cryengine_module_defines(ctx, kw)

    SetupRunTimeLibraries(ctx, kw)

    if hasattr(ctx, 'game_project'):

        if ctx.is_android_platform(ctx.env['PLATFORM']) and ctx.game_project is not None:
            if ctx.get_android_settings(ctx.game_project) == None:
                Logs.warn('[WARN] Game project - %s - not configured for Android.  Skipping...' % ctx.game_project)
                return

        kw['game_project'] = ctx.game_project
        if kw.get('use_gems', False):
            # if this is defined it means we need to add all the defines, includes and such that the gem provides
            # to this project.
            ctx.apply_gems_to_context(ctx.game_project, k, kw)


    LoadSharedSettings(ctx,k,kw)

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return None

    if ctx.is_build_monolithic():
        # For monolithic builds, simply collect all build settings
        kw['is_cryengine_module'] = True
        return MonolithicBuildModule(ctx, getattr(ctx, 'game_project', None), *k, **kw)

    if ctx.env['PLATFORM'] == 'darwin_x64':
        append_kw_entry(kw,'linkflags',['-install_name', '@rpath/lib{}.dylib'.format(kw['output_file_name'])])

    return RunTaskGenerator(ctx, *k, **kw)


###############################################################################
@build_shlib
def CryEngineSharedLibrary(ctx, *k, **kw):
    """
    Definition for shared libraries.  This is not considered a module, so it will not be implicitly included
    in project dependencies.
    """
    # Initialize the Task Generator
    if not InitializeTaskGenerator(ctx, kw):
        return

    # Append common modules
    AppendCommonModules(ctx,kw)

    # Setup TaskGenerator specific settings
    apply_cryengine_module_defines(ctx, kw)

    SetupRunTimeLibraries(ctx,kw)

    LoadSharedSettings(ctx,k,kw)

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return None

    if ctx.is_build_monolithic():
        # For monolithic builds, simply collect all build settings
        kw['is_cryengine_module'] = False
        return MonolithicBuildModule(ctx, getattr(ctx, 'game_project', None), *k, **kw)

    if ctx.env['PLATFORM'] == 'darwin_x64':
        append_kw_entry(kw,'linkflags',['-install_name', '@rpath/lib'+kw['output_file_name']+'.dylib'])

    return RunTaskGenerator(ctx, *k, **kw)


###############################################################################
@build_stlib
def CryEngineStaticLibraryLegacy(ctx, *k, **kw):
    """
    CryEngine Static Libraries are static libraries
    Except the build configuration requires a monolithic build
    """
    # Initialize the Task Generator
    if not InitializeTaskGenerator(ctx, kw):
        return

    apply_cryengine_module_defines(ctx, kw)

    SetupRunTimeLibraries(ctx, kw)

    LoadSharedSettings(ctx,k,kw)

    ConfigureTaskGenerator(ctx, kw)

    kw['stlib'] = True

    if not BuildTaskGenerator(ctx, kw):
        return None

    if ctx.cmd == 'generate_uber_files':
        return ctx(features='generate_uber_file', uber_file_list=kw['file_list_content'], target=kw['target'], pch=os.path.basename( kw.get('pch', '') ))

    if ctx.is_build_monolithic():
        append_kw_entry(kw, 'defines',[ 'AZ_MONOLITHIC_BUILD' ])

    append_kw_entry(kw,'features',['c', 'cxx', 'cstlib', 'cxxstlib', 'use'])

    return RunTaskGenerator(ctx, *k, **kw)


###############################################################################
@build_stlib
def CryEngine3rdPartyStaticLibrary(ctx, *k, **kw):
    """
    CryEngine Static Libraries are static libraries
    Except the build configuration requires a monolithic build
    """
    # Initialize the Task Generator
    if not InitializeTaskGenerator(ctx, kw):
        return
    apply_cryengine_module_defines(ctx, kw)

    SetupRunTimeLibraries(ctx, kw)

    LoadSharedSettings(ctx,k,kw)

    ConfigureTaskGenerator(ctx, kw)

    # We will only consider (re)building the 3rd party lib if the spec is set to '3rd_party'
    if '3rd_party' != getattr(ctx.options, 'project_spec', ''):
        return

    if 'base_path' not in kw:
        ctx.fatal('Mandatory "base_path" task generater parameter for 3rd Party Static Libraries missing in %s/wscript' % ctx.path.abspath())
    base_path = kw['base_path']

    # For 3rd party libraries, the output folder is calculated based on the base path, fixed name 'build', platform and configuration
    platform_key = ctx.env['PLATFORM']
    if platform_key != 'project_generator' and ctx.cmd != 'configure':

        platform_shortname = get_third_party_platform_name(ctx, platform_key)

        configuration_key = get_configuration(ctx, kw['target'])

        configuration_name = get_third_party_configuration_name(ctx, configuration_key)

        target_path = os.path.normcase(os.path.join(base_path, 'build', platform_shortname, configuration_name))
        kw['output_folder'] = target_path
        kw['stlib'] = True

    if not BuildTaskGenerator(ctx, kw):
        return None

    if ctx.cmd == 'generate_uber_files':
        return ctx(features='generate_uber_file', uber_file_list=kw['file_list_content'], target=kw['target'], pch=os.path.basename( kw.get('pch', '') ))

    append_kw_entry(kw,'features',['c', 'cxx', 'cstlib', 'cxxstlib', 'use','generate_3p_static_lib_config'])

    return RunTaskGenerator(ctx, *k, **kw)


@build_program
def CryLauncher(ctx, *k, **kw):
    """
    Wrapper for CryEngine Executables
    """

    # Copy kw dict and some internal values to prevent overwriting settings in one launcher from another
    apply_cryengine_module_defines(ctx, kw)

    if ctx.env['PLATFORM'] != 'project_generator':  # if we're making project files for an IDE, then don't quit early
        if ctx.is_building_dedicated_server():
            return # regular launchers do not build in dedicated server mode.

    active_projects = ctx.get_enabled_game_project_list()
    for project in active_projects:

        kw_per_launcher = copy.deepcopy(kw)
        kw_per_launcher['target'] = project + kw['target'] # rename the target!

        CryLauncher_Impl(ctx, project, *k, **kw_per_launcher)


def codegen_static_modules_cpp(ctx, static_modules, kw):

    if not ctx.is_build_monolithic():
        return

    # Write out json file listing modules. This will be fed into codegen.
    static_modules_json = {'modules': static_modules}
    static_modules_json_node = ctx.path.find_or_declare(kw['target'] + 'StaticModules.json')
    static_modules_json_node.write(json.dumps(static_modules_json))

    # get the full path of static_modules_json to extract out its path, and then set that path
    # as the input dir
    static_modules_json_node_abs_path = static_modules_json_node.abspath()
    static_modules_json_node_dir = os.path.dirname(static_modules_json_node_abs_path)
    kw['az_code_gen_input_dir'] = static_modules_json_node_dir

    # LMBR-30070: We should be generating this file with a waf task. Until then,
    # we need to manually set the cached signature.
    static_modules_json_node.cache_sig = Utils.h_file(static_modules_json_node.abspath())

    # Set up codegen for launcher.
    kw['features'] += ['az_code_gen']
    if 'az_code_gen' not in kw:
        kw['az_code_gen'] = []
        
    static_modules_py = ctx.engine_node.make_node('Code/LauncherUnified/CodeGen/StaticModules.py')
    static_modules_node_rel = static_modules_py.path_from(ctx.path)

    kw['az_code_gen'] += [
        {
            'files': [static_modules_json_node],
            'scripts': [static_modules_node_rel],
            'arguments': ['-JSON']
        }
    ]


def codegen_static_modules_cpp_for_launcher(ctx, project, k, kw):
    """
    Use codegen to create StaticModules.cpp and compile it into the launcher.
    StaticModules.cpp contains code that bootstraps each module used in a monolithic build.
    """
    if not ctx.is_build_monolithic():
        return
        
    # Gather modules used by this project
    static_modules = ctx.project_flavor_modules(project, 'Game')
    for gem in ctx.get_game_gems(project):
        if not gem.is_legacy_igem:
            for module in gem.modules:
                # Only include game modules in static module list
                if module.type == Gem.Module.Type.GameModule:
                    if module.name:
                        static_modules.append('{}_{}_{}'.format(gem.name, module.name, gem.id.hex))
                    else:
                        static_modules.append('{}_{}'.format(gem.name, gem.id.hex))

    codegen_static_modules_cpp(ctx, static_modules, kw)


def codegen_static_modules_cpp_for_application(ctx, modules_to_scan, gems_spec, k, kw):
    """
    Use codegen to create StaticModules.cpp and compile it into the launcher.
    StaticModules.cpp contains code that bootstraps each module used in a monolithic build.
    """
    if not ctx.is_build_monolithic():
        return

    # Gather modules used by this project
    static_modules = modules_to_scan[:] #ctx.project_flavor_modules(project, 'Game')

    gems_list = ctx.load_gems_from_gem_spec(gems_spec)
    # gems_list += [gem for gem in ctx.load_required_gems() if gem not in gems_list]

    for gem in gems_list:
        if not gem.is_legacy_igem:
            for module in gem.modules:
                # Only include game modules in static module list
                if module.type == Gem.Module.Type.GameModule:
                    if module.name:
                        static_modules.append('{}_{}_{}'.format(gem.name, module.name, gem.id.hex))
                    else:
                        static_modules.append('{}_{}'.format(gem.name, gem.id.hex))

    codegen_static_modules_cpp(ctx, static_modules, kw)



PROJECT_GAME_AND_DLL_DICT = {}


def read_project_game_folder_and_dll(ctx, project_name):

    global PROJECT_GAME_AND_DLL_DICT
    if project_name in PROJECT_GAME_AND_DLL_DICT:
        return PROJECT_GAME_AND_DLL_DICT[project_name]

    project_json_file = ctx.srcnode.make_node(project_name).make_node('project.json')
    if not os.path.exists(project_json_file.abspath()):
        return project_name, project_name
    project_json = ctx.parse_json_file(project_json_file)
    game_folder = game_dll = project_name
    if 'sys_game_folder' in project_json:
        game_folder = project_json['sys_game_folder']
    if 'sys_dll_game' in project_json:
        game_dll = project_json['sys_dll_game']
    PROJECT_GAME_AND_DLL_DICT[project_name] = game_folder, game_dll
    return game_folder, game_dll


def CryLauncher_Impl(ctx, project, *k, **kw_per_launcher):

    kw_per_launcher['vs_filter'] = 'Launchers'

    # Initialize the Task Generator
    if not InitializeTaskGenerator(ctx, kw_per_launcher):
        return

    # Append common modules
    AppendCommonModules(ctx, kw_per_launcher)

    # Setup TaskGenerator specific settings
    SetupRunTimeLibraries(ctx, kw_per_launcher)

    LoadSharedSettings(ctx,k,kw_per_launcher)

    kw_per_launcher['is_launcher']          = True

    ConfigureTaskGenerator(ctx, kw_per_launcher)

    game_folder, game_dll = read_project_game_folder_and_dll(ctx, project)
    append_kw_entry(kw_per_launcher, 'defines', 'LY_GAMEFOLDER="{}"'.format(game_folder))
    append_kw_entry(kw_per_launcher, 'defines', 'LY_GAMEDLL="{}"'.format(game_dll))

    if not BuildTaskGenerator(ctx, kw_per_launcher):
        return None

    # include the android studio command in the check for android otherwise the tgen is
    # incorrectly tagged as a program
    is_android = ctx.is_android_platform(ctx.env['PLATFORM']) or ctx.cmd == 'android_studio'
    is_monolithic = ctx.is_build_monolithic()

    if is_android and not ctx.get_android_settings(project):
        Logs.warn('[WARN] Game project - %s - not configured for Android.  Skipping...' % ctx.game_project)
        return None

    kw_per_launcher['idx']              = kw_per_launcher['idx'] + (1000 * (ctx.project_idx(project) + 1));
    # Setup values for Launcher Projects
    kw_per_launcher['resource_path']        = ctx.launch_node().make_node(ctx.game_code_folder(project) + '/Resources')
    kw_per_launcher['project_name']         = project
    kw_per_launcher['output_file_name']     = ctx.get_executable_name( project )

    Logs.debug("lumberyard: Generating launcher %s from %s" % (kw_per_launcher['output_file_name'], kw_per_launcher['target']))

    ctx.apply_gems_to_context(project, k, kw_per_launcher)

    codegen_static_modules_cpp_for_launcher(ctx, project, k, kw_per_launcher)

    if ctx.is_build_monolithic():
        append_kw_entry(kw_per_launcher,'defines',[ '_LIB', 'AZ_MONOLITHIC_BUILD' ])
        append_kw_entry(kw_per_launcher,'features',[ 'apply_monolithic_build_settings'])

    if not ctx.is_build_monolithic() and ctx.env['PLATFORM'] == 'darwin_x64':
        append_kw_entry(kw_per_launcher, 'features', ['apply_non_monolithic_launcher_settings'])

    if is_android:
        # android doesn't have the concept of native executables so we need to build it as a shlib
        kw_per_launcher['__build_type__'] = 'shlib'
        return RunTaskGenerator(ctx, *k, **kw_per_launcher)
    else:
        append_kw_entry(kw_per_launcher,'features', ['copy_3rd_party_binaries'])
        return RunTaskGenerator(ctx, *k, **kw_per_launcher)

###############################################################################
@build_program
def CryDedicatedServer(ctx, *k, **kw):
    """
    Wrapper for CryEngine Dedicated Servers
    """

    apply_cryengine_module_defines(ctx, kw)

    active_projects = ctx.get_enabled_game_project_list()

    # enable ASAN and ASLR by default on dedicated server
    kw.setdefault('use_asan', True)
    kw.setdefault('use_aslr', True)

    for project in active_projects:
        kw_per_launcher = copy.deepcopy(kw)
        kw_per_launcher['target'] = project + kw['target'] # rename the target!

        CryDedicatedserver_Impl(ctx, project, *k, **kw_per_launcher)

def CryDedicatedserver_Impl(ctx, project, *k, **kw_per_launcher):

    kw_per_launcher['vs_filter'] = 'Launchers'

    # Initialize the Task Generator
    if not InitializeTaskGenerator(ctx, kw_per_launcher):
        return

    # Append common modules
    AppendCommonModules(ctx,kw_per_launcher)

    # Setup TaskGenerator specific settings
    SetupRunTimeLibraries(ctx,kw_per_launcher)

    append_kw_entry(kw_per_launcher,'win_linkflags',[ '/SUBSYSTEM:WINDOWS' ])

    LoadSharedSettings(ctx,k,kw_per_launcher)

    ConfigureTaskGenerator(ctx, kw_per_launcher)

    game_folder, game_dll = read_project_game_folder_and_dll(ctx, project)
    append_kw_entry(kw_per_launcher, 'defines', 'LY_GAMEFOLDER="{}"'.format(game_folder))
    append_kw_entry(kw_per_launcher, 'defines', 'LY_GAMEDLL="{}"'.format(game_dll))

    if not BuildTaskGenerator(ctx, kw_per_launcher):
        return None

    kw_per_launcher['idx']          = kw_per_launcher['idx'] + (1000 * (ctx.project_idx(project) + 1));

    kw_per_launcher['is_dedicated_server']          = True
    kw_per_launcher['resource_path']                = ctx.launch_node().make_node(ctx.game_code_folder(project) + '/Resources')
    kw_per_launcher['project_name']                 = project
    kw_per_launcher['output_file_name']             = ctx.get_dedicated_server_executable_name(project)

    Logs.debug("lumberyard: Generating dedicated server %s from %s" % (kw_per_launcher['output_file_name'], kw_per_launcher['target']))

    ctx.apply_gems_to_context(project, k, kw_per_launcher)

    codegen_static_modules_cpp_for_launcher(ctx, project, k, kw_per_launcher)

    if ctx.is_build_monolithic():
        Logs.debug("lumberyard: Dedicated server monolithic build %s ... " % kw_per_launcher['target'])
        append_kw_entry(kw_per_launcher,'defines',[ '_LIB', 'AZ_MONOLITHIC_BUILD' ])
        append_kw_entry(kw_per_launcher,'features',[ 'apply_monolithic_build_settings' ])

    append_kw_entry(kw_per_launcher, 'features', ['copy_3rd_party_binaries'])

    # Make sure that we are actually building a server configuration
    if not isinstance(ctx, ConfigurationContext) and ctx.env['PLATFORM'] != 'project_generator':

        try:
            if not ctx.get_platform_configuration(ctx.env['PLATFORM'], ctx.env['CONFIGURATION']).is_server:
                return
        except Errors.WafError as e:
            Logs.debug("Skipping target '{}' due to error: {}".format(kw_per_launcher['target'], e))
            return

    return RunTaskGenerator(ctx, *k, **kw_per_launcher)


###############################################################################
@build_program
def CryConsoleApplication(ctx, *k, **kw):
    """
    Wrapper for CryEngine Executables
    """
    if ctx.is_module_exempt(kw.get('target','')):
        return

    # Initialize the Task Generator
    if not InitializeTaskGenerator(ctx, kw):
        return

    # Append common modules
    AppendCommonModules(ctx,kw)

    # Setup TaskGenerator specific settings
    apply_cryengine_module_defines(ctx, kw)
    SetupRunTimeLibraries(ctx, kw)

    append_kw_entry(kw,'win_linkflags',[ '/SUBSYSTEM:CONSOLE' ])
        
    # Default clang behavior is to disable exceptions. For console apps we want to enable them
    if 'CXXFLAGS' in ctx.env.keys() and 'darwin' in ctx.get_current_platform_list(ctx.env['PLATFORM']):
        if '-fno-exceptions' in ctx.env['CXXFLAGS']:
            ctx.env['CXXFLAGS'].remove("-fno-exceptions")

    LoadSharedSettings(ctx,k,kw)

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return None

    append_kw_entry(kw, 'features', ['copy_3rd_party_binaries'])

    return RunTaskGenerator(ctx, *k, **kw)


@build_program
def LyLauncherApplication(ctx, *k, **kw):
    """
    Module to build a custom launcher that will build monolithically when needed
    """
    if ctx.is_module_exempt(kw.get('target', '')):
        return

    # Initialize the Task Generator
    if not InitializeTaskGenerator(ctx, kw):
        return

    # Append common modules
    AppendCommonModules(ctx, kw)

    # Setup TaskGenerator specific settings
    apply_cryengine_module_defines(ctx, kw)
    SetupRunTimeLibraries(ctx, kw)

    append_kw_entry(kw, 'win_linkflags', ['/SUBSYSTEM:CONSOLE'])

    # Default clang behavior is to disable exceptions. For console apps we want to enable them
    if 'CXXFLAGS' in ctx.env.keys() and 'darwin' in ctx.get_current_platform_list(ctx.env['PLATFORM']):
        if '-fno-exceptions' in ctx.env['CXXFLAGS']:
            ctx.env['CXXFLAGS'].remove("-fno-exceptions")

    LoadSharedSettings(ctx, k, kw)

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return None

    append_kw_entry(kw, 'features', ['copy_3rd_party_binaries'])

    if ctx.is_build_monolithic():

        # Apply the monolithic logic to the application in the same way launchers are done
        append_kw_entry(kw, 'defines', [ '_LIB', 'AZ_MONOLITHIC_BUILD' ])
        append_kw_entry(kw, 'features', [ 'apply_monolithic_build_settings'])
        kw['is_launcher'] = True

        if 'gem_spec' in kw:
            # Specified both a project and an override gem spec
            gem_spec = kw['gem_spec']
            static_modules = kw.get('static_modules', [])
            codegen_static_modules_cpp_for_application(ctx, static_modules, gem_spec, k, kw)
            ctx.apply_gem_spec_to_context(gem_spec, kw)
        elif 'project' in kw:
            # Specified a game project
            project = kw['project']
            codegen_static_modules_cpp_for_launcher(ctx, project, k, kw)
            ctx.apply_gems_to_context(project, k, kw)
            ctx.apply_required_gems_to_context(kw['target'], kw)
        else:
            # If no gem spec is specified, then this console app needs to be treated the same way as a launcher: Append the game name in front
            # of the target, in order to handle multiple enabled games
            active_projects = ctx.get_enabled_game_project_list()
            for project in active_projects:

                kw_per_console_app = copy.deepcopy(kw)
                kw_per_console_app['target'] = project + kw['target']  # rename the target!

                ctx.apply_gems_to_context(project, k, kw_per_console_app)
                RunTaskGenerator(ctx, *k, **kw_per_console_app)
            return None

    return RunTaskGenerator(ctx, *k, **kw)


###############################################################################
@build_program
def CryBuildUtility(ctx, *k, **kw):
    """
    Wrapper for Build Utilities
    """
    # Initialize the Task Generator
    if not InitializeTaskGenerator(ctx, kw):
        return

    # Setup TaskGenerator specific settings
    SetupRunTimeLibraries(ctx,kw)

    append_kw_entry(kw,'win_linkflags',[ '/SUBSYSTEM:CONSOLE' ])

    LoadSharedSettings(ctx,k,kw)

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return None

    append_kw_entry(kw, 'features', ['copy_3rd_party_binaries'])

    return RunTaskGenerator(ctx, *k, **kw)


###############################################################################
@build_file_container
def CryFileContainer(ctx, *k, **kw):
    """
    Function to create a header only library
    """
    # Initialize the Task Generator
    if not InitializeTaskGenerator(ctx, kw):
        return

    LoadSharedSettings(ctx,k,kw)

    # Setup TaskGenerator specific settings
    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return

    # A file container is not supposed to actually generate any build tasks at all, but it still needs
    # to be a task gen so that it can export includes and export defines (in the 'use' system)
    # We want the files to show up in the generated projects (xcode and other IDEs) as if they are compile tasks
    # but when actually building, we want them only to affect use / defines / includes.
    
    if ctx.env['PLATFORM'] != 'project_generator':
        # clear out the 'source' and features so that as little as possible executes.
        kw['source'] = []
        kw['features'] = ['use']
    
    return ctx(*k, **kw)


###############################################################################
@build_program
def CryEditor(ctx, *k, **kw):
    """
    Wrapper for CryEngine Editor Executables
    """
    # Initialize the Task Generator
    if not InitializeTaskGenerator(ctx, kw):
        return

    # Append common modules
    AppendCommonModules(ctx,kw)

    # Setup TaskGenerator specific settings
    ctx.set_editor_flags(kw)
    apply_cryengine_module_defines(ctx, kw)

    SetupRunTimeLibraries(ctx, kw)

    # Additional Editor-specific settings
    append_kw_entry(kw,'defines',[ 'SANDBOX_EXPORTS' ])

    append_kw_entry(kw,'win_linkflags',[ '/SUBSYSTEM:WINDOWS' ])

    LoadSharedSettings(ctx,k,kw)

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return None

    append_kw_entry(kw, 'features', ['copy_3rd_party_binaries'])

    return RunTaskGenerator(ctx, *k, **kw)


###############################################################################
@build_program
def LumberyardApp(ctx, *k, **kw):
    """
    Wrapper to make lmbr_waf happy.  We shouldn't tack on any settings here,
    so we can make the waf transition easier later on.
    """
    # Initialize the Task Generator
    if not InitializeTaskGenerator(ctx, kw):
        return

    apply_cryengine_module_defines(ctx, kw)

    SetupRunTimeLibraries(ctx,kw)

    LoadSharedSettings(ctx,k,kw)

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return None

    append_kw_entry(kw, 'features', ['copy_3rd_party_binaries'])

    return RunTaskGenerator(ctx, *k, **kw)

###############################################################################
@build_shlib
def CryEditorLib(ctx, *k, **kw):
    """
    Wrapper for CryEngine Editor Library component
    """
    # Initialize the Task Generator
    if not InitializeTaskGenerator(ctx, kw):
        return

    # Append common modules
    AppendCommonModules(ctx,kw)

    # Additional Editor-specific settings
    append_kw_entry(kw,'defines',[ 'SANDBOX_EXPORTS' ])

    kw['enable_rtti'] = [ True ]

    # Setup TaskGenerator specific settings
    ctx.set_editor_flags(kw)
    apply_cryengine_module_defines(ctx, kw)

    SetupRunTimeLibraries(ctx,kw)
    append_kw_entry(kw,'msvc_cxxflags',['/EHsc'])
    append_kw_entry(kw,'msvc_cflags', ['/EHsc'])
    append_kw_entry(kw,'defines',['USE_MEM_ALLOCATOR', 'EDITOR', 'DONT_BAN_STD_STRING', 'FBXSDK_NEW_API=1' ])

    LoadSharedSettings(ctx,k,kw)

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return None

    return RunTaskGenerator(ctx, *k, **kw)

###############################################################################
@build_shlib
def CryEditorLib(ctx, *k, **kw):
    """
    Wrapper for CryEngine Editor Library component
    """
    # Initialize the Task Generator
    if not InitializeTaskGenerator(ctx, kw):
        return

    # Append common modules
    AppendCommonModules(ctx,kw)

    # Additional Editor-specific settings
    append_kw_entry(kw,'features',[ 'generate_rc_file' ])
    append_kw_entry(kw,'defines',[ 'SANDBOX_EXPORTS' ])

    kw['enable_rtti'] = [ True ]

    # Setup TaskGenerator specific settings
    ctx.set_editor_flags(kw)
    apply_cryengine_module_defines(ctx, kw)

    SetupRunTimeLibraries(ctx,kw)
    append_kw_entry(kw,'msvc_cxxflags',['/EHsc'])
    append_kw_entry(kw,'msvc_cflags', ['/EHsc'])
    append_kw_entry(kw,'defines',['USE_MEM_ALLOCATOR', 'EDITOR', 'DONT_BAN_STD_STRING', 'FBXSDK_NEW_API=1' ])

    LoadSharedSettings(ctx,k,kw)

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return None

    return RunTaskGenerator(ctx, *k, **kw)

###############################################################################
@build_shlib
def CryEditorCore(ctx, *k, **kw):
    """
    Wrapper for CryEngine Editor Core component
    """
    # Initialize the Task Generator
    if not InitializeTaskGenerator(ctx, kw):
        return

    # Append common modules
    AppendCommonModules(ctx,kw)

    # Setup TaskGenerator specific settings
    ctx.set_editor_flags(kw)
    apply_cryengine_module_defines(ctx, kw)

    SetupRunTimeLibraries(ctx,kw)

    append_kw_entry(kw,'msvc_cxxflags',['/EHsc'])
    append_kw_entry(kw,'msvc_cflags', ['/EHsc'])
    append_kw_entry(kw,'defines',['EDITOR_CORE', 'USE_MEM_ALLOCATOR', 'EDITOR', 'DONT_BAN_STD_STRING', 'FBXSDK_NEW_API=1' ])

    LoadSharedSettings(ctx,k,kw)

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return None

    if ctx.env['PLATFORM'] == 'darwin_x64':
        append_kw_entry(kw,'linkflags',['-install_name', '@rpath/lib'+kw['output_file_name']+'.dylib'])

    return RunTaskGenerator(ctx, *k, **kw)


###############################################################################
@build_shlib
def CryEditorUiQt(ctx, *k, **kw):
    """
    Wrapper for CryEngine Editor Core component
    """
    # Initialize the Task Generator
    if not InitializeTaskGenerator(ctx, kw):
        return

    # Append common modules
    AppendCommonModules(ctx,kw)

    # Setup TaskGenerator specific settings
    ctx.set_editor_flags(kw)
    apply_cryengine_module_defines(ctx, kw)

    SetupRunTimeLibraries(ctx,kw)

    append_kw_entry(kw,'msvc_cxxflags',['/EHsc'])
    append_kw_entry(kw,'msvc_cflags',['/EHsc'])
    append_kw_entry(kw,'defines',[  'NOMINMAX',
                                    'EDITOR_UI_UX_CHANGE',
                                    'EDITOR_QT_UI_EXPORTS',
                                    'IGNORE_CRY_COMMON_STATIC_VAR',
                                    'CRY_ENABLE_RC_HELPER',
                                    'PLUGIN_EXPORTS',
                                    'EDITOR_COMMON_IMPORTS'])

    LoadSharedSettings(ctx,k,kw)

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return None

    if ctx.env['PLATFORM'] == 'darwin_x64':
        append_kw_entry(kw,'linkflags',['-install_name', '@rpath/lib'+kw['output_file_name']+'.dylib'])

    return RunTaskGenerator(ctx, *k, **kw)


###############################################################################
@build_shlib
def CryPlugin(ctx, *k, **kw):
    """
    Wrapper for CryEngine Editor Plugins
    """
    # Initialize the Task Generator
    if not InitializeTaskGenerator(ctx, kw):
        return

    # Append common modules
    AppendCommonModules(ctx,kw)

    # Setup TaskGenerator specific settings
    ctx.set_editor_flags(kw)
    apply_cryengine_module_defines(ctx, kw)

    SetupRunTimeLibraries(ctx,kw)

    append_kw_entry(kw,'msvc_cxxflags',['/EHsc'])
    append_kw_entry(kw,'msvc_cflags',['/EHsc'])
    append_kw_entry(kw,'defines',[ 'SANDBOX_IMPORTS', 'PLUGIN_EXPORTS', 'EDITOR_COMMON_IMPORTS' ])
    kw['output_sub_folder']     = 'EditorPlugins'
    kw['features'] += ['qt5']#added QT to all plugins

    LoadSharedSettings(ctx,k,kw)

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return None

    if ctx.env['PLATFORM'] == 'darwin_x64':
        append_kw_entry(kw,'linkflags',['-install_name', '@rpath/lib'+kw['output_file_name']+'.dylib'])

    return RunTaskGenerator(ctx, *k, **kw)

    ###############################################################################
@build_shlib
def BuilderPlugin(ctx, *k, **kw):
    """
    Wrapper for Asset Builder SDK Builders
    """
    kw['output_sub_folder']='Builders'
    append_kw_entry(kw, 'msvc_cxxflags', ['/EHsc'])
    append_kw_entry(kw, 'msvc_cflags', ['/EHsc'])
    append_kw_entry(kw, 'win_defines', ['UNICODE'])
    append_kw_entry(kw, 'use', ['AzToolsFramework', 'AssetBuilderSDK'])
    append_kw_entry(kw, 'uselib', ['QT5CORE', 'QT5GUI', 'QT5WIDGETS'])
    defines = []

    append_kw_entry(kw, 'defines', defines)

    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    # Append common modules
    AppendCommonModules(ctx,kw)

    # Setup TaskGenerator specific settings
    ctx.set_editor_flags(kw)
    apply_cryengine_module_defines(ctx, kw)

    SetupRunTimeLibraries(ctx,kw)

    LoadSharedSettings(ctx,k,kw)

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return None

    if ctx.env['PLATFORM'] == 'darwin_x64':
        append_kw_entry(kw,'linkflags',['-install_name', '@rpath/lib'+kw['output_file_name']+'.dylib'])

    return RunTaskGenerator(ctx, *k, **kw)


###############################################################################
@build_shlib
def CryStandAlonePlugin(ctx, *k, **kw):
    """
    Wrapper for CryEngine Editor Plugins
    """
    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    # Append common modules
    AppendCommonModules(ctx,kw)
    apply_cryengine_module_defines(ctx, kw)

    # Setup TaskGenerator specific settings
    ctx.set_editor_flags(kw)
    SetupRunTimeLibraries(ctx,kw)

    append_kw_entry(kw,'msvc_cxxflags',['/EHsc'])
    append_kw_entry(kw,'msvc_cflags',['/EHsc'])
    append_kw_entry(kw,'defines',[ 'PLUGIN_EXPORTS' ])
    append_kw_entry(kw,'win_debug_linkflags',['/NODEFAULTLIB:libcmtd.lib', '/NODEFAULTLIB:libcd.lib'])
    append_kw_entry(kw,'win_profile_linkflags',['/NODEFAULTLIB:libcmt.lib', '/NODEFAULTLIB:libc.lib'])
    append_kw_entry(kw,'win_release_linkflags',['/NODEFAULTLIB:libcmt.lib', '/NODEFAULTLIB:libc.lib'])

    if not 'output_sub_folder' in kw:
        kw['output_sub_folder'] = 'EditorPlugins'
    kw['features'] += ['qt5'] #added QT to all plugins

    LoadSharedSettings(ctx,k,kw)

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return None

    kw['enable_rtti'] = [ True ]
    kw['remove_release_define'] = [ True ]

    if ctx.env['PLATFORM'] == 'darwin_x64':
        append_kw_entry(kw,'linkflags',['-install_name', '@rpath/lib'+kw['output_file_name']+'.dylib'])

    kw['is_editor_plugin'] = True

    return RunTaskGenerator(ctx, *k, **kw)


###############################################################################
@build_shlib
def CryPluginModule(ctx, *k, **kw):
    """
    Wrapper for CryEngine Editor Plugins Util dlls, those used by multiple plugins
    """
    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    # Append common modules
    AppendCommonModules(ctx,kw)

    apply_cryengine_module_defines(ctx, kw)

    # Setup TaskGenerator specific settings
    ctx.set_editor_flags(kw)

    SetupRunTimeLibraries(ctx,kw)

    append_kw_entry(kw,'msvc_cxxflags',['/EHsc'])
    append_kw_entry(kw,'msvc_cflags',['/EHsc'])
    append_kw_entry(kw,'defines',[ 'PLUGIN_EXPORTS', 'EDITOR_COMMON_EXPORTS' ])
    if not 'output_sub_folder' in kw:
        kw['output_sub_folder'] = 'EditorPlugins'

    kw['features'] += ['qt5']#added QT to all plugins

    LoadSharedSettings(ctx,k,kw)

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return None

    kw['remove_release_define'] = [ True ]

    if ctx.env['PLATFORM'] == 'darwin_x64':
        append_kw_entry(kw,'linkflags',['-install_name', '@rpath/lib'+kw['output_file_name']+'.dylib'])

    return RunTaskGenerator(ctx, *k, **kw)


###############################################################################
@build_shlib
def CryEditorCommon(ctx, *k, **kw):
    """
    Wrapper for CryEditorCommon
    """
    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    # Append common modules
    AppendCommonModules(ctx,kw)
    apply_cryengine_module_defines(ctx, kw)

    # Setup TaskGenerator specific settings
    ctx.set_editor_flags(kw)

    SetupRunTimeLibraries(ctx,kw)

    append_kw_entry(kw,'msvc_cxxflags',['/EHsc'])
    append_kw_entry(kw,'msvc_cflags',['/EHsc'])
    append_kw_entry(kw,'defines',[ 'PLUGIN_EXPORTS', 'EDITOR_COMMON_EXPORTS'])

    LoadSharedSettings(ctx,k,kw)

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return None

    kw['remove_release_define'] = [ True ]

    if ctx.env['PLATFORM'] == 'darwin_x64':
        append_kw_entry(kw,'linkflags',['-install_name', '@rpath/lib'+kw['output_file_name']+'.dylib'])

    return RunTaskGenerator(ctx, *k, **kw)


###############################################################################
@build_program
def CryResourceCompiler(ctx, *k, **kw):
    """
    Wrapper for RC
    """
    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    # Append common modules
    AppendCommonModules(ctx,kw)
    apply_cryengine_module_defines(ctx, kw)

    # Setup TaskGenerator specific settings
    SetupRunTimeLibraries(ctx,kw, 'dynamic')

    ctx.set_rc_flags(kw, ctx)

    kw['output_file_name']  = 'rc'
    kw['output_sub_folder'] = 'rc'

    Logs.debug('lumberyard: creating RC, with mirror_artifacts')

    append_kw_entry(kw,'win_debug_linkflags',['/NODEFAULTLIB:libcmtd.lib', '/NODEFAULTLIB:libcd.lib'])
    append_kw_entry(kw,'win_ndebug_linkflags',['/NODEFAULTLIB:libcmt.lib', '/NODEFAULTLIB:libc.lib'])
    append_kw_entry(kw,'win_linkflags',[ '/SUBSYSTEM:CONSOLE' ])

    LoadSharedSettings(ctx,k,kw)        

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return None

    append_kw_entry(kw, 'features', ['copy_3rd_party_binaries'])

    return RunTaskGenerator(ctx, *k, **kw)


###############################################################################
@build_shlib
def CryResourceCompilerModule(ctx, *k, **kw):
    """
    Wrapper for RC modules
    """
    # Initialize the Task Generator
    if not InitializeTaskGenerator(ctx, kw):
        return

    # Append common modules
    AppendCommonModules(ctx,kw)
    apply_cryengine_module_defines(ctx, kw)

    # Setup TaskGenerator specific settings
    SetupRunTimeLibraries(ctx,kw, 'dynamic')

    ctx.set_rc_flags(kw, ctx)
    kw['output_sub_folder'] = 'rc'

    append_kw_entry(kw,'win_debug_linkflags',['/NODEFAULTLIB:libcmtd.lib', '/NODEFAULTLIB:libcd.lib'])
    append_kw_entry(kw,'win_ndebug_linkflags',['/NODEFAULTLIB:libcmt.lib', '/NODEFAULTLIB:libc.lib'])

    if ctx.is_mac_platform(ctx.env['PLATFORM']):
        append_kw_entry(kw,'linkflags',['-dynamiclib'])

    LoadSharedSettings(ctx,k,kw)        
                
    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return None

    append_kw_entry(kw,'win_linkflags',[ '/SUBSYSTEM:CONSOLE' ])

    if ctx.env['PLATFORM'] == 'darwin_x64':
        append_kw_entry(kw,'linkflags',['-install_name', '@rpath/lib'+kw['output_file_name']+'.dylib'])

    return RunTaskGenerator(ctx, *k, **kw)


###############################################################################
@build_shlib
def CryPipelineModule(ctx, *k, **kw):
    """
    Wrapper for Pipleine modules (mostly DCC exporters)
    """
    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    AppendCommonModules(ctx,kw)

    # Setup TaskGenerator specific settings
    SetupRunTimeLibraries(ctx, kw, 'dynamic')

    apply_cryengine_module_defines(ctx, kw)

    ctx.set_pipeline_flags(kw, ctx)

    # LUMBERYARD
    append_kw_entry(kw,'win_linkflags',[ '/SUBSYSTEM:CONSOLE' ])
    append_kw_entry(kw,'win_debug_linkflags',['/NODEFAULTLIB:libcmtd.lib', '/NODEFAULTLIB:libcd.lib'])
    append_kw_entry(kw,'win_ndebug_linkflags',['/NODEFAULTLIB:libcmt.lib', '/NODEFAULTLIB:libc.lib'])

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return None

    if ctx.env['PLATFORM'] == 'darwin_x64':
        append_kw_entry(kw,'linkflags',['-install_name', '@rpath/lib'+kw['output_file_name']+'.dylib'])

    return RunTaskGenerator(ctx, *k, **kw)


###############################################################################
@build_program
def CryQtApplication(ctx, *k, **kw):
    """
    Wrapper for Qt programs launched by the editor
    """
    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    # Append common modules
    AppendCommonModules(ctx,kw)
    apply_cryengine_module_defines(ctx, kw)

    # Setup TaskGenerator specific settings
    SetupRunTimeLibraries(ctx,kw)

    append_kw_entry(kw,'win_linkflags',[ '/SUBSYSTEM:WINDOWS' ])

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return None

    append_kw_entry(kw, 'features', ['copy_3rd_party_binaries'])

    return RunTaskGenerator(ctx, *k, **kw)


###############################################################################
@build_program
def CryQtConsoleApplication(ctx, *k, **kw):
    """
    Wrapper for Qt programs launched by the editor
    """
    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    # Append common modules
    AppendCommonModules(ctx,kw)
    apply_cryengine_module_defines(ctx, kw)

    # Setup TaskGenerator specific settings
    SetupRunTimeLibraries(ctx,kw)

    append_kw_entry(kw,'win_linkflags',[ '/SUBSYSTEM:CONSOLE' ])

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return None

    append_kw_entry(kw, 'features', ['copy_3rd_party_binaries'])

    return RunTaskGenerator(ctx, *k, **kw)


###############################################################################
# Helper function to set Flags based on options
def ApplyBuildOptionSettings(self, kw):
    """
    Util function to apply flags based on waf options
    """
    # Add debug flags if requested
    if self.is_option_true('generate_debug_info'):
        kw['cflags'].extend(self.env['COMPILER_FLAGS_DebugSymbols'])
        kw['cxxflags'].extend(self.env['COMPILER_FLAGS_DebugSymbols'])
        kw['linkflags'].extend(self.env['LINKFLAGS_DebugSymbols'])

    # Add show include flags if requested
    if self.is_option_true('show_includes'):
        kw['cflags'].extend(self.env['SHOWINCLUDES_cflags'])
        kw['cxxflags'].extend(self.env['SHOWINCLUDES_cxxflags'])

    # Add preprocess to file flags if requested
    if self.is_option_true('show_preprocessed_file'):
        kw['cflags'].extend(self.env['PREPROCESS_cflags'])
        kw['cxxflags'].extend(self.env['PREPROCESS_cxxflags'])
        self.env['CC_TGT_F'] = self.env['PREPROCESS_cc_tgt_f']
        self.env['CXX_TGT_F'] = self.env['PREPROCESS_cxx_tgt_f']

    # Add disassemble to file flags if requested
    if self.is_option_true('show_disassembly'):
        kw['cflags'].extend(self.env['DISASSEMBLY_cflags'])
        kw['cxxflags'].extend(self.env['DISASSEMBLY_cxxflags'])
        self.env['CC_TGT_F'] = self.env['DISASSEMBLY_cc_tgt_f']
        self.env['CXX_TGT_F'] = self.env['DISASSEMBLY_cxx_tgt_f']

    # Add ASLR and ASAN flags
    is_debug = self.env['CONFIGURATION'] in ('debug', 'debug_test')
    if self.is_option_true('use_asan') or kw.get('use_asan', is_debug):
        kw['cflags'].extend(self.env['ASAN_cflags'])
        kw['cxxflags'].extend(self.env['ASAN_cxxflags'])

    if self.is_option_true('use_aslr') or kw.get('use_aslr', False):
        kw['linkflags'].extend(self.env['LINKFLAGS_ASLR'])

    # Crash reporter settings
    if self.options.external_crash_reporting:
        kw['defines'] += ['EXTERNAL_CRASH_REPORTING=' + self.options.external_crash_reporting]
    if self.options.crash_handler_token:
        kw['defines'] += ['CRASH_HANDLER_TOKEN=' + self.options.crash_handler_token]
    if self.options.crash_handler_url:
        kw['defines'] += ['CRASH_HANDLER_URL=' + self.options.crash_handler_url]

    # We always send in a packaged build time. It's only meaningful for packaged builds.
    packaged_build_time = 0
    if len(self.options.packaged_build_time) > 0:
        packaged_build_time = self.options.packaged_build_time
    kw['defines'] += ['LY_METRICS_BUILD_TIME={}'.format(packaged_build_time)]

###############################################################################
# Helper function to extract platform specific flags
@conf
def GetPlatformSpecificSettings(ctx, dict, entry, _platform, configuration):
    """
    Util function to apply flags based on current platform
    """
    def _to_list( value ):
        if isinstance(value,list):
            return value
        return [ value ]

    returnValue = []

    platforms = ctx.get_current_platform_list(_platform)

    # Check for entry in <platform>_<entry> style
    for platform in platforms:
        platform_entry = platform + '_' + entry
        if not platform_entry in dict:
            continue # No platfrom specific entry found

        returnValue += _to_list( dict[platform_entry] )

    if configuration == []:
        return [] # Dont try to check for configurations if we dont have any

    # Check for 'test' or 'dedicated' tagged entries
    if _platform and _platform != 'project_generator':
    
        platform_details = ctx.get_target_platform_detail(_platform)
        configuration_details = platform_details.get_configuration(configuration)
    
        if configuration_details.is_test:
            test_entry = 'test_{}'.format(entry)
            if test_entry in dict:
                returnValue += _to_list(dict[test_entry])
            for platform in platforms:
                platform_test_entry = '{}_test_{}'.format(platform, entry)
                if platform_test_entry in dict:
                    returnValue += _to_list(dict[platform_test_entry])
    
        if configuration_details.is_server:
            test_entry = 'dedicated_{}'.format(entry)
            if test_entry in dict:
                returnValue += _to_list(dict[test_entry])
            for platform in platforms:
                platform_test_entry = '{}_dedicated_{}'.format(platform, entry)
                if platform_test_entry in dict:
                    returnValue += _to_list(dict[platform_test_entry])

    # Check for entry in <configuration>_<entry> style
    configuration_entry = configuration + '_' + entry
    if configuration_entry in dict:
        returnValue += _to_list( dict[configuration_entry] )

    # Check for entry in <platform>_<configuration>_<entry> style
    for platform in platforms:
        platform_configuration_entry = platform + '_' + configuration + '_' + entry
        if not platform_configuration_entry in dict:
            continue # No platfrom /configuration specific entry found

        returnValue += _to_list( dict[platform_configuration_entry] )

    return returnValue


# Maintain a map of legacy keyword shortcuts to process the kw macro expansions (for shortcut aliases)
LEGACY_CONFIGURATION_SHORTCUT_ALIASES = {
    'debug_all': ['debug', 'debug_dedicated', 'debug_test', 'debug_test_dedicated'],
    'profile_all': ['profile', 'profile_dedicated', 'profile_test', 'profile_test_dedicated'],
    'performance_all': ['performance', 'performance_dedicated'],
    'release_all': ['release', 'release_dedicated'],
    'dedicated_all': ['debug_dedicated', 'profile_dedicated', 'performance_dedicated', 'release_dedicated'],
    'non_dedicated': ['debug', 'debug_test', 'profile', 'profile_test', 'performance', 'release'],
    'test_all': ['debug_test', 'debug_test_dedicated', 'profile_test', 'profile_test_dedicated'],
    'non_test': ['debug', 'debug_dedicated', 'profile', 'profile_dedicated', 'performance',
                 'performance_dedicated',
                 'release', 'release_dedicated']
}


def process_kw_macros_expansion(ctx, target, kw, platform, configuration):
    """
    Process the special kw expansion macros in the keyword dictionary based on the configuration
    Args:
        target:         Target name to report any warnings
        kw:             The keyword map to manipulate
        configuration:  The current build configuration
    """
    class KeywordMacroNDebug:
        """
        Keyword Macro handler to handle 'ndebug' macros.  These macros are a convenience macro to expand non-debug
        keywords to the appropriate non-debug configuration.  This is to reduce the need to repeat all of the non-debug
        configuration values in the keyword list
        """

        def process(self,keyword_name, keyword_value, current_platform, current_configuration):

            is_config_dedicated = current_configuration.endswith('_dedicated')
            is_kw_dedicated = '_dedicated' in kw_entry
            if is_config_dedicated != is_kw_dedicated:
                return None, None

            # Only process this keyword non-debug mode, otherwise it will be ignored
            if current_configuration not in ('debug', 'debug_dedicated', 'debug_test', 'debug_test_dedicated'):
                if '_ndebug_dedicated_' in kw_entry and is_config_dedicated:
                    new_kw_entry_name = keyword_name.replace('_ndebug_dedicated_','_{}_'.format(configuration))
                    return (new_kw_entry_name,keyword_value), keyword_name
                elif '_ndebug_' in kw_entry:
                    new_kw_entry_name = keyword_name.replace('_ndebug_','_{}_'.format(configuration))
                    return (new_kw_entry_name,keyword_value), keyword_name
                elif kw_entry.startswith('ndebug_dedicated') and is_config_dedicated:
                    new_kw_entry_name = keyword_name.replace('ndebug_dedicated_','{}_'.format(configuration))
                    return (new_kw_entry_name,keyword_value), keyword_name
                elif kw_entry.startswith('ndebug'):
                    new_kw_entry_name = keyword_name.replace('ndebug_','{}_'.format(configuration))
                    return (new_kw_entry_name,keyword_value), keyword_name
            return None, None

    class KeywordMacroShortcutAlias:
        """
        Keyword macro handler to handle shortcut aliases. These aliases are defined in waf_branch_spec and are used to
        group multiple configurations under one using a dictionary. This will make it so that aliases can be used in
        keywords as they are already used as a value for the 'configuration' keyword. For example:

        debug_test_file_list = 'module_tests.waf_files',
        profile_test_file_list = 'module_tests.waf_files'

        becomes

        test_all_file_list = 'module_tests.waf_files'
        """
        def process(self, keyword_name, keyword_value, current_platform, current_configuration):
            for alias, configs in LEGACY_CONFIGURATION_SHORTCUT_ALIASES.iteritems():
                if alias == 'all':
                    continue  # Do not use 'all' alias, it conflicts with other aliases
                if current_configuration not in configs:
                    continue
                if keyword_name.startswith(alias):
                    remove_entry_name = keyword_name
                    new_kw_entry_name = keyword_name.replace(alias, current_configuration)
                elif '_{}_'.format(alias) in keyword_name:
                    remove_entry_name = keyword_name
                    new_kw_entry_name = keyword_name.replace('_{}_'.format(alias), '_{}_'.format(current_configuration))
                else:
                    continue
                return (new_kw_entry_name, keyword_value), remove_entry_name
            return None, None

    class KeywordWinX64PlatformGeneralization:

        """
        Keyword macro handler support the generic win_x64 platform to expand to the current vs<version> platform
        """
        def process(self, keyword_name, keyword_value, current_platform, current_configuration):

            # This only processes for the win_ platform
            if not ctx.is_windows_platform(current_platform):
                return None, None

            # Skip any concrete win_x64 platforms
            for win_platform in LUMBERYARD_SETTINGS.get_platforms_for_alias('win'):
                if win_platform in kw_entry:
                    return None, None

            if kw_entry.startswith('win_x64_test') and current_platform.endswith('_test'):
                remove_entry_name = keyword_name
                new_kw_entry_name = keyword_name.replace('win_x64_test', current_platform)
            elif kw_entry.startswith('win_x64') and not current_platform.endswith('_test'):
                remove_entry_name = keyword_name
                new_kw_entry_name = keyword_name.replace('win_x64', current_platform)
            elif kw_entry.startswith('win_test') and current_platform.endswith('_test'):
                remove_entry_name = keyword_name
                new_kw_entry_name = keyword_name.replace('win_test', current_platform)
            elif kw_entry.startswith('win') and not current_platform.endswith('_test'):
                remove_entry_name = keyword_name
                new_kw_entry_name = keyword_name.replace('win', current_platform)
            else:
                return None, None

            return (new_kw_entry_name, keyword_value), remove_entry_name

    # Only valid and supported configurations
    if not configuration:
        return
    if not LUMBERYARD_SETTINGS.is_valid_configuration(configuration) and not LUMBERYARD_SETTINGS.is_valid_configuration_alias(configuration):
        return

    macros = [KeywordMacroNDebug(),KeywordWinX64PlatformGeneralization(),KeywordMacroShortcutAlias()]

    if configuration != 'project_generator':

        for macro in macros:
            kw_entries_to_add = []
            kw_entries_to_remove = []

            for kw_entry, kw_value in kw.iteritems():
                kw_entry_to_add, kw_entry_to_remove = macro.process(kw_entry, kw_value, platform, configuration)
                if kw_entry_to_add is not None or kw_entry_to_remove is not None:
                    if kw_entry_to_add is not None:
                        kw_entries_to_add.append(kw_entry_to_add)
                    if kw_entry_to_remove is not None:
                        kw_entries_to_remove.append(kw_entry_to_remove)

            if len(kw_entries_to_add)>0:
                for new_kw_key, new_kw_value in kw_entries_to_add:
                    if new_kw_key in kw:
                        append_to_unique_list(kw[new_kw_key], new_kw_value)
                    else:
                        kw[new_kw_key] = new_kw_value
            if len(kw_entries_to_remove)>0:
                for kw_to_delete in kw_entries_to_remove:
                    del kw[kw_to_delete]

###############################################################################
# Wrapper for ApplyPlatformSpecificFlags for all flags to apply
@conf
def ApplyPlatformSpecificSettings(ctx, kw, target):
    """
    Check each compiler/linker flag for platform specific additions
    """

    platform = ctx.env['PLATFORM']
    configuration = get_configuration( ctx, target )

    # Expand any special macros
    process_kw_macros_expansion(ctx, target, kw, platform, configuration)

    # handle list entries
    for entry in COMMON_INPUTS:
        append_kw_entry(kw,entry,GetPlatformSpecificSettings(ctx, kw, entry, platform, configuration))

    # Handle string entries
    for entry in 'output_file_name'.split():
        if not entry in kw or kw[entry] == []: # No general one set yet
            kw[entry] = GetPlatformSpecificSettings(ctx, kw, entry, platform, configuration)

    # Recurse for additional settings
    for setting in kw['additional_settings']:
        ApplyPlatformSpecificSettings(ctx, setting, target)


###############################################################################
# Set env in case a env overwrite is specified for this project
def ApplyConfigOverwrite(ctx, kw):

    target = kw['target']
    if not target in ctx.env['CONFIG_OVERWRITES']:
        return

    platform                    =  ctx.env['PLATFORM']
    overwrite_config    = ctx.env['CONFIG_OVERWRITES'][target]

    # Need to set crytek specific shortcuts if loading another enviorment
    ctx.all_envs[platform + '_' + overwrite_config]['PLATFORM']             = platform
    ctx.all_envs[platform + '_' + overwrite_config]['CONFIGURATION']  = overwrite_config

    # Create a deep copy of the env for overwritten task to prevent modifying other task generator envs
    kw['env'] = ctx.all_envs[platform + '_' + overwrite_config].derive()
    kw['env'] .detach()


@conf
def get_current_spec_defines(ctx):
    if ctx.env['PLATFORM'] == 'project_generator' or ctx.env['PLATFORM'] == []:
        return []   # Return only an empty list when generating a project

    return ctx.spec_defines()


@feature('apply_non_monolithic_launcher_settings')
@before_method('process_source')
def apply_non_monolithic_launcher_settings(self):
    self.env['LINKFLAGS_MACBUNDLE'] =  []  # disable the '-dynamiclib' flag


@feature('apply_monolithic_build_settings')
@before_method('process_source')
def apply_monolithic_build_settings(self):
    # Add collected settings to link task
    # Don't do 'list(set(...))' on these values as duplicates will be removed
    # by waf later and some arguments need to be next to each other (such as
    # -force_load <lib>). The set will rearrange the order in a
    # non-deterministic way

    def _apply_monolithic_build_settings(monolithic_dict, prefix=''):
        append_to_unique_list(self.use, list(monolithic_dict[prefix + 'use']))
        append_to_unique_list(self.uselib, list(monolithic_dict[prefix + 'uselib']))
        append_to_unique_list(self.framework, list(monolithic_dict[prefix + 'framework']))
        self.linkflags  += list(monolithic_dict[prefix + 'linkflags'])

        # static libs
        self.stlib      += list(monolithic_dict[prefix + 'stlib'])
        append_to_unique_list(self.stlibpath, list(monolithic_dict[prefix + 'stlibpath']))

        # shared libs
        self.lib        += list(monolithic_dict[prefix + 'lib'])
        append_to_unique_list(self.libpath, list(monolithic_dict[prefix + 'libpath']))

    Logs.debug("lumberyard: Applying monolithic build settings for %s ... " % self.name)

    if not hasattr(self.bld, 'monolithic_build_settings'):
        self.bld.monolithic_build_settings = defaultdict(lambda: [])

    # All CryEngineModules use AzCore
    _apply_monolithic_build_settings(self.bld.monolithic_build_settings)

    # if we're compiling a tool that isn't part of a project, then project_name will not be set.
    if getattr(self, 'project_name', None):
        # Add game specific files
        prefix = self.project_name + '_'
        _apply_monolithic_build_settings(self.bld.monolithic_build_settings,prefix)


@feature('apply_monolithic_build_settings')
@after_method('apply_link')
def apply_monolithic_pch_objs(self):

    """ You also need the output obj files from a MS PCH compile to be linked into any modules using it."""
    for tgen_name in self.use:

        try:
            other_tg = self.bld.get_tgen_by_name(tgen_name)
        except:
            # If we cannot find the use name, check if its a uselib
            if not is_third_party_uselib_configured(self.bld, tgen_name):
                Errors.WafError("Invalid 'use' reference ({}) defined in module {}'".format(tgen_name,self.name))

        other_pch_task = getattr(other_tg, 'pch_task', None)
        if other_pch_task:
            if other_pch_task.outputs[0] not in self.link_task.inputs:
                Logs.debug('Lumberyard: Monolithic build: Adding pch %s from %s to %s ' % (other_pch_task.outputs[0], tgen_name, self.target))
                self.link_task.inputs.append(other_pch_task.outputs[0])

@conf
def LoadSharedSettings(ctx, k, kw, file_path = None):
    shared_list = kw.get('shared_settings',None)

    if not shared_list:
        return

    source_folder = file_path
    if not source_folder:
        source_folder = ctx.CreateRootRelativePath('Code/Tools/SharedSettings')

    if not source_folder:
        return

    for settings_file in shared_list:
        this_file = os.path.join(source_folder, settings_file)

        if os.path.exists(this_file):
            source_node = ctx.root.make_node(this_file)
            parsed_json = ctx.parse_json_file(source_node)

            for key, value in parsed_json.iteritems():
                append_kw_entry(kw, key, value)

@conf
def is_building_dedicated_server(ctx):
    if ctx.cmd == 'configure':
        return False
    if ctx.cmd == 'generate_uber_files':
        return False
    if ctx.cmd == 'generate_module_def_files':
        return False
    if ctx.cmd == 'generate_module_dependency_files':
        return False

    config = ctx.env['CONFIGURATION']
    if (config == '') or (config == []) or (config == 'project_generator'):
        return False

    return config.endswith('_dedicated')


@feature('cxx')
@before_method('process_source')
@before_method('add_pch_msvc')
@before_method('add_pch_clang')
def apply_custom_flags(self):
    rtti_include_option = getattr(self,'enable_rtti',False)
    if  'msvc' in (self.env.CC_NAME, self.env.CXX_NAME):
        remove_release_define_option = getattr(self,'remove_release_define',False)
        rtti_flag = '/GR' if rtti_include_option == [True] else '/GR-'

        self.env['CXXFLAGS'] = list(filter(lambda r: not r.startswith('/GR'), self.env['CXXFLAGS']))
        self.env['CXXFLAGS'] += [rtti_flag]
        if remove_release_define_option:
            self.env['DEFINES'] = list(filter(lambda r: r != '_RELEASE', self.env['DEFINES']))
    else:
        if rtti_include_option == [True]:
            self.env['CXXFLAGS'] = list(filter(lambda r:not r.startswith('-fno-rtti'), self.env['CXXFLAGS']))


@feature('cxxprogram', 'cxxshlib', 'cprogram', 'cshlib', 'cxx', 'c')
@after_method('apply_link')
@before_method('process_use')
def set_link_outputs(self):
    if self.env['PLATFORM'] == 'project_generator' or not getattr(self, 'link_task', None):
        return
    # If no type is defined, this is just a stub task that shouldnt handle any additional build/link tasks
    if not hasattr(self,'_type'):
        return

    # apply_link creates the link task, and uses the output_file_name to create the outputs for the task
    # unfortunately, it will only create outputs in the build folder (BinTemp/) only.  The outputs are deleted here and
    # recreated to move them into the output folder without copying if appropriate (dll/exe).  these outputs may be
    # large, > 1GB for a pdb, and copying these files during a build can be a serious performance issue.
    # I opted for this method instead of modifying larger portions of waf, and trying to create the outputs in
    # a different directory earlier
    self.link_task.outputs = []

    # If there is an attribute for 'output_folder', then this is an override of the output target folder
    # Note that subfolder if present will still be applied below
    # SanitizeInputs() forces output_folder to be a list.
    output_folders = getattr(self, 'output_folder', None)
    if output_folders:
        if not isinstance(output_folders, list):
            output_folders = [output_folders]
        # Process each output folder and build the list of output nodes
        output_nodes = []
        for output_folder in output_folders:
            if os.path.isabs(output_folder):
                target_path = self.bld.root.make_node(output_folder)
            else:
                target_path = self.bld.path.make_node(output_folder)
            target_path.mkdir()
            output_nodes.append(target_path)

    else:
        output_nodes = self.bld.get_output_folders(self.bld.env['PLATFORM'], self.bld.env['CONFIGURATION'])

    # append sub folder if it exists
    output_sub_folder = getattr(self, 'output_sub_folder', None)
    if output_sub_folder:
        new_output_nodes = []
        for output_node in output_nodes:
            output_node = output_node.make_node(output_sub_folder)
            output_node.mkdir()
            new_output_nodes.append(output_node)
        output_nodes = new_output_nodes

    # process only the first output, additional outputs will copy from the first.  Its rare that additional copies are needed
    output_node = output_nodes[0]
    if self._type == 'stlib':
        # add_target() will create an output in the temp/intermediate directory.  Since .libs are not directly executable, they
        # are left in the temp directory.  If an additional lib copy is specified with copy_static_library, a copy will
        # be done below where additional copies are handled.
        self.link_task.add_target(self.output_file_name)
    else:
        # add_target() creates the intermediate nodes directories, but enforces that the output is always
        # under the parent task, which is a folder in the build directory.
        # Instead, use make_node()/set_outputs() to put the target will be in the output location

        # find the pattern to apply to the output target, similar to add_target()
        pattern = self.env[self.link_task.__class__.__name__ + '_PATTERN']
        if not pattern:
            pattern = '%s'

        # apply pattern to target
        target = pattern % self.output_file_name
        # create node (and intermediate folders)
        target = output_node.make_node(target)
        self.link_task.set_outputs(target)

    # remove extensions to get the name of the target, we will be using it as a base for additional outputs below
    target_node = self.link_task.outputs[0]
    name = os.path.splitext(target_node.name)[0]

    # msvc-specific handling.  its easier to do here because we have access to the output_nodes
    is_msvc = 'msvc' in (self.env.CC_NAME, self.env.CXX_NAME)
    is_secondary_copy_install = getattr(self, 'is_secondary_copy_install', False)
    if is_msvc:
        # add the import library to the output list.  Only add if secondary_copy_install is set
        if self._type == 'shlib' and (self.bld.artifacts_cache or is_secondary_copy_install):
            import_lib_node = output_node.make_node(name + '.lib')
            import_lib_exp_node = output_node.make_node(name + '.exp')
            self.link_task.set_outputs(import_lib_node)
            self.link_task.set_outputs(import_lib_exp_node)

        # create map files
        if self.bld.is_option_true('generate_map_file'):
            if self._type != 'stlib':
                map_file_node = output_node.make_node(name + '.map')
                self.link_task.outputs.append(map_file_node)
                self.env.append_value('LINKFLAGS', '/MAP:' + map_file_node.abspath())

        # add pdb to outputs
        if self.bld.is_option_true('generate_debug_info'):
            if self._type != 'stlib':
                pdb_node = output_node.make_node(name + '.pdb')
                self.link_task.outputs.append(pdb_node)

        if self.env.MSVC_MANIFEST:
            """
            Special linker for MSVC with support for embedding manifests into DLL's
            and executables compiled by Visual Studio 2005 or probably later. Without
            the manifest file, the binaries are unusable.
            See: http://msdn2.microsoft.com/en-us/library/ms235542(VS.80).aspx
            """

            if self._type == 'shlib' or getattr(self, "additional_manifests", None):
                man_node = output_node.make_node(target_node.name + '.manifest')
                self.link_task.outputs.append(man_node)
                self.link_task.do_manifest = True

    # prep additional copies variables
    output_sub_folder_copy_attr = getattr(self, 'output_sub_folder_copy', [])
    if isinstance(output_sub_folder_copy_attr, str):
        output_sub_folder_copy_attr = [ output_sub_folder_copy_attr ]
    is_import_library = is_msvc and self._type == 'shlib'   # only do import lib copy on windows, .dylibs/.so don't work like this
    skip_secondary_copy = (not output_sub_folder_copy_attr) and (is_import_library and not is_secondary_copy_install)

    # copy task creator lambda
    def _create_sub_folder_copy_task(output_sub_folder_copy, output_node):
        if output_sub_folder_copy is not None and output_sub_folder_copy != output_sub_folder:
            output_node_parent = output_node.parent.make_node(output_sub_folder_copy)
            output_node_copy = output_node_parent.make_node(os.path.basename(output_node.abspath()))
            self.create_copy_task(output_node, output_node_copy, False, True)

    # handle additional copies
    copy_static_library = getattr(self, 'copy_static_library', False)
    for idx, output_node in enumerate(output_nodes):
        # for additional output copies, create copy tasks
        for output in self.link_task.outputs:
            if (idx > 0) or copy_static_library:
                output_node_copy = output_node.make_node(output.name)
                self.create_copy_task(output, output_node_copy, False, True)

            # Special case to handle additional copies
            if not skip_secondary_copy:
                for output_sub_folder_copy_attr_item in output_sub_folder_copy_attr:
                    if isinstance(output_sub_folder_copy_attr_item, str):
                        # only add the import library to the output list (allowing it to be copied)
                        # if we're trying to do an install
                        _, ext = os.path.splitext(output.name)
                        if is_secondary_copy_install or (ext in ['.dll', '.pdb', '.dylib', '.so']):

                            # For secondary copies, we only want to perform them if we are building specifically for a spec
                            # that contains the current target, otherwise skip because this task may have been pulled in
                            # by the dependency tree check
                            if (self.bld.is_project_spec_specified() and self.target in self.bld.spec_modules(self.bld.options.project_spec)) or not self.bld.is_project_spec_specified():
                                _create_sub_folder_copy_task(output_sub_folder_copy_attr_item, output)
                    else:
                        Logs.warn("[WARN] attribute items in 'output_sub_folder_copy' must be a string.")


# Use this feature if you want to write to an existing exe/dll that may be currently in use.  The os prevents
# writing to files that are currently inuse because it may have not loaded the whole file, so changing part of it
# could be catastrophic later as it pages in the file.  We can get around this restriction by changing the file
# name: the same inode will still be used/referenced by the os to complete paging operations.  We will then be
# able to write to a new file with that existing file name.  The old process will continue to run as a .exe_tmp (or .dll_tmp) file.
# Another trick is used to delete the temporary renamed files: unlink.  When the os loads a file, it increments
# a ref count to the file and decrements it when its done with the file.  This is how temporary files work.  These
# files will be hidden if you try to look at them through explorer or ls.  Unlink allows us to remove the file
# system's reference to an existing file, but that file won't be deleted from the system until all programs that
# are using it also close their reference, whereas delete/erase requires that the file be removable at that time.
# Unfortunately, unlink() doesn't work well on all platforms, so its optional cleanup to help keep your temporaries low.
@feature('link_running_program')
@after_method('set_link_outputs')
def patch_run_method(self):
    if self.env['PLATFORM'] == 'project_generator' or not getattr(self, 'link_task', None):
        return

    def temp_file_name(file):
        temp_file_name = file + '_tmp'
        return temp_file_name

    # lambda to callback with a saved 'self' variable
    old_run = self.link_task.run
    def patch_run():
        # verify that the dest files are writable.
        file_rename_pairs = []
        unexpected_lock_error = False
        Logs.debug('link_running_program: applying: %s' % (self.name))
        for output in self.link_task.outputs:
            tgt = output.abspath()
            temp_tgt = temp_file_name(tgt)

            if os.access(temp_tgt, os.F_OK):
                if not os.access(temp_tgt, os.W_OK):
                    Logs.warn('link_running_program: temp file %s marked read-only, will cause issues' % (temp_tgt))
                # delete existing file.  On win, unlink() and delete() are the same implementation in python
                # unlink on posix allows us to remove a file if its currently in use
                try:
                    Logs.debug('link_running_program: removing temporary file %s' % (temp_tgt))
                    os.unlink(temp_tgt)
                except OSError as e:
                    if e.errno in [errno.EACCES]:
                        # something is currently locking the temp file, that is ok, assume the tgt file is available as the target
                        Logs.debug('link_running_program: unable to remove temporary file %s, assuming it in use' % (temp_tgt))
                        continue

            try:
                # check for file existence.
                file_exists = os.access(tgt, os.F_OK)
                if not file_exists:
                    Logs.debug('link_running_program: file %s doesnt exist - there should be no issue with locks' % (tgt))
                    continue

                # check for file write-ability.  must do this after checking for existance because it will fail silently
                # if the file doesn't exist, and F_OK is defined as 0 so it can't be or'd with the other constants
                # actually tests for both permissions because an unreadable file is useless, and some linkers will read
                # the contents of the file for delta modification
                file_writable = os.access(tgt, os.W_OK)
                if not file_writable:
                    # if the file doesn't have write permissions, let the compile task fail, it will provide a good error message
                    # renaming write-protected files should not occur, the user has decided that the file should not change
                    # and the build should respect that
                    Logs.warn('link_running_program: file has +W - %s' % (tgt))
                    continue

                # rename an existing file to temp file, may fail if tmp file is locked (in use)
                # rename them to a temporary file, which will allow a new
                # file of that name to be written to the actual dest
                Logs.debug('link_running_program: rename %s -> %s' % (tgt, temp_tgt))
                os.rename(tgt, temp_tgt)
                # save the rename, will try to unroll these changes on task error
                file_rename_pairs.append((tgt, temp_tgt))
            except OSError as e:
                # unable to move to temporary, which may fail if temp_tgt is in use.  If temp_tgt is write protected, it will
                # return EEXISTS, which is ok as long as tgt is also not locked.  There is a warning above for write
                # protected temp files.
                if e.errno in [errno.EACCES, errno.EEXIST]:
                    continue
                Logs.debug('link_running_program: unable to move %s -> %s due to errno %d' % (tgt, temp_tgt, e.errno))
                unexpected_lock_error = True

        # run the command, generating a new output files
        ret = 1         # assume failure in case an exception fires and fails to write ret
        try:
            ret = old_run()
        finally:
            if ret and unexpected_lock_error:
                # log a message if we detected an oddity before running the command
                Logs.error("link_running_program: Error detected trying to move files to temporary files before compiling, may cause compilation failure")

            if ret:
                # failure case, restore temporary file to original name, leaving the file still runnable
                for (tgt, temp_tgt) in file_rename_pairs:
                    try:
                        Logs.debug('link_running_program: bailing out - restoring %s -> %s' % (temp_tgt, tgt))
                        try:
                            # the linker may write out a partial file, which happens with pdb files that contain missing
                            # symbols.  Remove the potentially partial file before restoring
                            os.unlink(tgt)
                        except OSError as e:
                            # ignore errors, it will be caught in the next rename operation, if its actually an error
                            pass
                        os.rename(temp_tgt, tgt)
                    except OSError as e:
                        Logs.warn('link_running_program: Could not restore existing file on failure: %s -> %s' % (temp_tgt, tgt))
            else:
                # success case, unlink/delete temp file
                for (tgt, temp_tgt) in file_rename_pairs:
                    try:
                        # unlink() will mark files that are currently open for deletion on close
                        # unlink() and delete() system calls are slightly different, delete() won't return until
                        # it successfully deletes the file.  On Win python, these 2 function both map to delete()
                        # internally, which is wrong and confusing.  There is another windows api that works
                        # like unlink on posix, DeleteFile().  In previous versions of windows, this would work
                        # on executables as well, but no longer does.  Since this is optional cleanup, I didn't
                        # bother to attempt to use the windows specific api.  The individual executable should
                        # also attempt to clean up temp copies
                        Logs.debug('link_running_program: unlinking temp %s' % (temp_tgt))
                        os.unlink(temp_tgt)
                    except OSError as e:
                        if os.name != 'nt':
                            # other oses that unlink does what actually should won't hit this code path as much
                            # as win, and if it does, its more serious
                            Logs.error('link_running_program:  Could not unlink %s, temporary file may cause issues' % (temp_tgt))
                        # stop if any file fails to delete.  The exe will come first in this list, we want to
                        # leave the .pdb if it fails to delete the exe for any future debugging
                        # this may leave extra files, but shouldn't be harmful otherwise, and hopefully
                        # less harmful than deleting too many of them
                        break

        return ret


    # replace the link tasks' run function with the lambda above
    if self._type in ['program', 'shlib']:
        # replace the run method with a lambda, save the old run method so we can wrap/call it
        self.link_task.run = patch_run


###############################################################################
@conf
def CryEngineStaticLibrary(ctx, *k, **kw):
    """
    CryEngine Static Libraries are static libraries
    Except the build configuration requires a monolithic build
    """
    
    # Only create a test driver shared if the 'create_test_driver' is specified and set to true
    create_test_driver = kw.get('create_test_driver', False)
    if create_test_driver:
        kw_static, kw_shared = ctx.split_keywords_from_static_lib_definition_for_test(kw)
        ctx.CryEngineStaticLibraryLegacy(**kw_static)
        if kw_shared:
            ctx.CryEngineSharedLibrary(**kw_shared)
    else:
        ctx.CryEngineStaticLibraryLegacy(**kw)
