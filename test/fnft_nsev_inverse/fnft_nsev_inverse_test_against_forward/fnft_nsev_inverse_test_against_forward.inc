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

#include "fnft_nsev_inverse.h"
#include "fnft_nsev.h"
#include "fnft__nse_fscatter.h"
#include "fnft__poly_chirpz.h"
#include "fnft__errwarn.h"
#include "fnft__nse_discretization.h"
#include "fnft__misc.h"
#include <stdio.h>

static INT fnft_nsev_inverse_test(const UINT M, const REAL error_bound,
    const INT kappa, fnft_nsev_inverse_opts_t * opts_ptr)
{
    const UINT D = 8;
    COMPLEX q_exact[8] = { 0.1, 0.1*I, 0.2, -0.2, 0.0, 0.05+0.05*I, -0.03*I,
        0.06 };
    COMPLEX q[8];
    REAL T[2] = { 0.0, D-1.0 };
    REAL XI[2];

    if (M < D)
        return E_INVALID_ARGUMENT(M);
    if (!(error_bound > 0))
        return E_INVALID_ARGUMENT(error_bound);

    INT ret_code = SUCCESS;

    const UINT numel = nse_fscatter_numel(D, opts_ptr->discretization);
    COMPLEX * const transfer_matrix = malloc(numel * sizeof(COMPLEX));
    COMPLEX * const a_vals = malloc(M * sizeof(COMPLEX));
    COMPLEX * const b_vals = malloc(M * sizeof(COMPLEX));
    COMPLEX * const contspec = malloc(M * sizeof(COMPLEX));
    if (transfer_matrix == NULL || a_vals == NULL || b_vals == NULL
    || contspec == NULL) {
        ret_code = E_NOMEM;
        goto leave_fun;
    }

    fnft_nsev_opts_t opts_fwd = fnft_nsev_default_opts();
    opts_fwd.discretization = opts_ptr->discretization;
    ret_code = fnft_nsev_inverse_XI(D, T, M, XI, opts_ptr->discretization);
    CHECK_RETCODE(ret_code, leave_fun);
    ret_code = fnft_nsev(D, q_exact, T, M, contspec, XI, NULL, NULL, NULL,
                         kappa, &opts_fwd);

    ret_code = fnft_nsev_inverse(M, contspec, XI, 0, NULL, NULL, D, q, T,
        kappa, opts_ptr);
    CHECK_RETCODE(ret_code, leave_fun);

    REAL error = misc_rel_err(D, q, q_exact);
#ifdef DEBUG
    printf("error = %g\n", error);
#endif
    if (!(error < error_bound)) {
        ret_code = E_TEST_FAILED;
        goto leave_fun;
    }

leave_fun:
    free(transfer_matrix);
    free(a_vals);
    free(b_vals);
    free(contspec);
    return ret_code;
}
