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

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include "field.hpp"
#include "field_descriptor.hpp"

#ifndef VORTICITY_EQUATION

#define VORTICITY_EQUATION

extern int myrank, nprocs;


/* container for field descriptor, fields themselves, parameters, etc
 * This particular class is only meant as a stepping stone to a proper solver
 * that only uses the field class (and related layout and kspace classes), and
 * HDF5 for I/O.
 * */

template <typename rnumber,
          field_backend be>
class vorticity_equation
{
    public:
        /* name */
        char name[256];

        /* iteration */
        int iteration;
        int checkpoint;
        int checkpoints_per_file;

        /* fields */
        field<rnumber, be, THREE> *cvorticity, *cvelocity;
        field<rnumber, be, THREE> *rvorticity;
        kspace<be, SMOOTH> *kk;


        /* short names for velocity, and 4 vorticity fields */
        field<rnumber, be, THREE> *u, *v[4];

        /* physical parameters */
        double nu;
        int fmode;         // for Kolmogorov flow
        double famplitude; // both for Kflow and band forcing
        double fk0, fk1;   // for band forcing
        char forcing_type[128];

        /* constructor, destructor */
        vorticity_equation(
                const char *NAME,
                int nx,
                int ny,
                int nz,
                double DKX = 1.0,
                double DKY = 1.0,
                double DKZ = 1.0,
                unsigned FFTW_PLAN_RIGOR = FFTW_MEASURE);
        ~vorticity_equation(void);

        /* solver essential methods */
        void omega_nonlin(int src);
        void step(double dt);
        void impose_zero_modes(void);
        void add_forcing(field<rnumber, be, THREE> *dst,
                         field<rnumber, be, THREE> *src_vorticity,
                         rnumber factor);
        void compute_vorticity(void);
        void compute_velocity(field<rnumber, be, THREE> *vorticity);

        /* I/O stuff */
        inline std::string get_current_fname()
        {
            return (
                    std::string(this->name) +
                    std::string("_checkpoint_") +
                    std::to_string(this->checkpoint) +
                    std::string(".h5"));
        }
        void update_checkpoint(void);
        inline void io_checkpoint(bool read = true)
        {
            assert(!this->cvorticity->real_space_representation);
            if (!read)
                this->update_checkpoint();
            std::string fname = this->get_current_fname();
            this->cvorticity->io(
                    fname,
                    "vorticity",
                    this->iteration,
                    read);
            if (read)
            {
                #if (__GNUC__ <= 4 && __GNUC_MINOR__ < 7)
                    this->kk->low_pass<rnumber, THREE>(this->cvorticity->get_cdata(), this->kk->kM);
                    this->kk->force_divfree<rnumber>(this->cvorticity->get_cdata());
                #else
                    this->kk->template low_pass<rnumber, THREE>(this->cvorticity->get_cdata(), this->kk->kM);
                    this->kk->template force_divfree<rnumber>(this->cvorticity->get_cdata());
                #endif
            }
        }

        /* statistics and general postprocessing */
        void compute_pressure(field<rnumber, be, ONE> *pressure);
        void compute_Eulerian_acceleration(field<rnumber, be, THREE> *acceleration);
        void compute_Lagrangian_acceleration(field<rnumber, be, THREE> *acceleration);
};

#endif//VORTICITY_EQUATION

