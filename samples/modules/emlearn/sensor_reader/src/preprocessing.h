
#include <math.h>

#define ACCELGYRO_INPUT_CHANNELS 6

// TODO: add motion_mag_p2p
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
    // configuration
    float lowpass_alpha;

    // output buffer
    float features[accelgyro_features_length];

    // decomposed gravity and motion (linear acceleration) vectors
    float motion[3];
    float gravity[3];

    int32_t frames_processed;
};

void
accelgyro_preprocessor_init(struct accelgyro_preprocessor *self)
{
    self->frames_processed = 0;
}

int
accelgyro_preprocessor_set_gravity_lowpass(struct accelgyro_preprocessor *self, float cutoff, int samplerate)
{
    if (cutoff >= samplerate/2.0f) {
        return -1;
    }

    const float rc = 1.0f/(2.0f*3.14f*cutoff);
    const float dt = 1.0f/samplerate;
    self->lowpass_alpha = rc / (rc + dt);

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
    for (int i=0; i<n_frames; i++) {
        const int offset = i * ACCELGYRO_INPUT_CHANNELS;

        // NOTE: accelerometer XYZ must be first 3 components
        const float *xyz = data+offset;

        // Gravity vector separation using low-pass
        // TODO: do sensor-fusion with gyro, and use complimentary filter for gravity separation
        if (self->frames_processed == 0) {

            // initialize with current, to avoid gradual rampin from 0 from lowpass
            self->gravity[0] = xyz[0];
            self->gravity[1] = xyz[1];
            self->gravity[2] = xyz[2];

            self->motion[0] = 0.0;
            self->motion[1] = 0.0;
            self->motion[2] = 0.0;

        } else {
            // Estimate gravity with low-pass,
            // and subtract it to estimate linear acceleration / "motion"
            const float a = self->lowpass_alpha;
            for (int i=0; i<3; i++) {
                self->gravity[i] = (a * self->gravity[i]) + ((1.0f - a) * xyz[i]);
                self->motion[i] = xyz[i] - self->gravity[i];
            }
        }

        const float motion_x = self->motion[0];
        const float motion_y = self->motion[1];
        const float motion_z = self->motion[2];

        // magnitude squared for rms
        const float motion_mag = (motion_x*motion_x) + (motion_y*motion_y) + (motion_z*motion_z);
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
    const float gravity_mag = (gv[0]*gv[0]) + (gv[1]*gv[1]) + (gv[2]*gv[2]);
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

