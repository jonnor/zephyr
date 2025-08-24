
#include <math.h>

#define ACCELGYRO_INPUT_CHANNELS 6

enum accelgyro_feature {
    accelgyro_feature_orientation_x,
    accelgyro_feature_orientation_y,
    accelgyro_feature_orientation_z,
    accelgyro_feature_acceleration_mag_rms,
    accelgyro_features_length
};
//#define ACCELGYRO_PREPROCESSOR_FEATURES 6

struct accelgyro_preprocessor {
    float features[accelgyro_features_length]; // reused working-buffer
};

// TODO: support gravity separation
int
accelgyro_preprocessor_run(struct accelgyro_preprocessor *self,
                            const float *data,
                            int length)
{
    // TODO: verify input dimensions

    float orientation_x = 0.0;
    float orientation_y = 0.0;
    float orientation_z = 0.0;

#if 0
    float rms_x = 0.0;
    float rms_y = 0.0;
    float rms_z = 0.0;
#endif

    float acc_mag_squared = 0.0f;

    const int n_frames = length / ACCELGYRO_INPUT_CHANNELS;
    for (int i=0; i<n_frames; i++) {
        const int offset = i * ACCELGYRO_INPUT_CHANNELS;

        // NOTE: order must match the defined in sensor readout
        const float acc_x = data[offset+0];
        const float acc_y = data[offset+1];
        const float acc_z = data[offset+2];

        // TODO: utilize gyro data
#if 0
        const float gyro_x = data[offset+3];
        const float gyro_y = data[offset+4];
        const float gyro_z = data[offset+5];
#endif

        // Compute features
        const float acc_mag = (acc_x*acc_x) + (acc_y*acc_y) + (acc_z*acc_z);
        acc_mag_squared += (acc_mag*acc_mag);

        // TODO: do sensor-fusion with gyro data. Complimentary filter or Kalman
        orientation_x += acc_x;
        orientation_y += acc_y;
        orientation_z += acc_z;
    }

    float *features = self->features;

    // TODO: normalize the orientation vector
    orientation_x /= n_frames;
    orientation_z /= n_frames;
    orientation_y /= n_frames;
    features[accelgyro_feature_orientation_x] = orientation_x;
    features[accelgyro_feature_orientation_y] = orientation_y;
    features[accelgyro_feature_orientation_z] = orientation_z;

    const float acc_rms_mag = sqrtf(acc_mag_squared / n_frames);
    features[accelgyro_feature_acceleration_mag_rms] = acc_rms_mag;

    return 0;
}

