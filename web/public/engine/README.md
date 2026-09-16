# Generated Pickle WebAssembly

`pickle.js` and `pickle_web.wasm` are generated from the C++ engine at the repository root.

Build them with:

```bash
bash scripts/build_wasm.sh
```

The GitHub Actions build also regenerates these files after engine changes so the Vercel-hosted UI runs the same Pickle source as the native engine.
