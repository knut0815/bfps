/**********************************************************************
*                                                                     *
*  Copyright 2017 Max Planck Institute                                *
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



#ifndef CODE_BASE_HPP
#define CODE_BASE_HPP

#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>
#include "base.hpp"

/** \class code_base
 *  \brief Defines basic timer and method to check stopping condition.
 *
 *  Any computational task will consist of a loop over temporal snapshots, be it
 *  a simulation or a postprocessing job.
 *  This class declares the three required methods (initialize, step and finalize
 *  functionalities).
 *  Implementation should be done in children classes, since it will be different
 *  for simulations or postprocessing jobs.
 *
 *  What the class actually implements is a basic timer (calls to system clock),
 *  and a method to check for a stopping condition.
 *  These are meant to be used by children classes as needed.
 */

class code_base
{
    private:
        clock_t time0, time1;
    public:
        int myrank, nprocs;
        MPI_Comm comm;

        std::string simname;
        int iteration;

        bool stop_code_now;

        int nx;
        int ny;
        int nz;
        int dealias_type;
        double dkx;
        double dky;
        double dkz;

        code_base(
                const MPI_Comm COMMUNICATOR,
                const std::string &simulation_name);
        virtual ~code_base(){}

        int check_stopping_condition(void);

        int start_simple_timer(void)
        {
            this->time0 = clock();
            return EXIT_SUCCESS;
        }

        int print_simple_timer(
                const std::string operation_name)
        {
            this->time1 = clock();
            double local_time_difference = ((
                    (unsigned int)(this->time1 - this->time0)) /
                    ((double)CLOCKS_PER_SEC));
            double time_difference = 0.0;
            MPI_Allreduce(
                    &local_time_difference,
                    &time_difference,
                    1,
                    MPI_DOUBLE,
                    MPI_SUM,
                    MPI_COMM_WORLD);
            if (this->myrank == 0)
                std::cout << operation_name <<
                             " took " << time_difference/this->nprocs <<
                             " seconds" << std::endl;
            if (this->myrank == 0)
                std::cerr << operation_name <<
                             " took " << time_difference/this->nprocs <<
                             " seconds" << std::endl;
            this->time0 = this->time1;
            return EXIT_SUCCESS;
        }

        virtual int initialize(void) = 0;
        virtual int main_loop(void) = 0;
        virtual int finalize(void) = 0;
};

#endif//CODE_BASE_HPP

