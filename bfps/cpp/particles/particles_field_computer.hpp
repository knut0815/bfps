#ifndef PARTICLES_FIELD_COMPUTER_HPP
#define PARTICLES_FIELD_COMPUTER_HPP

#include <array>
#include <utility>

#include "abstract_particles_distr.hpp"
#include "scope_timer.hpp"

template <class interpolator_class, class field_class, int interp_neighbours, class positions_updater_class >
class particles_field_computer : public abstract_particles_distr<3,3,1> {
    const std::array<size_t,3> field_grid_dim;
    const std::pair<int,int> current_partition_interval;

    const interpolator_class& interpolator;
    const field_class& field;

    const positions_updater_class positions_updater;

    const std::array<double,3> spatial_box_width;
    const double box_step_width;
    const double my_spatial_low_limit;
    const double my_spatial_up_limit;

    int deriv[3];

    ////////////////////////////////////////////////////////////////////////
    /// Computation related
    ////////////////////////////////////////////////////////////////////////

    virtual void init_result_array(double particles_current_rhs[],
                                   const int nb_particles) final{
        // Set values to zero initialy
        std::fill(particles_current_rhs, particles_current_rhs+nb_particles*3, 0);
    }

    virtual void apply_computation(const double particles_positions[],
                                   double particles_current_rhs[],
                                   const int nb_particles) const final{
        TIMEZONE("particles_field_computer::apply_computation");
        for(int idxPart = 0 ; idxPart < nb_particles ; ++idxPart){
            double bx[interp_neighbours*2+2], by[interp_neighbours*2+2], bz[interp_neighbours*2+2];
            interpolator.compute_beta(deriv[0], particles_positions[idxPart*3], bx);
            interpolator.compute_beta(deriv[1], particles_positions[idxPart*3+1], by);
            interpolator.compute_beta(deriv[2], particles_positions[idxPart*3+2], bz);

            const int partGridIdx_x = int(particles_positions[idxPart*3]/box_step_width);
            const int partGridIdx_y = int(particles_positions[idxPart*3+1]/box_step_width);
            const int partGridIdx_z = int(particles_positions[idxPart*3+2]/box_step_width);

            assert(0 <= partGridIdx_z && partGridIdx_z < field_grid_dim[2]);
            assert(0 <= partGridIdx_x && partGridIdx_x < field_grid_dim[0]);
            assert(0 <= partGridIdx_y && partGridIdx_y < field_grid_dim[1]);

            const int interp_limit_mx = partGridIdx_x-interp_neighbours;
            const int interp_limit_x = partGridIdx_x+interp_neighbours+1;
            const int interp_limit_my = partGridIdx_y-interp_neighbours;
            const int interp_limit_y = partGridIdx_y+interp_neighbours+1;

            int interp_limit_mz[2];
            int interp_limit_z[2];
            int nb_z_intervals;

            if((partGridIdx_z-interp_neighbours) < 0){
                assert(partGridIdx_z+interp_neighbours+1 < field_grid_dim[2]);
                interp_limit_mz[0] = ((partGridIdx_z-interp_neighbours)+field_grid_dim[2])%field_grid_dim[2];
                interp_limit_z[0] = current_partition_interval.second-1;

                interp_limit_mz[1] = std::max(0, current_partition_interval.first);// max is not really needed here
                interp_limit_z[1] = std::min(partGridIdx_z+interp_neighbours+1, current_partition_interval.second-1);

                nb_z_intervals = 2;
            }
            else if(field_grid_dim[2] <= (partGridIdx_z+interp_neighbours+1)){
                interp_limit_mz[0] = std::max(current_partition_interval.first, partGridIdx_z-interp_neighbours);
                interp_limit_z[0] = std::min(int(field_grid_dim[2])-1,current_partition_interval.second-1);// max is not really needed here

                interp_limit_mz[1] = std::max(0, current_partition_interval.first);
                interp_limit_z[1] = std::min(int((partGridIdx_z+interp_neighbours+1+field_grid_dim[2])%field_grid_dim[2]), current_partition_interval.second-1);

                nb_z_intervals = 2;
            }
            else{
                interp_limit_mz[0] = std::max(partGridIdx_z-interp_neighbours, current_partition_interval.first);
                interp_limit_z[0] = std::min(partGridIdx_z+interp_neighbours+1, current_partition_interval.second-1);
                nb_z_intervals = 1;
            }

            for(int idx_inter = 0 ; idx_inter < nb_z_intervals ; ++idx_inter){
                for(int idx_z = interp_limit_mz[idx_inter] ; idx_z <= interp_limit_z[idx_inter] ; ++idx_z ){
                    const int idx_z_pbc = (idx_z + field_grid_dim[2])%field_grid_dim[2];
                    assert(current_partition_interval.first <= idx_z_pbc && idx_z_pbc < current_partition_interval.second);
                    assert(idx_z-interp_limit_mz[idx_inter] < interp_neighbours*2+2);

                    for(int idx_x = interp_limit_mx ; idx_x <= interp_limit_x ; ++idx_x ){
                        const int idx_x_pbc = (idx_x + field_grid_dim[0])%field_grid_dim[0];
                        assert(idx_x-interp_limit_mx < interp_neighbours*2+2);

                        for(int idx_y = interp_limit_my ; idx_y <= interp_limit_y ; ++idx_y ){
                            const int idx_y_pbc = (idx_y + field_grid_dim[1])%field_grid_dim[1];
                            assert(idx_y-interp_limit_my < interp_neighbours*2+2);

                            const double coef = (bz[idx_z-interp_limit_mz[idx_inter]]
                                    * by[idx_y-interp_limit_my]
                                    * bx[idx_x-interp_limit_mx]);

                            const ptrdiff_t tindex = field.getIndexFromGlobalPosition(idx_x_pbc, idx_y_pbc, idx_z_pbc);

                            particles_current_rhs[idxPart*3+0] += field.getValue(tindex,0)*coef;
                            particles_current_rhs[idxPart*3+1] += field.getValue(tindex,1)*coef;
                            particles_current_rhs[idxPart*3+2] += field.getValue(tindex,2)*coef;
                        }
                    }
                }
            }
        }
    }

    virtual void reduce_particles(const double /*particles_positions*/[],
                                  double particles_current_rhs[],
                                  const double extra_particles_current_rhs[],
                                  const int nb_particles) const final{
        TIMEZONE("particles_field_computer::reduce_particles");
        // Simply sum values
        for(int idxPart = 0 ; idxPart < nb_particles ; ++idxPart){
            particles_current_rhs[idxPart*3+0] += extra_particles_current_rhs[idxPart*3+0];
            particles_current_rhs[idxPart*3+1] += extra_particles_current_rhs[idxPart*3+1];
            particles_current_rhs[idxPart*3+2] += extra_particles_current_rhs[idxPart*3+2];
        }
    }


    ////////////////////////////////////////////////////////////////////////
    /// Re-distribution related
    ////////////////////////////////////////////////////////////////////////

    void apply_pbc_xy(double* inout_particles, const int size) const final {
        TIMEZONE("particles_field_computer::apply_pbc_xy");
        for(int idxPart = 0 ; idxPart < size ; ++idxPart){
            // Consider it will never move for more than one box repeatition
            for(int idxDim = 0 ; idxDim < 2 ; ++idxDim){
                if(inout_particles[idxPart*3+idxDim] < 0) inout_particles[idxPart*3+idxDim] += spatial_box_width[idxDim];
                else if(spatial_box_width[idxDim] <= inout_particles[idxPart*3+idxDim]) inout_particles[idxPart*3+idxDim] -= spatial_box_width[idxDim];
                assert(0 <= inout_particles[idxPart*3+idxDim] && inout_particles[idxPart*3+idxDim] < spatial_box_width[idxDim]);
            }
        }
    }

    void apply_pbc_z_new_particles(double* values, const int size) const final {
        TIMEZONE("particles_field_computer::apply_pbc_z_new_particles");
        if(my_rank == 0){
            const int idxDim = 2;
            for(int idxPart = 0 ; idxPart < size ; ++idxPart){
                assert(values[idxPart*3+idxDim] < my_spatial_up_limit || spatial_box_width[idxDim] <= values[idxPart*3+idxDim]);
                assert(my_spatial_low_limit <= values[idxPart*3+idxDim]);

                if(spatial_box_width[idxDim] <= values[idxPart*3+idxDim]) values[idxPart*3+idxDim] -= spatial_box_width[idxDim];

                assert(0 <= values[idxPart*3+idxDim] && values[idxPart*3+idxDim] < spatial_box_width[idxDim]);
                assert(my_spatial_low_limit <= values[idxPart*3+idxDim] && values[idxPart*3+idxDim] < my_spatial_up_limit);
            }
        }
        else if(my_rank == nb_processes - 1){
            const int idxDim = 2;
            for(int idxPart = 0 ; idxPart < size ; ++idxPart){
                assert(my_spatial_low_limit <= values[idxPart*3+idxDim] || values[idxPart*3+idxDim] < 0);
                assert(values[idxPart*3+idxDim] < spatial_box_width[idxDim]);

                if(values[idxPart*3+idxDim] < 0) values[idxPart*3+idxDim] += spatial_box_width[idxDim];

                assert(0 <= values[idxPart*3+idxDim] && values[idxPart*3+idxDim] < spatial_box_width[idxDim]);
                assert(my_spatial_low_limit <= values[idxPart*3+idxDim] && values[idxPart*3+idxDim] < my_spatial_up_limit);
            }
        }
        else{
            const int idxDim = 2;
            for(int idxPart = 0 ; idxPart < size ; ++idxPart){
                assert(my_spatial_low_limit <= values[idxPart*3+idxDim] && values[idxPart*3+idxDim] < my_spatial_up_limit);
            }
        }
    }

public:

    particles_field_computer(MPI_Comm in_current_com, const std::array<size_t,3>& in_field_grid_dim,
                             const std::pair<int,int>& in_current_partitions,
                             const interpolator_class& in_interpolator,
                             const field_class& in_field,
                             const std::array<double,3>& in_spatial_box_width,
                             const double in_box_step_width, const double in_my_spatial_low_limit,
                             const double in_my_spatial_up_limit)
        : abstract_particles_distr(in_current_com, in_current_partitions),
          field_grid_dim(in_field_grid_dim), current_partition_interval(in_current_partitions),
          interpolator(in_interpolator), field(in_field), positions_updater(),
          spatial_box_width(in_spatial_box_width), box_step_width(in_box_step_width),
          my_spatial_low_limit(in_my_spatial_low_limit), my_spatial_up_limit(in_my_spatial_up_limit){
        deriv[0] = 0;
        deriv[1] = 0;
        deriv[2] = 0;
    }

    ////////////////////////////////////////////////////////////////////////
    /// Update position
    ////////////////////////////////////////////////////////////////////////

    void move_particles(double particles_positions[],
                   const int nb_particles,
                   const std::unique_ptr<double[]> particles_current_rhs[],
                   const int nb_rhs, const double dt) const final{
        TIMEZONE("particles_field_computer::move_particles");
        positions_updater.move_particles(particles_positions, nb_particles,
                                         particles_current_rhs, nb_rhs, dt);
    }

};


#endif
