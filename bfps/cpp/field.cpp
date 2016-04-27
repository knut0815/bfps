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


#include <cstdlib>
#include <algorithm>
#include <cassert>
#include "field.hpp"

template <field_components fc>
field_layout<fc>::field_layout(
        hsize_t *SIZES,
        hsize_t *SUBSIZES,
        hsize_t *STARTS,
        MPI_Comm COMM_TO_USE)
{
    this->comm = COMM_TO_USE;
    MPI_Comm_rank(this->comm, &this->myrank);
    MPI_Comm_size(this->comm, &this->nprocs);

    std::copy(SIZES, SIZES + 3, this->sizes);
    std::copy(SUBSIZES, SUBSIZES + 3, this->subsizes);
    std::copy(STARTS, STARTS + 3, this->starts);
    if (fc == THREE || fc == THREExTHREE)
    {
        this->sizes[3] = 3;
        this->subsizes[3] = 3;
        this->starts[3] = 0;
    }
    if (fc == THREExTHREE)
    {
        this->sizes[4] = 3;
        this->subsizes[4] = 3;
        this->starts[4] = 0;
    }
    this->local_size = 1;
    this->full_size = 1;
    for (int i=0; i<ndim(fc); i++)
    {
        this->local_size *= this->subsizes[i];
        this->full_size *= this->sizes[i];
    }

    /*field will at most be distributed in 2D*/
    this->rank.resize(2);
    this->all_start.resize(2);
    this->all_size.resize(2);
    for (int i=0; i<2; i++)
    {
        this->rank[i].resize(this->sizes[i]);
        std::vector<int> local_rank;
        local_rank.resize(this->sizes[i], 0);
        for (int ii=this->starts[i]; ii<this->starts[i]+this->subsizes[i]; ii++)
            local_rank[ii] = this->myrank;
        MPI_Allreduce(
                &local_rank.front(),
                &this->rank[i].front(),
                this->sizes[i],
                MPI_INT,
                MPI_SUM,
                this->comm);
        this->all_start[i].resize(this->nprocs);
        std::vector<int> local_start;
        local_start.resize(this->nprocs, 0);
        local_start[this->myrank] = this->starts[i];
        MPI_Allreduce(
                &local_start.front(),
                &this->all_start[i].front(),
                this->nprocs,
                MPI_INT,
                MPI_SUM,
                this->comm);
        this->all_size[i].resize(this->nprocs);
        std::vector<int> local_subsize;
        local_subsize.resize(this->nprocs, 0);
        local_subsize[this->myrank] = this->subsizes[i];
        MPI_Allreduce(
                &local_subsize.front(),
                &this->all_size[i].front(),
                this->nprocs,
                MPI_INT,
                MPI_SUM,
                this->comm);
    }
}

template <class rnumber,
          field_backend be,
          field_components fc>
field<rnumber, be, fc>::field(
                const int nx,
                const int ny,
                const int nz,
                const MPI_Comm COMM_TO_USE,
                const unsigned FFTW_PLAN_RIGOR)
{
    this->comm = COMM_TO_USE;
    MPI_Comm_rank(this->comm, &this->myrank);
    MPI_Comm_size(this->comm, &this->nprocs);

    this->fftw_plan_rigor = FFTW_PLAN_RIGOR;
    this->real_space_representation = true;

    /* generate HDF5 data types */
    if (typeid(rnumber) == typeid(float))
        this->rnumber_H5T = H5Tcopy(H5T_NATIVE_FLOAT);
    else if (typeid(rnumber) == typeid(double))
        this->rnumber_H5T = H5Tcopy(H5T_NATIVE_DOUBLE);
    typedef struct {
        rnumber re;   /*real part*/
        rnumber im;   /*imaginary part*/
    } tmp_complex_type;
    this->cnumber_H5T = H5Tcreate(H5T_COMPOUND, sizeof(tmp_complex_type));
    H5Tinsert(this->cnumber_H5T, "r", HOFFSET(tmp_complex_type, re), this->rnumber_H5T);
    H5Tinsert(this->cnumber_H5T, "i", HOFFSET(tmp_complex_type, im), this->rnumber_H5T);

    /* switch on backend */
    switch(be)
    {
        case FFTW:
            ptrdiff_t nfftw[3];
            nfftw[0] = nz;
            nfftw[1] = ny;
            nfftw[2] = nx;
            //ptrdiff_t tmp_local_size;
            ptrdiff_t local_n0, local_0_start;
            ptrdiff_t local_n1, local_1_start;
            //tmp_local_size = fftw_mpi_local_size_many_transposed(
            fftw_mpi_local_size_many_transposed(
                    3, nfftw, ncomp(fc),
                    FFTW_MPI_DEFAULT_BLOCK, FFTW_MPI_DEFAULT_BLOCK, this->comm,
                    &local_n0, &local_0_start,
                    &local_n1, &local_1_start);
            hsize_t sizes[3], subsizes[3], starts[3];
            sizes[0] = nz; sizes[1] = ny; sizes[2] = nx;
            subsizes[0] = local_n0; subsizes[1] = ny; subsizes[2] = nx;
            starts[0] = local_0_start; starts[1] = 0; starts[2] = 0;
            this->rlayout = new field_layout<fc>(
                    sizes, subsizes, starts, this->comm);
            sizes[0] = nz; sizes[1] = ny; sizes[2] = nx+2;
            subsizes[0] = local_n0; subsizes[1] = ny; subsizes[2] = nx+2;
            starts[0] = local_0_start; starts[1] = 0; starts[2] = 0;
            this->rmemlayout = new field_layout<fc>(
                    sizes, subsizes, starts, this->comm);
            sizes[0] = nz; sizes[1] = ny; sizes[2] = nx/2+1;
            subsizes[0] = local_n1; subsizes[1] = ny; subsizes[2] = nx/2+1;
            starts[0] = local_1_start; starts[1] = 0; starts[2] = 0;
            this->clayout = new field_layout<fc>(
                    sizes, subsizes, starts, this->comm);
            this->data = (rnumber*)fftw_malloc(
                    sizeof(rnumber)*this->rmemlayout->local_size);
            if(typeid(rnumber) == typeid(float))
            {
                this->c2r_plan = new fftwf_plan;
                this->r2c_plan = new fftwf_plan;
                *((fftwf_plan*)this->c2r_plan) = fftwf_mpi_plan_many_dft_c2r(
                        3, nfftw, ncomp(fc),
                        FFTW_MPI_DEFAULT_BLOCK, FFTW_MPI_DEFAULT_BLOCK,
                        (fftwf_complex*)this->data, (float*)this->data,
                        this->comm,
                        this->fftw_plan_rigor | FFTW_MPI_TRANSPOSED_IN);
                *((fftwf_plan*)this->r2c_plan) = fftwf_mpi_plan_many_dft_r2c(
                        3, nfftw, ncomp(fc),
                        FFTW_MPI_DEFAULT_BLOCK, FFTW_MPI_DEFAULT_BLOCK,
                        (float*)this->data, (fftwf_complex*)this->data,
                        this->comm,
                        this->fftw_plan_rigor | FFTW_MPI_TRANSPOSED_OUT);
            }
            if (typeid(rnumber) == typeid(double))
            {
                this->c2r_plan = new fftw_plan;
                this->r2c_plan = new fftw_plan;
                *((fftw_plan*)this->c2r_plan) = fftw_mpi_plan_many_dft_c2r(
                        3, nfftw, ncomp(fc),
                        FFTW_MPI_DEFAULT_BLOCK, FFTW_MPI_DEFAULT_BLOCK,
                        (fftw_complex*)this->data, (double*)this->data,
                        this->comm,
                        this->fftw_plan_rigor | FFTW_MPI_TRANSPOSED_IN);
                *((fftw_plan*)this->r2c_plan) = fftw_mpi_plan_many_dft_r2c(
                        3, nfftw, ncomp(fc),
                        FFTW_MPI_DEFAULT_BLOCK, FFTW_MPI_DEFAULT_BLOCK,
                        (double*)this->data, (fftw_complex*)this->data,
                        this->comm,
                        this->fftw_plan_rigor | FFTW_MPI_TRANSPOSED_OUT);
            }
            break;
    }
}

template <class rnumber,
          field_backend be,
          field_components fc>
field<rnumber, be, fc>::~field()
{
    /* close data types */
    H5Tclose(this->rnumber_H5T);
    H5Tclose(this->cnumber_H5T);
    switch(be)
    {
        case FFTW:
            delete this->rlayout;
            delete this->rmemlayout;
            delete this->clayout;
            fftw_free(this->data);
            if (typeid(rnumber) == typeid(float))
            {
                fftwf_destroy_plan(*(fftwf_plan*)this->c2r_plan);
                delete (fftwf_plan*)this->c2r_plan;
                fftwf_destroy_plan(*(fftwf_plan*)this->r2c_plan);
                delete (fftwf_plan*)this->r2c_plan;
            }
            else if (typeid(rnumber) == typeid(double))
            {
                fftw_destroy_plan(*(fftw_plan*)this->c2r_plan);
                delete (fftw_plan*)this->c2r_plan;
                fftw_destroy_plan(*(fftw_plan*)this->r2c_plan);
                delete (fftw_plan*)this->r2c_plan;
            }
            break;
    }
}

template <class rnumber,
          field_backend be,
          field_components fc>
void field<rnumber, be, fc>::ift()
{
    if (typeid(rnumber) == typeid(float))
        fftwf_execute(*((fftwf_plan*)this->c2r_plan));
    else if (typeid(rnumber) == typeid(double))
        fftw_execute(*((fftw_plan*)this->c2r_plan));
    this->real_space_representation = true;
}

template <class rnumber,
          field_backend be,
          field_components fc>
void field<rnumber, be, fc>::dft()
{
    if (typeid(rnumber) == typeid(float))
        fftwf_execute(*((fftwf_plan*)this->r2c_plan));
    else if (typeid(rnumber) == typeid(double))
        fftw_execute(*((fftw_plan*)this->r2c_plan));
    this->real_space_representation = false;
}

template <class rnumber,
          field_backend be,
          field_components fc>
int field<rnumber, be, fc>::io(
        const std::string fname,
        const std::string dset_name,
        const int iteration,
        const bool read)
{
    hid_t file_id, dset_id, plist_id;
    hid_t dset_type;
    bool io_for_real = false;

    /* open file */
    plist_id = H5Pcreate(H5P_FILE_ACCESS);
    H5Pset_fapl_mpio(plist_id, this->comm, MPI_INFO_NULL);
    if (read)
        file_id = H5Fopen(fname.c_str(), H5F_ACC_RDONLY, plist_id);
    else
        file_id = H5Fopen(fname.c_str(), H5F_ACC_RDWR, plist_id);
    H5Pclose(plist_id);

    /* open data set */
    dset_id = H5Dopen(file_id, dset_name.c_str(), H5P_DEFAULT);
    dset_type = H5Dget_type(dset_id);
    io_for_real = (
            H5Tequal(dset_type, H5T_IEEE_F32BE) ||
            H5Tequal(dset_type, H5T_IEEE_F32LE) ||
            H5Tequal(dset_type, H5T_INTEL_F32) ||
            H5Tequal(dset_type, H5T_NATIVE_FLOAT) ||
            H5Tequal(dset_type, H5T_IEEE_F64BE) ||
            H5Tequal(dset_type, H5T_IEEE_F64LE) ||
            H5Tequal(dset_type, H5T_INTEL_F64) ||
            H5Tequal(dset_type, H5T_NATIVE_DOUBLE));

    /* generic space initialization */
    hid_t fspace, mspace;
    fspace = H5Dget_space(dset_id);
    hsize_t count[ndim(fc)+1], offset[ndim(fc)+1], dims[ndim(fc)+1];
    hsize_t memoffset[ndim(fc)+1], memshape[ndim(fc)+1];
    H5Sget_simple_extent_dims(fspace, dims, NULL);
    if (io_for_real)
    {
        for (int i=0; i<ndim(fc); i++)
        {
            count[i+1] = this->rlayout->subsizes[i];
            offset[i+1] = this->rlayout->starts[i];
            assert(dims[i+1] == this->rlayout->sizes[i]);
            memshape[i+1] = this->rmemlayout->subsizes[i];
            memoffset[i+1] = 0;
        }
        count[0] = 1;
        offset[0] = iteration;
        memshape[0] = 1;
        memoffset[0] = 0;
        mspace = H5Screate_simple(ndim(fc)+1, memshape, NULL);
        H5Sselect_hyperslab(fspace, H5S_SELECT_SET, offset, NULL, count, NULL);
        H5Sselect_hyperslab(mspace, H5S_SELECT_SET, memoffset, NULL, count, NULL);
        if (read)
        {
            H5Dread(dset_id, this->rnumber_H5T, mspace, fspace, H5P_DEFAULT, this->data);
            this->real_space_representation = true;
        }
        else
        {
            H5Dwrite(dset_id, this->rnumber_H5T, mspace, fspace, H5P_DEFAULT, this->data);
            if (!this->real_space_representation)
                DEBUG_MSG("I just wrote complex field into real space dataset. It's probably nonsense.\n");
        }
        H5Sclose(mspace);
    }
    else
    {
        for (int i=0; i<ndim(fc); i++)
        {
            count[i+1] = this->clayout->subsizes[i];
            offset[i+1] = this->clayout->starts[i];
            assert(dims[i+1] == this->clayout->sizes[i]);
            memshape[i+1] = count[i+1];
            memoffset[i+1] = 0;
        }
        count[0] = 1;
        offset[0] = iteration;
        memshape[0] = 1;
        memoffset[0] = 0;
        mspace = H5Screate_simple(ndim(fc)+1, memshape, NULL);
        H5Sselect_hyperslab(fspace, H5S_SELECT_SET, offset, NULL, count, NULL);
        H5Sselect_hyperslab(mspace, H5S_SELECT_SET, memoffset, NULL, count, NULL);
        if (read)
        {
            H5Dread(dset_id, this->cnumber_H5T, mspace, fspace, H5P_DEFAULT, this->data);
            this->real_space_representation = false;
        }
        else
        {
            H5Dwrite(dset_id, this->cnumber_H5T, mspace, fspace, H5P_DEFAULT, this->data);
            if (this->real_space_representation)
                DEBUG_MSG("I just wrote real space field into complex dataset. It's probably nonsense.\n");
        }
        H5Sclose(mspace);
    }

    H5Tclose(dset_type);
    H5Sclose(fspace);
    /* close data set */
    H5Dclose(dset_id);
    /* close file */
    H5Fclose(file_id);
    return EXIT_SUCCESS;
}

template class field<float, FFTW, ONE>;
template class field<float, FFTW, THREE>;
template class field<float, FFTW, THREExTHREE>;
template class field<double, FFTW, ONE>;
template class field<double, FFTW, THREE>;
template class field<double, FFTW, THREExTHREE>;

