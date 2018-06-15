#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates build.ninja that will build GN."""

import contextlib
import errno
import logging
import optparse
import os
import platform
import shutil
import subprocess
import sys
import tempfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)
GN_ROOT = os.path.join(REPO_ROOT, 'tools', 'gn')

is_win = sys.platform.startswith('win')
is_linux = sys.platform.startswith('linux')
is_mac = sys.platform.startswith('darwin')
is_posix = is_linux or is_mac


def main(argv):
  parser = optparse.OptionParser(description=sys.modules[__name__].__doc__)
  parser.add_option('-d', '--debug', action='store_true',
                    help='Do a debug build. Defaults to release build.')
  parser.add_option('-v', '--verbose', action='store_true',
                    help='Log more details')
  options, args = parser.parse_args(argv)

  if args:
    parser.error('Unrecognized command line arguments: %s.' % ', '.join(args))

  logging.basicConfig(level=logging.DEBUG if options.verbose else logging.ERROR)

  out_dir = os.path.join(REPO_ROOT, 'out')
  if not os.path.isdir(out_dir):
    os.makedirs(out_dir)
  write_gn_ninja(os.path.join(out_dir, 'build.ninja'), options)
  return 0


def write_generic_ninja(path, static_libraries, executables,
                        cc, cxx, ar, ld, options,
                        cflags=[], cflags_cc=[], ldflags=[], libflags=[],
                        include_dirs=[], solibs=[]):
  ninja_header_lines = [
    'cc = ' + cc,
    'cxx = ' + cxx,
    'ar = ' + ar,
    'ld = ' + ld,
    '',
    'rule regen',
    '  command = %s ../build/gen.py%s' % (
        sys.executable, ' -d' if options.debug else ''),
    '  description = Regenerating ninja files',
    '',
    'build build.ninja: regen',
    '  generator = 1',
    '  depfile = build.ninja.d',
    '',
  ]


  template_filename = os.path.join(SCRIPT_DIR, {
      'win32': 'build_win.ninja.template',
      'darwin': 'build_mac.ninja.template',
      'linux2': 'build_linux.ninja.template'
  }[sys.platform])

  with open(template_filename) as f:
    ninja_template = f.read()

  if is_win:
    executable_ext = '.exe'
    library_ext = '.lib'
    object_ext = '.obj'
  else:
    executable_ext = ''
    library_ext = '.a'
    object_ext = '.o'

  def escape_path_ninja(path):
      return path.replace('$ ', '$$ ').replace(' ', '$ ').replace(':', '$:')

  def src_to_obj(path):
    return escape_path_ninja('%s' % os.path.splitext(path)[0] + object_ext)

  def library_to_a(library):
    return '%s%s' % (library, library_ext)

  ninja_lines = []
  def build_source(src_file, settings):
    ninja_lines.extend([
        'build %s: %s %s' % (src_to_obj(src_file),
                             settings['tool'],
                             escape_path_ninja(
                                 os.path.join(REPO_ROOT, src_file))),
        '  includes = %s' % ' '.join(
            ['-I' + escape_path_ninja(dirname) for dirname in
             include_dirs + settings.get('include_dirs', [])]),
        '  cflags = %s' % ' '.join(cflags + settings.get('cflags', [])),
        '  cflags_cc = %s' %
            ' '.join(cflags_cc + settings.get('cflags_cc', [])),
    ])

  for library, settings in static_libraries.iteritems():
    for src_file in settings['sources']:
      build_source(src_file, settings)

    ninja_lines.append('build %s: alink_thin %s' % (
        library_to_a(library),
        ' '.join([src_to_obj(src_file) for src_file in settings['sources']])))
    ninja_lines.append('  libflags = %s' % ' '.join(libflags))


  for executable, settings in executables.iteritems():
    for src_file in settings['sources']:
      build_source(src_file, settings)

    ninja_lines.extend([
      'build %s%s: link %s | %s' % (
          executable, executable_ext,
          ' '.join([src_to_obj(src_file) for src_file in settings['sources']]),
          ' '.join([library_to_a(library) for library in settings['libs']])),
      '  ldflags = %s' % ' '.join(ldflags),
      '  solibs = %s' % ' '.join(solibs),
      '  libs = %s' % ' '.join(
          [library_to_a(library) for library in settings['libs']]),
    ])

  ninja_lines.append('')  # Make sure the file ends with a newline.

  with open(path, 'w') as f:
    f.write('\n'.join(ninja_header_lines))
    f.write(ninja_template)
    f.write('\n'.join(ninja_lines))

  with open(path + '.d', 'w') as f:
    f.write('build.ninja: ' +
            os.path.relpath(os.path.join(SCRIPT_DIR, 'gen.py'),
                            os.path.dirname(path)) + ' ' +
            os.path.relpath(template_filename, os.path.dirname(path)) + '\n')


def write_gn_ninja(path, options):
  if is_win:
    cc = os.environ.get('CC', 'cl.exe')
    cxx = os.environ.get('CXX', 'cl.exe')
    ld = os.environ.get('LD', 'link.exe')
    ar = os.environ.get('AR', 'lib.exe')
  else:
    cc = os.environ.get('CC', 'cc')
    cxx = os.environ.get('CXX', 'c++')
    ld = cxx
    ar = os.environ.get('AR', 'ar')

  cflags = os.environ.get('CFLAGS', '').split()
  cflags_cc = os.environ.get('CXXFLAGS', '').split()
  ldflags = os.environ.get('LDFLAGS', '').split()
  libflags = os.environ.get('LIBFLAGS', '').split()
  include_dirs = [REPO_ROOT, os.path.join(REPO_ROOT, 'src')]
  libs = []

  if is_posix:
    if options.debug:
      cflags.extend(['-O0', '-g'])
    else:
      # The linux::ppc64 BE binary doesn't "work" when
      # optimization level is set to 2 (0 works fine).
      # Note that the current bootstrap script has no way to detect host_cpu.
      # This can be easily fixed once we start building using a GN binary,
      # as the optimization flag can then just be set using the
      # logic inside //build/toolchain.
      cflags.extend(['-O2', '-g0'])

    cflags.extend([
        '-D_FILE_OFFSET_BITS=64',
        '-D__STDC_CONSTANT_MACROS', '-D__STDC_FORMAT_MACROS',
        '-pthread',
        '-pipe',
        '-fno-exceptions'
    ])
    cflags_cc.extend(['-std=c++14', '-Wno-c++11-narrowing'])
  elif is_win:
    if not options.debug:
      cflags.extend(['/Ox', '/DNDEBUG', '/GL'])
      libflags.extend(['/LTCG'])
      ldflags.extend(['/LTCG', '/OPT:REF', '/OPT:ICF'])

    cflags.extend([
        '/DNOMINMAX',
        '/DUNICODE',
        '/DWIN32_LEAN_AND_MEAN',
        '/DWINVER=0x0A00',
        '/D_CRT_SECURE_NO_DEPRECATE',
        '/D_SCL_SECURE_NO_DEPRECATE',
        '/D_UNICODE',
        '/D_WIN32_WINNT=0x0A00',
        '/FS',
        '/Gy',
        '/W4',
        '/WX',
        '/Zi',
        '/wd4099',
        '/wd4100',
        '/wd4127',
        '/wd4244',
        '/wd4267',
        '/wd4838',
        '/wd4996',
    ])
    cflags_cc.extend([
        '/GR-',
        '/D_HAS_EXCEPTIONS=0',
    ])

    ldflags.extend(['/DEBUG', '/MACHINE:x64'])

  static_libraries = {
      'base': {'sources': [
        'base/callback_internal.cc',
        'base/command_line.cc',
        'base/environment.cc',
        'base/files/file.cc',
        'base/files/file_enumerator.cc',
        'base/files/file_path.cc',
        'base/files/file_path_constants.cc',
        'base/files/file_util.cc',
        'base/files/scoped_file.cc',
        'base/files/scoped_temp_dir.cc',
        'base/json/json_parser.cc',
        'base/json/json_reader.cc',
        'base/json/json_string_value_serializer.cc',
        'base/json/json_writer.cc',
        'base/json/string_escape.cc',
        'base/logging.cc',
        'base/md5.cc',
        'base/memory/ref_counted.cc',
        'base/memory/weak_ptr.cc',
        'base/process/kill.cc',
        'base/process/memory.cc',
        'base/process/process_handle.cc',
        'base/process/process_iterator.cc',
        'base/sha1.cc',
        'base/strings/string_number_conversions.cc',
        'base/strings/string_piece.cc',
        'base/strings/string_split.cc',
        'base/strings/string_util.cc',
        'base/strings/string_util_constants.cc',
        'base/strings/stringprintf.cc',
        'base/strings/utf_string_conversion_utils.cc',
        'base/strings/utf_string_conversions.cc',
        'base/third_party/icu/icu_utf.cc',
        'base/time/clock.cc',
        'base/time/time.cc',
        'base/timer/elapsed_timer.cc',
        'base/value_iterators.cc',
        'base/values.cc',
      ], 'tool': 'cxx', 'include_dirs': []},
      'gn_lib': {'sources': [
        'src/exe_path.cc',
        'src/msg_loop.cc',
        'src/sys_info.cc',
        'src/worker_pool.cc',
        'tools/gn/action_target_generator.cc',
        'tools/gn/action_values.cc',
        'tools/gn/analyzer.cc',
        'tools/gn/args.cc',
        'tools/gn/binary_target_generator.cc',
        'tools/gn/builder.cc',
        'tools/gn/builder_record.cc',
        'tools/gn/build_settings.cc',
        'tools/gn/bundle_data.cc',
        'tools/gn/bundle_data_target_generator.cc',
        'tools/gn/bundle_file_rule.cc',
        'tools/gn/c_include_iterator.cc',
        'tools/gn/command_analyze.cc',
        'tools/gn/command_args.cc',
        'tools/gn/command_check.cc',
        'tools/gn/command_clean.cc',
        'tools/gn/command_desc.cc',
        'tools/gn/command_format.cc',
        'tools/gn/command_gen.cc',
        'tools/gn/command_help.cc',
        'tools/gn/command_ls.cc',
        'tools/gn/command_path.cc',
        'tools/gn/command_refs.cc',
        'tools/gn/commands.cc',
        'tools/gn/config.cc',
        'tools/gn/config_values.cc',
        'tools/gn/config_values_extractors.cc',
        'tools/gn/config_values_generator.cc',
        'tools/gn/copy_target_generator.cc',
        'tools/gn/create_bundle_target_generator.cc',
        'tools/gn/deps_iterator.cc',
        'tools/gn/desc_builder.cc',
        'tools/gn/eclipse_writer.cc',
        'tools/gn/err.cc',
        'tools/gn/escape.cc',
        'tools/gn/exec_process.cc',
        'tools/gn/filesystem_utils.cc',
        'tools/gn/function_exec_script.cc',
        'tools/gn/function_foreach.cc',
        'tools/gn/function_forward_variables_from.cc',
        'tools/gn/function_get_label_info.cc',
        'tools/gn/function_get_path_info.cc',
        'tools/gn/function_get_target_outputs.cc',
        'tools/gn/function_process_file_template.cc',
        'tools/gn/function_read_file.cc',
        'tools/gn/function_rebase_path.cc',
        'tools/gn/functions.cc',
        'tools/gn/function_set_defaults.cc',
        'tools/gn/function_set_default_toolchain.cc',
        'tools/gn/functions_target.cc',
        'tools/gn/function_template.cc',
        'tools/gn/function_toolchain.cc',
        'tools/gn/function_write_file.cc',
        'tools/gn/group_target_generator.cc',
        'tools/gn/header_checker.cc',
        'tools/gn/import_manager.cc',
        'tools/gn/inherited_libraries.cc',
        'tools/gn/input_conversion.cc',
        'tools/gn/input_file.cc',
        'tools/gn/input_file_manager.cc',
        'tools/gn/item.cc',
        'tools/gn/json_project_writer.cc',
        'tools/gn/label.cc',
        'tools/gn/label_pattern.cc',
        'tools/gn/lib_file.cc',
        'tools/gn/loader.cc',
        'tools/gn/location.cc',
        'tools/gn/ninja_action_target_writer.cc',
        'tools/gn/ninja_binary_target_writer.cc',
        'tools/gn/ninja_build_writer.cc',
        'tools/gn/ninja_bundle_data_target_writer.cc',
        'tools/gn/ninja_copy_target_writer.cc',
        'tools/gn/ninja_create_bundle_target_writer.cc',
        'tools/gn/ninja_group_target_writer.cc',
        'tools/gn/ninja_target_writer.cc',
        'tools/gn/ninja_toolchain_writer.cc',
        'tools/gn/ninja_utils.cc',
        'tools/gn/ninja_writer.cc',
        'tools/gn/operators.cc',
        'tools/gn/output_file.cc',
        'tools/gn/parse_node_value_adapter.cc',
        'tools/gn/parser.cc',
        'tools/gn/parse_tree.cc',
        'tools/gn/path_output.cc',
        'tools/gn/pattern.cc',
        'tools/gn/pool.cc',
        'tools/gn/qt_creator_writer.cc',
        'tools/gn/runtime_deps.cc',
        'tools/gn/scheduler.cc',
        'tools/gn/scope.cc',
        'tools/gn/scope_per_file_provider.cc',
        'tools/gn/settings.cc',
        'tools/gn/setup.cc',
        'tools/gn/source_dir.cc',
        'tools/gn/source_file.cc',
        'tools/gn/source_file_type.cc',
        'tools/gn/standard_out.cc',
        'tools/gn/string_utils.cc',
        'tools/gn/substitution_list.cc',
        'tools/gn/substitution_pattern.cc',
        'tools/gn/substitution_type.cc',
        'tools/gn/substitution_writer.cc',
        'tools/gn/switches.cc',
        'tools/gn/target.cc',
        'tools/gn/target_generator.cc',
        'tools/gn/template.cc',
        'tools/gn/token.cc',
        'tools/gn/tokenizer.cc',
        'tools/gn/tool.cc',
        'tools/gn/toolchain.cc',
        'tools/gn/trace.cc',
        'tools/gn/value.cc',
        'tools/gn/value_extractors.cc',
        'tools/gn/variables.cc',
        'tools/gn/visibility.cc',
        'tools/gn/visual_studio_utils.cc',
        'tools/gn/visual_studio_writer.cc',
        'tools/gn/xcode_object.cc',
        'tools/gn/xcode_writer.cc',
        'tools/gn/xml_element_writer.cc',
      ], 'tool': 'cxx', 'include_dirs': []},
  }

  executables = {
      'gn': {'sources': [ 'tools/gn/gn_main.cc' ],
      'tool': 'cxx', 'include_dirs': [], 'libs': []},

      'gn_unittests': { 'sources': [
        'src/test/gn_test.cc',
        'tools/gn/action_target_generator_unittest.cc',
        'tools/gn/analyzer_unittest.cc',
        'tools/gn/args_unittest.cc',
        'tools/gn/builder_unittest.cc',
        'tools/gn/c_include_iterator_unittest.cc',
        'tools/gn/command_format_unittest.cc',
        'tools/gn/config_unittest.cc',
        'tools/gn/config_values_extractors_unittest.cc',
        'tools/gn/escape_unittest.cc',
        'tools/gn/exec_process_unittest.cc',
        'tools/gn/filesystem_utils_unittest.cc',
        'tools/gn/function_foreach_unittest.cc',
        'tools/gn/function_forward_variables_from_unittest.cc',
        'tools/gn/function_get_label_info_unittest.cc',
        'tools/gn/function_get_path_info_unittest.cc',
        'tools/gn/function_get_target_outputs_unittest.cc',
        'tools/gn/function_process_file_template_unittest.cc',
        'tools/gn/function_rebase_path_unittest.cc',
        'tools/gn/function_template_unittest.cc',
        'tools/gn/function_toolchain_unittest.cc',
        'tools/gn/function_write_file_unittest.cc',
        'tools/gn/functions_target_unittest.cc',
        'tools/gn/functions_unittest.cc',
        'tools/gn/header_checker_unittest.cc',
        'tools/gn/inherited_libraries_unittest.cc',
        'tools/gn/input_conversion_unittest.cc',
        'tools/gn/label_pattern_unittest.cc',
        'tools/gn/label_unittest.cc',
        'tools/gn/loader_unittest.cc',
        'tools/gn/ninja_action_target_writer_unittest.cc',
        'tools/gn/ninja_binary_target_writer_unittest.cc',
        'tools/gn/ninja_build_writer_unittest.cc',
        'tools/gn/ninja_bundle_data_target_writer_unittest.cc',
        'tools/gn/ninja_copy_target_writer_unittest.cc',
        'tools/gn/ninja_create_bundle_target_writer_unittest.cc',
        'tools/gn/ninja_group_target_writer_unittest.cc',
        'tools/gn/ninja_target_writer_unittest.cc',
        'tools/gn/ninja_toolchain_writer_unittest.cc',
        'tools/gn/operators_unittest.cc',
        'tools/gn/parse_tree_unittest.cc',
        'tools/gn/parser_unittest.cc',
        'tools/gn/path_output_unittest.cc',
        'tools/gn/pattern_unittest.cc',
        'tools/gn/runtime_deps_unittest.cc',
        'tools/gn/scope_per_file_provider_unittest.cc',
        'tools/gn/scope_unittest.cc',
        'tools/gn/source_dir_unittest.cc',
        'tools/gn/source_file_unittest.cc',
        'tools/gn/string_utils_unittest.cc',
        'tools/gn/substitution_pattern_unittest.cc',
        'tools/gn/substitution_writer_unittest.cc',
        'tools/gn/target_unittest.cc',
        'tools/gn/template_unittest.cc',
        'tools/gn/test_with_scheduler.cc',
        'tools/gn/test_with_scope.cc',
        'tools/gn/tokenizer_unittest.cc',
        'tools/gn/unique_vector_unittest.cc',
        'tools/gn/value_unittest.cc',
        'tools/gn/visibility_unittest.cc',
        'tools/gn/visual_studio_utils_unittest.cc',
        'tools/gn/visual_studio_writer_unittest.cc',
        'tools/gn/xcode_object_unittest.cc',
        'tools/gn/xml_element_writer_unittest.cc',
      ], 'tool': 'cxx', 'include_dirs': [], 'libs': []},
  }

  if is_posix:
    static_libraries['base']['sources'].extend([
        'base/files/file_enumerator_posix.cc',
        'base/files/file_posix.cc',
        'base/files/file_util_posix.cc',
        'base/posix/file_descriptor_shuffle.cc',
        'base/posix/safe_strerror.cc',
        'base/process/kill_posix.cc',
        'base/process/process_handle_posix.cc',
        'base/process/process_posix.cc',
        'base/strings/string16.cc',
        'base/synchronization/condition_variable_posix.cc',
        'base/synchronization/lock_impl_posix.cc',
        'base/threading/platform_thread_posix.cc',
        'base/time/time_conversion_posix.cc',
    ])

  if is_linux:
    static_libraries['base']['sources'].extend([
        'base/process/internal_linux.cc',
        'base/process/memory_linux.cc',
        'base/process/process_handle_linux.cc',
        'base/process/process_info_linux.cc',
        'base/process/process_iterator_linux.cc',
        'base/strings/sys_string_conversions_posix.cc',
        'base/synchronization/waitable_event_posix.cc',
        'base/time/time_exploded_posix.cc',
        'base/time/time_now_posix.cc',
        'base/threading/platform_thread_linux.cc',
    ])
    libs.extend([
        '-lc',
        '-lgcc_s',
        '-lm',
        '-lpthread',
        '-lrt',
        '-latomic',
    ])

  if is_mac:
    static_libraries['base']['sources'].extend([
        'base/files/file_util_mac.mm',
        'base/mac/bundle_locations.mm',
        'base/mac/dispatch_source_mach.cc',
        'base/mac/foundation_util.mm',
        'base/mac/mach_logging.cc',
        'base/mac/scoped_mach_port.cc',
        'base/mac/scoped_nsautorelease_pool.mm',
        'base/process/process_handle_mac.cc',
        'base/process/process_info_mac.cc',
        'base/process/process_iterator_mac.cc',
        'base/strings/sys_string_conversions_mac.mm',
        'base/synchronization/waitable_event_mac.cc',
        'base/time/time_exploded_posix.cc',
        'base/time/time_mac.cc',
        'base/threading/platform_thread_mac.mm',
    ])

    libs.extend([
        '-framework', 'AppKit',
        '-framework', 'CoreFoundation',
        '-framework', 'Foundation',
        '-framework', 'Security',
    ])

  if is_win:
    static_libraries['base']['sources'].extend([
        'base/files/file_enumerator_win.cc',
        'base/files/file_util_win.cc',
        'base/files/file_win.cc',
        'base/process/kill_win.cc',
        'base/process/memory_win.cc',
        'base/process/process_handle_win.cc',
        'base/process/process_info_win.cc',
        'base/process/process_iterator_win.cc',
        'base/process/process_win.cc',
        'base/strings/sys_string_conversions_win.cc',
        'base/synchronization/condition_variable_win.cc',
        'base/synchronization/lock_impl_win.cc',
        'base/synchronization/waitable_event_win.cc',
        'base/threading/platform_thread_win.cc',
        'base/time/time_win.cc',
        'base/win/core_winrt_util.cc',
        'base/win/enum_variant.cc',
        'base/win/iat_patch_function.cc',
        'base/win/iunknown_impl.cc',
        'base/win/pe_image.cc',
        'base/win/process_startup_helper.cc',
        'base/win/registry.cc',
        'base/win/resource_util.cc',
        'base/win/scoped_handle.cc',
        'base/win/scoped_process_information.cc',
        'base/win/shortcut.cc',
        'base/win/startup_information.cc',
        'base/win/win_util.cc',
        'base/win/windows_version.cc',
    ])

    libs.extend([
        'advapi32.lib',
        'dbghelp.lib',
        'kernel32.lib',
        'ole32.lib',
        'shell32.lib',
        'user32.lib',
        'userenv.lib',
        'version.lib',
        'winmm.lib',
        'ws2_32.lib',
        'Shlwapi.lib',
    ])

  # we just build static libraries that GN needs
  executables['gn']['libs'].extend(static_libraries.keys())
  executables['gn_unittests']['libs'].extend(static_libraries.keys())

  write_generic_ninja(path, static_libraries, executables, cc, cxx, ar, ld,
                      options, cflags, cflags_cc, ldflags, libflags,
                      include_dirs, libs)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
