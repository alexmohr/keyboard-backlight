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
#include <sstream>
#include <iostream>
#include <filesystem>
#include <regex>
#include <future>
#include <csignal>
#include <fstream>

using namespace std::chrono_literals;

std::chrono::time_point<std::chrono::system_clock> lastEvent_;
uint64_t originalBrightness_;
uint64_t currentBrightness_;

bool end_ = false;
const std::string DEFAULT_BACKLIGHT_PATH = "/sys/class/leds/tpacpi::kbd_backlight/brightness";


enum MOUSE_MODE {
  ALL = 0,
  INTERNAL = 1,
  NONE = 2
};

#if DEBUG
#define print_debug(fmt, ...) printf("%s:%d: " fmt, __FILE__, __LINE__, __VA_ARGS__)
#define print_debug_n(fmt) printf("%s:%d: " fmt, __FILE__, __LINE__)
#else
#define print_debug(...)
#define print_debug_n(...)
#endif

void help(const char *name) {
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
		 "       defaults to %s\n"
		 "    -f stay in foreground and do not start daemon\n"
		 "    -s Set a brightness value and exit\n",
		 DEFAULT_BACKLIGHT_PATH.c_str()

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

/* Get keyboards from /proc/bus/input/devices
 * Example entry
	I: Bus=0011 Vendor=0001 Product=0001 Version=ab54
	N: Name="AT Translated Set 2 keyboard"
	P: Phys=isa0060/serio0/input0
	S: Sysfs=/devices/platform/i8042/serio0/input/input3
	U: Uniq=
	H: Handlers=sysrq kbd event3 leds
	B: PROP=0
	B: EV=120013
	B: KEY=402000000 3803078f800d001 feffffdfffefffff fffffffffffffffe
 */
void get_keyboards(std::vector<std::string> &ignoredDevices,
				   std::vector<std::string> &keyboards) {
  const std::string path = "/proc/bus/input/devices";
  std::ifstream file(path);
  if (!file.is_open()) {
	print_debug("Failed to open %s...\n", path.c_str());
	return;
  }

  bool isKeyboard = false;
  std::string line;
  std::string token;
  std::istringstream ss;
  while (std::getline(file, line)) {
	// get device name
	if (line.find("Name=") != std::string::npos) {
	  isKeyboard = line.find("keyboard") != std::string::npos;
	  if (isKeyboard) {
		print_debug("Detected keyboard: %s\n", line.c_str());
	  } else {
		print_debug("Ignoring non keyboard device: %s\n", line.c_str());
	  }
	}

	if (line.find("Handlers=") != std::string::npos) {
	  if (!isKeyboard) {
		continue;
	  }

	  ss = std::istringstream(line);
	  while (std::getline(ss, token, ' ')) {
		if (token.find("event") != std::string::npos) {
		  std::string deviceEventPath = "/dev/input/" + token;
		  if (!is_device_ignored(deviceEventPath, ignoredDevices)) {
			print_debug_n("Added keyboard\n");
			keyboards.emplace_back(deviceEventPath);
		  } else {
			print_debug_n("Keyboard is ignored\n");
		  }
		  break;
		}
	  }
	}
  }
}

void get_devices_in_path(const std::vector<std::string> &ignoredDevices,
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
  unsigned long tmpBrightness = currentBrightness_;
  while (!end_) {
	auto passedMs = std::chrono::duration_cast<
		std::chrono::milliseconds>(
		std::chrono::system_clock::now() - lastEvent_);

	if (lastEvent_ < std::chrono::system_clock::now()) {
	  auto sleepTime = std::chrono::milliseconds(timeoutMs - passedMs.count());
	  if (0 != sleepTime.count()) {
		print_debug("Sleeping for %lu ms\n", sleepTime.count());
		std::this_thread::sleep_for(sleepTime);
	  }
	}

	passedMs = std::chrono::duration_cast<
		std::chrono::milliseconds>(
		std::chrono::system_clock::now() - lastEvent_);
	print_debug("Ms since last event: %lu\n", passedMs.count());
	if (passedMs.count() >= static_cast<long>(timeoutMs)) {

	  print_debug_n("Timeout reached \n");
	  print_debug("Original brightness: %lu Current Brightness: %lu\n",
				  originalBrightness_,
				  currentBrightness_);

	  file_read_uint64(brightnessPath, &tmpBrightness);
	  if (tmpBrightness != 0) {
		originalBrightness_ = tmpBrightness;
		currentBrightness_ = 0;
		file_write_uint64(brightnessPath, 0);
		print_debug("New Original brightness: %lu New Current Brightness: %lu\n",
					originalBrightness_,
					currentBrightness_);
		print_debug_n("Turning lights off\n");
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

		print_debug("Event in fd %i, turning lights on\n", devFd);
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
  std::filesystem::path p(brightnessPath);
  if (!std::filesystem::exists(p)) {
	printf("Brightness device %s does not exist\n", brightnessPath.c_str());
	return false;
  }

  if (!file_read_uint64(brightnessPath, &originalBrightness_)
	  || !file_write_uint64(brightnessPath, originalBrightness_)) {
	printf("Write access to brightness device %s failed."
		   " Please run with root privileges", brightnessPath.c_str());
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
  long mode;

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

		  // if the device is a symlink add the actual target to the
		  // ignored device list too
		  std::filesystem::path p = token;
		  if (!std::filesystem::exists(p))
			continue;

		  std::filesystem::current_path(p.parent_path());
		  if (std::filesystem::is_symlink(p)) {
			p = std::filesystem::read_symlink(p);
		  }

		  if (!p.is_absolute()) {
			p = std::filesystem::canonical(p);
		  }
		  ignoredDevices.push_back(p);
		}
		break;
	  case 'm':
		mode = strtol(optarg, nullptr, 0);
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

  bool foreground = false;
  std::string backlightPath = DEFAULT_BACKLIGHT_PATH;
  print_debug_n("Parsing options...\n");
  parse_opts(argc,
			 argv,
			 ignoredDevices,
			 timeout,
			 mouseMode,
			 backlightPath,
			 foreground,
			 setBrightness);
  print_debug("Using backlight device: %s\n", backlightPath.c_str());

  print_debug_n("Getting keyboards...\n");
  get_keyboards(ignoredDevices, inputDevices);
  if (inputDevices.empty()) {
	std::cout << "Warning no keyboards found!" << std::endl;
  }

  switch (mouseMode) {
	case ALL:
	  get_devices_in_path(ignoredDevices,
						  "/dev/input/",
						  std::regex(".*mice.*"),
						  inputDevices);
	  break;
	case INTERNAL:
	  get_devices_in_path(ignoredDevices,
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

  if (!is_brightness_writable(backlightPath)) {
	exit(EXIT_FAILURE);
  }

  if (setBrightness >= 0) {
	file_write_uint64(backlightPath, setBrightness);
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
							  backlightPath));
  }

  brightness_control(backlightPath, timeout * 1000);

  for (const auto &fd : fds) {
	close(fd);
  }

  exit(0);
}
