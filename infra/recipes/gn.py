# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.
"""Recipe for building GN."""

DEPS = [
    'recipe_engine/buildbucket',
    'recipe_engine/cipd',
    'recipe_engine/context',
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
        'win': {},
    }[api.platform.name])
    api.cipd.ensure(cipd_dir, packages)

  environ = {
      'linux': {
          'CC': cipd_dir.join('bin', 'clang'),
          'CXX': cipd_dir.join('bin', 'clang++'),
          'AR': cipd_dir.join('bin', 'llvm-ar'),
          'LDFLAGS': '-static-libstdc++ -ldl -lpthread',
      },
      'mac': {},
      'win': {},
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
        with api.context(env=environ, cwd=src_dir):
          api.python('generate',
                     src_dir.join('build', 'gen.py'),
                     args=config['args'])

        api.step('ninja', [cipd_dir.join('ninja'), '-C', src_dir.join('out')])

      with api.context(cwd=src_dir):
        api.step('test', [src_dir.join('out', 'gn_unittests')])


def GenTests(api):
  for platform in ('linux', 'mac', 'win'):
    yield (api.test('ci_' + platform) + api.platform.name(platform) +
           api.properties(buildbucket={
               'build': {
                   'tags': [
                       'buildset:commit/gitiles/gn.googlesource.com/gn/+/'
                       'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
                   ]
               }
           }))
    yield (
        api.test('cq_' + platform) + api.platform.name(platform) +
        api.properties(buildbucket={
            'build': {
                'tags': [
                    'buildset:patch/gerrit/gn-review.googlesource.com/1000/1',
                ]
            }
        }))
