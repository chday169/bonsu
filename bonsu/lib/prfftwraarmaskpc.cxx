/*
#############################################
##   Filename: prfftwhiomaskpc.cxx
##
##    Copyright (C) 2013 Marcus C. Newton
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.
##
## Contact: Bonsu.Devel@gmail.com
#############################################
*/

#define NO_IMPORT_ARRAY
#define PY_ARRAY_UNIQUE_SYMBOL prfftw_ARRAY_API
#include <Python.h>
#include "prfftwmodule.h"

void RAARMaskPC
(
	double* seqdata,
	double* expdata,
	double* support,
	double* mask,
	double gammaHWHM, 
	int gammaRS,
	int numiterRL,
	int startiterRL,
	int waititerRL,
	int zex,
	int zey,
	int zez,
	double beta,
	int startiter,
	int numiter,
	int ndim,
	double* rho_m1,
	double* pca_gamma_ft,
	int32_t* nn,
	double* residual,
	double* residualRL,
	int32_t* citer_flow,
	double* visual_amp_real,
	double* visual_phase_real,
	double* visual_amp_recip,
	double* visual_phase_recip,
	PyObject* updatereal,
	PyObject* updaterecip,
	PyObject* updatelog,
	PyObject* updatelog2,
	int accel
)
{
	Py_BEGIN_ALLOW_THREADS;

	fftw_init_threads();
	fftw_plan_with_nthreads(citer_flow[7]);

	fftw_plan torecip;
	fftw_plan toreal;
	int32_t iter;
	int i;
	double sos = 0.0;
	double sos1 = 0.0;
	double sos2 = 0.0;
	double res = 0.0;
	double norm = 0.0;
	int32_t update_count_real = 0;
	int32_t update_count_recip = 0;
	int32_t gamma_count = (int32_t) waititerRL +1;
	double itnsty_sum = 0.0;
	
	
	int len = ((int) nn[0]) * ((int) nn[1]) * ((int) nn[2]);
	double* pca_inten = (double*) fftw_malloc( 2*len * sizeof(double));
	
	double* pca_rho_m1_ft = (double*) fftw_malloc( 2*len * sizeof(double));
	
	
	wrap_array(pca_gamma_ft, nn, 1);
	double gamma_sum;
	SumArray(pca_gamma_ft, nn, &gamma_sum);
	ScaleArray(pca_gamma_ft, nn, (1.0/gamma_sum));

	if (!pca_inten || !pca_rho_m1_ft)
	{
		fftw_free(pca_inten);
		fftw_free(pca_rho_m1_ft);
		return;
	}
	
	
	
	double* pca_Idm_iter = (double*) fftw_malloc( 2*len * sizeof(double));
	double* pca_Idmdiv_iter = (double*) fftw_malloc( 2*len * sizeof(double));
	double* pca_IdmdivId_iter = (double*) fftw_malloc( 2*len * sizeof(double));
	if (!pca_Idm_iter || !pca_Idmdiv_iter || !pca_IdmdivId_iter)
	{
		fftw_free(pca_inten);
		fftw_free(pca_rho_m1_ft);
		fftw_free(pca_Idm_iter);
		fftw_free(pca_Idmdiv_iter);
		fftw_free(pca_IdmdivId_iter);
		return;
	}
	
	int32_t nnh[3] = {(nn[0] - zex), (nn[1] - zey), (nn[2] - zez)};
	if( nnh[0] < 1)
	{
		nnh[0] = 1;
	}
	if( nnh[1] < 1)
	{
		nnh[1] = 1;
	}
	if( nnh[2] < 1)
	{
		nnh[2] = 1;
	}
	
	
	fftw_plan torecip_tmp;
	fftw_plan toreal_tmp;
	int32_t nn2[3] = {0, 0, 0};
	nn2[0] = nn[0] + 2*(nn[0]/2);
	nn2[1] = nn[1] + 2*(nn[1]/2);
	nn2[2] = nn[2] + 2*(nn[2]/2);
	if( nn[0] == 1)
	{
		nn2[0] = nn[0];
	}
	if( nn[1] == 1)
	{
		nn2[1] = nn[1];
	}
	if( nn[2] == 1)
	{
		nn2[2] = nn[2];
	}
	int len2 = ((int) nn2[0]) * ((int) nn2[1]) * ((int) nn2[2]);
	double* tmpdata1 = (double*) fftw_malloc( 2*len2 * sizeof(double));
	double* tmpdata2 = (double*) fftw_malloc( 2*len2 * sizeof(double));
	if (!tmpdata1 || !tmpdata2)
	{
		fftw_free(pca_inten);
		fftw_free(pca_rho_m1_ft);
		fftw_free(pca_Idm_iter);
		fftw_free(pca_Idmdiv_iter);
		fftw_free(pca_IdmdivId_iter);
		fftw_free(tmpdata1);
		fftw_free(tmpdata2);
		return;
	}
	FFTPlan( &torecip_tmp, &toreal_tmp, tmpdata1, nn2, ndim );
	
	
	CopyArray(seqdata, rho_m1, nn); 
	FFTPlan( &torecip, &toreal, seqdata, nn, ndim );
	CopyArray(rho_m1, seqdata, nn); 

	MaskedSumOfSquares( expdata, mask, nn, &sos );

	for( iter=startiter; iter < (numiter+startiter); iter++)
	{
		while( citer_flow[1] == 1 ) sleep(PRFFTW_PSLEEP);
		if( citer_flow[1] == 2 ) break; 
		CopyArray( seqdata, rho_m1, nn );
		
		FFTStride(seqdata, nn, &torecip);
		if( (iter - startiter+1) == startiterRL )
		{
			CopyArray( seqdata, pca_rho_m1_ft, nn );
		}
		if( gamma_count > waititerRL &&  (iter - startiter+1) > startiterRL)
		{
			
			if(gammaRS > 0)
			{
				lorentz_ft_fill(pca_gamma_ft, nn, gammaHWHM);
				SumArray(pca_gamma_ft, nn, &gamma_sum);
				ScaleArray(pca_gamma_ft, nn, (1.0/gamma_sum));
				wrap_array(pca_gamma_ft, nn, 1);
			}
			
			citer_flow[8] = 0;
			for(i=0; i<numiterRL; i++)
			{
				if( citer_flow[1] == 2 ) break;
				ZeroArray(pca_Idmdiv_iter, nn);
				make_Id_iter(seqdata, pca_rho_m1_ft, pca_Idm_iter, nn);
				SumArray(pca_Idm_iter, nn, &itnsty_sum);
				
				
				
				CopyArray(pca_Idm_iter, pca_IdmdivId_iter, nn);
				conj_reflect(pca_IdmdivId_iter, nn);
				
				wrap_array(pca_Idm_iter, nn, -1);
				wrap_array(pca_gamma_ft, nn, -1);
				convolve_nomem2(pca_Idm_iter, pca_gamma_ft, ndim, nn, tmpdata1, tmpdata2, &torecip_tmp, &toreal_tmp);
				wrap_array(pca_Idm_iter, nn, 1);
				wrap_array(pca_gamma_ft, nn, 1);
				
				
				
				divide_I_Id_iter(expdata, pca_Idm_iter, mask, pca_Idmdiv_iter, nn);
				
				wrap_array(pca_IdmdivId_iter, nn, -1);
				wrap_array(pca_Idmdiv_iter, nn, -1);
				convolve_nomem2(pca_IdmdivId_iter, pca_Idmdiv_iter, ndim, nn, tmpdata1, tmpdata2, &torecip_tmp, &toreal_tmp);
				wrap_array(pca_IdmdivId_iter, nn, 1);
				wrap_array(pca_Idmdiv_iter, nn, 1);
				
				
				
				ScaleArray(pca_IdmdivId_iter, nn, (1.0/itnsty_sum));
				ExponentArray(pca_IdmdivId_iter, nn, accel);
				
				MultiplyArray(pca_gamma_ft, pca_IdmdivId_iter, pca_gamma_ft, nn);
				
				mask_gamma(pca_gamma_ft, nn, nnh); 
				
				SumArray(pca_IdmdivId_iter, nn, &residualRL[0]);
				residualRL[0] = residualRL[0]/((double) len);
				
				Py_BLOCK_THREADS;
				PyObject_CallObject(updatelog2, NULL);
				Py_UNBLOCK_THREADS;
				
				citer_flow[8] += 1;
				
				
				SumArray(pca_gamma_ft, nn, &gamma_sum);
				ScaleArray(pca_gamma_ft, nn, (1.0/gamma_sum));
			}

			gamma_count = 1;
			
			
			CopyArray( seqdata, pca_rho_m1_ft, nn );
		}
		
		
		if( citer_flow[5] > 0 && update_count_recip == citer_flow[5] ) 
		{
			CopyAmp( seqdata, visual_amp_recip, nn );
			if( citer_flow[6] > 0 ) CopyPhase( seqdata, visual_phase_recip, nn );
			update_count_recip = 0;
			Py_BLOCK_THREADS;
			PyObject_CallObject(updaterecip, NULL);
			Py_UNBLOCK_THREADS;
		}
		else
		{
			update_count_recip ++;
		}
		
		MaskedCalculateResiduals(seqdata, expdata, mask, nn, &res);
		
		if( (iter - startiter) > startiterRL)
		{
			CopySquare(seqdata, pca_inten, nn);
			wrap_array(pca_inten, nn, -1);
			wrap_array(pca_gamma_ft, nn, -1);
			convolve_nomem2(pca_inten, pca_gamma_ft, ndim, nn, tmpdata1, tmpdata2, &torecip_tmp, &toreal_tmp);
			wrap_array(pca_inten, nn, 1);
			wrap_array(pca_gamma_ft, nn, 1);
			MaskedSetPCAmplitudes(seqdata, expdata, pca_inten, mask, nn);
		}
		else
		{
			MaskedSetAmplitudes(seqdata, expdata, mask, nn);
		}
		FFTStride(seqdata, nn, &toreal);
		
		residual[iter] = (double) ( (double) res/sos);
		
		SumOfSquares( seqdata, nn, &sos1);
		RS_RAAR(seqdata, rho_m1, support, nn, beta );
		SumOfSquares( seqdata, nn, &sos2 );
		norm = sqrt( (double) sos1/sos2 );
		ScaleArray( seqdata, nn, norm );
		
		if( citer_flow[3] > 0 && update_count_real == citer_flow[3] ) 
		{
			CopyAmp( seqdata, visual_amp_real, nn );
			if( citer_flow[6] > 0 ) CopyPhase( seqdata, visual_phase_real, nn );
			update_count_real = 0;
			Py_BLOCK_THREADS;
			PyObject_CallObject(updatereal, NULL);
			Py_UNBLOCK_THREADS;
		}
		else
		{
			update_count_real ++;
		}
		
		Py_BLOCK_THREADS;
		PyObject_CallObject(updatelog, NULL);
		Py_UNBLOCK_THREADS;
		
		citer_flow[0] += 1;
		gamma_count += 1;
	}
	
	
	wrap_array(pca_gamma_ft, nn, -1);
	
	fftw_free(tmpdata1);
	fftw_free(tmpdata2);
	fftw_free(pca_inten);
	fftw_free(pca_rho_m1_ft);
	
	fftw_free(pca_Idm_iter);
	fftw_free(pca_Idmdiv_iter);
	fftw_free(pca_IdmdivId_iter);
	
	fftw_destroy_plan( torecip_tmp );
	fftw_destroy_plan( toreal_tmp );

	fftw_destroy_plan( torecip );
	fftw_destroy_plan( toreal );

	fftw_cleanup_threads();

	Py_END_ALLOW_THREADS;
}
