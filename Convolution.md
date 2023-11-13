Disambiguation: see also [[Polygon Convolution]]

Convolution (see [wikipedia](http://en.wikipedia.org/wiki/Convolution)) is used in the GeoDms to calculate a [[Neighbourhood Potential]] using an input grid with n\*m elements and a kernel of k\*j elements, see the description of the [[Potential]] function.

The GeoDms utilizes the IPP 7.0 library convolution functions ippsConv_16s_Sfs, ippsConv_32f, ippsConv_64f in the OperPot.cpp code unit as default implementation for potential calculations. These functions perform an FFT on both their input arrays of size N, then multiply the spectra element-wize and then reverse-FFT the product to generate the requested result. The IPP 7.0 library also utilizes multiple cores when present and available, still available as potentialSlow for comparison. This results in a O(N\*log(N)) operation, whereas the classical naive implementation with four nested loops
requires O(n\*m\*k\*j) operations.

To make this possible, the 2D *n* \* *m* grid and *k* \* *j* grid are translated to two uniform signals of *N* = (*n*+*k*−1) \* (*m*+*j*−1) elements each with appropriate zero-padding.

The resulting grid can sometimes contain small round-off errors resulting from different frequencies that have to cancel-out, especially in empty zero-valued regions sometimes very small values occur, sometimes even negative when all input is non-negative. In order to remove these undesired artifacts, a smoothing post-processing is performed by default that resets to zero all values that are nearer to zero than $\\sqrt{\\sum\\limits_i(v_i^2) \\over 1000000000.0}$. The potential_raw operations does not perform this post-processing.

## ToDo

Investigate FFTW as substitute for IPPS 7.0

-   <http://www.fftw.org/fftw-paper-ieee.pdf>