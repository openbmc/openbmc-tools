// This benchmark is used to find more about the performance for reading
// sensor data from the file system.

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <filesystem>
#include <string>
#include <algorithm>
#include <set>
#include <vector>
#include <fstream>
#include <sys/time.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unistd.h>

#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/read.hpp>

constexpr bool DEBUG = false;

struct Stat;

int g_time_inputs = 0;
bool g_done = false;
std::vector<bool> g_flags;
std::vector<bool> g_ready;
bool g_use_async_read = false;
bool g_use_files = false;

std::string g_hwmon_root = "/sys/class/hwmon/";

std::vector<std::string> g_hwmon_device_paths; // example: /sys/class/hwmon/hwmon72
std::vector<std::string> g_hwmon_input_paths; // example: /sys/class/hwmon/hwmon72/in2_input
std::vector<int> g_input2device; // input index to device index

std::vector<std::ifstream> g_hwmon_ifstreams;
std::vector<std::string> g_hwmon_cache;

// Generate files for input, rather than using sysfs files
class TestFileGenerator {
public:
	TestFileGenerator(const char* root, int num_sensors, int num_inputs_per_sensor) {
		this->root = std::string(root);
		this->num_sensors           = num_sensors;
		this->num_inputs_per_sensor = num_inputs_per_sensor;

	}
	void Generate() {
		std::filesystem::directory_entry ety(this->root.c_str());
		if (ety.exists()) {
			printf("[TestFileGenerator] Error: Path %s already exists, cannot proceed to generate test files.\n");
			exit(0);
		} else {
			printf("[TestFileGenerator] Will create %d fake sensor folders, each containing %d fake sensor reading files.\n",
				   num_sensors, num_inputs_per_sensor);
			if (!std::filesystem::create_directory(std::filesystem::path(root))) {
				printf("[TestFileGenerator] Could not create folder %s\n", root.c_str());
				exit(0);
			}
			for (int i=1; i<=num_sensors; i++) {
				std::string folder_name = std::string("hwmon") + std::to_string(i);
				std::filesystem::path folder_path = std::filesystem::path(root + "/" + folder_name);
				if (!std::filesystem::create_directory(folder_path)) {
					printf("[TestFileGenerator] Could not create folder %s\n",
							folder_path.string().c_str());
					exit(0);
				} else {
					// Create files
					for (int j=1; j<=num_inputs_per_sensor; j++) {
						std::string file_path = folder_path.string() + "/in" + std::to_string(j) + "_input";
						std::ofstream ofs(file_path);
						if (!ofs.good()) {
							printf("[TestFileGenerator] Could not create file %s\n", file_path.c_str());
						}
						ofs << std::to_string(j + i * 100);
						ofs << "\n"; // This is expected in the stock psusensor
					}
				}
			}
			printf("[TestFileGenerator] Successfully generated test files at %s\n",
					this->root.c_str());
		}
	}
private:
	std::string root;
	int num_sensors;
	int num_inputs_per_sensor;
};

class ThreadBarrier {
public:
	ThreadBarrier(int nthds) {
		nthds_   = nthds;
		counter_ = 0;
		done_ = false;
	}
	void SyncThreads() {
		std::unique_lock<std::mutex> lock(mutex_);
		if (done_) return;
		// Lock is acquired here

		counter_ ++;

		 // If all nthds threads arrive here, all threads go
		if (counter_ == nthds_) {
			counter_ = 0;
			cv_.notify_all();
		} else { // Otherwise, wait until all nthds threads arrive here
			cv_.wait(lock); // This statement unlocks the lock
		}
		// Leaving the scope also unlocks the lock
	}
	void TearDown() {
		std::unique_lock<std::mutex> lock(mutex_);
		done_ = true;
		cv_.notify_all();
	}
	int nthds_;
	int counter_; // How many threads have arrived here
	std::mutex mutex_, mutex1_;
	std::condition_variable cv_;
	bool done_;
};

void FindAllHwmonPaths(const char* hwmon_root,
		               std::vector<std::string>* devices,
		               std::vector<std::string>* inputs) {
	const char* hwmon_path = hwmon_root;//"/sys/class/hwmon/";
	int device_idx = 0;
	for (const auto & hwmon_entry : std::filesystem::directory_iterator(hwmon_path)) {
		const std::string& path = hwmon_entry.path();
		devices->push_back(path);

		for (const auto & hwmon_file : std::filesystem::directory_iterator(path)) {
			std::string p = hwmon_file.path();
			ssize_t idx = p.find("_input");
			if (idx == int(p.size()) - 6) { inputs->push_back(p); }
			g_input2device.push_back(device_idx);
		}
		device_idx ++;
	}

	std::sort(devices->begin(), devices->end());
	std::sort(inputs->begin(),  inputs->end());
}

std::string ExtractFileName(const std::string x) {
	ssize_t idx = x.rfind("/");
	if (idx != std::string::npos) {
		return x.substr(idx+1);
	} else return x;
}

float DeltaMillis(struct timeval* t0, struct timeval *t1) {
	return (t1->tv_sec - t0->tv_sec) * 1000.0f + (t1->tv_usec - t0->tv_usec) / 1000.0f;
}

long DeltaUsec(struct timeval* t0, struct timeval *t1) {
	return (t1->tv_sec - t0->tv_sec) * 1000000L + (t1->tv_usec - t0->tv_usec);
}

struct SensorReadingSnapshot {
	std::vector<float> readings;
	void SaveToFile(const char* fn) {
		FILE* f = fopen(fn, "wb");
		for (float r : readings) fwrite(&r, sizeof(float), 1, f);
		fclose(f);
	}
	bool ReadFromFile(const char* fn) { // returns true if OK, false otherwise
		FILE* f = fopen(fn, "rb");
		if (!f) {
			printf("Could not open file %s for reading\n", fn);
			return false;
		}
		size_t n; float r;
		readings.clear();
		while ((n = fread(&r, sizeof(float), 1, f)) == 1) {
			readings.push_back(r);
		}
		fclose(f);
		return true;
	}
	// Returns MSE
	void CompareAgainst(const SensorReadingSnapshot& other) {
		bool verbose = false;
		if (getenv("VERBOSE1")) { verbose = true; }

		const size_t n1 = this->readings.size(), n2 = other.readings.size();
		if (n1 != n2) {
			printf("Error: sensor reading snapshot sizes mismatch: %lu vs %lu readings\n", n1, n2);
			return;
		}
		printf("--- Sensor reading snapshot comparison ---\n");
		float s = 0.0f;
		int num_violators = 0;
		for (int i=0; i<int(n1); i++) {
			float r1 = this->readings[i], r2 = other.readings[i];
			s = s + (r1-r2) * (r1-r2);
			float rel_diff = (r1-r2) / r2;
			bool violated = false;
			if (fabs(rel_diff) > 0.1) { violated = true; num_violators ++; }

			if (violated || verbose) {
				printf("%-60s: %11g vs %11g (%.2f %%)\n",
						g_hwmon_input_paths[i].c_str(), r1, r2, rel_diff * 100);
			}
		}
//		printf("Mean Squared Error: across %lu sensors: %g\n", n1, s / n1);
		printf("%d / %d sensors differ by 10%% or more\n", num_violators, n1);
	}
};

struct Stat {
	Stat(int num_inputs, int num_iters) {
		printf("[Stat] %d inputs, %d iterations\n", num_inputs, num_iters);
//		const size_t num_inputs  = g_hwmon_input_paths.size();
		t0_inputs.resize(num_inputs); t1_inputs.resize(num_inputs);

		for (int i=0; i<num_iters; i++) {
			std::vector<std::string> this_iter(num_inputs, "-1e99"); // Make enough space
			readings.push_back(this_iter);
			if (g_time_inputs) {
				std::vector<float> m(g_hwmon_input_paths.size(), 0);
				millis_inputs.push_back(m);
			}
		}
		iter = 0;

		this->num_total_inputs = num_inputs;
	}
	void Start() {
		// Increment iter here to prevent data-race condition at the last iteration.
		// namely, main thread has exited but set iter to be 1 past the end
		if (last_iter != -999) iter ++;
		gettimeofday(&t0, nullptr);
		num_todo = num_total_inputs;
	}
	bool AllSensorsDone() { return (num_todo == 0); }
	void End() { 
		last_iter = iter;
		gettimeofday(&t1, nullptr); 
		float millis = DeltaMillis(&t0, &t1);
		millis_iters.push_back(millis);
	}
	void Print() {
		if (millis_iters.size() > 0) {
			printf("Total time for %lu iterations (milliseconds):", millis_iters.size());
			float avg_time = 0.0f;
			for (int i=0; i<int(millis_iters.size()); i++) {
				printf(" %.2f", i, millis_iters[i]);
				avg_time += millis_iters[i];
			}
			printf("\nAverage time: %.2f ms\n", avg_time / millis_iters.size());
		}
	}
	void PrintAllReadings() {
		const int iters = int(readings.size());
		const int num_inputs = g_hwmon_input_paths.size();
		for (int i=0; i<num_inputs; i++) {
			printf("%-60s ", g_hwmon_input_paths[i].c_str());
			for (int j=0; j<iters; j++) {
				printf(" %11s", readings[j][i].c_str());
			}
			printf("\n");
		}
	}
	void PrintAllTimings() { // Aggregate timing
		printf("Per-input times (milliseconds) for %lu iterations:\n", millis_iters.size());
		const int num_inputs = g_hwmon_input_paths.size();
		const int iters = millis_inputs.size();
		for (int i=0; i<num_inputs; i++) {
			printf("%-60s ", g_hwmon_input_paths[i].c_str());
			float total = 0;
			for (int j=0; j<iters; j++) {
				printf(" %11g", millis_inputs[j][i]);
				total += millis_inputs[j][i];
			}
			printf("   Total=%g", total);
			printf("\n");
		}
	}
	void PrintAllTimestamps() { // Timestamps
		const int num_inputs = g_hwmon_input_paths.size();
		printf("ID\tInputName\tShiftedStart\tShiftedEnd\n");
		unsigned long min_timestamp = 0;

		for (int i=0; i<num_inputs; i++) {
			printf("%d\t%s\t%ld\t%ld\n",
					i+1, g_hwmon_input_paths[i].c_str(),
					DeltaUsec(&t0, &t0_inputs[i]),
					DeltaUsec(&t0, &t1_inputs[i]));
		}
	}
	void LogCurrIterReading(int idx, std::string reading) {
		readings[iter][idx] = reading;
	}
	void StartInput(int input_idx) {
		if (g_time_inputs == false) return;
		gettimeofday(&t0_inputs[input_idx], nullptr);
	}
	void EndInput(int input_idx) {
		num_todo --;

		if (g_time_inputs == false) return;
		gettimeofday(&t1_inputs[input_idx], nullptr);
		millis_inputs.back()[input_idx] = DeltaMillis(&t0_inputs[input_idx], &t1_inputs[input_idx]);
	}
	SensorReadingSnapshot GetSnapshot() {
		SensorReadingSnapshot ret;
		// 1. Average the reading of each sensor
		const int N = g_hwmon_input_paths.size();
		const int iters = millis_iters.size();
		for (int j=0; j<N; j++) {
			float sum = 0;
			for (int i=0; i<iters; i++) {
				float reading = std::atof(readings[i][j].c_str());
				sum = sum + reading;
			}
			sum = sum / iters;
			ret.readings.push_back(sum);
		}
		return ret;
	};
private:
	struct timeval t0, t1;
	std::vector<float> millis_iters;
	std::vector<std::vector<float> > millis_inputs;
	std::vector<struct timeval> t0_inputs, t1_inputs;
	std::vector<std::vector<std::string> > readings;
	int iter = 0;
	int last_iter = -999;
	int num_total_inputs;
    std::atomic<int> num_todo;
};

// Mimic the behavior of PSUSensor
class MiniPSUSensor {
public:
	MiniPSUSensor(Stat* stat_, int idx_, std::string& path_, boost::asio::io_service& io, int mode_) : inputStream(io) {
		path = path_;
		stat = stat_;
		idx = idx_;
		ioservice = &io;
		mode = mode_;
		result_populated = false;

		if (mode == 1) { // Use Async Read
			int fd = open(path.c_str(), O_RDONLY);
			if (fd < 0) {
				printf("FD of %s = %d\n", path.c_str(), fd);
				abort();
			}
			inputStream.assign(fd);
			inputStream.non_blocking(true);
		} else if (mode == 0) { // Read ifstream
			inputStream1 = std::ifstream(path.c_str());
		} else { // read from cacahe
			// Do nothing
		}
	}
	int idx;
	Stat* stat;
	std::string path, reading;
	std::mutex mtx;

	std::atomic<bool> result_populated;
	void PopulateResult(std::string& x) {
		std::unique_lock<std::mutex> lock(mtx);
		result_populated = true;
		cv_result_wait.notify_one();
		reading = x;
	}
	std::condition_variable cv_result_wait;

	// for async read
	boost::asio::posix::stream_descriptor inputStream;
	boost::asio::streambuf                inputBuf{4096}; // page size

	boost::asio::io_service*              ioservice;

	// for sync read
	std::ifstream                         inputStream1;

	int mode;

	void PerformRead() {
//		printf("PerformRead %s\n", path.c_str());
		ioservice->post([&]() {

			if (mode == 1) {
				stat->StartInput(idx);
				if (inputStream.is_open() == false) {
					printf("inputStream of %s got closed for some reason\n", path.c_str());
					abort();
				}
				lseek(inputStream.native_handle(), 0, SEEK_SET);
				boost::asio::async_read_until(
					inputStream, inputBuf, '\n',
					[&](const boost::system::error_code& ec, std::size_t nr){
//						printf(">> async read #%d\n", idx);
						// HandleResponse
						if (ec != boost::system::errc::success) {
							printf("async_read of %s got an error: %s\n", path.c_str(),
									ec.message().c_str());
							abort();
						}
						std::istream responseStream(&inputBuf);
						std::getline(responseStream, reading);
						inputBuf.consume(inputBuf.size());
						stat->EndInput(idx);
						stat->LogCurrIterReading(idx, reading);
	//					printf("async_read of %s got %lu bytes\n", path.c_str(), nr);
//						printf("<< async read #%d\n", idx);
					});
			} else {
				stat->StartInput(idx);
				inputStream1.seekg(0);
				inputStream1 >> reading;
				stat->EndInput(idx);
				stat->LogCurrIterReading(idx, reading);
			}
		});
	}

	void PerformReadCached() { // Reads sensor reading from g_hwmon_cache; need to pre-populate
		ioservice->post([&]() {
			std::unique_lock<std::mutex> lock(mtx);
			stat->StartInput(idx);
//			printf("[PerformReadCached] sensor %p reading %s\n", this, reading.c_str());
			stat->LogCurrIterReading(idx, reading);
			stat->EndInput(idx);
		});
	}

	void PerformReadCachedProactive() {
		ioservice->post([&]() {
			std::unique_lock<std::mutex> lock(mtx);
			{
				stat->StartInput(idx);
				if (!result_populated) {
					cv_result_wait.wait(lock);
				}
			}
			{ // The second lock is needed to prevent data race over this->reading
				stat->LogCurrIterReading(idx, reading);
				stat->EndInput(idx);
				result_populated = false;
			}
		});
	}
};


void do_single_threaded(Stat* s, int niter) {
	for (int iter = 0; iter < niter; iter ++) {
		printf("iteration %d/%d\n", iter+1, niter);
		s->Start();
		int idx = 0;
		for (const std::string& x : g_hwmon_input_paths) {
			s->StartInput(idx);

			bool good = true;
			std::ifstream ifs(x);
			if (!ifs.good()) good = false;
			std::string content;
			ifs >> content;

			s->EndInput(idx);
			if (good) s->LogCurrIterReading(idx, content);
			idx ++;
		}
		s->End();
	}
}

void do_single_threaded1(Stat* s, int niter) {
	// Preparation
	const int N = int(g_hwmon_input_paths.size());
	g_hwmon_ifstreams.clear();
	for (int i=0; i<N; i++) {
		g_hwmon_ifstreams.emplace_back(g_hwmon_input_paths[i].c_str());
	}

	for (int iter = 0; iter < niter; iter ++) {
		printf("iteration %d/%d\n", iter+1, niter);
		s->Start();
		for (int i=0; i<N; i++) {
			s->StartInput(i);
			std::ifstream* ifs = &(g_hwmon_ifstreams[i]);
			std::string content;
			(*ifs) >> content;
			ifs->seekg(0);
			s->EndInput(i);
			if (ifs->good()) s->LogCurrIterReading(i, content);
		}
		s->End();
	}
}

// lb = Lower Bound, ub = Upper Bound
// lb and ub are inclusive
void* do_thread_start(Stat* s, int lb, int ub) {
	for (int i=lb; i<=ub; i++) {
		std::string x = g_hwmon_input_paths[i];
		s->StartInput(i);

		bool good = true;
		std::ifstream ifs(x);
		if (!ifs.good()) good = false;
		std::string content;
		ifs >> content;

		s->EndInput(i);
		if (good) s->LogCurrIterReading(i, content);
	}
	return nullptr;
}

void do_thread_start1(Stat* s, int lb, int ub) {
	for (int i=lb; i<=ub; i++) {
		std::ifstream* ifs = &(g_hwmon_ifstreams[i]);
		s->StartInput(i);
		std::string content;
		(*ifs) >> content;
		ifs->seekg(0);
		s->EndInput(i);
		if (ifs->good()) s->LogCurrIterReading(i, content);
	}
}

void do_multithreaded(Stat* s, int niter, int nthds, int which) {

	const int N = g_hwmon_input_paths.size();
	if (which == 1) {
		g_hwmon_ifstreams.clear();
		for (int i=0; i<N; i++) g_hwmon_ifstreams.emplace_back(g_hwmon_input_paths[i].c_str());
	}
	
	for (int iter = 0; iter < niter; iter++) {
		printf("iteration %d / %d\n", iter+1, niter);
		s->Start();
		int increment = N / nthds;
		if (increment < 1) increment = 1;
		int lb = 0;
		std::vector<std::thread*> thds;
		for (int i=0; i<nthds; i++) {
			int ub = lb + increment;
			if (ub >= N) ub = N-1;

			// Only print this during the first iteration
			if (iter == 0) printf("Thread %d reads sensors # %d through %d inclusive\n", i, lb, ub);

			std::thread* t;
			if (which == 0) {
				t = new std::thread(do_thread_start, s, lb, ub);
			} else if (which == 1) {
				t = new std::thread(do_thread_start1, s, lb, ub);
			} else assert(0);
			thds.push_back(t);
			lb += increment + 1;
			if (lb >= N) break;
		}

		printf("Launched %lu threads\n", thds.size());
		
		for (int i=0; i<thds.size(); i++) thds[i]->join();
		s->End();
	}
}

void do_mini_psusensors(Stat* s, int niter) {
	boost::asio::io_service io;
	const int N = g_hwmon_input_paths.size();
	std::vector<MiniPSUSensor*> mps;
	for (int i=0; i<N; i++) { 
		mps.push_back(new MiniPSUSensor(s, i, g_hwmon_input_paths[i], io, 1));
	}
	for (int iter=0; iter<niter; iter++) {
		printf("iteration %d/%d\n", iter+1, niter);
		io.reset();
		s->Start();
		for (int i=0; i<N; i++) { mps[i]->PerformRead(); }
		io.run();
		s->End();
	}
}

void* thread_start_mini_psusensors(int tid, boost::asio::io_service* s) {
	s->run();
	return nullptr;
}

// Multiple ASIO Services; multiple threads;
// Threads are equally assigned to ASIO services
// Sensors are equally assigned to ASIO services too
void do_mini_psusensors_mt(Stat* s, int niter, int nthds, int nioservices) {
	assert (nthds >= nioservices);
	std::vector<boost::asio::io_service*> ioservices;
	for (int i=0; i<nioservices; i++) {
		ioservices.push_back(new boost::asio::io_service(BOOST_ASIO_CONCURRENCY_HINT_UNSAFE));
	}
	const int N = g_hwmon_input_paths.size();
	std::vector<MiniPSUSensor*> mps;
	for (int i=0; i<N; i++) {
		mps.push_back(new MiniPSUSensor(s, i, g_hwmon_input_paths[i],
				*(ioservices[i % nioservices]), int(g_use_async_read)));
	}

	std::vector<int> thd2nio(nthds, -999);
	if (nthds >= nioservices) {
		for (int i=0; i<nthds; i++) thd2nio[i] = i % nioservices;
	} else { 
		abort();
	}

	for (int iter=0; iter<niter; iter++) {
		printf("iteration %d/%d\n", iter+1, niter);
		std::vector<std::thread*> thds;
		for (int i=0; i<nioservices; i++) ioservices[i]->reset();

		s->Start();
		for (int i=0; i<N; i++) { mps[i]->PerformRead(); }
		// Must post first then start worker threads, or the threads will just exit
		for (int i=0; i<nthds; i++) {
			thds.push_back(new std::thread(thread_start_mini_psusensors, i,
    				               	         ioservices[thd2nio[i]]));
		}
		for (int i=0; i<nthds; i++) { thds[i]->join(); }
		s->End();

	}
}


void do_mini_psusensors_asio_work(Stat* s, int niter, int nthds) {
	boost::asio::io_service io;
	const int N = g_hwmon_input_paths.size();
	std::vector<MiniPSUSensor*> mps;
	for (int i=0; i<N; i++) { 
		mps.push_back(new MiniPSUSensor(s, i, g_hwmon_input_paths[i], io, int(g_use_async_read)));
	}
	for (int iter=0; iter<niter; iter++) {

		boost::asio::io_service::work* work = new boost::asio::io_service::work(io);

		printf("iteration %d/%d\n", iter+1, niter);
		s->Start();

		std::vector<std::thread> thds;
		for (int i=0; i<nthds; i++) { thds.emplace_back([&]{ 
			io.run(); });
		}

		for (int i=0; i<N; i++) { mps[i]->PerformRead(); }

		delete work;

		for (int i=0; i<thds.size(); i++) {
			thds[i].join();
		}
		s->End();

		io.reset();
	}
}

// The worker thread in this method reads the contents in the sysfs files into the cache
void* thread_start_worker_populate(int tidx, int lb, int ub) {
	for (int i=lb; i<=ub; i++) {
		std::string x = g_hwmon_input_paths[i];
		std::ifstream ifs(x);
		if (ifs.good()) {
			std::string content;
			ifs >> content;
			g_hwmon_cache[i] = content;
		}
	}

	return nullptr;
}

// Cache using nthds threads, read cache using nthds2 threads
void cached_asio_read(Stat* s, int niter, int nthds, int nthds2) {
	g_flags.resize(nthds);
	for (int i=0; i<nthds; i++) g_flags[i] = false;
	const int N = g_hwmon_input_paths.size();
	g_ready.resize(N);
	// Prepare thread pool
	// 1. cache
	int increment = N / nthds;
	if (increment < 1) increment = 1;
	g_hwmon_cache.resize(N);


	for (int iter=0; iter<niter; iter++) {
		printf("Iteration %d/%d\n", iter+1, niter);

		boost::asio::io_service io;
		boost::asio::io_service::work* work = new boost::asio::io_service::work(io);
		std::vector<MiniPSUSensor*> mps;
		for (int i=0; i<N; i++) { mps.push_back(new MiniPSUSensor(s, i, g_hwmon_input_paths[i], io, 2)); }

		s->Start();

		// 1. cache
		int lb = 0;
		std::vector<std::thread> thds;
		for (int i=0; i<nthds; i++) {
			int ub = lb + increment;
			if (ub >= N) ub = N-1;
			thds.emplace_back(thread_start_worker_populate, i, lb, ub);
			lb += (increment + 1);
			if (lb >= N) break;
		}
		for (int i=0; i<thds.size(); i++) thds[i].join();

		std::vector<std::thread> workers;
		for (int i=0; i<nthds2; i++) { workers.emplace_back([&]{ io.run(); }); }

		// 2. submit jobs to asio
		for (int i=0; i<N; i++) { mps[i]->PerformReadCached(); }

		delete work;
		for (int i=0; i<nthds2; i++) { workers[i].join(); }
		s->End();
	}
}

class WorkerThread {
public:
	WorkerThread(int _lb, int _ub, std::vector<MiniPSUSensor*>*, ThreadBarrier*[]);
	void NotifyFinish();
private:
	int lb, ub; // Sensor ID lower and upper bounds, inclusive
	std::thread* thd;
	bool active;
	int idx;
	static int global_idx;
};

int WorkerThread::global_idx = 0;

std::mutex g_mutex_consumer;
std::condition_variable g_cv_consumer;
std::atomic<int> g_num_threads;
std::atomic<int> g_num_mps_todo(0); // MPS = Mini Psu Sensor
WorkerThread::WorkerThread(int _lb, int _ub, std::vector<MiniPSUSensor*>* mps,
		ThreadBarrier* worker_barrier[2]) {
	lb = _lb; ub = _ub;
	idx = global_idx ++;
	active = true;
	thd = new std::thread([this, mps, worker_barrier] {
		printf("Worker thd %d started\n", idx);
		while (active) {
			if (active) { 
				if constexpr(DEBUG) printf("Worker thd %d barrier 0\n", idx);
				worker_barrier[0]->SyncThreads(); 
			}
//			printf("WorkerThread %d-%d\n", lb, ub);
			for (int i=lb; i<=ub; i++) {
				std::string x = g_hwmon_input_paths[i];
				std::ifstream ifs(x);
				g_hwmon_cache[i] = "";
				if (ifs.good()) {
					std::string content;
					ifs >> content;
					g_hwmon_cache[i] = content;
					mps->at(i)->PopulateResult(content);
				} else {
					printf("[WorkerThread for %d-%d] Cannot read %d\n", lb, ub, i);
				}
			}
			if (active) { 
				if constexpr(DEBUG) printf("Worker thd %d barrier 1\n", idx);
				worker_barrier[1]->SyncThreads();
			}
		}
	});
}

void WorkerThread::NotifyFinish() {
	active = false;
	thd->join();
}

// handler_mode == 0: handlers do not start until all reading completes
// handler_mode == 1: handlers start as soon as reading starts
void cached_asio_read_threadpool(Stat* s, int niter, int nthds, int nthds2, int handler_mode) {
	std::vector<MiniPSUSensor*> mps;
	const int N = g_hwmon_input_paths.size();
	int increment = N / nthds;
	if (increment < 1) increment = 1;
	g_hwmon_cache.resize(N);

	ThreadBarrier* worker_barrier0 = new ThreadBarrier(nthds+1);
	ThreadBarrier* worker_barrier1 = new ThreadBarrier(nthds+1);
	ThreadBarrier* worker_barriers[] = { worker_barrier0, worker_barrier1 };

	std::vector<WorkerThread*> workers;
	int lb = 0;
	for (int i=0; i<nthds; i++) {
		int ub = lb + increment;
		if (ub >= N) ub = N-1;
		workers.push_back(new WorkerThread(lb, ub, &mps, worker_barriers));
		lb += (increment + 1);
//		if (lb >= N) break;
	}

	boost::asio::io_service io;
	boost::asio::io_service::work* work = new boost::asio::io_service::work(io);
	std::vector<std::thread> workers2;
	for (int i=0; i<nthds2; i++) { workers2.emplace_back([&]{ io.run(); }); }

	for (int i=0; i<N; i++) { mps.push_back(new MiniPSUSensor(s, i, g_hwmon_input_paths[i], io, 2)); }

	for (int iter = 0; iter < niter; iter++) {
		printf("Iteration %d/%d\n", iter, niter);

		s->Start();

		g_num_mps_todo = int(g_hwmon_input_paths.size());

		if (handler_mode == 0) {
			if constexpr(DEBUG) printf("Main thread barrier0 >>\n");
			worker_barriers[0]->SyncThreads();
			if constexpr(DEBUG) printf("Main thread barrier0 <<\n");
			if constexpr(DEBUG) printf("Main thread barrier1 >>\n");
			worker_barriers[1]->SyncThreads();
			if constexpr(DEBUG) printf("Main thread barrier1 <<\n");
			for (int i=0; i<N; i++) { mps[i]->PerformReadCached(); }
		} else {
			if constexpr(DEBUG) printf("Main thread barrier0 >>\n");
			worker_barriers[0]->SyncThreads();
			if constexpr(DEBUG) printf("Main thread barrier0 <<\n");
			for (int i=0; i<N; i++) { mps[i]->PerformReadCachedProactive(); }
			if constexpr(DEBUG) printf("Main thread barrier1 >>\n");
			worker_barriers[1]->SyncThreads();
			if constexpr(DEBUG) printf("Main thread barrier1 <<\n");
		}

		// Wait for all ASIO work items to complete. This step is not needed when hacking
		// psusensor. It is needed here because
		// 1) MiniPSUSensor objects write to the global Stat object and their iteration
		//    index and the iteration index of the worker threads must be synchronized.
		// 2) psusensor's interval is long enough so that the ASIO work items will be
		//    completed way before the next wave of sensor polling starts, whereas
		//    there is no wait between iterations in this microbenchmark.
		//
		// Thus the synchronization here.
		while (!s->AllSensorsDone()) { usleep(100); }

		s->End();
	}

	worker_barriers[0]->TearDown();
	worker_barriers[1]->TearDown();
	for (int i=0; i<nthds; i++) { workers[i]->NotifyFinish(); }
	io.stop();
	delete work;
	for (int i=0; i<nthds2; i++) { workers2[i].join(); }
}

void PrintHelp() {
	printf("Please specify a valid running mode:\n");
	printf("createtestfiles <path>: create test files\n");
	printf("0         : single thread,   open ifstream, read\n");
	printf("1  <nthds>: multiple thread using std::thread, open ifstream, read\n");
	printf("10        : single thread,   keep ifstream open\n");
	printf("11 <nthds>: multiple thread using std::thread, keep ifstream open\n");
	printf("2         : single thread,   async, open streambuf & stream_descriptor\n");
	printf("31 <nthds>: multiple thread, open ifstream, multiple io_services\n");
	printf("41 <nthds>: multiple thread, open ifstream, one io_service\n");
	printf("5  <nthds>: N worker threads + asio, 1 thread; launch new threads every iteration\n");
	printf("5  <nthds>: N worker threads + asio, 1 thread; threads in thread pool and are reused\n");
	printf("\n");
	printf("To print all sensor readings as well as comparison against snapshot, set environment variable VERBOSE1\n");
	printf("To print the aggregate timing for each individual sensor input,  set environment VERBOSE2\n");
	printf("To print timestamps timing for each individual sensor input,  set environment VERBOSE3\n");
}

int main(int argc, char** argv) {
	SensorReadingSnapshot* baseline = nullptr;
	int niter = 5;
	int mode = 0;

	if (getenv("VERBOSE2")) { g_time_inputs = true; }
	if (getenv("VERBOSE3")) { g_time_inputs = true; }

	int nthds = 2, nthds2 = 2;
	char * x = getenv("NITER");

	if (x) niter = std::atoi(x);

	x = getenv("BASELINE"); // For comparison
	if (x) {
		baseline = new SensorReadingSnapshot();
		if (!baseline->ReadFromFile(x)) {
			printf("Could not load sensor reading snapshot from %s\n", x);
			delete baseline;
			baseline = nullptr;
		} else {
			printf("Will compare this run's average readings against %s\n", x);
		}
	}

	x = getenv("HWMON_ROOT");
	if (x) {
		printf("HWMON_ROOT set to %s\n", x);
		g_hwmon_root = std::string(x);
	}

	if (argc > 1 && std::string(argv[1]) == "createtestfiles") {
		int num_sensors = 100;
		int num_inputs_per_sensor = 7;
		if (argc > 3) num_sensors           = std::atoi(argv[3]);
		if (argc > 4) num_inputs_per_sensor = std::atoi(argv[4]);
		TestFileGenerator gen(argv[2], num_sensors, num_inputs_per_sensor);
		gen.Generate();
		exit(0);
	}

	if (argc > 1) { mode = std::atoi(argv[1]); }
	else { printf(""); }

	if (argc > 2) { nthds = std::atoi(argv[2]); }
	if (nthds < 2) { nthds = 2; }

	if (argc > 3) { nthds2 = std::atoi(argv[3]); }
	if (nthds2 < 1) { nthds2 = 1; }

	// Specifying machine allows this step to be skipped
	FindAllHwmonPaths(g_hwmon_root.c_str(), &g_hwmon_device_paths, &g_hwmon_input_paths);

	if (argc > 1 && std::string(argv[1]) == "show") {
		printf("const std::vector<std::string> hwmon_device_paths = {\n");
		for (int i=0; i<g_hwmon_device_paths.size(); i++) {
			printf("  \"%s\",\n", g_hwmon_device_paths[i].c_str());
		}
		printf("};\n");
		printf("const std::vector<std::string> hwmon_input_paths = {\n");
		for (int i=0; i<g_hwmon_input_paths.size(); i++) {
			printf("  \"%s\",\n", g_hwmon_input_paths[i].c_str());
		}
		printf("};\n");
		return 0;
	}

	struct timeval t0, t1;
	Stat s(int(g_hwmon_input_paths.size()), niter);
	switch (mode) {
		case 0: { // "Method 1" in the document
			printf("Using ifstream, single-threaded mode.\n");
			do_single_threaded(&s, niter);
			break;
		}
		case 1: { // "Method 1" in the document
			printf("Using ifstream, %d std::threads\n", nthds);
			do_multithreaded(&s, niter, nthds, 0);
			break;
		}
		case 10: { // "Method 1A" in the document
			printf("Using ifstream but keeping the ifstream open, single-threaded\n");
			do_single_threaded1(&s, niter);
			break;
		}
		case 11: { // "Method 1A" in the document
			printf("Using ifstream but keeping the ifstream open, %d threads\n", nthds);
			do_multithreaded(&s, niter, nthds, 1);
			break;
		} 
		case 2: { // "Method 2A" in the document"
			printf("Using boost::asio::streambuf and boost::asio::posix::stream_descriptor, single-threaded.\n");
			do_mini_psusensors(&s, niter);
			break;
		}
		case 31: case 32:	{ // 31 = "Method 2A", 32 = "Method 2B"
			if (mode == 31) g_use_async_read = false; else g_use_async_read = true;
			int nioservices = nthds;
			if (argc > 3) {
				nioservices = std::atoi(argv[3]);
				if (nioservices < 1) { nioservices = 1; }
			}
			if (mode == 31) {
				printf("Using ifstream and io_service, %d threads, %d IO services\n", nthds, nioservices);
			} else {
				printf("Using boost::asio::streambuf and boost::asio::posix::stream_descriptor, %d threads, %d IO services\n", nthds, nioservices, g_use_async_read);
			}
			do_mini_psusensors_mt(&s, niter, nthds, nioservices);
			break;
		}
		case 41: case 42: { // 41 = "Method 3A", 42 = "Method 3B"
			if (mode == 41) g_use_async_read = false; else g_use_async_read = true;
			if (mode == 41) {
				printf("Using ifstream and io_service, %d threads and only 1 io_service\n", nthds);
			} else {
				printf("Using boost::asio::streambuf and boost::asio::posix::stream_descriptor, %d threads and only 1 io_service\n", nthds);
			}
			do_mini_psusensors_asio_work(&s, niter, nthds);
			break;
		}
		case 5: {
			// cache
			printf("Using \"read cached value from asio\" method, using %d threads to cache, and %d threads for asio\n",
					nthds, nthds2);
			cached_asio_read(&s, niter, nthds, nthds2);
			break;
		}
		case 6: {
			// cache
			printf("Using \"read cached value from asio\" method, using %d threads in a thread pool to cache, and %d threads for asio\n",
					nthds, nthds2);
			cached_asio_read_threadpool(&s, niter, nthds, nthds2, 0);
			break;
		}
		case 7: {
			// cache
			printf("Using \"read cached value from asio\" method, using %d threads in a thread pool to cache, and %d threads for asio, asio handlers start as soon as reading starts\n",
					nthds, nthds2);
			cached_asio_read_threadpool(&s, niter, nthds, nthds2, 1);
			break;
		}
		default: {
			PrintHelp();
			return 0;
		};
	}
	s.Print();
	if (getenv("VERBOSE1")) { s.PrintAllReadings(); }
	if (getenv("VERBOSE2")) { s.PrintAllTimings(); }
	if (getenv("VERBOSE3")) { s.PrintAllTimestamps(); }
	
	SensorReadingSnapshot this_snapshot = s.GetSnapshot();

	if (baseline) {
		this_snapshot.CompareAgainst(*baseline);
	} else {
		std::ifstream f("hwmon_bmk_snapshot");
		if (f.good()) {
			f.close();
		} else {
			this_snapshot.SaveToFile("hwmon_bmk_snapshot");
			printf("Saved this run's sensor readings to hwmon_bmk_snapshot\n");
		}
	}

	return 0;
}
