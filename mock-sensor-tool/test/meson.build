gtest = dependency('gtest', main: true, disabler: true, required: false)
gmock = dependency('gmock', disabler: true, required: false)
if not gtest.found() or not gmock.found()
  gtest_proj = import('cmake').subproject('googletest', required: false)
  if gtest_proj.found()
    gtest = declare_dependency(
      dependencies: [
        dependency('threads'),
        gtest_proj.dependency('gtest'),
        gtest_proj.dependency('gtest_main'),
      ])
    gmock = gtest_proj.dependency('gmock')
  else
    assert(not build_tests.enabled(), 'Googletest is required')
  endif
endif

gtests = [
    'test-mock',
    'test-user',
]

foreach t : gtests
  test(t, executable(t.underscorify(), t + '.cpp',
                     build_by_default: false,
                     implicit_include_directories: false,
                     dependencies: [gtest, gmock]))
endforeach