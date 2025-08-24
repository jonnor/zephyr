/*
 * Copyright (c) 2018 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <stdio.h>
#include <zephyr/sys/util.h>

#include "sensor_reader.h"

// Configuration
#define SAMPLERATE 104
#define WINDOW_LENGTH 100
#define HOP_LENGTH 25

#define N_CHANNELS 6
enum sensor_channel sensor_reader_channels[N_CHANNELS] = {
	SENSOR_CHAN_ACCEL_X,
	SENSOR_CHAN_ACCEL_Y,
	SENSOR_CHAN_ACCEL_Z,
	SENSOR_CHAN_GYRO_X,
	SENSOR_CHAN_GYRO_Y,
	SENSOR_CHAN_GYRO_Z,
};


// Reader internals
struct sensor_value sensor_reader_output_buffer[WINDOW_LENGTH*N_CHANNELS];
struct sensor_value sensor_reader_new_buffer[HOP_LENGTH*N_CHANNELS];

#define SENSOR_READER_STACK_SIZE 1000
K_THREAD_STACK_DEFINE(sensor_reader_stack, SENSOR_READER_STACK_SIZE);
struct k_thread sensor_reader_thread;

K_MSGQ_DEFINE(sensor_reader_queue, sizeof(struct sensor_chunk_msg), 1, 1);


int
setup_sensor(const struct device *const lsm6dsl_dev)
{
	struct sensor_value odr_attr;
	if (!device_is_ready(lsm6dsl_dev)) {
		printk("sensor: device not ready.\n");
		return -1;
	}

	/* set accel/gyro sampling frequency */
	odr_attr.val1 = SAMPLERATE;
	odr_attr.val2 = 0;

	if (sensor_attr_set(lsm6dsl_dev, SENSOR_CHAN_ACCEL_XYZ,
			    SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr) < 0) {
		printk("Cannot set sampling frequency for accelerometer.\n");
		return -2;
	}

	if (sensor_attr_set(lsm6dsl_dev, SENSOR_CHAN_GYRO_XYZ,
			    SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr) < 0) {
		printk("Cannot set sampling frequency for gyro.\n");
		return -3;
	}

    return 0;
}

enum accelgyro_feature {
    accelgyro_feature_orientation_x,
    accelgyro_feature_orientation_y,
    accelgyro_feature_orientation_z,
    accelgyro_feature_acceleration_mag_rms,
    accelgyro_features_length;
}
//#define ACCELGYRO_PREPROCESSOR_FEATURES 6

struct accelgyro_preprocessor {

    float features[accelgyro_features_length]; // reused working-buffer

};

int
accelgyro_preprocessor_run(struct accelgyro_preprocessor *self,
                            const struct sensor_value *data,
                            int length)
{
    // TODO: verify input dimensions

    float orientation_x = 0.0;
    float orientation_y = 0.0;
    float orientation_z = 0.0;

    float rms_x = 0.0;
    float rms_y = 0.0;
    float rms_z = 0.0;

    float acc_mag_squared = 0.0f;

    const int n_frames = length / N_CHANNELS;
    for (int i=0; i<n_frames; i++) {
        const int offset = i * N_CHANNELS;

        // NOTE: order must match the defined in sensor readout
        const float acc_x = sensor_value_to_double(data[offset+0]);
        const float acc_y = sensor_value_to_double(data[offset+1]);
        const float acc_z = sensor_value_to_double(data[offset+2]);

        const float gyro_x = sensor_value_to_double(data[offset+3]);
        const float gyro_y = sensor_value_to_double(data[offset+4]);
        const float gyro_z = sensor_value_to_double(data[offset+5]);

        // Compute features
        const float acc_mag = (acc_x*acc_x) + (acc_y*acc_y) + (acc_z*acc_z);
        acc_mag_squared += (acc_mag*acc_mag);

        // TODO: do sensor-fusion with gyro data. Complimentary filter or Kalman
        orientation_x += acc_x;
        orientation_y += acc_y;
        orientation_z += acc_z;
    }

    // TODO: normalize the orientation vector
    orientation_x /= n_frames;
    orientation_z /= n_frames;
    orientation_y /= n_frames;
    features[accelgyro_feature_orientation_x] = orientation_x;
    features[accelgyro_feature_orientation_y] = orientation_y;
    features[accelgyro_feature_orientation_z] = orientation_z;

    const float acc_rms_mag = sqrtf(acc_mag_squared / n_frames);
    features[accelgyro_feature_acceleration_mag_rms] = acc_rms_mag;
}

void
process(const struct sensor_value *data, int length)
{    
    printk("process-chunk length=%d \n", length);
    accelgyro_preprocessor_run(data, length);



}

int main(void) {

    struct sensor_chunk_msg chunk;

	const struct device *const lsm6dsl_dev = DEVICE_DT_GET_ONE(st_lsm6dsl);

    struct sensor_chunk_reader reader = {
        .samplerate = SAMPLERATE,
        .window_length = WINDOW_LENGTH,
        .hop_length = HOP_LENGTH,
        .thread = &sensor_reader_thread,
        .thread_priority = 10,
        .read_samples = sensor_reader_new_buffer,
        .read_samples_index = 0,
        .output_buffer = sensor_reader_output_buffer,
        .output_buffer_index = 0,
        .n_channels = N_CHANNELS,
        .channels = sensor_reader_channels,
        .dev = lsm6dsl_dev,
        .queue = &sensor_reader_queue,
        .stack = sensor_reader_stack,
        .stack_size = SENSOR_READER_STACK_SIZE,
        .fetch_errors = 0,
        .get_errors = 0,
        .put_errors = 0
    };

    struct accelgyro_preprocessor preprocessor = {
    };


    if (!device_is_ready(lsm6dsl_dev)) {
        printk("sensor: device %s not ready.\n", lsm6dsl_dev->name);
        return 0;
    }

    // Setup sensor
    setup_sensor(lsm6dsl_dev);


    // Start high-priority thread for collecting data
    sensor_chunk_reader_start(&reader);

    int iteration = 0;
    while (1) {

        // check for new data
        const int get_status = k_msgq_get(reader.queue, &chunk, K_NO_WAIT);
        if (get_status != 0) {
            process(chunk.buffer, chunk.length);
        }

        printk("main-loop-iter iteration=%d \n", iteration);

        iteration += 1;
	    k_msleep(200);
    }

    return 0;
}

