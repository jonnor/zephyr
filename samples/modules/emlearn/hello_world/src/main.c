/*
 * Copyright 2024 emlearn authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "xor_model.h"

void xor_test(void)
{
    const float test_inputs[4][2] = {
        { 0.0f, 0.0f },
        { 1.0f, 0.0f },
        { 0.0f, 1.0f },
        { 1.0f, 1.0f },
    };

    for (int i=0; i<4; i++) {
        const float *features = test_inputs[i];
        const int out = xor_model_predict(features, 2);

        printf("xor(%.2f, %.2f) = %d\n", features[0], features[1], out);
    }
}


/* This is the default main used on systems that have the standard C entry
 * point. Other devices (for example FreeRTOS or ESP32) that have different
 * requirements for entry code (like an app_main function) should specialize
 * this main.cc file in a target-specific subfolder.
 */
int main(int argc, char *argv[])
{
    xor_test();

	return 0;
}
