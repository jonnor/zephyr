
#include <math.h>
#include <eml_iir.h>

#define ACCELGYRO_INPUT_CHANNELS 6

// IIR filters are biquads, to 2 stages -> 4th order
#define ACCELGYRO_GRAVITY_FILTER_STAGES 2
#define ACCELGYRO_GRAVITY_FILTER_COEFFICIENTS (ACCELGYRO_GRAVITY_FILTER_STAGES*6)
#define ACCELGYRO_GRAVITY_FILTER_STATES (ACCELGYRO_GRAVITY_FILTER_STAGES*3*4)


enum accelgyro_feature {
    accelgyro_feature_orientation_x = 0,
    accelgyro_feature_orientation_y,
    accelgyro_feature_orientation_z,
    accelgyro_feature_motion_mag_rms,
    accelgyro_feature_motion_mag_p2p,
    accelgyro_feature_motion_x_rms,
    accelgyro_feature_motion_y_rms,
    accelgyro_feature_motion_z_rms,
    accelgyro_features_length
};
//#define ACCELGYRO_PREPROCESSOR_FEATURES 6

struct accelgyro_preprocessor {

    // output buffer
    float features[accelgyro_features_length];

    // decomposed gravity and motion (linear acceleration) vectors
    float motion[3];
    float gravity[3];

    int32_t frames_processed;

    // IIR filter for gravity separation
    // Multiple filters and associated states in XYZ order
    // Coefficients are shared, same for X,Y,Z
    EmlIIR gravity_filters[3];
    float gravity_coefficients[ACCELGYRO_GRAVITY_FILTER_COEFFICIENTS];
    float gravity_states[ACCELGYRO_GRAVITY_FILTER_STATES];
    bool gravity_filter_enable;
};

int
accelgyro_preprocessor_init(struct accelgyro_preprocessor *self)
{
    self->frames_processed = 0;

    // Gravity separation
    self->gravity_filter_enable = false;

    for (int i=0; i<3; i++) {
        self->gravity_filters[i] = (EmlIIR){
            ACCELGYRO_GRAVITY_FILTER_STAGES,
            self->gravity_states + i * ACCELGYRO_GRAVITY_FILTER_STATES,
            ACCELGYRO_GRAVITY_FILTER_STATES,
            self->gravity_coefficients + i*ACCELGYRO_GRAVITY_FILTER_COEFFICIENTS,
            ACCELGYRO_GRAVITY_FILTER_COEFFICIENTS,
        };

        const EmlError filter_err = eml_iir_check(self->gravity_filters[i]);
        if (filter_err != EmlOk) {
            return -1;
        }

    }
    for (int i=0; i<ACCELGYRO_GRAVITY_FILTER_STATES; i++) {
        self->gravity_states[i] = 0.0f;
    }

    return 0;
}

// coeff must be for a 4th-order IIR filter, on EmlIIR format
int
accelgyro_preprocessor_set_gravity_lowpass(struct accelgyro_preprocessor *self,
        const float *coeff, int n_coefficients)
{
    if (n_coefficients != ACCELGYRO_GRAVITY_FILTER_COEFFICIENTS) {
        return -1;
    }

    memcpy(self->gravity_coefficients, coeff, n_coefficients*sizeof(float));
    self->gravity_filter_enable = true;

    return 0;
}


int
accelgyro_preprocessor_run(struct accelgyro_preprocessor *self,
                            const float *data,
                            int length)
{
    if ((length % ACCELGYRO_INPUT_CHANNELS) != 0) {
        return -1;
    }

    float motion_x_squared = 0.0;
    float motion_y_squared = 0.0;
    float motion_z_squared = 0.0;

    float motion_mag_squared = 0.0f;
    float motion_mag_min = INFINITY;
    float motion_mag_max = -INFINITY;

    if (self->frames_processed <= 0) {
        // handle overflow
        self->frames_processed = 0;
    }

    const int n_frames = length / ACCELGYRO_INPUT_CHANNELS;
    for (int frame=0; frame<n_frames; frame++) {

        // NOTE: accelerometer XYZ must be first 3 components
        const int offset = frame * ACCELGYRO_INPUT_CHANNELS;
        const float *xyz = data+offset;

        // NOTE: gyro data currently ignored
        // TODO: do sensor-fusion with gyro, and use complimentary filter for gravity separation

        // Gravity vector separation using low-pass
        if (self->frames_processed == 0) {
            // warm up the low-pass filter
            // avoids slow ramp-in from startup 0 to the near-DC values
            const int initialization_repetitions = 10;
            for (int i=0; i<3; i++) {
                for (int r=0; r<initialization_repetitions; r++) {
                    eml_iir_filter(self->gravity_filters[i], xyz[i]);
                }
            }
        }

        // Estimate gravity with low-pass,
        // and subtract gravity to estimate linear acceleration / "motion"
        for (int i=0; i<3; i++) {
            self->gravity[i] = eml_iir_filter(self->gravity_filters[i], xyz[i]);
            self->motion[i] = xyz[i] - self->gravity[i];
        }

        const float motion_x = self->motion[0];
        const float motion_y = self->motion[1];
        const float motion_z = self->motion[2];

        // magnitude squared for rms
        const float motion_mag = sqrtf((motion_x*motion_x) + (motion_y*motion_y) + (motion_z*motion_z));
        motion_mag_squared += (motion_mag*motion_mag);

        // motion min/max for p2p
        if (motion_mag < motion_mag_min) {
            motion_mag_min = motion_mag;
        }
        if (motion_mag > motion_mag_max) {
            motion_mag_max = motion_mag;
        }

        // XYZ motion
        motion_x_squared += (motion_x*motion_x);
        motion_y_squared += (motion_y*motion_y);
        motion_z_squared += (motion_z*motion_z);

        self->frames_processed += 1;
    }

    float *features = self->features;

    // Orientation vector
    // Is the gravity vector estimate, normalized to 1.0 magitude
    const float *gv = self->gravity;
    const float gravity_mag = sqrtf((gv[0]*gv[0]) + (gv[1]*gv[1]) + (gv[2]*gv[2]));
    features[accelgyro_feature_orientation_x] = self->gravity[0] / gravity_mag;
    features[accelgyro_feature_orientation_y] = self->gravity[1] / gravity_mag;
    features[accelgyro_feature_orientation_z] = self->gravity[2] / gravity_mag;

    // Motion in XYZ
    features[accelgyro_feature_motion_x_rms] = sqrtf(motion_x_squared / n_frames);
    features[accelgyro_feature_motion_y_rms] = sqrtf(motion_y_squared / n_frames);
    features[accelgyro_feature_motion_z_rms] = sqrtf(motion_z_squared / n_frames);

    // Motion magnitude RMS
    const float motion_mag_rms = sqrtf(motion_mag_squared / n_frames);
    features[accelgyro_feature_motion_mag_rms] = motion_mag_rms;

    // Motion magnitude p2p
    const float motion_mag_p2p = motion_mag_max - motion_mag_min;
    features[accelgyro_feature_motion_mag_p2p] = motion_mag_p2p;

    return 0;
}

