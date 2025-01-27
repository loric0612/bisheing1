/* compile with: cmex gammaln.c mexutil.c util.c -lm
 * test in matlab:
 *   gammaln(1:10)
 */
#include "mex.h"
#include "mexutil.h"
#include "util.h"

void mexFunction(int nlhs, mxArray *plhs[],
		 int nrhs, const mxArray *prhs[])
{
  int ndims, len, i;
  int *dims;
  double *indata, *outdata, d;

  if((nlhs > 1) || (nrhs < 1) || (nrhs > 2))    
    mexErrMsgTxt("Usage: x = gammaln(n) or gammaln(n,d)");

  /* prhs[0] is first argument.
   * mxGetPr returns double*  (data, col-major)
   */
  ndims = mxGetNumberOfDimensions(prhs[0]);
  dims = (int*)mxGetDimensions(prhs[0]);
  indata = mxGetPr(prhs[0]);
  len = mxGetNumberOfElements(prhs[0]);

  if(mxIsSparse(prhs[0]))
    mexErrMsgTxt("Cannot handle sparse matrices.  Sorry.");

  /* plhs[0] is first output */
  plhs[0] = mxCreateNumericArrayE(ndims, dims, mxDOUBLE_CLASS, mxREAL);
  outdata = mxGetPr(plhs[0]);

  /* compute gammaln of every element */
  if(nrhs == 1) {
    for(i=0;i<len;i++)
      *outdata++ = gammaln(*indata++);
  } else {
    d = *mxGetPr(prhs[1]);
    for(i=0;i<len;i++)
      *outdata++ = gammaln2(*indata++,d);
  }
}

