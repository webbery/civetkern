{
  "targets": [
    {
      "target_name": "civetkern",
      "sources": [
        "src/util/util.cpp",
        "src/util/pinyin.cpp",
        "src/interface.cpp",
        "src/db_manager.cpp",
        "src/log.cpp",
        "src/upgrader.cpp",
        "src/FileLink.cpp",
        "src/civetkern.cpp" ],
      "include_dirs": [
        "include",
        "src",
        "gqlite/include",
        # "<!(node -e \"require('nan')\")",
		    '<!@(node -p "require(\'node-addon-api\').include")',
      ],
      'cflags_c': [],
      'cflags_cc': [
        '-std=c++17',
        '-frtti',
        '-Wno-pessimizing-move'
      ],
      "cflags!": [
        '-fno-exceptions'
      ],
      "cflags_cc!": [
        '-fno-exceptions',
        '-std=gnu++1y',
        '-std=gnu++0x'
      ],
      
      'xcode_settings': {
        'CLANG_CXX_LANGUAGE_STANDARD': 'c++17',
        'MACOSX_DEPLOYMENT_TARGET': '10.9',
        'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
        'GCC_ENABLE_CPP_RTTI': 'YES',
        'OTHER_CPLUSPLUSFLAGS': [
          '-fexceptions',
          '-Wall',
          '-mmacosx-version-min=10.15',
          '-Wno-pessimizing-move'
          '-O3'
        ]
      },
      'conditions':[
        ['OS=="win"', {
          'configurations':{
            'Release': {
	            'msvs_settings': {
                'VCCLCompilerTool': {
                  "ExceptionHandling": 1,
                  'AdditionalOptions': [ '-std:c++17'],
                  'WholeProgramOptimization': 'true',
                  'RuntimeTypeInfo': 'true'
                },
                'VCLibrarianTool': {
                  'AdditionalOptions': [
                    '/LTCG:INCREMENTAL'
                  ]
                },
                'VCLinkerTool': {
                  'ImageHasSafeExceptionHandlers': 'false',
                  'OptimizeReferences': 2,
                  'EnableCOMDATFolding': 2,
                  'LinkIncremental': 1,
                  'AdditionalOptions': [
                    '/LTCG:INCREMENTAL'
                  ]
                }
              }
            },
            'Debug': {
	            'msvs_settings': {
                'VCCLCompilerTool': {
                  "ExceptionHandling": 1,
                  'AdditionalOptions': [ '-std:c++17' ],
                  'RuntimeTypeInfo': 'true'
                }
              }
            }
          },
          "libraries": [
            "<!(echo %cd%)/gqlite/build/Release/gqlite.lib"
          ]
        }],
        ['OS=="linux"', {
          "link_settings": {
            'library_dirs': ['<!(pwd)/gqlite/build'],
            'libraries': [
              '-lgqlite'
            ],
            'ldflags': [
              # Ensure runtime linking is relative to civetkern.node
              '-Wl,-s -Wl,--disable-new-dtags -Wl,-rpath=\'<!(pwd)/gqlite/build\''
            ]
          }
        }],
        ['OS=="mac"', {
          "link_settings": {
            'library_dirs': ['<!(pwd)/gqlite/build'],
            'libraries': [
              'libgqlite.dylib'
            ],
            'ldflags': [
              # Ensure runtime linking is relative to civetkern.node
              '-Wl,-s -Wl,--disable-new-dtags -Wl,-rpath=\'<!(pwd)/build/Release\''
            ]
          }
        }]
      ]
    }
  ],
  "scripts": {
    # "prebuild": ["<!(node ./prebuild.js)"]
  }
}