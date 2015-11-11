/**********************************************************************
*                                                                     *
*  Copyright 2015 Max Planck Institute                                *
*                 for Dynamics and Self-Organization                  *
*                                                                     *
*  This file is part of bfps.                                         *
*                                                                     *
*  bfps is free software: you can redistribute it and/or modify       *
*  it under the terms of the GNU General Public License as published  *
*  by the Free Software Foundation, either version 3 of the License,  *
*  or (at your option) any later version.                             *
*                                                                     *
*  bfps is distributed in the hope that it will be useful,            *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of     *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the      *
*  GNU General Public License for more details.                       *
*                                                                     *
*  You should have received a copy of the GNU General Public License  *
*  along with bfps.  If not, see <http://www.gnu.org/licenses/>       *
*                                                                     *
* Contact: Cristian.Lalescu@ds.mpg.de                                 *
*                                                                     *
**********************************************************************/



#include "field_descriptor.hpp"
#include "fftw_tools.hpp"
#include "fluid_solver_base.hpp"
#include "spline_n1.hpp"
#include "spline_n2.hpp"
#include "spline_n3.hpp"
#include "spline_n4.hpp"
#include "spline_n5.hpp"
#include "spline_n6.hpp"
#include "Lagrange_polys.hpp"

#ifndef INTERPOLATOR

#define INTERPOLATOR

typedef void (*base_polynomial_values)(
        int derivative,
        double fraction,
        double *destination);

template <class rnumber, int interp_neighbours>
class interpolator
{
    public:
        ptrdiff_t buffer_size;
        base_polynomial_values compute_beta;
        field_descriptor<rnumber> *descriptor;
        field_descriptor<rnumber> *unbuffered_descriptor;
        rnumber *f0, *f1, *temp;

        interpolator(
                fluid_solver_base<rnumber> *FSOLVER,
                base_polynomial_values BETA_POLYS);
        ~interpolator();

        void operator()(double t, int *xg, double *xx, double *dest, int *deriv = NULL);
        /* destroys input */
        int read_rFFTW(void *src);
};

#endif//INTERPOLATOR
