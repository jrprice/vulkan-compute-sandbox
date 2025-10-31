# Reproducer for a Vulkan FP16 bug on Nvidia devices

The SPIR-V shader in `shader.spvasm` (compiled into `out.spv`) declares two
constant arrays of Vec2<Float16> elements. Every element of the first array is
`(4.0h, 4.0h)`, and every element of the second array is `(1.0h, 1.0h)`. The
arrays are copied into private variables and then added together in a loop,
writing the results to a storage buffer.

The results should all be `(5.0h, 5.0h)`, but on Nvidia GPUs the result is
`(4.0h, 4.0h)`. The correct result is computed if the array length is changed:
```
length=19: FAIL
length=18: pass
length=17: FAIL
length=16: pass
length=15: FAIL
length=14: pass
length=13: pass
length=12: pass
length=11: pass
...
```

## Building and Running

Build with CMake, then run the binary:
```
./vulkan_compute_test.exe out.spv
```

## Output

```
Device name: NVIDIA RTX 4000 Ada Generation
Driver: 580.88
Output data:
0x4400 0x4400 0x4400 0x4400 0x4400 0x4400 0x4400 0x4400 0x4400 0x4400 0x4400 0x4400 0x4400 0x4400 0x4400 0x4400 0x4400 0x4400 0x4400 0x4400 0x4400 0x4400 0x4400 0x4400 0x4400 0x4400 0x4400 0x4400 0x4400 0x4400

FAIL (expected 0x4500)
```

This was run on a Windows machine with an RTX 4070 using driver 580.88.
It is expected to also reproduce on Linux, as the WebGPU conformance test suite
fails the same tests on Nvidia GPUs on our test farm.
