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



//#define NDEBUG

#include <cmath>
#include <cassert>
#include <cstring>
#include <string>
#include <sstream>

#include "base.hpp"
#include "distributed_particles.hpp"
#include "fftw_tools.hpp"


extern int myrank, nprocs;

template <int particle_type, class rnumber, int interp_neighbours>
distributed_particles<particle_type, rnumber, interp_neighbours>::distributed_particles(
        const char *NAME,
        interpolator<rnumber, interp_neighbours> *FIELD,
        const int NPARTICLES,
        const int TRAJ_SKIP,
        const int INTEGRATION_STEPS)
{
    switch(particle_type)
    {
        case VELOCITY_TRACER:
            this->ncomponents = 3;
            break;
        default:
            this->ncomponents = 3;
            break;
    }
    assert((INTEGRATION_STEPS <= 6) &&
           (INTEGRATION_STEPS >= 1));
    strncpy(this->name, NAME, 256);
    this->nparticles = NPARTICLES;
    this->vel = FIELD;
    this->integration_steps = INTEGRATION_STEPS;
    this->traj_skip = TRAJ_SKIP;
    this->myrank = this->vel->descriptor->myrank;
    this->nprocs = this->vel->descriptor->nprocs;
    this->comm = this->vel->descriptor->comm;
}

template <int particle_type, class rnumber, int interp_neighbours>
distributed_particles<particle_type, rnumber, interp_neighbours>::~distributed_particles()
{
}

template <int particle_type, class rnumber, int interp_neighbours>
void distributed_particles<particle_type, rnumber, interp_neighbours>::sample(
        interpolator<rnumber, interp_neighbours> *field,
        std::unordered_map<int, single_particle_state<POINT3D>> &y)
{
    int xg[3];
    double xx[3];
    double yy[3];
    y.clear();
    for (auto &pp: this->state)
    {
        field->get_grid_coordinates(pp.second.data, xg, xx);
        (*field)(xg, xx, yy);
        y[pp.first] = yy;
    }
}

template <int particle_type, class rnumber, int interp_neighbours>
void distributed_particles<particle_type, rnumber, interp_neighbours>::get_rhs(
        const std::unordered_map<int, single_particle_state<particle_type>> &x,
        std::unordered_map<int, single_particle_state<particle_type>> &y)
{
    int xg[3];
    double xx[3];
    double yy[this->ncomponents];
    y.clear();
    for (auto &pp: x)
    {
        this->vel->get_grid_coordinates(pp.second.data, xg, xx);
        (*this->vel)(xg, xx, yy);
        y[pp.first] = yy;
    }
}

template <int particle_type, class rnumber, int interp_neighbours>
void distributed_particles<particle_type, rnumber, interp_neighbours>::sample(
        interpolator<rnumber, interp_neighbours> *field,
        const hid_t data_file_id,
        const char *dset_name)
{
    std::unordered_map<int, single_particle_state<POINT3D>> y;
    this->sample(field, y);
    this->write(data_file_id, dset_name, y);
}

template <int particle_type, class rnumber, int interp_neighbours>
void distributed_particles<particle_type, rnumber, interp_neighbours>::roll_rhs()
{
    for (int i=this->integration_steps-2; i>=0; i--)
        rhs[i+1] = rhs[i];
}



template <int particle_type, class rnumber, int interp_neighbours>
void distributed_particles<particle_type, rnumber, interp_neighbours>::AdamsBashforth(
        const int nsteps)
{
    this->get_rhs(this->state, this->rhs[0]);
    for (auto &pp: this->state)
        for (int i=0; i<this->ncomponents; i++)
            switch(nsteps)
            {
                case 1:
                    pp.second[i] += this->dt*this->rhs[0][pp.first][i];
                    break;
                case 2:
                    pp.second[i] += this->dt*(3*this->rhs[0][pp.first][i]
                                            -   this->rhs[1][pp.first][i])/2;
                    break;
                case 3:
                    pp.second[i] += this->dt*(23*this->rhs[0][pp.first][i]
                                            - 16*this->rhs[1][pp.first][i]
                                            +  5*this->rhs[2][pp.first][i])/12;
                    break;
                case 4:
                    pp.second[i] += this->dt*(55*this->rhs[0][pp.first][i]
                                            - 59*this->rhs[1][pp.first][i]
                                            + 37*this->rhs[2][pp.first][i]
                                            -  9*this->rhs[1][pp.first][i])/24;
                    break;
                case 5:
                    pp.second[i] += this->dt*(1901*this->rhs[0][pp.first][i]
                                            - 2774*this->rhs[1][pp.first][i]
                                            + 2616*this->rhs[2][pp.first][i]
                                            - 1274*this->rhs[3][pp.first][i]
                                            +  251*this->rhs[4][pp.first][i])/720;
                    break;
                case 6:
                    pp.second[i] += this->dt*(4277*this->rhs[0][pp.first][i]
                                            - 7923*this->rhs[1][pp.first][i]
                                            + 9982*this->rhs[2][pp.first][i]
                                            - 7298*this->rhs[3][pp.first][i]
                                            + 2877*this->rhs[4][pp.first][i]
                                            -  475*this->rhs[5][pp.first][i])/1440;
                    break;
    }
    this->roll_rhs();
}


template <int particle_type, class rnumber, int interp_neighbours>
void distributed_particles<particle_type, rnumber, interp_neighbours>::step()
{
    this->AdamsBashforth((this->iteration < this->integration_steps) ?
                            this->iteration+1 :
                            this->integration_steps);
    this->iteration++;
}


template <int particle_type, class rnumber, int interp_neighbours>
void distributed_particles<particle_type, rnumber, interp_neighbours>::read(
        const hid_t data_file_id)
{
    double *temp = new double[this->nparticles*this->ncomponents];
    if (this->myrank == 0)
    {
        std::string temp_string = (std::string("/") +
                                   std::string(this->name) +
                                   std::string("/state"));
        hid_t dset = H5Dopen(data_file_id, temp_string.c_str(), H5P_DEFAULT);
        hid_t mspace, rspace;
        hsize_t count[4], offset[4];
        rspace = H5Dget_space(dset);
        H5Sget_simple_extent_dims(rspace, count, NULL);
        count[0] = 1;
        offset[0] = this->iteration / this->traj_skip;
        offset[1] = 0;
        offset[2] = 0;
        mspace = H5Screate_simple(3, count, NULL);
        H5Sselect_hyperslab(rspace, H5S_SELECT_SET, offset, NULL, count, NULL);
        H5Dread(dset, H5T_NATIVE_DOUBLE, mspace, rspace, H5P_DEFAULT, temp);
        H5Sclose(mspace);
        H5Sclose(rspace);
        H5Dclose(dset);
    }
    MPI_Bcast(
            temp,
            this->nparticles*this->ncomponents,
            MPI_DOUBLE,
            0,
            this->comm);
    for (int p=0; p<this->nparticles; p++)
    {
        if (this->vel->z_is_here(temp[this->ncomponents*p+2]))
            this->state[p] = temp + this->ncomponents*p;
    }
    DEBUG_MSG("size of %s->state is %ld\n", this->name, this->state.size());
    //if (this->myrank == 0)
    //{
    //    if (this->iteration > 0)
    //    {
    //        temp_string = (std::string("/") +
    //                       std::string(this->name) +
    //                       std::string("/rhs"));
    //        dset = H5Dopen(data_file_id, temp_string.c_str(), H5P_DEFAULT);
    //        rspace = H5Dget_space(dset);
    //        H5Sget_simple_extent_dims(rspace, count, NULL);
    //        //reading from last available position
    //        offset[0] = count[0] - 1;
    //        offset[3] = 0;
    //        count[0] = 1;
    //        count[1] = 1;
    //        mspace = H5Screate_simple(4, count, NULL);
    //        for (int i=0; i<this->integration_steps; i++)
    //        {
    //            offset[1] = i;
    //            H5Sselect_hyperslab(rspace, H5S_SELECT_SET, offset, NULL, count, NULL);
    //            H5Dread(dset, H5T_NATIVE_DOUBLE, mspace, rspace, H5P_DEFAULT, this->rhs[i]);
    //        }
    //        H5Sclose(mspace);
    //        H5Sclose(rspace);
    //        H5Dclose(dset);
    //    }
    //}
    //MPI_Bcast(
    //        temp,
    //        this->array_size,
    //        MPI_DOUBLE,
    //        0,
    //        this->comm);
    //for (int i = 0; i<this->integration_steps; i++)
    //    MPI_Bcast(
    //            this->rhs[i],
    //            this->array_size,
    //            MPI_DOUBLE,
    //            0,
    //            this->comm);
    delete[] temp;
}

template <int particle_type, class rnumber, int interp_neighbours>
void distributed_particles<particle_type, rnumber, interp_neighbours>::write(
        const hid_t data_file_id,
        const char *dset_name,
        std::unordered_map<int, single_particle_state<POINT3D>> &y)
{
    double *data = new double[this->nparticles*3];
    double *yy = new double[this->nparticles*3];
    std::fill_n(yy, this->nparticles*3, 0);
    for (int p=0; p<this->nparticles; p++)
    {
        auto pp = y.find(p);
        if (pp != y.end())
            std::copy(pp->second.data,
                      pp->second.data + 3,
                      yy + pp->first*3);
    }
    MPI_Allreduce(
            yy,
            data,
            3*this->nparticles,
            MPI_DOUBLE,
            MPI_SUM,
            this->comm);
    delete[] yy;
    if (this->myrank == 0)
    {
        std::string temp_string = (std::string(this->name) +
                                   std::string("/") +
                                   std::string(dset_name));
        hid_t dset = H5Dopen(data_file_id, temp_string.c_str(), H5P_DEFAULT);
        hid_t mspace, wspace;
        hsize_t count[3], offset[3];
        wspace = H5Dget_space(dset);
        H5Sget_simple_extent_dims(wspace, count, NULL);
        assert(count[2] == 3);
        count[0] = 1;
        offset[0] = this->iteration / this->traj_skip;
        offset[1] = 0;
        offset[2] = 0;
        mspace = H5Screate_simple(3, count, NULL);
        H5Sselect_hyperslab(wspace, H5S_SELECT_SET, offset, NULL, count, NULL);
        H5Dwrite(dset, H5T_NATIVE_DOUBLE, mspace, wspace, H5P_DEFAULT, data);
        H5Sclose(mspace);
        H5Sclose(wspace);
        H5Dclose(dset);
    }
    delete[] data;
}

template <int particle_type, class rnumber, int interp_neighbours>
void distributed_particles<particle_type, rnumber, interp_neighbours>::write(
        const hid_t data_file_id,
        const char *dset_name,
        std::unordered_map<int, single_particle_state<particle_type>> &y)
{
    double *data = new double[this->nparticles*this->ncomponents];
    double *yy = new double[this->nparticles*this->ncomponents];
    std::fill_n(yy, this->nparticles*this->ncomponents, 0);
    for (int p=0; p<this->nparticles; p++)
    {
        auto pp = y.find(p);
        if (pp != y.end())
            std::copy(pp->second.data,
                      pp->second.data + this->ncomponents,
                      yy + pp->first*this->ncomponents);
    }
    MPI_Allreduce(
            yy,
            data,
            this->ncomponents*nparticles,
            MPI_DOUBLE,
            MPI_SUM,
            this->comm);
    delete[] yy;
    if (this->myrank == 0)
    {
        std::string temp_string = (std::string(this->name) +
                                   std::string("/") +
                                   std::string(dset_name));
        hid_t dset = H5Dopen(data_file_id, temp_string.c_str(), H5P_DEFAULT);
        hid_t mspace, wspace;
        hsize_t count[3], offset[3];
        wspace = H5Dget_space(dset);
        H5Sget_simple_extent_dims(wspace, count, NULL);
        count[0] = 1;
        offset[0] = this->iteration / this->traj_skip;
        offset[1] = 0;
        offset[2] = 0;
        mspace = H5Screate_simple(3, count, NULL);
        H5Sselect_hyperslab(wspace, H5S_SELECT_SET, offset, NULL, count, NULL);
        H5Dwrite(dset, H5T_NATIVE_DOUBLE, mspace, wspace, H5P_DEFAULT, data);
        H5Sclose(mspace);
        H5Sclose(wspace);
        H5Dclose(dset);
    }
    delete[] data;
}

template <int particle_type, class rnumber, int interp_neighbours>
void distributed_particles<particle_type, rnumber, interp_neighbours>::write(
        const hid_t data_file_id,
        const bool write_rhs)
{
    this->write(data_file_id, "state", this->state);
    //if (this->myrank == 0)
    //{
    //    if (write_rhs)
    //    {
    //        std::string temp_string = (
    //                std::string("/") +
    //                std::string(this->name) +
    //                std::string("/rhs"));
    //        hid_t dset = H5Dopen(data_file_id, temp_string.c_str(), H5P_DEFAULT);
    //        hid_t wspace = H5Dget_space(dset);
    //        hsize_t count[4], offset[4];
    //        H5Sget_simple_extent_dims(wspace, count, NULL);
    //        //writing to last available position
    //        offset[0] = count[0] - 1;
    //        offset[1] = 0;
    //        offset[2] = 0;
    //        offset[3] = 0;
    //        count[0] = 1;
    //        count[1] = 1;
    //        hid_t mspace = H5Screate_simple(4, count, NULL);
    //        for (int i=0; i<this->integration_steps; i++)
    //        {
    //            offset[1] = i;
    //            H5Sselect_hyperslab(wspace, H5S_SELECT_SET, offset, NULL, count, NULL);
    //            H5Dwrite(dset, H5T_NATIVE_DOUBLE, mspace, wspace, H5P_DEFAULT, this->rhs[i]);
    //        }
    //        H5Sclose(mspace);
    //        H5Sclose(wspace);
    //        H5Dclose(dset);
    //    }
    //}
}


/*****************************************************************************/
template class distributed_particles<VELOCITY_TRACER, float, 1>;
template class distributed_particles<VELOCITY_TRACER, float, 2>;
template class distributed_particles<VELOCITY_TRACER, float, 3>;
template class distributed_particles<VELOCITY_TRACER, float, 4>;
template class distributed_particles<VELOCITY_TRACER, float, 5>;
template class distributed_particles<VELOCITY_TRACER, float, 6>;
template class distributed_particles<VELOCITY_TRACER, double, 1>;
template class distributed_particles<VELOCITY_TRACER, double, 2>;
template class distributed_particles<VELOCITY_TRACER, double, 3>;
template class distributed_particles<VELOCITY_TRACER, double, 4>;
template class distributed_particles<VELOCITY_TRACER, double, 5>;
template class distributed_particles<VELOCITY_TRACER, double, 6>;
/*****************************************************************************/
