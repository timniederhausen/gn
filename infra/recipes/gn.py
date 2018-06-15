# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.
"""Recipe for building GN."""

DEPS = [
    'recipe_engine/buildbucket',
    'recipe_engine/cipd',
    'recipe_engine/context',
    'recipe_engine/json',
    'recipe_engine/path',
    'recipe_engine/platform',
    'recipe_engine/properties',
    'recipe_engine/python',
    'recipe_engine/step',
]


def RunSteps(api):
  src_dir = api.path['start_dir'].join('gn')

  with api.step.nest('git'), api.context(infra_steps=True):
    api.step('init', ['git', 'init', src_dir])

    with api.context(cwd=src_dir):
      build_input = api.buildbucket.build_input
      ref = (
          build_input.gitiles_commit.id
          if build_input.gitiles_commit else 'refs/heads/master')
      api.step('fetch', ['git', 'fetch', 'https://gn.googlesource.com/gn', ref])
      api.step('checkout', ['git', 'checkout', 'FETCH_HEAD'])
      for change in build_input.gerrit_changes:
        api.step('fetch %s/%s' % (change.change, change.patchset), [
            'git', 'fetch',
            'https://%s/gn' % change.host,
            'refs/changes/%s/%s/%s' %
            (str(change.change)[-2:], change.change, change.patchset)
        ])
        api.step('cherry-pick %s/%s' % (change.change, change.patchset),
                 ['git', 'cherry-pick', 'FETCH_HEAD'])

  with api.context(infra_steps=True):
    cipd_dir = api.path['start_dir'].join('cipd')
    packages = {
        'infra/ninja/${platform}': 'version:1.8.2',
    }
    packages.update({
        'linux': {
            'fuchsia/clang/${platform}': 'goma',
        },
        'mac': {},
        'win': {
            'chrome_internal/third_party/sdk/windows': 'uploaded:2018-06-13',
        },
    }[api.platform.name])
    api.cipd.ensure(cipd_dir, packages)

  environ_pre = {}

  win_env = {}
  if api.platform.name == 'win':
    # Load .../win_sdk/bin/SetEnv.x64.json to extract the required environment.
    # It contains a dict that looks like this:
    # {
    #   "env": {
    #     "VAR": [["..", "..", "x"], ["..", "..", "y"]],
    #     ...
    #   }
    # }
    # All these environment variables need to be added to the environment
    # for the compiler and linker to work.

    json_file = cipd_dir.join('win_sdk', 'bin', 'SetEnv.x64.json')
    env = api.json.read('SetEnv.x64.json', json_file).json.output.get('env')
    for k in env:
      # recipes' Path() does not like .., ., \, or /, so this is cumbersome.
      # What we want to do is:
      #   [sdk_bin_dir.join(*e) for e in env[k]]
      # Instead do that badly, and rely (but verify) on the fact that the paths
      # are all specified relative to the root, but specified relative to
      # win_sdk/bin (i.e. everything starts with "../../".)
      results = []
      for data in env[k]:
        assert data[0] == '..' and (data[1] == '..' or data[1] == '..\\')
        root_relative = data[2:]
        results.append('%s' % cipd_dir.join(*root_relative))

      # PATH is special-cased because we don't want to overwrite other things
      # like C:\Windows\System32. Others are replacements because prepending
      # doesn't necessarily makes sense, like VSINSTALLDIR.
      if k.lower() == 'path':
        # env_prefixes wants a list, not a string like env.
        environ_pre[k] = results
      else:
        win_env[k] = ';'.join(results)

  environ = {
      'linux': {
          'CC': cipd_dir.join('bin', 'clang'),
          'CXX': cipd_dir.join('bin', 'clang++'),
          'AR': cipd_dir.join('bin', 'llvm-ar'),
          'LDFLAGS': '-static-libstdc++ -ldl -lpthread',
      },
      'mac': {},
      'win': win_env,
  }[api.platform.name]

  configs = [
      {
          'name': 'debug',
          'args': ['-d']
      },
      {
          'name': 'release',
          'args': []
      },
  ]

  for config in configs:
    with api.step.nest(config['name']):
      with api.step.nest('build'):
        with api.context(env=environ, env_prefixes=environ_pre, cwd=src_dir):
          api.python('generate',
                     src_dir.join('build', 'gen.py'),
                     args=config['args'])

          # Windows requires the environment modifications when building too.
          api.step('ninja', [cipd_dir.join('ninja'), '-C', src_dir.join('out')])

        if api.platform.name == 'win':
          # Swarming won't be able to tidy up after the compiler leaves this
          # daemon running, so we have to manually kill it.
          api.step('taskkill mspdbsrv',
                   ['taskkill.exe', '/f', '/t', '/im', 'mspdbsrv.exe'])

        api.step('test', [src_dir.join('out', 'gn_unittests')])


def GenTests(api):
  WIN_TOOLCHAIN_DATA = {
    "env": {
      "VSINSTALLDIR": [["..", "..\\"]],
      "INCLUDE": [["..", "..", "win_sdk", "Include", "10.0.17134.0", "um"], ["..", "..", "win_sdk", "Include", "10.0.17134.0", "shared"], ["..", "..", "win_sdk", "Include", "10.0.17134.0", "winrt"]],
      "PATH": [["..", "..", "win_sdk", "bin", "10.0.17134.0", "x64"], ["..", "..", "VC", "Tools", "MSVC", "14.14.26428", "bin", "HostX64", "x64"]],
    }
  }
  for platform in ('linux', 'mac', 'win'):
    to_yield = (api.test('ci_' + platform) + api.platform.name(platform) +
                api.properties(buildbucket={
                  'build': {
                    'tags': [
                      'buildset:commit/gitiles/gn.googlesource.com/gn/+/'
                      'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
                    ]
                  }
                }))
    if platform == 'win':
      to_yield += api.override_step_data('SetEnv.x64.json',
          api.json.output(WIN_TOOLCHAIN_DATA))
    yield to_yield

    to_yield = (api.test('cq_' + platform) + api.platform.name(platform) +
                api.properties(buildbucket={
                  'build': {
                    'tags': [
                      'buildset:patch/gerrit/gn-review.googlesource.com/1000/1',
                    ]
                  }
                }))
    if platform == 'win':
      to_yield += api.override_step_data('SetEnv.x64.json',
          api.json.output(WIN_TOOLCHAIN_DATA))
    yield to_yield
