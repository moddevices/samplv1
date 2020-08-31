// samplv1_pshifter.cpp
//
/****************************************************************************
   Copyright (C) 2012-2020, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#include "samplv1_pshifter.h"

#ifdef CONFIG_LIBRUBBERBAND
#include <rubberband/RubberBandStretcher.h>
#else

#include <stdlib.h>
#include <string.h>

#include <math.h>

#ifdef CONFIG_FFTW3
// nothing here.
#else

/*
	Based on: smbPitchShift.cpp
	Version: 1.2
	URL: http://www.dspdimension.com

	Routine for doing pitch shifting while maintaining
	duration using the Short Time Fourier Transform.

	COPYRIGHT 1999-2009 Stephan M. Bernsee <smb [AT] dspdimension [DOT] com>

			The Wide Open License (WOL)

	Permission to use, copy, modify, distribute and sell this software and its
	documentation for any purpose is hereby granted without fee, provided that
	the above copyright notice and this license appear in all source copies. 
	THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY OF
	ANY KIND. See http://www.dspguru.com/wol.htm for more information.
*/ 

/* 
	FFT routine, (C) 1996 Stephan M. Bernsee.
	
	sign = +1 is FFT (direct); -1 is iFFT (inverse).

	Fills pframes[0...2*nsize-1] with the Fourier transform of the
	time domain data in pframes[0...2*nsize-1]. The FFT array takes
	and returns the cosine and sine parts in an interleaved manner,
	ie. pframes[0] = cosPart[0], pframes[1] = sinPart[0], asf. nsize
	must be a power of 2. It expects a complex input signal;
	ie. when working with 'common' audio signals our input signal has
	to be passed as {in[0],0,in[1],0,in[2],0,...} asf. In that case,
	the transform of the frequencies of interest is in pframes[0...nsize].
*/
static
void smbFft ( float *pframes, uint32_t nframes, int sign )
{
	uint32_t i, bitm, j, le, le2, k;
	float wr, wi, arg, *p1, *p2, temp;
	float tr, ti, ur, ui, *p1r, *p1i, *p2r, *p2i;

	const uint32_t nsize2 = (nframes << 1);
	for (i = 2; i < nsize2 - 2; i += 2) {
		for (bitm = 2, j = 0; bitm < nsize2; bitm <<= 1) {
			if (i & bitm) ++j;
			j <<= 1;
		}
		if (i < j) {
			p1 = pframes + i; p2 = pframes + j;
			temp = *p1; *(p1++) = *p2;
			*(p2++) = temp; temp = *p1;
			*p1 = *p2; *p2 = temp;
		}
	}

	const uint32_t nsize = uint32_t(::rintf(::log2f(nframes)));
	for (k = 0, le = 2; k < nsize; ++k) {
		le <<= 1;
		le2 = (le >> 1);
		ur = 1.0f;
		ui = 0.0f;
		arg = M_PI / (le2 >> 1);
		wr = ::cosf(arg);
		wi = - sign * ::sinf(arg);
		for (j = 0; j < le2; j += 2) {
			p1r = pframes + j; p1i = p1r + 1;
			p2r = p1r + le2; p2i = p2r + 1;
			for (i = j; i < nsize2; i += le) {
				tr = *p2r * ur - *p2i * ui;
				ti = *p2r * ui + *p2i * ur;
				*p2r = *p1r - tr; *p2i = *p1i - ti;
				*p1r += tr; *p1i += ti;
				p1r += le; p1i += le;
				p2r += le; p2i += le;
			}
			tr = ur * wr - ui * wi;
			ui = ur * wi + ui * wr;
			ur = tr;
		}
	}
}

#endif	// CONFIG_FFTW3

#endif	// CONFIG_LIBRUBBERBAND


//---------------------------------------------------------------------------
// samplv1_pshifter - Pitch-shift processor.
//

// Constructor.
samplv1_pshifter::samplv1_pshifter (
	uint16_t nchannels, float srate, uint32_t nsize, uint16_t nover)
	: m_nchannels(nchannels), m_srate(srate), m_nsize(nsize), m_nover(nover)
{
#ifdef CONFIG_LIBRUBBERBAND
	// nothing to ctor.
#else

	// allocate working arrays
	m_fwind = new float [m_nsize];
	m_ififo = new float [m_nsize];
	m_ofifo = new float [m_nsize];
#ifdef CONFIG_FFTW3
	m_iwork = new float [m_nsize << 1];
	m_owork = new float [m_nsize << 1];
#else
	m_fwork = new float [m_nsize << 1];
#endif
	m_plast = new float [(m_nsize >> 1) + 1];
	m_phase = new float [(m_nsize >> 1) + 1];
	m_accum = new float [m_nsize << 1];
	m_afreq = new float [m_nsize];
	m_amagn = new float [m_nsize];
	m_sfreq = new float [m_nsize];
	m_smagn = new float [m_nsize];

	// initialize working arrays
//	::memset(m_fwind, 0, m_nsize * sizeof(float));
	::memset(m_ififo, 0, m_nsize * sizeof(float));
	::memset(m_ofifo, 0, m_nsize * sizeof(float));
#ifdef CONFIG_FFTW3
	::memset(m_iwork, 0, (m_nsize << 1) * sizeof(float));
	::memset(m_owork, 0, (m_nsize << 1) * sizeof(float));
#else
	::memset(m_fwork, 0, (m_nsize << 1) * sizeof(float));
#endif
	::memset(m_plast, 0, ((m_nsize >> 1) + 1) * sizeof(float));
	::memset(m_phase, 0, ((m_nsize >> 1) + 1) * sizeof(float));
	::memset(m_accum, 0, (m_nsize << 1) * sizeof(float));
	::memset(m_afreq, 0, m_nsize * sizeof(float));
	::memset(m_amagn, 0, m_nsize * sizeof(float));

#ifdef CONFIG_FFTW3
	// create plans
	m_aplan = ::fftwf_plan_r2r_1d(nsize, m_iwork, m_owork, FFTW_R2HC, FFTW_ESTIMATE);
	m_splan = ::fftwf_plan_r2r_1d(nsize, m_iwork, m_owork, FFTW_HC2R, FFTW_ESTIMATE);
#endif
	// pre-compute windowing table...
	for (uint32_t j = 0; j < m_nsize; ++j)
		m_fwind[j] = 0.5f - 0.5f * ::cosf(2.0f * M_PI * float(j) / float(m_nsize));

#endif	// !CONFIG_LIBRUBBERBAND
}


// Destructor.
samplv1_pshifter::~samplv1_pshifter (void)
{
#ifdef CONFIG_LIBRUBBERBAND
	// nothing to dtor.
#else
	// de-allocate working arrays
	delete [] m_fwind;
	delete [] m_ififo;
	delete [] m_ofifo;
#ifdef CONFIG_FFTW3
	delete [] m_iwork;
	delete [] m_owork;
#else
	delete [] m_fwork;
#endif
	delete [] m_plast;
	delete [] m_phase;
	delete [] m_accum;
	delete [] m_afreq;
	delete [] m_amagn;
	delete [] m_sfreq;
	delete [] m_smagn;
#ifdef CONFIG_FFTW3
	// destroy plans
	::fftwf_destroy_plan(m_aplan);
	::fftwf_destroy_plan(m_splan);
#endif
#endif	// !CONFIG_LIBRUBBERBAND
}


// Processor.
void samplv1_pshifter::process ( float **pframes, uint32_t nframes, float pshift )
{
#ifdef CONFIG_LIBRUBBERBAND
	RubberBand::RubberBandStretcher stretcher(
		size_t(m_srate), size_t(m_nchannels),
		RubberBand::RubberBandStretcher::OptionWindowLong |
		RubberBand::RubberBandStretcher::OptionChannelsTogether |
		RubberBand::RubberBandStretcher::OptionThreadingNever |
		RubberBand::RubberBandStretcher::OptionPitchHighQuality,
		1.0, double(pshift));
//	stretcher.setExpectedInputDuration(nframes);
	stretcher.setMaxProcessSize(nframes);
	stretcher.study(pframes, nframes, true);
	stretcher.process(pframes, nframes, true);
	uint32_t navail = stretcher.available();
	if (navail > nframes)
		navail = nframes;
	if (navail > 0)
		stretcher.retrieve(pframes, navail);
#else
	for (uint16_t k = 0; k < m_nchannels; ++k)
		process_k(pframes[k], nframes, pshift);
#endif	// CONFIG_LIBRUBBERBAND
}


// Channel processor.
void samplv1_pshifter::process_k ( float *pframes, uint32_t nframes, float pshift )
{
#ifdef CONFIG_LIBRUBBERBAND
#define UNUSED(x) (void)(x)
	UNUSED(pframes);
	UNUSED(nframes);
	UNUSED(pshift);
#undef UNUSED
#else

	// set up some handy variables
	const uint32_t nsize2   = (m_nsize >> 1);
	const uint32_t nstep    = (m_nsize / m_nover);
	const uint32_t nlatency = m_nsize - nstep;

	const float freqb = m_srate / float(m_nsize); // frequency per bin.
	const float expct = 2.0f * M_PI * float(nstep) / float(m_nsize);

	uint32_t i, j, k, noffset = nlatency;
	float magn, phase, temp;

	// main processing loop...
	//
	for (i = 0; i < nframes; ++i) {

		// as long as we have not yet collected enough data just read in
		m_ififo[noffset] = pframes[i];
		pframes[i] = m_ofifo[noffset - nlatency];
		++noffset;

		// now we have enough data for processing...
		//
		if (noffset >= m_nsize) {

			noffset = nlatency;

			// do windowing and real, imag interleave
			for (j = 0; j < m_nsize; ++j) {
			#ifdef CONFIG_FFTW3
				m_iwork[j] = m_ififo[j] * m_fwind[j];
				if (j > 0)
					m_iwork[(m_nsize << 1) - j] = 0.0f;
			#else
				k = (j << 1);
				m_fwork[k] = m_ififo[j] * m_fwind[j];
				m_fwork[k + 1] = 0.0f;
			#endif
			}

			// analysis direct transform...
			//
		#ifdef CONFIG_FFTW3
			::fftwf_execute(m_aplan);
		#else
			smbFft(m_fwork, m_nsize, +1);
		#endif

			// this is the analysis step...
			for (j = 0; j <= nsize2; ++j) {

				// de-interlace FFT buffer
			#ifdef CONFIG_FFTW3
				const float real = m_owork[j];
				const float imag = m_owork[m_nsize - j];
			#else
				k = (j << 1);
				const float real = m_fwork[k];
				const float imag = m_fwork[k + 1];
			#endif

				// compute magnitude and phase
				magn = 2.0f * ::sqrtf(real * real + imag * imag);
				phase = ::atan2f(imag, real);

				// compute phase difference
				temp = phase - m_plast[j];
				m_plast[j] = phase;

				// subtract expected phase difference
				temp -= float(j) * expct;

				// map delta phase into +/- Pi interval
				long q = ::lrintf(temp / M_PI);
				if (q < 0)
					q -= (q & 1);
				else 
					q += (q & 1);
				temp -= M_PI * float(q);

				// get deviation from bin frequency from the +/- Pi interval
				temp = m_nover * temp / (2.0f * M_PI);

				// compute the k-th partials' true frequency
				temp = float(j) * freqb + temp * freqb;

				// store magnitude and true frequency in analysis arrays
				m_amagn[j] = magn;
				m_afreq[j] = temp;
			}

			// do the actual pitch shifting...
			//
			::memset(m_sfreq, 0, m_nsize * sizeof(float));
			::memset(m_smagn, 0, m_nsize * sizeof(float));

			for (j = 0; j <= nsize2; ++j) {
				k = j * pshift;
				if (k <= nsize2) {
					m_sfreq[k]  = m_afreq[j] * pshift;
					m_smagn[k] += m_amagn[j];
				} 
			}

			// synthesis step...
			//
			for (j = 0; j <= nsize2; ++j) {

				// get magnitude and true frequency from synthesis arrays
				temp = m_sfreq[j];
				magn = m_smagn[j];

				// subtract bin mid frequency
				temp -= float(j) * freqb;

				// get bin deviation from freq deviation
				temp /= freqb;

				// take osamp into account
				temp = 2.0f * M_PI * temp / float(m_nover);

				// add the overlap phase advance back in
				temp += float(j) * expct;

				// accumulate delta phase to get bin phase
				m_phase[j] += temp;
				phase = m_phase[j];

				// get real and imag part and re-interleave 
			#ifdef CONFIG_FFTW3
				m_iwork[j] = magn * ::cosf(phase);
				m_iwork[m_nsize - j] = magn * ::sinf(phase);
			#else
				k = (j << 1);
				m_fwork[k] = magn * ::cosf(phase);
				m_fwork[k + 1] = magn * ::sinf(phase);
			#endif
			} 

			// synthesis inverse transform...
			//
		#ifdef CONFIG_FFTW3
			::fftwf_execute(m_splan);
		#else
			// zero negative frequencies
			for (j = m_nsize + 2; j < (m_nsize << 1); ++j)
				m_fwork[j] = 0.0f;
			smbFft(m_fwork, m_nsize, -1);
		#endif

			// do windowing and add to output accumulator
			for (j = 0; j < m_nsize; ++j)
			#ifdef CONFIG_FFTW3
				m_accum[j] += m_fwind[j] * m_owork[j] / (nsize2 * m_nover);
			#else
				m_accum[j] += 2.0f * m_fwind[j] * m_fwork[j << 1] / (nsize2 * m_nover);
			#endif

			for (j = 0; j < nstep; ++j)
				m_ofifo[j] = m_accum[j];

			// shift accumulator
			::memmove(m_accum, m_accum + nstep, m_nsize * sizeof(float));

			// move input
			for (j = 0; j < nlatency; ++j)
				m_ififo[j] = m_ififo[j + nstep];
		}
	}

	// shift result
	const uint32_t ndelta = (nlatency >> 1);

	::memmove(pframes, pframes + ndelta, (nframes - ndelta) * sizeof(float));

#endif	// CONFIG_LIBRUBBERBAND
}


// end of samplv1_pshifter.cpp