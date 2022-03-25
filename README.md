# Arctic

A script language for the needs of [IceShard](https://github.com/iceshard-engine/engine).

Goals the the script language:
* Separation of logic (bytecode), data (interop + constants) and VM (stack and registers)
* Minimal overhead during execution.
* Simple execution over multiple threads.
* Explicit typing, without implicit conversions.
* Direct access to AST for easy transpiling / testing / interop.
* Context based parsing and extension points.

Planed use cases:
* Direct access to arrays of entity data in script for quick updates.
* Script to Shader transpilation for VK, DX and others (if needed)
  * Unit Tests for shader scripts.
* Multi threaded updates. VM's are separated from data and bytecode, so they can run same script over various entities.

## Others

If you are interested or have questions. Feel free to message me.
