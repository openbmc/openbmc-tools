#include <fcntl.h>
#include <vector>
#include "proto/metricblob.pb.h"

int main(int argc, char** argv) {
	freopen(nullptr, "rb", stdin);
	char rbuf[4096];
	std::vector<char> data;

	while (true) {
		size_t r = fread(rbuf, 1, sizeof(rbuf), stdin);
		if (r > 0) {
			for (int i=0; i<r; i++) {
				data.push_back(rbuf[i]);
			}
		} else {
			break;
		}
	}

	printf("%d bytes read\n", int(data.size()));

	bmcmetrics::metricproto::BmcMetricSnapshot snapshot;
	if (!snapshot.ParseFromArray(data.data(), data.size())) {
		printf("Failed to parse BMC health snapshot\n");
		return 0;
	}
	printf("Parse succeeced\n");
	printf("------------- BMC Health Blob Contents: -------------------\n");
	bmcmetrics::metricproto::BmcMemoryMetric bm = snapshot.memory_metric();
	printf("Memory Metrics\n");
	printf("  KernelStack:  %d\n", bm.kernel_stack());
	printf("  MemAvailable: %d\n", bm.mem_available());
	printf("  Slab:         %d\n", bm.slab());

	bmcmetrics::metricproto::BmcDiskSpaceMetric dm = snapshot.storage_space_metric();
	printf("Storage Space Metric\n");
	printf("  RWFS space:   %d KiB\n", dm.rwfs_kib_available());

	bmcmetrics::metricproto::BmcUptimeMetric um = snapshot.uptime_metric();
	printf("Uptime Metrics\n");
	printf("  Uptime:            %g s\n", um.uptime());
	printf("  Idle Process Time: %g s\n", um.idle_process_time());

	bmcmetrics::metricproto::BmcStringTable st = snapshot.string_table();
	printf("String table has %d entries\n", st.entries_size());

	bmcmetrics::metricproto::BmcFdStatMetric fm = snapshot.fdstat_metric();
	printf("FD stat metrics\n");
	for (int i=0; i<fm.stats_size(); i++) {
		std::string s;
		int sidx = fm.stats(i).sidx_cmdline();
		if (sidx >= st.entries().size()) s = "[Outside the bounds of the string table]";
		else s = st.entries(sidx).value();
		printf("  %3d  %s  %d\n", i, s.c_str(), fm.stats(i).fd_count());
	}

	bmcmetrics::metricproto::BmcProcStatMetric pm = snapshot.procstat_metric();
	printf("Process stat metrics (stime, utime)\n");
	for (int i=0; i<pm.stats_size(); i++) {
		std::string s;
		int sidx = pm.stats(i).sidx_cmdline();
		if (sidx >= st.entries().size()) s = "[Outside the bounds of the string table]";
		else s = st.entries(sidx).value();
		printf("  %3d  %s  s:%g, u:%g\n", i, s.c_str(), pm.stats(i).stime(), pm.stats(i).utime());
	}
}
