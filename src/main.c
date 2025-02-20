#include "algorithm.h"
#include "common.h"
#include "display16x2.h"
#include "fiber_list.h"
#include "gpio.h"
#include "instruction.h"
#include "list_utils.h"
#include "mawa_control.h"
#include "piezos.h"
#include "prj_setup.h"
#include "resource.h"
#include "serial_control.h"
#include "usbutils.h"
#include <math.h>
#include <sched.h> /* To set the priority on linux */
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>

#define CONCAT(A, B) CONCAT_(A, B)
#define CONCAT_(DEVICE, FUNCTION) DEVICE##_##FUNCTION

#define adc_device_create CONCAT(adc_device, create)
#define dac_device_create CONCAT(dac_device, create)
#define dac_device_spi_write CONCAT(dac_device, spi_write)

#define NUM_OF_ADCS ((NUM_OF_FIBERS - 1) / ADC_CHANNELS + 1)
#define NUM_OF_DACS ((NUM_OF_FIBERS * 2 - 1) / DAC_CHANNELS + 1)

#define LOOP_FIBERS(index) for (int index = 0; index < NUM_OF_FIBERS; index++)
#define TO_DB(val) (logf((float)val) / 0.23258f - 36.0f)

#define DEFAULT_LOG_FILE "logs/mams.log"
#define ALGORITHM_DATA_FILE_PATH "logs/mams-algorithm.log"

#define EXIT_ON_ERROR(expr)                                                                        \
    if (expr != ERR_ALL_GOOD)                                                                      \
    {                                                                                              \
        LOG_ERROR("FATAL ERROR - this should not happen");                                         \
        exit(ERR_FATAL);                                                                           \
    }

Error set_program_priority(int priorityLevel)
{

    struct sched_param sched;
    memset(&sched, 0, sizeof(sched));
    int max_priority_level = sched_get_priority_max(SCHED_RR);
    if (priorityLevel > max_priority_level)
    {
        priorityLevel = max_priority_level;
    }
    sched.sched_priority = priorityLevel;

    if (sched_setscheduler(0, SCHED_RR, &sched))
    {
        return ERR_UNEXPECTED;
    }
    return ERR_ALL_GOOD;
}

#if TEST == 0
// Global variables
bool g_exit_alignment_loop = false;
bool g_exit_serial_loop    = false;
int g_serial_fd;
ssize_t g_serial_bytes_in;
char g_serial_bias_buff_out[COMMUNICATION_BUFF_OUT_SIZE]     = {0};
char g_serial_coupling_buff_out[COMMUNICATION_BUFF_OUT_SIZE] = {0};
char g_serial_generic_buff_out[COMMUNICATION_BUFF_OUT_SIZE]  = {0};
uint16_t g_new_coupling[NUM_OF_FIBERS];
uint16_t g_coupling[NUM_OF_FIBERS];
display16x2 g_display;
adc_device g_adc_list[NUM_OF_ADCS];
dac_device g_dac_list[NUM_OF_DACS];
FiberMotionState g_fiber_motion_state_list[NUM_OF_FIBERS];
FiberList* g_move_fiber_list_p = NULL;
FiberList g_read_fiber_list;
FiberList g_input_fiber_list;
FiberList g_output_fiber_list;
uint16_t g_min_step_size;
uint16_t g_hysteresis_step_size;
FILE* g_algorithm_data_file = NULL;

typedef enum
{
    LOOP_OWNER_MAWA,
    LOOP_OWNER_SERIAL,
} AlignmentLoopOwner;

int get_time_ms()
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == -1)
    {
        LOG_ERROR("Failed to start timer");
        return 0;
    }
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void send_data_to_serial_device_and_wait_for_ack()
{
    // TODO - don't wait if MAWA started the loop
    if (serial_control_wait_for_next() != ERR_ALL_GOOD)
    {
        g_exit_alignment_loop = true;
        return;
    }
    serial_control_set_response("coupling:%s\n", g_serial_coupling_buff_out);
    serial_control_send_response();
    memset(g_serial_coupling_buff_out, 0, COMMUNICATION_BUFF_OUT_SIZE);

    if (serial_control_wait_for_next() != ERR_ALL_GOOD)
    {
        g_exit_alignment_loop = true;
        return;
    }
    serial_control_set_response("bias:%s\n", g_serial_bias_buff_out);
    serial_control_send_response();
    memset(g_serial_bias_buff_out, 0, COMMUNICATION_BUFF_OUT_SIZE);
}

void append_data_to_file()
{
    LOG_INFO("Writing coupling `%s` to data file.", g_serial_coupling_buff_out);
    fprintf(g_algorithm_data_file, "coupling:%s\n", g_serial_coupling_buff_out);
    memset(g_serial_coupling_buff_out, 0, COMMUNICATION_BUFF_OUT_SIZE);
    LOG_INFO("Writing bias `%s` to data file.", g_serial_bias_buff_out);
    fprintf(g_algorithm_data_file, "bias:%s\n", g_serial_bias_buff_out);
    memset(g_serial_bias_buff_out, 0, COMMUNICATION_BUFF_OUT_SIZE);
}

void signal_handler(int signum)
{
    LOG_INFO("Process interrupted by signal `%d`.", signum);
    g_exit_alignment_loop = true;
    g_exit_serial_loop    = true;
}

Error _send_piezo_bias_to_dac(
    const uint8_t fiber_index,
    const uint16_t bias_left,
    const uint16_t bias_right)
{
    uint8_t dac_number;
    uint8_t dac_address;
    return_on_err(piezo_pairs_get_dac_left(fiber_index, &dac_number, &dac_address));
    dac_device_spi_write(&g_dac_list[dac_number], dac_address, bias_left);
    LOG_DEBUG(
        "[%d]: %d\t DAC number: %d - DAC address: %d",
        fiber_index,
        bias_left,
        dac_number,
        dac_address);
    return_on_err(piezo_pairs_get_dac_right(fiber_index, &dac_number, &dac_address));
    dac_device_spi_write(&g_dac_list[dac_number], dac_address, bias_right);
    LOG_DEBUG(
        "[%d]: %d\t DAC number: %d - DAC address: %d",
        fiber_index,
        bias_right,
        dac_number,
        dac_address);
    return ERR_ALL_GOOD;
}

Error move()
{
    uint8_t fiber_nr;
    g_serial_bias_buff_out[0] = 0;
    fiber_list_loop(*g_move_fiber_list_p, index)
    {
        return_on_err(fiber_list_get_element_at(*g_move_fiber_list_p, index, &fiber_nr));
        LOG_TRACE(
            "Move [%d]: %d %c",
            fiber_nr,
            g_fiber_motion_state_list[fiber_nr].move_cmd.steps,
            g_fiber_motion_state_list[fiber_nr].move_cmd.axis);
        int16_t delta_bias_left;
        int16_t delta_bias_right;
        algorithm_fiber_displacement_to_delta_bias(
            &g_fiber_motion_state_list[fiber_nr], &delta_bias_left, &delta_bias_right);

        uint16_t new_bias_left;
        uint16_t new_bias_right;
        // Change stored bias for current piezo pair.
        // [MIC-106] resolve over/underflow when trying to move out of range.
        if (piezo_pairs_increment_bias(
                fiber_nr,
                delta_bias_left,
                delta_bias_right,
                g_hysteresis_step_size,
                &new_bias_left,
                &new_bias_right)
            != ERR_ALL_GOOD)
        {
            LOG_WARNING("Fiber [%d] just hit the boundaries.", fiber_nr);
            algorithm_handle_boundary_hit(&g_fiber_motion_state_list[fiber_nr]);
        }
        // Send new values to DAC.
        return_on_err(_send_piezo_bias_to_dac(fiber_nr, new_bias_left, new_bias_right));
        LOG_TRACE("Fiber [%d] - %d,%d", fiber_nr, new_bias_left, new_bias_right);
        sprintf(
            g_serial_bias_buff_out + strlen(g_serial_bias_buff_out),
            "F%dL%dR%d",
            fiber_nr + 1,
            new_bias_left,
            new_bias_right);
    }

    usleep(SETTLING_TIME_MICRO_SEC);
    return ERR_ALL_GOOD;
}

void _discharge_hysteresis(void)
{
    if (g_hysteresis_step_size == 0)
    {
        LOG_INFO("Hysteresis step size set to 0 - do not discharge");
        return;
    }
    const int16_t delta_bias      = 64;
    uint16_t bias                 = HALF_BIAS;
    int16_t waveform_maxima_vec[] = {
        HALF_BIAS,
        HALF_BIAS / 4 * 3,
        HALF_BIAS / 4 * 2,
        HALF_BIAS / 4 * 1,
        0,
    };
    LOOP_FIBERS(fiber_nr)
    {
        LOG_INFO(
            "Discharging hysteresis for fiber `%d` with step size `%d`.", fiber_nr, delta_bias);
        for (size_t w = 0; w < sizeof(waveform_maxima_vec) / sizeof(int16_t); w++)
        {
            while (bias < HALF_BIAS + waveform_maxima_vec[w])
            {
                _send_piezo_bias_to_dac(fiber_nr, bias, bias);
                usleep(100);
                bias += delta_bias;
            }
            while (bias > HALF_BIAS - waveform_maxima_vec[w])
            {
                _send_piezo_bias_to_dac(fiber_nr, bias, bias);
                usleep(100);
                if (bias < delta_bias)
                {
                    // Make sure you don't try to go negative.
                    break;
                }
                bias -= delta_bias;
            }
        }
        _send_piezo_bias_to_dac(fiber_nr, HALF_BIAS, HALF_BIAS);
        piezo_pairs_set_bias(fiber_nr, HALF_BIAS, HALF_BIAS);
        LOG_INFO("Fiber `%d`, to half bias: `%d`.", fiber_nr, bias);
    }
}

Error read_coupling(
    const FiberList* read_fiber_list_p,
    const uint16_t num_of_samples,
    uint16_t out_coupling[])
{
    uint8_t fiber_nr;
    uint8_t adc_number;
    uint8_t adc_address;
    uint16_t readings[NUM_OF_ADCS][ADC_CHANNELS];
    double average[NUM_OF_FIBERS] = {0};
    g_serial_coupling_buff_out[0] = 0;
    for (uint16_t sample = 0; sample < num_of_samples; sample++)
    {

        for (adc_number = 0; adc_number < NUM_OF_ADCS; adc_number++)
        {
            max1229_spi_read_first_n_channels(
                &g_adc_list[adc_number], ADC_CHANNELS, readings[adc_number]);
        }
        fiber_list_loop(*read_fiber_list_p, index)
        {
            return_on_err(fiber_list_get_element_at(*read_fiber_list_p, index, &fiber_nr));
            adc_number             = fiber_nr / ADC_CHANNELS;
            adc_address            = fiber_nr % ADC_CHANNELS;
            out_coupling[fiber_nr] = readings[adc_number][adc_address];
            average[fiber_nr] += (double)out_coupling[fiber_nr] / (double)num_of_samples;

            if (sample == num_of_samples - 1)
            {
                out_coupling[fiber_nr] = (uint16_t)average[fiber_nr];
                LOG_TRACE("ADC number `%u` - address `%u`", adc_number, adc_address);
                LOG_DEBUG(
                    "Coupling for [%d]: %f dB (%4d)",
                    fiber_nr,
                    TO_DB(out_coupling[fiber_nr]),
                    out_coupling[fiber_nr]);
                // Populating serial buffer out with the collected data.
                sprintf(
                    g_serial_coupling_buff_out + strlen(g_serial_coupling_buff_out),
                    "F%dC%d",
                    fiber_nr + 1,
                    out_coupling[fiber_nr]);
            }
            else
            {
                LOG_TRACE("Coupling for [%d]: %d", fiber_nr, out_coupling[fiber_nr]);
            }
        }
    }
    return ERR_ALL_GOOD;
}

Error read_averaged_coupling(
    const uint8_t one_based_fiber_nr,
    const uint16_t num_of_samples,
    uint16_t* out_min_coupling_p,
    uint16_t* out_max_coupling_p,
    uint16_t* out_ave_coupling_p)
{
    if (one_based_fiber_nr < 1 || one_based_fiber_nr > NUM_OF_FIBERS)
    {
        LOG_ERROR("Fiber number `%d` out of range [1, %d]", one_based_fiber_nr, NUM_OF_FIBERS);
        return ERR_OUT_OF_RANGE;
    }
    uint8_t fiber_nr          = one_based_fiber_nr - 1;
    *out_min_coupling_p       = 0;
    *out_max_coupling_p       = 0;
    *out_ave_coupling_p       = 0;
    uint16_t tmp_max_coupling = 0;
    uint16_t tmp_min_coupling = -1;
    double tmp_ave_coupling   = 0.0f;
    uint16_t tmp_coupling     = 0;
    for (uint16_t sample = 0; sample < num_of_samples; sample++)
    {
        uint16_t readings[NUM_OF_ADCS][ADC_CHANNELS];
        uint8_t adc_number  = fiber_nr / ADC_CHANNELS;
        uint8_t adc_address = fiber_nr % ADC_CHANNELS;
        for (adc_number = 0; adc_number < NUM_OF_ADCS; adc_number++)
        {
            max1229_spi_read_first_n_channels(
                &g_adc_list[adc_number], ADC_CHANNELS, readings[adc_number]);
        }
        tmp_coupling = readings[adc_number][adc_address];
        LOG_TRACE("Measured coupling: %d", tmp_coupling);
        if (tmp_coupling > tmp_max_coupling)
        {
            tmp_max_coupling = tmp_coupling;
        }
        if (tmp_coupling < tmp_min_coupling)
        {
            tmp_min_coupling = tmp_coupling;
        }
        tmp_ave_coupling += (double)tmp_coupling / (double)num_of_samples;
    }
    *out_min_coupling_p = tmp_min_coupling;
    *out_max_coupling_p = tmp_max_coupling;
    *out_ave_coupling_p = (uint16_t)tmp_ave_coupling;
    return ERR_ALL_GOOD;
}

Error alignment_run(const int num_of_samples, const AlignmentLoopOwner loop_owner)
{
    switch (loop_owner)
    {
    case LOOP_OWNER_SERIAL:
        LOG_INFO("Starting alignment loop launched via SERIAL");
        break;
    case LOOP_OWNER_MAWA:
        LOG_INFO("Starting alignment loop launched via MAWA");
        break;
    }
    const uint16_t max_step_size = 1 << MAX_STEP_BITS;
    LOG_INFO("Number of fibers    : `%d`.", NUM_OF_FIBERS);
    LOG_INFO("Number of ADC's     : `%d`.", NUM_OF_ADCS);
    LOG_INFO("Number of DAC's     : `%d`.", NUM_OF_DACS);
    LOG_INFO("Max step size       : `%d`.", max_step_size);
    LOG_INFO("Min step size       : `%d`.", g_min_step_size);
    LOG_INFO("Hysteresis step size: `%d`.", g_hysteresis_step_size);
    LOG_INFO("Settling time       : `%d us`.", SETTLING_TIME_MICRO_SEC);

    bool continuous_alignment_started = false;
    uint32_t iteration                = 0;
    bool new_max_found                = false;
    bool global_max_found             = false;
    {
        LOG_INFO("Trying to initialize algorithm");
        g_exit_alignment_loop     = false;
        g_exit_serial_loop        = false;
        g_serial_bias_buff_out[0] = 0;
        fiber_list_initialize(&g_read_fiber_list, 0);
        fiber_list_initialize(&g_input_fiber_list, 0);
        fiber_list_initialize(&g_output_fiber_list, 0);
        load_mapping();

        return_on_err(list_utils_make_io_lists(
            get_mapping(), NUM_OF_FIBERS, &g_input_fiber_list, &g_output_fiber_list));

        fiber_list_loop(g_input_fiber_list, index)
        {
            uint8_t value;
            return_on_err(fiber_list_get_element_at(g_input_fiber_list, index, &value));
            LOG_INFO("Input list `%d`:\t%d", index, value);
        }
        fiber_list_loop(g_output_fiber_list, index)
        {
            uint8_t value;
            return_on_err(fiber_list_get_element_at(g_output_fiber_list, index, &value));
            LOG_INFO("Output list `%d`:\t%d", index, value);
        }

        LOG_INFO("Algorithm initialized successfully.");
    }
    {
        LOG_INFO("Resetting piezos");
        LOOP_FIBERS(fiber_index)
        {
            // Set DAC values.
            return_on_err(_send_piezo_bias_to_dac(fiber_index, HALF_BIAS, HALF_BIAS));
            // Store piezo values.
            return_on_err(piezo_pairs_set_bias(fiber_index, HALF_BIAS, HALF_BIAS));
            sprintf(
                g_serial_bias_buff_out + strlen(g_serial_bias_buff_out),
                "F%dL%dR%d",
                fiber_index + 1,
                HALF_BIAS,
                HALF_BIAS);
        }
        usleep(SETTLING_TIME_MICRO_SEC);
    }
    {
        LOG_INFO("Trying to initialize fiber lists.");
        // Get initially measured coupling from all the outputs. Since the move list is not
        // ready, pass the output list as the second argument to avoid failure.
        return_on_err(read_coupling(&g_output_fiber_list, NUM_OF_SAMPLES, g_coupling));
        // Create move list based on measured coupling.
        return_on_err(list_utils_make_read_and_move_lists(
            &g_input_fiber_list,
            &g_output_fiber_list,
            get_mapping(),
            g_coupling,
            NUM_OF_FIBERS,
            &g_read_fiber_list,
            &g_move_fiber_list_p));
        fiber_list_loop(*g_move_fiber_list_p, index)
        {
            uint8_t value;
            return_on_err(fiber_list_get_element_at(*g_move_fiber_list_p, index, &value));
            LOG_INFO("Move list `%d`:\t%d", index, value);
        }
        fiber_list_loop(g_read_fiber_list, index)
        {
            uint8_t value;
            return_on_err(fiber_list_get_element_at(g_read_fiber_list, index, &value));
            LOG_INFO("Read list `%d`:\t%d", index, value);
        }

        // reset everything.
        LOOP_FIBERS(fiber_nr)
        {
            algorithm_reset_fiber_motion_state(
                &g_fiber_motion_state_list[fiber_nr], g_min_step_size, max_step_size, false);
        }

        // Set into motion only the fibers that should move.
        return_on_err(algorithm_enable_motion(g_move_fiber_list_p, g_fiber_motion_state_list));
        LOG_INFO("Fiber lists successfully initialized.");
    }

    if (loop_owner == LOOP_OWNER_SERIAL)
    {
        send_data_to_serial_device_and_wait_for_ack();
    }
    else
    {
        append_data_to_file();
    }
    if (g_exit_alignment_loop)
    {
        LOG_INFO("Alignment not started as requested by the user.");
    }
    while (!g_exit_alignment_loop)
    {
        PRINT_SEPARATOR();
        LOG_INFO("Iteration: %d", iteration);
        // Make a move and read resulting coupling.
        return_on_err(move());
        return_on_err(read_coupling(&g_read_fiber_list, num_of_samples, g_new_coupling));

        if (loop_owner == LOOP_OWNER_SERIAL)
        {
            send_data_to_serial_device_and_wait_for_ack();
        }
        else
        {
            append_data_to_file();
        }
        if (g_exit_alignment_loop)
        {
            LOG_INFO("Exiting main loop as requested.");
            break;
        }
        // For each fiber, set the next step
        fiber_list_loop(g_read_fiber_list, index)
        {
            uint8_t read_fiber_nr, move_fiber_nr;
            fiber_list_get_element_at(g_read_fiber_list, index, &read_fiber_nr);
            fiber_list_get_element_at(*g_move_fiber_list_p, index, &move_fiber_nr);
            LOG_TRACE(
                "Move fiber `%d` - retries: %d, axis: %c.",
                move_fiber_nr,
                g_fiber_motion_state_list[move_fiber_nr].attempt,
                g_fiber_motion_state_list[move_fiber_nr].move_cmd.axis);

            // Update state

            if (g_fiber_motion_state_list[move_fiber_nr].attempt == fail_1_2)
            {
                LOG_INFO("Returning to center. Overwriting max coupling");
                g_coupling[read_fiber_nr] = g_new_coupling[read_fiber_nr];
            }
            if (g_fiber_motion_state_list[move_fiber_nr].attempt == fail_2_2)
            {
                LOG_INFO("Returning to center. Overwriting max coupling");
                g_coupling[read_fiber_nr] = g_new_coupling[read_fiber_nr];
            }
            new_max_found = algorithm_compare_coupling(
                &g_coupling[read_fiber_nr], g_new_coupling[read_fiber_nr]);

            LOG_DEBUG(
                "[%d] Hit on reset  : %d",
                move_fiber_nr,
                g_fiber_motion_state_list[move_fiber_nr].boundary_hit_on_reset);
            LOG_DEBUG(
                "[%d] Hit on fail 1 : %d",
                move_fiber_nr,
                g_fiber_motion_state_list[move_fiber_nr].boundary_hit_on_fail_1);
            LOG_DEBUG("[%d] Next max found: %d", move_fiber_nr, new_max_found);
            // If the `boundary_hit_on_reset` flag is true, it might be that we just hit the
            // boundary or we have already changed direction. In the former case, then our current
            // state must be "reset", and we should count this event as a failure. Otherwise, it's a
            // success. Similarly, we should check the state when the `boundary_hit_on_fail` flag is
            // set.
            // Obviously, it is also a failure if the `new_max_found` flag is false.
            if (!new_max_found
                || (g_fiber_motion_state_list[move_fiber_nr].boundary_hit_on_reset
                    && (g_fiber_motion_state_list[move_fiber_nr].attempt == reset_1
                        || g_fiber_motion_state_list[move_fiber_nr].attempt == reset_2))
                || (g_fiber_motion_state_list[move_fiber_nr].boundary_hit_on_fail_1
                    && (g_fiber_motion_state_list[move_fiber_nr].attempt == fail_1_2
                        || g_fiber_motion_state_list[move_fiber_nr].attempt == fail_2_2)))
            {
                algorithm_handle_failure(
                    g_min_step_size, &g_fiber_motion_state_list[move_fiber_nr]);
            }
            else
            {
                algorithm_handle_success(
                    g_min_step_size, &g_fiber_motion_state_list[move_fiber_nr]);
            }
        }
        // Check if all the fibers have found their global max.
        global_max_found = true;
        fiber_list_loop(*g_move_fiber_list_p, index)
        {
            uint8_t move_fiber_nr;
            return_on_err(fiber_list_get_element_at(*g_move_fiber_list_p, index, &move_fiber_nr));
            // Fibers whose location is the global maximum have a null step size.
            if (g_fiber_motion_state_list[move_fiber_nr].curr_step_size != 0)
            {
                global_max_found = false;
                break; // exit fiber loop
            }
        }

        // Handle cases in which the global maximum is found for all fibers.
        if (global_max_found)
        {
            PRINT_SEPARATOR();
            LOG_INFO("Alignment completed.\n");
            if (g_move_fiber_list_p != &g_output_fiber_list)
            {
                LOG_INFO("Output fiber alignment starts now\n");
                fiber_list_copy(&g_output_fiber_list, &g_read_fiber_list);
                g_move_fiber_list_p = &g_output_fiber_list;
                LOOP_FIBERS(i)
                {
                    algorithm_reset_fiber_motion_state(
                        &g_fiber_motion_state_list[i],
                        g_min_step_size,
                        max_step_size,
                        continuous_alignment_started);
                }

                // Set into motion only the fibers that should move.
                return_on_err(
                    algorithm_enable_motion(g_move_fiber_list_p, g_fiber_motion_state_list));
            }
            else
            {
                if (loop_owner == LOOP_OWNER_MAWA)
                {
                    LOG_INFO("Exiting alignment loop launched by MAWA");
                    break;
                }
                if (!continuous_alignment_started)
                {
                    LOG_INFO("Continuous alignment starting now");
                    continuous_alignment_started = true;
                }
                global_max_found = false;
                new_max_found    = false;
                if (g_exit_alignment_loop)
                {
                    LOG_INFO("Exiting main loop as requested.");
                    break;
                }
                // [MIC-87] Restart the while loop with minimum step size
                return_on_err(list_utils_make_read_and_move_lists(
                    &g_input_fiber_list,
                    &g_output_fiber_list,
                    get_mapping(),
                    g_coupling,
                    NUM_OF_FIBERS,
                    &g_read_fiber_list,
                    &g_move_fiber_list_p));

                LOOP_FIBERS(i)
                {
                    algorithm_reset_fiber_motion_state(
                        &g_fiber_motion_state_list[i],
                        g_min_step_size,
                        max_step_size,
                        continuous_alignment_started);
                }

                // Set into motion only the fibers that should move.
                return_on_err(
                    algorithm_enable_motion(g_move_fiber_list_p, g_fiber_motion_state_list));
            }
        }
        iteration++;
    }
    return ERR_ALL_GOOD;
}

void serial_control_run(void)
{
    uint16_t min_coupling;
    uint16_t max_coupling;
    uint16_t ave_coupling;
    SerialCommand serial_cmd_req;
    while (!g_exit_serial_loop)
    {
        Error res = serial_control_wait_for_request(&serial_cmd_req);
        switch (res)
        {
        case ERR_INTERRUPTION:
            // Interruption handled without throwing an error.
            return;
        case ERR_INVALID:
            // Invalid command received. The response is set by `serial_control`.
            serial_control_send_response();
            continue;
        case ERR_ALL_GOOD:
            // Proceed normally
            break;
        default:
            LOG_ERROR("FATAL: Unhandled error");
            exit(ERR_FATAL);
        }

        // NOTE: If we have reached this point `res == ERR_ALL_GOOD`.
        if (res != ERR_ALL_GOOD)
        {
            LOG_ERROR("FATAL: This should not happen");
            exit(ERR_FATAL);
        }
        if (resource_start_serial() != ERR_ALL_GOOD)
        {
            LOG_ERROR("Resource busy.");
            serial_control_set_response("BUSY\n");
            serial_control_send_response();
            continue;
        }

        switch (serial_cmd_req)
        {
        case serial_command_idn:
            break;
        case serial_command_read:
            res = read_averaged_coupling(
                serial_control_get_fiber_nr(),
                serial_control_get_num_of_samples(),
                &min_coupling,
                &max_coupling,
                &ave_coupling);
            if (res != ERR_ALL_GOOD)
            {
                break;
            }
            serial_control_set_response("%u %u %u\n", min_coupling, max_coupling, ave_coupling);
            LOG_INFO("MIN: %u, MAX: %u, AVE: %u", min_coupling, max_coupling, ave_coupling);
            break;
        case serial_command_write:
        {
            uint8_t cantilever_pair_nr     = serial_control_get_cantilever_pair_nr() - 1;
            uint16_t cantilever_left_bias  = serial_control_get_cantilever_left_bias();
            uint16_t cantilever_right_bias = serial_control_get_cantilever_right_bias();
            res                            = piezo_pairs_set_bias(
                cantilever_pair_nr, cantilever_left_bias, cantilever_right_bias);
            if (res != ERR_ALL_GOOD)
            {
                LOG_ERROR("Failed to update piezo bias");
                serial_control_set_response("ERR\n");
                break;
            }
            res = _send_piezo_bias_to_dac(
                cantilever_pair_nr, cantilever_left_bias, cantilever_right_bias);
            if (res != ERR_ALL_GOOD)
            {
                LOG_ERROR("Failed to update piezo bias");
                serial_control_set_response("ERR\n");
                break;
            }
            LOG_INFO(
                "Cantilever pair: %d, VL: %d, VR: %d",
                cantilever_pair_nr,
                cantilever_left_bias,
                cantilever_right_bias);
        }
        break;
        case serial_command_start_alignment:
        {
            uint16_t hysteresis_step_size = serial_control_get_hysteresis_step_size();
            uint16_t min_step_size_bits   = serial_control_get_min_step_size();

            if ((min_step_size_bits > 15) || min_step_size_bits > (MAX_STEP_BITS - 2))
            {
                LOG_ERROR(
                    "Min step size bits `%d` incompatible with max step size bits `%d`.",
                    min_step_size_bits,
                    MAX_STEP_BITS);
                res = ERR_INVALID;
                break;
            }
            if (hysteresis_step_size > MAX_HYSTERESIS_STEP_SIZE)
            {
                LOG_ERROR(
                    "Max hysteresis step size exceeded: `%d` >= %d.",
                    hysteresis_step_size,
                    MAX_HYSTERESIS_STEP_SIZE);
                res = ERR_INVALID;
                break;
            }
            else
            {
                g_hysteresis_step_size = hysteresis_step_size;
                g_min_step_size        = 1 << min_step_size_bits;
                LOG_INFO("Min step size set to `%d`", g_min_step_size);
                LOG_INFO("Hysteresis step size set to `%d`", g_hysteresis_step_size);
            }
            _discharge_hysteresis();
            serial_control_set_response("STARTING\n");
            serial_control_send_response();

            display16x2_write_line_center(&g_display, 1, "Algo running");

            res = alignment_run(serial_control_get_num_of_samples(), LOOP_OWNER_SERIAL);
            display16x2_write_line_center(&g_display, 1, "Ready");

            serial_control_set_response("STOPPED\n");
        }
        break;
        case serial_command_invalid:
        default:
            res = ERR_INVALID;
            break;
        }
        if (res != ERR_ALL_GOOD)
        {
            serial_control_set_response("ERR\n");
        }
        EXIT_ON_ERROR(serial_control_send_response());
        if (resource_stop() != ERR_ALL_GOOD)
        {
            LOG_ERROR("FATAL: Failed to stop algorithm or algorithm not running.");
            exit(ERR_FATAL);
        }
    }
}

void* mawa_control_run(void* unused)
{
    UNUSED(unused);
    SerialCommand mawa_cmd_req;
    while (!g_exit_serial_loop)
    {
        Error res = mawa_control_wait_for_request(&mawa_cmd_req);
        switch (res)
        {
        case ERR_INTERRUPTION:
            // Interruption handled without throwing an error.
            return (void*)NULL;
        case ERR_INVALID:
            // Invalid command received. The response is set by `serial_control`.
            mawa_control_send_response();
            continue;
        case ERR_ALL_GOOD:
            // Proceed normally
            break;
        default:
            LOG_ERROR("FATAL: Unhandled error");
            exit(ERR_FATAL);
        }
        // NOTE: If we have reached this point `res == ERR_ALL_GOOD`.
        if (res != ERR_ALL_GOOD)
        {
            LOG_ERROR("FATAL: This should not happen");
            exit(ERR_FATAL);
        }

        if (resource_start_mawa() != ERR_ALL_GOOD)
        {
            LOG_ERROR("Resource busy.");
            mawa_control_set_response("BUSY\n");
            mawa_control_send_response();
            continue;
        }

        switch (mawa_cmd_req)
        {
        case serial_command_start_alignment:
        {
            uint16_t num_of_samples       = mawa_control_get_num_of_samples();
            uint16_t hysteresis_step_size = mawa_control_get_hysteresis_step_size();
            uint16_t min_step_size_bits   = mawa_control_get_min_step_size();

            if ((min_step_size_bits > 15) || min_step_size_bits > (MAX_STEP_BITS - 2))
            {
                LOG_ERROR(
                    "Min step size bits `%d` incompatible with max step size bits `%d`.",
                    min_step_size_bits,
                    MAX_STEP_BITS);
                res = ERR_INVALID;
                break;
            }
            if (hysteresis_step_size > MAX_HYSTERESIS_STEP_SIZE)
            {
                LOG_ERROR(
                    "Max hysteresis step size exceeded: `%d` >= %d.",
                    hysteresis_step_size,
                    MAX_HYSTERESIS_STEP_SIZE);
                res = ERR_INVALID;
                break;
            }
            else
            {
                g_hysteresis_step_size = hysteresis_step_size;
                g_min_step_size        = 1 << min_step_size_bits;
                LOG_INFO("Min step size set to `%d`", g_min_step_size);
                LOG_INFO("Hysteresis step size set to `%d`", g_hysteresis_step_size);
            }

            display16x2_write_line_center(&g_display, 1, "Algo running");

            g_algorithm_data_file = fopen(ALGORITHM_DATA_FILE_PATH, "w");
            if (g_algorithm_data_file == NULL)
            {
                LOG_PERROR("Failed to open algorithm data file");
                exit(ERR_FATAL);
            }
            const uint16_t max_step_size = 1 << MAX_STEP_BITS;
            fprintf(g_algorithm_data_file, "N:%d\n", NUM_OF_FIBERS);
            fprintf(g_algorithm_data_file, "MIN STEP            : %d\n", g_min_step_size);
            fprintf(g_algorithm_data_file, "MAX STEP            : %d\n", max_step_size);
            fprintf(g_algorithm_data_file, "NUM OF SAMPLES      : %d\n", num_of_samples);
            fprintf(g_algorithm_data_file, "HYSTERESIS STEP SIZE: %d\n", g_hysteresis_step_size);

            _discharge_hysteresis();
            mawa_control_set_response("STARTING\n");
            EXIT_ON_ERROR(mawa_control_send_response());
            int time_start_ms = get_time_ms();
            res               = alignment_run(num_of_samples, LOOP_OWNER_MAWA);
            int time_end_ms   = get_time_ms();
            fprintf(g_algorithm_data_file, "Elapsed time:%d\n", time_end_ms - time_start_ms);
            fclose(g_algorithm_data_file);
            g_algorithm_data_file = NULL;
            display16x2_write_line_center(&g_display, 1, "Ready");

            mawa_control_set_response("STOPPED\n");
        }
        break;
        default:
            res = ERR_INVALID;
            break;
        }
        if (res != ERR_ALL_GOOD)
        {
            mawa_control_set_response("ERR\n");
        }
        EXIT_ON_ERROR(mawa_control_send_response());
        if (resource_stop() != ERR_ALL_GOOD)
        {
            LOG_ERROR("FATAL: Failed to stop algorithm or algorithm not running.");
            exit(ERR_FATAL);
        }
    }
    return (void*)NULL;
}

int main(int argc, char* argv[])
{
    PRINT_SEPARATOR();
    if (argc == 2)
    {
        if (strncmp(argv[1], "-v\0", 3) == 0)
        {
            printf(MAMS_VERSION);
            return 0;
        }
    }
    else if (argc != 1)
    {
        printf("Invalid parameters\n");
        return ERR_INVALID;
    }
    struct sigaction sa = {.sa_handler = signal_handler};
    sigaction(SIGINT, &sa, 0);
    sigaction(SIGTERM, &sa, 0);

    EXIT_ON_ERROR(set_program_priority(sched_get_priority_max(SCHED_RR)));
    EXIT_ON_ERROR(resource_state_init());
    logger_init(DEFAULT_LOG_FILE, DEFAULT_LOG_FILE);
    EXIT_ON_ERROR(gpio_init());
    // Power on LCD and button's LED
    gpio_assign(ON_LED, output);
    gpio_clr(ON_LED);
    {
        LOG_INFO("Trying to create devices.");
        // Device creation.
        gpio_number_t* adc_cs_list       = get_adc_cs_list(NUM_OF_ADCS);
        gpio_number_t* adc_data_out_list = get_adc_data_out_list(NUM_OF_ADCS);

        LOG_DEBUG("Trying to create ADC's.");
        for (int index = 0; index < NUM_OF_ADCS; index++)
        {
            EXIT_ON_ERROR(adc_device_create(
                GPIO_CLK /* CLK */,
                GPIO_DATA_OUT /* DIN */,
                adc_cs_list[index] /* CS_ */,
                adc_data_out_list[index] /* DOUT */,
                &g_adc_list[index]));
        }
        LOG_DEBUG("ADC's created successfully.");

        LOG_DEBUG("Trying to create DAC's.");
        gpio_number_t* dac_cs_list = get_dac_cs_list(NUM_OF_DACS);
        for (int index = 0; index < NUM_OF_DACS; index++)
        {
            EXIT_ON_ERROR(
                dac_device_create(GPIO_CLK, GPIO_DATA_OUT, dac_cs_list[index], &g_dac_list[index]));
        }
        LOG_DEBUG("ADC's created successfully.");

        LOG_DEBUG("Trying to create Display.");
        EXIT_ON_ERROR(display16x2_create(
            GPIO_DISPLAY_RS, GPIO_DISPLAY_E, GPIO_DATA_OUT, GPIO_CLK, &g_display));
        LOG_DEBUG("Display created successfully.");
        LOG_INFO("Device creation successfully completed.");
    }

    {
        LOG_INFO("Wait 100 ms before initializing the display.");
        usleep(100000);
        LOG_INFO("Initializing display.");
        display16x2_clear(&g_display);
        display16x2_home(&g_display);
        display16x2_enable_second_row(&g_display);
        display16x2_show_cursor(&g_display, false);
        LOG_INFO("Display initialized.");
    }

    // Initialize piezo mapping
    {
        LOG_TRACE("Initialize piezo pairs");
        // For each fiber, assign a pair of DAC channels.
        // A DAC channel is a logical channel only. If we have N DAC's with M physical channel
        // each, then we have M*N logical channels, numbered in order from 0 to N*M-1.
        uint8_t dac_channel_left  = 0;
        uint8_t dac_channel_right = 1;
        LOOP_FIBERS(fiber_index)
        {
            uint8_t dac_number_left   = dac_channel_left / DAC_CHANNELS;
            uint8_t dac_address_left  = dac_channel_left % DAC_CHANNELS;
            uint8_t dac_number_right  = dac_channel_right / DAC_CHANNELS;
            uint8_t dac_address_right = dac_channel_right % DAC_CHANNELS;
            EXIT_ON_ERROR(piezo_pairs_set_dac(
                fiber_index,
                dac_address_left,
                dac_number_left,
                dac_address_right,
                dac_number_right,
                MAX_BIAS));
            EXIT_ON_ERROR(piezo_pairs_set_bias(fiber_index, HALF_BIAS, HALF_BIAS));
            dac_channel_left += 2;
            dac_channel_right += 2;
        }
        LOG_TRACE("Piezo pairs initialized successfully");
    }
    display16x2_write_line_center(&g_display, 0, "SERIAL MODE");
    display16x2_write_line_center(&g_display, 1, "Ready");
    /*********** Application loops start ***********/
    pthread_t t_mawa;
    mawa_control_init(&g_exit_serial_loop);
    serial_control_init("/dev/serial0");
    // MAWA control runs on a secondary thread.
    if (pthread_create(&t_mawa, NULL, &mawa_control_run, NULL) < 0)
    {
        return ERR_FATAL;
    }
    // Serial control runs on the main thread.
    serial_control_run();
    /*********** Application loops end ***********/
    display16x2_write_line(&g_display, 0, "Goodbye.");
    display16x2_write_line_center(&g_display, 1, "");
    // GPIO destruction.
    gpio_destroy();
    LOG_INFO("Goodbye.");
    return ERR_ALL_GOOD;
}
#elif TEST == 1
int main()
{
    EXIT_ON_ERROR(set_program_priority(sched_get_priority_max(SCHED_RR)));
    test_gpio();
    test_list_utils();

    test_piezos();
    test_algorithm();
    test_fiber_list();
    //    test_mcp4922();
    //    test_mcp3304();
    test_max1229();
    test_ad5674();
    test_instruction();
    test_prj_setup();
    test_display();
    PRINT_SEPARATOR();
    return 0;
}
#else
#error TEST should be set to 0 or 1
#endif
