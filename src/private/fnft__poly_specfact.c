/*
 * This file is part of FNFT.
 *
 * FNFT is free software; you can redistribute it and/or
 * modify it under the terms of the version 2 of the GNU General
 * Public License as published by the Free Software Foundation.
 *
 * FNFT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Contributors:
 * Sander Wahls (TU Delft) 2018.
 */

#define FNFT_ENABLE_SHORT_NAMES

#include "fnft__errwarn.h"
#include "fnft__poly_specfact.h"
#include "fnft__fft_wrapper.h"

INT poly_specfact(const UINT deg,
                  COMPLEX const * const poly,
                  COMPLEX * const result,
                  const UINT oversampling_factor,
                  const INT kappa)
{
    if (deg == 0)
        return E_INVALID_ARGUMENT(deg);
    if (poly == NULL)
        return E_INVALID_ARGUMENT(poly);
    if (result == NULL)
        return E_INVALID_ARGUMENT(result);
    if (oversampling_factor == 0)
        return E_INVALID_ARGUMENT(oversampling_factor);

    // Prepare buffers and (inverse) FFT's

    fft_wrapper_plan_t plan_fwd = fft_wrapper_safe_plan_init();
    fft_wrapper_plan_t plan_inv = fft_wrapper_safe_plan_init();

    const UINT M = fft_wrapper_next_fft_length( (deg+1)*oversampling_factor );
    INT ret_code = SUCCESS;
    UINT i;

    COMPLEX * const buf_in = fft_wrapper_malloc(M * sizeof(COMPLEX));
    COMPLEX * const buf_out = fft_wrapper_malloc(M * sizeof(COMPLEX));
    COMPLEX * const buf_x = fft_wrapper_malloc(M * sizeof(COMPLEX));
    if (buf_in == NULL || buf_out == NULL || buf_x == NULL) {
        ret_code = E_NOMEM;
        goto leave_fun;
    }

    ret_code = fft_wrapper_create_plan(&plan_fwd, M, buf_in, buf_out, -1);
    CHECK_RETCODE(ret_code, leave_fun)
    ret_code = fft_wrapper_create_plan(&plan_inv, M, buf_in, buf_out, +1);
    CHECK_RETCODE(ret_code, leave_fun)

    // We now follow the description in Appendix B.4 of the book "Positive
    // Trigonometric Polynomials and Signal Processing Applications" by
    // B. Dumitrescu, Springer, 2007.

    // Step 1: Compute x_l = log |P(w_l)| [or x_l = 0.5*log 1+-|P(w_l)|^2)
    // if kappa != 0] on an oversampled grid

    for (i=0; i<=deg; i++)
        buf_in[i] = poly[i];
    for (i=deg+1; i<M; i++)
        buf_in[i] = 0.0;
    ret_code = fft_wrapper_execute_plan(plan_fwd, buf_in, buf_out);
    CHECK_RETCODE(ret_code, leave_fun);

    const REAL tol = SQRT(FNFT_EPSILON);
    INT warn_flag = 0;
    switch (kappa) {
    case 0:
        for (i=0; i<M; i++) {
            const REAL abs_buf = CABS(buf_out[i]);
            if (abs_buf < tol)
                warn_flag = 1;
            buf_x[i] = CLOG(abs_buf);
        }
        break;

    case -1:
        for (i=0; i<M; i++) {
            REAL abs_buf = CABS(buf_out[i]);
            buf_x[i] = 0.5*CLOG(1.0 + abs_buf*abs_buf);
        }
        break;

    case +1:
        for (i=0; i<M; i++) {
            REAL abs2_buf = CABS(buf_out[i]);
            abs2_buf *= abs2_buf;
            if (abs2_buf > 1.0 - tol)
                warn_flag = 1;
            buf_x[i] = 0.5*CLOG(1.0 - kappa*abs2_buf);
        }
        break;

    default:
        ret_code = E_INVALID_ARGUMENT(kappa);
        goto leave_fun;
    }
    if (warn_flag != 0)
        WARN("Ill-posed spectral factorization problem.");

    // Step 2: Compute the Hilbert transform y_l of x_l

    ret_code = fft_wrapper_execute_plan(plan_fwd, buf_x, buf_in);
    CHECK_RETCODE(ret_code, leave_fun);

    buf_in[0] = 0.0;
    for (i=1; i<M/2-1; i++)
        buf_in[i] *= -I/M;
    buf_in[M/2-1] = 0.0;
    for (i=M/2; i<M; i++)
        buf_in[i] *= I/M;

    ret_code = fft_wrapper_execute_plan(plan_inv, buf_in, buf_out);
    CHECK_RETCODE(ret_code, leave_fun);

    // Step 3: The spectral factor has the frequency response exp(x_l-j*y_l).
    // Apply the inverse FFT to these values and truncate.

    for (i=0; i<M; i++)
        buf_in[i] = CEXP(buf_x[i] - I*buf_out[i]) / M;

    ret_code = fft_wrapper_execute_plan(plan_inv, buf_in, buf_out);
    CHECK_RETCODE(ret_code, leave_fun);

    for (i=0; i<=deg; i++)
        result[i] = CONJ( buf_out[deg-i] );

leave_fun:
    fft_wrapper_free(buf_in);
    fft_wrapper_free(buf_out);
    fft_wrapper_free(buf_x);
    fft_wrapper_destroy_plan(&plan_fwd);
    fft_wrapper_destroy_plan(&plan_inv);
    return ret_code;
}
