/*--------------------------------------------------------------------------
Copyright (c) 2014, The Linux Foundation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
--------------------------------------------------------------------------*/

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <float.h>
#include <sys/select.h>
#include <cutils/log.h>

#include "OrientationSensor.h"
#include "sensors.h"

/*****************************************************************************/

OrientationSensor::OrientationSensor(const struct SensorContext *ctx)
	: SensorBase(NULL, NULL),
	  mEnabled(0),
	  mHasPendingEvent(false),
	  mEnabledTime(0),
	  context(ctx),
	  mCurr(mBuffer),
	  mHead(mBuffer),
	  mBufferEnd(mBuffer + MAX_EVENTS),
	  mFreeSpace(MAX_EVENTS)

{
	enable(0, 1);
}

OrientationSensor::~OrientationSensor() {
	if (mEnabled) {
		enable(0, 0);
	}
}

int OrientationSensor::enable(int32_t, int en) {
	mEnabled = en? 1 : 0;
	return 0;
}

bool OrientationSensor::hasPendingEvents() const {
	return mBufferEnd - mBuffer - mFreeSpace;
}

int OrientationSensor::readEvents(sensors_event_t* data, int count)
{
	int number = 0;

	if (count < 1)
		return -EINVAL;

	while (count && (mBufferEnd - mBuffer - mFreeSpace)) {
		*data++ = *mCurr++;
		if (mCurr >= mBufferEnd)
			mCurr = mBuffer;
		number++;
		mFreeSpace++;
		count--;
	}

	return number;
}

int OrientationSensor::injectEvents(sensors_event_t* data, int count)
{
	int i;
	int flag;
	sensors_event_t event;

	for (i = 0; i < count; i++) {
		flag = 1;

		event = data[i];
		switch (data->type) {
			case SENSOR_TYPE_ACCELEROMETER:
				da = data[i].acceleration;
				break;
			case SENSOR_TYPE_MAGNETIC_FIELD:
				dm = data[i].magnetic;
				break;
			default:
				flag = 0;
				break;
		}

		/* Calculate the orientation data */
		if (flag) {
			if (mFreeSpace) {
				float av;
				float pitch, roll, azimuth;
				const float rad2deg = 180 / M_PI;

				event.version = sizeof(sensors_event_t);
				event.sensor = '_ypr';
				event.type = SENSOR_TYPE_ORIENTATION;

				av = sqrtf(da.x*da.x + da.y*da.y + da.z*da.z);
				if (av >= DBL_EPSILON) {
					pitch = asinf(-da.y / av);
					roll = asinf(da.x / av);
					event.orientation.pitch = pitch * rad2deg;
					event.orientation.roll = roll * rad2deg;
					azimuth = atan2(-(dm.x) * cosf(roll) + dm.z * sinf(roll),
							dm.x*sinf(pitch)*sinf(roll) + dm.y*cosf(pitch) + dm.z*sinf(pitch)*cosf(roll));
					event.orientation.azimuth =  azimuth * rad2deg;
					event.orientation.status = dm.status;

					*mHead++ = event;
					mFreeSpace--;
					if (mHead >= mBufferEnd) {
						mHead = mBuffer;
					}
				}
			} else {
				ALOGW("Circular buffer is full\n");
			}
		}
	}

	return 0;
}

