// This microbenchmark is used to study the effect of ASIO workload on DBus call time.
//
// Build command:
// g++ dbus_asio_bmk.cpp -std=c++17 -lpthread -lboost_system -lsystemd -lsdbusplus -O2 -o dbus_asio_bmk

#include <boost/asio/io_service.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <stdio.h>
#include <unistd.h>
#include <fstream>
#include <stdlib.h>

sd_bus* g_bus;
std::unique_ptr<boost::asio::io_service>        g_io;
std::shared_ptr<sdbusplus::asio::connection>    g_conn;
std::unique_ptr<sdbusplus::asio::object_server> g_object_server;

boost::asio::steady_timer *g_timer;
int g_interval_secs = 10;
int g_fake_read_time = 5000;
int num_fake_sensors = 750;
bool g_is_system_bus = false;
// 0: timer
// 1: manually calculate time
int g_timer_mode = 0;

std::chrono::steady_clock::time_point g_last_update;

class FakeSensor {
public:
	FakeSensor() {
		idx = serial++;
	}
	void Init() {
		iface_path = "/com/edgeofmap/DbusAsioBenchmarkInterface" + std::to_string(idx);
		iface_name = "com.edgeofmap.DbusAsioBenchmarkInterface" + std::to_string(idx);
		property_name = "value";
		iface = g_object_server->add_interface(iface_path, iface_name);
		iface->register_property(property_name, 0, sdbusplus::asio::PropertyPermission::readWrite);
		iface->initialize();
		counter = 0;
	}
	void Update() {
		g_io->post([this]() {
			usleep(g_fake_read_time); // 5 ms
			counter ++;
			if (!(iface->set_property(property_name, counter))) {
				fprintf(stderr, "Could not update %s\n", iface_name.c_str());
			}
		});
	}
private:
	int idx, counter;
	std::string iface_path, iface_name, property_name;
	std::shared_ptr<sdbusplus::asio::dbus_interface> iface;
	static int serial;
};
int FakeSensor::serial = 0;

std::vector<FakeSensor*> g_fake_sensors;

void CreateFakeSensors(int num_fake_sensors) {
	for (int i=0; i<num_fake_sensors; i++) {
		FakeSensor* s = new FakeSensor();
		s->Init();
		g_fake_sensors.push_back(s);
	}
}

void PollingLoop(int count) {
	printf("Polling loop #%d\n", count);

	for (int i=0; i<g_fake_sensors.size(); i++) {
		g_fake_sensors[i]->Update();
	}

	if (g_timer_mode == 0) {
		g_timer->expires_at(std::chrono::steady_clock::now() + std::chrono::seconds(g_interval_secs));
		g_timer->async_wait([count](const boost::system::error_code& ec) {
			g_io->post([count]() {
				PollingLoop(count + 1);
			});
		});
	} else {
		if (count == 0) {
			g_last_update = std::chrono::steady_clock::now();
		}
		g_io->post([count]() {
			std::chrono::steady_clock::time_point n = std::chrono::steady_clock::now();
			float elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(n - g_last_update).count();
			g_last_update = n;
			float need_to_sleep = g_interval_secs*1000000 - elapsed_us;
			printf("Elapsed %gus, need to sleep for %gus\n", elapsed_us, need_to_sleep);
			if (need_to_sleep > 0) usleep(need_to_sleep);
			PollingLoop(count + 1);
		});
	}
}

void* TestThread() {
	int nreps = 500;
	printf("Test thread started\n");
	if (g_interval_secs <= 5) { nreps = 100; }

	std::ofstream outfile("data/" + std::to_string(num_fake_sensors) + "_" + std::to_string(g_interval_secs) + 
			"_" + std::to_string(g_fake_read_time) + ".txt");

	if (outfile.good() == false) {
		printf("Could not open file for output\n");
		return nullptr;
	}

	sleep(1);
	for (int i=0; i<nreps; i++) {
		if (i % 10 == 0) {
			printf("Test iteartion %d/%d\n", i+1, nreps);
		}
		std::chrono::time_point<std::chrono::steady_clock> t0, t1;
		t0 = std::chrono::steady_clock::now();
		int rc;
		if (g_is_system_bus == false) {
			rc = system("busctl --user tree com.edgeofmap.DbusAsioBenchmark --no-page 1>/dev/null");
		} else {
			rc = system("busctl --system tree com.edgeofmap.DbusAsioBenchmark --no-page 1>/dev/null");
		}
		t1 = std::chrono::steady_clock::now();
		std::chrono::duration<double> diff = t1 - t0;
		outfile << std::to_string(diff.count()) << "\n";
	}

	outfile.close();

	printf("Test thread done, terminating program\n");
	exit(0);
	return nullptr;
}

int main(int argc, char** argv) {

	if (argc > 1) {
		num_fake_sensors = std::atoi(argv[1]);
		num_fake_sensors = std::max(1, num_fake_sensors);
	}
	if (argc > 2) {
		g_interval_secs = std::atoi(argv[2]);
		g_interval_secs = std::max(0, g_interval_secs);
	}
	if (argc > 3) {
		g_fake_read_time = std::atoi(argv[3]);
		g_fake_read_time = std::max(0, g_fake_read_time);
	}

	printf("Creating %d fake sensors\n", num_fake_sensors);
	printf("Polling interval set to %d seconds\n", g_interval_secs);
	printf("Fake read time set to %d us\n", g_fake_read_time);

	int r;
	char* x = getenv("USE_SYSTEM_BUS");
	if (x && std::atoi(x)>0) {
		g_is_system_bus = true;
	}
	x = getenv("TIMER_MODE");
	if (x) {
		g_timer_mode = std::atoi(x);
		printf("Timer mode set to %d\n", g_timer_mode);
	}

	if (g_is_system_bus) {
		r = sd_bus_open_system(&g_bus);
		if (r < 0) {
			printf("Failed to create system bus");
			exit(0);
		}
	} else {
		r = sd_bus_open_user(&g_bus);
		if (r < 0) {
			printf("Failed to create user bus");
			exit(0);
		}
	}

	printf("Creating io service\n");
	g_io = std::make_unique<boost::asio::io_service>();
	printf("Creating io connection\n");
	g_conn = std::make_shared<sdbusplus::asio::connection>(*g_io);
	printf("Requesting name\n");
	g_conn->request_name("com.edgeofmap.DbusAsioBenchmark");
	printf("Creating object server\n");
	g_object_server = std::make_unique<sdbusplus::asio::object_server>(g_conn);
	printf("Creating timer\n");
	g_timer = new boost::asio::steady_timer(*g_io);

	CreateFakeSensors(num_fake_sensors);
	PollingLoop(0);

	std::thread test_thd(TestThread);

	g_io->run();
}
