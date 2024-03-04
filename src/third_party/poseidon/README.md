# poseidon

C and x86_64 implementation of Poseidon hash function - first version.

## Building

- This version has been built and tested on MacOS. To build and test on Linux, replace the statement `.section __TEXT,__text` by `.section .text` in `f251.s`.
- To build the x86_64 accelerated version, define the macro ASSEMBLY.
- See the Makefile in `sources` folder for example.

## First report

- For the modular reduction, I've compared Barrett and Montgomery. The former was significantly slower in C and after some investigation I figured there was no way it would give better results in assembly, so I went with Montgomery. (I also considered sparse-modulus reduction such as Solinas, but it is not interesting for our prime).
- For the Montgomery multiplication, I reused the x86_64 code shared by Ilya with a tweak -namely an additional overflow reduction- to support two operands on [0,2^256) (while the original code requires one operand on [0,p)).
- For the MixLayer implementation, I've taken advantage of the particular form of the MDS matrix (only 1's except on the diagonal). The idea is to sum all the coordinates of the input vector, then each output coordinate equals this sum plus a small multiple of an input coordinate.
- For the AddRoundConstant, I’ve used a tweak which removes n-1 constant additions for each partial round (a constant addition is only necessary for the cell of the state entering the s-box).
- Here are some benchmarks for one Poseidon permutation on an Intel Core i5 (2.3GHz):
    - Full C implementation:
        - n=3 : 0.1694 ms (390219 cc)
        - n=4 : 0.1772 ms (408208 cc)
        - n=5 : 0.2082 ms (479637 cc)
        - n=9 : 0.3194 ms (735828 cc)
        
    - Assembly accelerated implementation:
        - n=3 : 0.0105 ms (24129 cc)
        - n=4 : 0.0109 ms (25218 cc)
        - n=5 : 0.0130 ms (29880 cc)
        - n=9 : 0.0194 ms (44636 cc)
        
    This makes between 150 (for n=9) and 250 (for n=3) cycles per byte.

## Still to be done

* Document entry points in `f251.h` and `poseidon.h`
* optimize context saving/restoring
* AVX2 version

## Contact

[matthieu.rivain@cryptoexperts.com](mailto:matthieu.rivain@cryptoexperts.com)