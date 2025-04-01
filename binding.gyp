{
  "targets": [
    {
      "target_name": "memorystore",
      "cflags!": ["-fno-exceptions"],
      "cflags_cc!": ["-fno-exceptions"],
      "sources": ["src/memorystore.cpp"],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "defines": ["NAPI_DISABLE_CPP_EXCEPTIONS"],
      "conditions": [
        ["OS=='win'", {
          "msvs_settings": {
            "VCCLCompilerTool": {
              "ExceptionHandling": 1
            }
          }
        }]
      ]
    }
  ]
}