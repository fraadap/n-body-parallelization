/* Imports */
#include "omp_exhaustive.h"

/* Global variables */
Body* bodies;

/* Runs the simulation using the set simulation data and the given file pointer as output file */
void run_simulation(FILE* out_fp) {
    u32 i, j, k, interval;
    u32 local_n, remainder, my_rank, my_slice_size, my_slice_start, my_slice_end;
    double dist_ij, scalar;
    Vec3D vel_i, pos_i, pos_j;

    /* set the writing interval to have a maximum of 500 lines in the csv */
    if(num_steps <= 500)
        interval = 1;
    else
        interval = num_steps / 500;

    local_n = num_bodies / thread_count;
    remainder = num_bodies % thread_count;

    #pragma omp parallel num_threads(thread_count) \
        default(none) \
        shared(bodies, num_bodies, num_steps, out_fp, interval, local_n, remainder) \
        private(i, j, k, vel_i, pos_i, pos_j, dist_ij, scalar, \
            my_rank, my_slice_size, my_slice_start, my_slice_end) 
    {
        my_rank = omp_get_thread_num();

        my_slice_size = local_n;
        my_slice_start = my_rank * my_slice_size;

        if(my_rank < remainder){  // it then takes one of the remaining bodies
            my_slice_size++;
            my_slice_start += my_rank; // and sets appropriately its displacement
        }
        else
            my_slice_start += remainder; // just set the movement

        my_slice_end = my_slice_start + my_slice_size - 1;

        for (k = 0; k < num_steps; ++k){
            for (i = my_slice_start; i <= my_slice_end; ++i) {

                /* calculation of the new velocities of each body */
                vel_i = bodies[i].vel;
                pos_i = bodies[i].pos;

                for (j = 0; j < num_bodies; ++j) {
                    if (i == j) continue;

                    pos_j = bodies[j].pos;

                    dist_ij = vector_distance(&pos_i, &pos_j);
                    scalar = G * bodies[j].mass * (pow(dist_ij, -3.0)) * TIME_DELTA;

                    vel_i.x += scalar * (pos_j.x - pos_i.x);
                    vel_i.y += scalar * (pos_j.y - pos_i.y);
                    vel_i.z += scalar * (pos_j.z - pos_i.z);
                }

                bodies[i].vel = vel_i;
            }

            #pragma omp barrier
            
            /* calculation and updating of new positions */
            for (i = my_slice_start; i <= my_slice_end; ++i) {
                bodies[i].pos.x += bodies[i].vel.x * TIME_DELTA;
                bodies[i].pos.y += bodies[i].vel.y * TIME_DELTA;
                bodies[i].pos.z += bodies[i].vel.z * TIME_DELTA;
            }
            
            #pragma omp barrier 

            // Only the master thread writes on file
            if(k % interval == 0){
                #pragma omp single
                {
                    for (size_t i = 0; i < num_bodies; i++) {
                        fprintf(out_fp,"%lf,%lf,%lf,", bodies[i].pos.x, bodies[i].pos.y, bodies[i].pos.z);
                    }

                    fprintf(out_fp,"\n");
                }
            }
        }    
    }
}

int main(int argc, char* argv[]){
    const char* input_file;
    FILE *in_fp, *out_fp;

    /* Print correct usage */
    if (argc < 4){
        fprintf(stderr, "Correct usage: %s <input_file> <simulation_steps> <thread_count>\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* Get input values */
    input_file = argv[1];
    num_steps = strtol(argv[2], NULL, 10);
    thread_count = strtol(argv[3], NULL, 10);
    
    /* Read input data */
    in_fp = fopen(input_file, "r");

    if (!in_fp) {
        fprintf(stderr, "Cannot open %s: %s.\n", input_file, strerror(errno));
        return EXIT_FAILURE;
    }

    num_bodies = bodies_readall(in_fp, &bodies);
    
    if (fclose(in_fp)) {
        fprintf(stderr, "Error closing %s.\n", input_file);
        return EXIT_FAILURE;
    }

    if (errno) {
        fprintf(stderr, "Parsing error: %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }

    /* Open output file */
    out_fp = fopen(OUTPUT_FILE, "w");

    if (!out_fp) {
        fprintf(stderr, "Cannot open %s: %s.\n", OUTPUT_FILE, strerror(errno));
        return EXIT_FAILURE;
    }

    /* Run simulation */
    run_simulation(out_fp);

    if (fclose(out_fp)) {
        fprintf(stderr, "Error closing %s.\n", OUTPUT_FILE);
        return EXIT_FAILURE;
    }
    
    free(bodies);

    return EXIT_SUCCESS;
}
