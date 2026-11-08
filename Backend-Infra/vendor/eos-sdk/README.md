# Local EOS SDK layout

The EOS SDK is a local, proprietary build input and must not be committed. Extract the Linux SDK files into this exact layout before building:

```text
Backend-Infra/vendor/eos-sdk/
├── Include/
│   ├── eos_sdk.h
│   └── ...
└── lib/
    └── libEOSSDK-Linux-Shipping.so
```

Only this README is tracked. The headers and shared library are ignored by Git but intentionally remain available to the Docker build context.
