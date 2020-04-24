/*
 * Thinkpad backlight service
 *
 * Copyright (c) 2020 Alexander Mohr
 *
 * Disables the thinkpad keyboard backlight when is not needed
*  MIT License
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
 */

#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include <vector>
#include <string>
#include <iostream>
#include <filesystem>
#include <regex>
#include <future>
#include <csignal>

using namespace std::chrono_literals;

std::chrono::time_point<std::chrono::system_clock> lastEvent_;
uint64_t originalBrightness_;
uint64_t currentBrightness_;

bool end_ = false;

enum MOUSE_MODE {
  ALL = 0,
  INTERNAL = 1,
  NONE = 2
};

void help(const char* name) {
  printf("%s %s \n", name, VERSION);
  printf(""
		 "    -h show this help\n"
		 "    -i ignore an input device\n"
		 "       This device does not re enable keyboard backlight.\n"
		 "       Separate multiple device by space.\n"
		 "       Default: use all mice and keyboard.\n"
		 "    -t configure timeout in seconds after which the backlight will be turned off\n"
		 "       Defaults to 30s \n"
		 "    -m configure mouse mode (0..2)\n"
		 "       0 use all mice (default)\n"
		 "       1 use all internal mice only\n"
		 "       2 ignore mice\n"
		 "    -b set keyboard backlight device path\n"
		 "       defaults to /sys/class/leds/tpacpi::kbd_backlight\n"
		 "    -f stay in foreground and do not start daemon\n"
		 "    -s Set a brightness value from 0..2 and exit\n"

  );
}

bool file_read_uint64(const std::string &filename, uint64_t *val) {
  FILE *fp;
  uint64_t data;

  fp = fopen(filename.c_str(), "r");
  if (!fp) {
	return false;
  }

  if (fscanf(fp, "%lu", &data) != 1) {
	fclose(fp);
	return false;
  }

  *val = data;

  fclose(fp);
  return true;
}

bool file_write_uint64(const std::string &filename, uint64_t val) {
  FILE *fp;

  fp = fopen(filename.c_str(), "w");
  if (!fp) {
	return false;
  }

  if (fprintf(fp, "%lu", val) < 0) {
	fclose(fp);
	return false;
  }

  fclose(fp);
  return true;
}

bool is_device_ignored(const std::string &device,
					   const std::vector<std::string> &ignoredDevices) {
  for (const auto &ignoredDev : ignoredDevices) {
	if (device.find(ignoredDev) != std::string::npos) {
	  return true;
	}
  }
  return false;
}

void get_devices_for_path(const std::vector<std::string> &ignoredDevices,
						  const std::string &devicePath,
						  const std::regex &regex,
						  std::vector<std::string> &devices) {
  for (const auto &dev : std::filesystem::directory_iterator(devicePath)) {
	if (is_device_ignored(dev.path(), ignoredDevices)) {
	  continue;
	}

	if (regex_match(std::string(dev.path()), regex)) {
	  devices.push_back(dev.path());
	}
  }
}

int open_device(const std::string &path) {
  int fd;

  if ((fd = open(path.c_str(), O_RDONLY)) < 0) {
	perror("tp_kbd_backlight: open");
	return -1;
  }

  return fd;
}

std::vector<int> open_devices(const std::vector<std::string> &input_devices) {
  std::vector<int> fds;
  for (const auto &dev : input_devices) {
	fds.push_back(open_device(dev));
  }
  return fds;
}
void brightness_control(const std::string &brightnessPath,
						unsigned long timeoutMs) {
  while (!end_) {
	auto passedMs = std::chrono::duration_cast<
		std::chrono::milliseconds>(
		std::chrono::system_clock::now() - lastEvent_);

	if (lastEvent_ < std::chrono::system_clock::now()) {
	  auto sleepTime = std::chrono::milliseconds(timeoutMs - passedMs.count());
	  if (0 != sleepTime.count()) {
		std::this_thread::sleep_for(sleepTime);
	  }
	}

	passedMs = std::chrono::duration_cast<
		std::chrono::milliseconds>(
		std::chrono::system_clock::now() - lastEvent_);
#if DEBUG
	printf("passed: %lu\n", passedMs.count());
#endif
	if (passedMs.count() >= static_cast<long>(timeoutMs)) {

#if DEBUG
	  printf("timeoutMs reached \n");
	  printf("o: %lu c: %lu\n", originalBrightness_, currentBrightness_);
#endif

	  if (currentBrightness_ != 0) {
		file_read_uint64(brightnessPath, &originalBrightness_);
		currentBrightness_ = 0;

		file_write_uint64(brightnessPath, 0);

#if DEBUG
		printf("o: %lu c: %lu\n", originalBrightness_, currentBrightness_);
		printf("turning off \n");
#endif
	  }

	  lastEvent_ = std::chrono::system_clock::now();
	}
  }
}

void read_events(int devFd, const std::string &brightnessPath) {
  while (!end_) {
	struct input_event ie = {};
	int rd = read(devFd, &ie, sizeof(struct input_event));
	if (rd != 0) {
	  lastEvent_ = std::chrono::system_clock::now();

	  if (currentBrightness_ != originalBrightness_) {
		file_write_uint64(brightnessPath, originalBrightness_);
		currentBrightness_ = originalBrightness_;

#if DEBUG
		printf("on\n");
#endif
	  }
	}
  }
  close(devFd);
}

void signal_handler(int sig) {
  switch (sig) {
	case SIGTERM:
	case SIGKILL:
	  end_ = true;
	  break;
	default:
	  break;
  }
}

bool is_brightness_writable(const std::string &brightnessPath) {
  if (!file_read_uint64(brightnessPath, &originalBrightness_)
	  || !file_write_uint64(brightnessPath, originalBrightness_)) {
	std::cout << "Write access to brightness device descriptor failed.\n"
				 "Please run with root privileges" << std::endl;
	return false;
  }
  return true;
}

void parse_opts(int argc,
				char *const *argv,
				std::vector<std::string> &ignoredDevices,
				unsigned long &timeout,
				MOUSE_MODE &mouseMode,
				std::string &backlightPath,
				bool &foreground,
				long &setBrightness) {
  int c;
  std::istringstream ss;
  std::string token;
  unsigned long mode;

  while ((c = getopt(argc, argv, "hs:i:t:m:b:f")) != -1) {
	switch (c) {
	  case 'b':
		backlightPath = optarg;
		break;
	  case 'f':
		foreground = true;
		break;
	  case 'i':
		ss = std::istringstream(optarg);
		while (std::getline(ss, token, ' ')) {
		  ignoredDevices.push_back(token);
		}
		break;
	  case 'm':
		mode = strtoul(optarg, nullptr, 0);
		if ((MOUSE_MODE::ALL > mode) | (MOUSE_MODE::NONE < mode)) {
		  printf("%s is not a valid mouse mode\n", optarg);
		  exit(EXIT_FAILURE);
		}
		mouseMode = static_cast<MOUSE_MODE>(mode);
		break;
	  case 't':
		timeout = strtoul(optarg, nullptr, 0);
		if (0 >= timeout) {
		  printf("%s is not a valid timeout\n", optarg);
		  exit(EXIT_FAILURE);
		}
		break;
	  case 's':
		setBrightness = strtol(optarg, nullptr, 0);
		if (setBrightness > 2 || setBrightness < 0) {
		  printf("%s is not a valid brightness\n", optarg);
		  exit(EXIT_FAILURE);
		}
		break;
	  case 'h':
	  default:
		help(argv[0]);
		exit(EXIT_FAILURE);
	}
  }
}

int main(int argc, char **argv) {
  std::vector<int> eventFd;

  std::vector<std::string> inputDevices;
  std::vector<std::string> ignoredDevices;

  signal(SIGTERM, signal_handler);
  signal(SIGKILL, signal_handler);

  unsigned long timeout = 15;
  long setBrightness = -1;
  MOUSE_MODE mouseMode = MOUSE_MODE::ALL;
  std::string backlightPath = "/sys/class/leds/tpacpi::kbd_backlight";
  bool foreground = false;
  parse_opts(argc,
			 argv,
			 ignoredDevices,
			 timeout,
			 mouseMode,
			 backlightPath,
			 foreground,
			 setBrightness);

  get_devices_for_path(ignoredDevices,
					   "/dev/input/by-path",
					   std::regex(".*event\\-kbd.*"),
					   inputDevices);

  switch (mouseMode) {
	case ALL:
	  get_devices_for_path(ignoredDevices,
						   "/dev/input/",
						   std::regex(".*mice.*"),
						   inputDevices);
	  break;
	case INTERNAL:
	  get_devices_for_path(ignoredDevices,
						   "/dev/input/by-path",
						   std::regex("..*event\\-mouse.*"),
						   inputDevices);
	  break;
	case NONE:
	  break;
  }

  if (inputDevices.empty()) {
	std::cout << "No input device found or all ignored\n";
	exit(EXIT_FAILURE);
  }

  if (backlightPath.at(backlightPath.size() - 1) != '/') {
	backlightPath += '/';
  }

  std::string brightnessPath = backlightPath + "brightness";
  if (!is_brightness_writable(brightnessPath)){
    exit(EXIT_FAILURE);
  }

  if (setBrightness >= 0) {
	file_write_uint64(brightnessPath, setBrightness);
	exit(0);
  }

  currentBrightness_ = originalBrightness_;

  auto fds = open_devices(inputDevices);
  lastEvent_ = std::chrono::system_clock::now();

  if (!foreground) {
	if (daemon(0, 0)) {
	  std::cout << "failed to daemonize" << std::endl;
	  exit(EXIT_FAILURE);
	}
  }

  inputDevices.clear();
  ignoredDevices.clear();

  std::vector<std::future<void>> f;
  for (const auto &fd : fds) {
	f.emplace_back(std::async(std::launch::async,
							  read_events,
							  fd,
							  brightnessPath));
  }

  brightness_control(brightnessPath, timeout * 1000);

  for (const auto &fd : fds) {
	close(fd);
  }
  exit(0);
}
