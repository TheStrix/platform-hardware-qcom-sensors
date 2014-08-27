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
#include "NativeSensorManager.h"

ANDROID_SINGLETON_STATIC_INSTANCE(NativeSensorManager);

const struct SysfsMap NativeSensorManager::node_map[] = {
	{offsetof(struct sensor_t, name), SYSFS_NAME, TYPE_STRING},
	{offsetof(struct sensor_t, vendor), SYSFS_VENDOR, TYPE_STRING},
	{offsetof(struct sensor_t, version), SYSFS_VERSION, TYPE_INTEGER},
	{offsetof(struct sensor_t, type), SYSFS_TYPE, TYPE_INTEGER},
	{offsetof(struct sensor_t, maxRange), SYSFS_MAXRANGE, TYPE_FLOAT},
	{offsetof(struct sensor_t, resolution), SYSFS_RESOLUTION, TYPE_FLOAT},
	{offsetof(struct sensor_t, power), SYSFS_POWER, TYPE_FLOAT},
	{offsetof(struct sensor_t, minDelay), SYSFS_MINDELAY, TYPE_INTEGER},
};

NativeSensorManager::NativeSensorManager():
	mSensorCount(0)
{
	int i;

	memset(sensor_list, 0, sizeof(sensor_list));
	memset(context, 0, sizeof(context));

	for (i = 0; i < MAX_SENSORS; i++) {
		context[i].sensor = &sensor_list[i];
		sensor_list[i].name = context[i].name;
		sensor_list[i].vendor = context[i].vendor;
	}

	if(getDataInfo()) {
		ALOGE("Get data info failed\n");
	}

	dump();
}

void NativeSensorManager::dump()
{
	int i;

	for (i = 0; i < mSensorCount; i++) {
		ALOGI("name:%s\ntype:%d\nhandle:%d\ndata_fd=%d\ndata_path=%s\nenable_path=%s\n",
				context[i].sensor->name,
				context[i].sensor->type,
				context[i].sensor->handle,
				context[i].data_fd,
				context[i].data_path,
				context[i].enable_path);
		ALOGI("delay_ms:%ld\ndep_mask:%lld\nlistener:%lld\n",
				context[i].delay_ms,
				context[i].dep_mask,
				context[i].listener);
	}
}

const SensorContext* NativeSensorManager::getInfoByFd(int fd) {
	int i;
	struct SensorContext *list;

	for (i = 0; i < mSensorCount; i++) {
		list = &context[i];
		if (fd == list->data_fd)
			return list;
	}

	return NULL;
}

const SensorContext* NativeSensorManager::getInfoByHandle(int handle) {
	int i;
	struct SensorContext *list;

	for (i = 0; i < mSensorCount; i++) {
		list = &context[i];
		if (handle == list->sensor->handle)
			return list;
	}

	return NULL;
}

const SensorContext* NativeSensorManager::getInfoByType(int type) {
	int i;
	struct SensorContext *list;

	for (i = 0; i < mSensorCount; i++) {
		list = &context[i];
		if (type == list->sensor->type)
			return list;
	}

	return NULL;
}

int NativeSensorManager::getDataInfo() {
	struct dirent **namelist;
	char *file;
	char path[PATH_MAX];
	char name[80];
	int nNodes;
	int i, j;
	int fd = -1;
	struct SensorContext *list;
	int has_acc = 0;
	int has_compass = 0;
	int event_count = 0;

	strlcpy(path, EVENT_PATH, sizeof(path));
	file = path + strlen(EVENT_PATH);
	nNodes = scandir(path, &namelist, 0, alphasort);
	if (nNodes < 0) {
		ALOGE("scan %s failed.(%s)\n", EVENT_PATH, strerror(errno));
		return -1;
	}

	for (event_count = 0, j = 0; (j < nNodes) && (j < MAX_SENSORS); j++) {
		if (namelist[j]->d_type != DT_CHR) {
			continue;
		}

		strlcpy(file, namelist[j]->d_name, sizeof(path) - strlen(EVENT_PATH));

		fd = open(path, O_RDONLY);
		if (fd < 0) {
			ALOGE("open %s failed(%s)", path, strerror(errno));
			continue;
		}

		if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), &name) < 1) {
			name[0] = '\0';
		}

		strlcpy(event_list[event_count].data_name, name, sizeof(event_list[0].data_name));
		strlcpy(event_list[event_count].data_path, path, sizeof(event_list[0].data_path));
		close(fd);
		event_count++;
	}

	for (j = 0; j <nNodes; j++ ) {
		free(namelist[j]);
	}

	free(namelist);

	mSensorCount = getSensorListInner();
	for (i = 0; i < mSensorCount; i++) {
		list = &context[i];
		list->dep_mask |= 1ULL << list->sensor->type;

		/* Initialize data_path and data_fd */
		for (j = 0; (j < event_count) && (j < MAX_SENSORS); j++) {
			if (strcmp(list->sensor->name, event_list[j].data_name) == 0) {
				list->data_path = strdup(event_list[j].data_path);
				break;
			}

			if (strcmp(event_list[j].data_name, type_to_name(list->sensor->type)) == 0) {
				list->data_path = strdup(event_list[j].data_path);
			}
		}

		list->data_fd = open(list->data_path, O_RDONLY);

		switch (list->sensor->type) {
			case SENSOR_TYPE_ACCELEROMETER:
				has_acc = 1;
				list->driver = new AccelSensor(list);
				break;
			case SENSOR_TYPE_MAGNETIC_FIELD:
				has_compass = 1;
				list->driver = new CompassSensor(list);
				break;
			case SENSOR_TYPE_PROXIMITY:
				list->driver = new ProximitySensor(list);
				break;
			case SENSOR_TYPE_LIGHT:
				list->driver = new LightSensor(list);
				break;
			case SENSOR_TYPE_GYROSCOPE:
				list->driver = new GyroSensor(list);
				break;
			case SENSOR_TYPE_PRESSURE:
				list->driver = new PressureSensor(list);
				break;
			default:
				list->driver = NULL;
				ALOGE("No handle %d for this type sensor!", i);
				break;
		}
	}

	if (has_acc && has_compass) {
		int index;

		list = &context[mSensorCount];
		list->sensor = &sensor_list[mSensorCount];
		list->sensor->name = "orientation";
		list->sensor->vendor = "quic";
		list->sensor->version = 1;
		list->sensor->handle = '_ypr';
		list->sensor->type = SENSOR_TYPE_ORIENTATION;
		list->sensor->maxRange = 360.0f;
		list->sensor->resolution = 1.0f/256.0f;
		list->sensor->power = 1;
		list->sensor->minDelay = 1;

		list->data_fd = -1;
		list->data_path = NULL;
		list->enable_path = NULL;
		list->driver = new OrientationSensor(list);

		mSensorCount++;

		list->dep_mask |= 1 << SENSOR_TYPE_ACCELEROMETER;
		list->dep_mask |= 1 << SENSOR_TYPE_MAGNETIC_FIELD;
	}

	return 0;
}

int NativeSensorManager::getSensorList(const sensor_t **list) {
	*list = mSensorCount ? sensor_list:NULL;

	return mSensorCount;
}

int NativeSensorManager::getNode(char *buf, char *path, const struct SysfsMap *map) {
	char * fret;
	ssize_t len = 0;
	int fd;
	char tmp[SYSFS_MAXLEN];

	if (NULL == buf || NULL == path)
		return -1;

	memset(tmp, 0, sizeof(tmp));

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		ALOGE("open %s failed.(%s)\n", path, strerror(errno));
		return -1;
	}

	len = read(fd, tmp, sizeof(tmp));
	if (len <= 0) {
		ALOGE("read %s failed.(%s)\n", path, strerror(errno));
		close(fd);
		return -1;
	}

	tmp[len - 1] = '\0';

	if (tmp[strlen(tmp) - 1] == '\n')
		tmp[strlen(tmp) - 1] = '\0';

	if (map->type == TYPE_INTEGER) {
		int *p = (int *)(buf + map->offset);
		*p = atoi(tmp);
	} else if (map->type == TYPE_STRING) {
		char **p = (char **)(buf + map->offset);
		strlcpy(*p, tmp, SYSFS_MAXLEN);
	} else if (map->type == TYPE_FLOAT) {
		float *p = (float*)(buf + map->offset);
		*p = atof(tmp);
	}

	close(fd);
	return 0;
}

int NativeSensorManager::getSensorListInner()
{
	int number = 0;
	int err = -1;
	const char *dirname = SYSFS_CLASS;
	char devname[PATH_MAX];
	char *filename;
	char *nodename;
	DIR *dir;
	struct dirent *de;
	struct SensorContext *list;
	unsigned int i;

	dir = opendir(dirname);
	if(dir == NULL) {
		return 0;
	}
	strlcpy(devname, dirname, PATH_MAX);
	filename = devname + strlen(devname);

	while ((de = readdir(dir))) {
		if(de->d_name[0] == '.' &&
			(de->d_name[1] == '\0' ||
				(de->d_name[1] == '.' && de->d_name[2] == '\0')))
			continue;

		list = &context[number];

		strlcpy(filename, de->d_name, PATH_MAX - strlen(SYSFS_CLASS));
		nodename = filename + strlen(de->d_name);
		*nodename++ = '/';

		for (i = 0; i < ARRAY_SIZE(node_map); i++) {
			strlcpy(nodename, node_map[i].node, PATH_MAX - strlen(SYSFS_CLASS) - strlen(de->d_name));
			err = getNode((char*)(list->sensor), devname, &node_map[i]);
			if (err)
				break;
		}

		if (i < ARRAY_SIZE(node_map))
			continue;

		if (!((1ULL << list->sensor->type) & SUPPORTED_SENSORS_TYPE))
			continue;

		/* Setup other information */
		list->sensor->handle = number;
		list->data_path = NULL;

		strlcpy(nodename, "", SYSFS_MAXLEN);
		list->enable_path = strdup(devname);

		number++;
	}
	closedir(dir);
	return number;
}

int NativeSensorManager::activate(int handle, int enable)
{
	const SensorContext *list;
	int index;
	int i;
	int number = getSensorCount();
	int err = -1;

	list = getInfoByHandle(handle);
	if (list == NULL) {
		ALOGE("Invalid handle(%d)", handle);
		return -EINVAL;
	}

	for (i = 0; i < number; i++) {
		if (list->dep_mask & (1ULL << context[i].sensor->type)) {
			if (enable) {
				err = context[i].driver->enable(context[i].sensor->handle, 1);
				if (!err)
					context[i].listener |= 1ULL << list->sensor->type;
			} else {
				context[i].listener &= ~(1ULL << list->sensor->type);

				/* Recover the original rate. Should be enhanced to get the smallest delay. */
				context[i].driver->setDelay(context[i].sensor->handle,
						context[i].delay_ms * 1000);
				if (context[i].listener == 0) {
					err =context[i].driver->enable(context[i].sensor->handle, 0);
				}
			}
		}
	}

	return err;
}

/* TODO: The polling delay may not correctly set for some special case */
int NativeSensorManager::setDelay(int handle, int64_t ns)
{
	const SensorContext *list;
	int i;
	int number = getSensorCount();
	unsigned long delay_ms = ns / 1000;
	int index;

	list = getInfoByHandle(handle);
	if (list == NULL) {
		ALOGE("Invalid handle(%d)", handle);
		return -EINVAL;
	}
	list->driver->setDelay(handle, ns);
	index = list - &context[0];
	context[index].delay_ms = delay_ms;

	for (i = 0; i < number; i++) {
		if (list->dep_mask & (1ULL << context[i].sensor->type)) {
			if ((delay_ms < context[i].delay_ms) || (context[i].delay_ms == 0)) {
				context[i].driver->setDelay(context[i].sensor->handle, delay_ms * 1000);
			}
		}
	}

	return 0;
}

int NativeSensorManager::readEvents(int handle, sensors_event_t* data, int count)
{
	const SensorContext *list;
	int i;
	int number = getSensorCount();
	int nb;

	list = getInfoByHandle(handle);
	if (list == NULL) {
		ALOGE("Invalid handle(%d)", handle);
		return -EINVAL;
	}
	nb = list->driver->readEvents(data, count);

	/* Need to make some enhancement to use hash search to improve the performance */
	for (i = 0; i < number; i++) {
		if (context[i].dep_mask & (1ULL << list->sensor->type)) {
			context[i].driver->injectEvents(data, nb);
		}
	}

	return nb;
}

int NativeSensorManager::hasPendingEvents(int handle)
{
	const SensorContext *list;

	list = getInfoByHandle(handle);
	if (list == NULL) {
		ALOGE("Invalid handle(%d)", handle);
		return -EINVAL;
	}

	return list->driver->hasPendingEvents();
}

