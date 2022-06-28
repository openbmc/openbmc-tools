import os
import sys

Proc_dict = {}
Proc_dict2 = {}
Pid_dict = {}
Mem_dict = {}


class proc_obj():
    def __init__(self, proc_line):
        self.dir = self.getProc_dir(proc_line)
        self.proc_line = proc_line
        self.proc_num = self.getProc_num(proc_line)
        self.name = ""
        self.run_results = {}

    def getProc_dir(self, proc_line):
        return proc_line

    def getProc_num(self, proc_line):
        proc_dir = proc_line.split("/")
        if proc_dir[2].isdigit():
            return proc_dir[2]
        else:
            return 0

    def add_result(self, result_obj):
        self.run_results[result_obj.id] = result_obj

    def print(self):
        print(self.dir)
        for x in self.run_results.keys():
            self.run_results[x].print()

    def sum_proc(self, mem_type):
        total = 0
        for x in self.run_results.keys():
            total = total + self.run_results[x].get_val(mem_type)
        return total

    def set_name(self, proc_name):
        self.name = proc_name


class result_obj():
    def __init__(self, result_line):
        self.id = result_line
        self.name = self.get_name(result_line)
        self.results = {}

    def get_name(self, result_line):
        if "/" in result_line:
            name_dir = result_line.split("/")
            return name_dir[-1]
        else:
            return "None"

    def add_result(self, mem_type, size):
        self.results[mem_type] = size

    def print(self):
        print(self.id)
        for x in self.results.keys():
            print(x + ": " + self.results[x])

    def get_val(self, mem_type):
        return int(self.results[mem_type][:-2])


def Usage(script_name):
    sys.stdout.write("Usage: python3 " + script_name + " -d " + " <data_file>"
                     + " (optional)-p " + "(optional)<PID data>\n")
    sys.stdout.write("Enter " + script_name + " -h" +
                     " for help with option menu\n")
    sys.exit()


def menu_usage(script_name):
    sys.stdout.write("Menu options:\n 0: Recieve full proc result from proc" +
                     " specified.\n" + " 1: Get individual result(s)" +
                     " from proc.\n 2: Generate report.\n")
    sys.stdout.write(" 0: Enter desired proc i.e '==> /proc/1/smaps <=='" +
                     " to retrieve full information of mem leaks runs" +
                     " for the specified.\n")
    sys.stdout.write(" 1: Enter individual data point to draw from given" +
                     " report, returns metric for each proc and entry.\n")
    sys.stdout.write(" 2: Enter individual data point to be put into output" +
                     "file specified. i.e 'Rss', 'Pss', 'Shared_Clean'.\n")
    sys.stdout.write(" 3: Enter an additional data file to track changes in" +
                     "metrics over time.\n")
    sys.exit()


def setup_pid():
    for x in Proc_dict.keys():
        pid = Proc_dict[x].proc_num
        Pid_dict[pid] = x


def Discovery(data_file, Proc_dict):
    data = open(data_file, "r")
    for x in data.readlines():
        line = x.strip("\n")
        if len(line) < 2:
            continue
        if "/proc/" in x:
            proc_run = proc_obj(line)
            Proc_dict[line] = proc_run
        elif line[0].isdigit() or line[0:2] == "ff":
            result = result_obj(line)
            proc_run.add_result(result)
        else:
            data_results = line.split(":")
            data_key = data_results[0]
            if len(data_results) != 2:
                data_val = ""
            else:
                data_val = data_results[1]
            result.add_result(data_key, data_val)
    setup_pid()


def full_proc_query():
    proc_query = input("Enter proc to grab information: ")
    if proc_query in Proc_dict.keys():
        print(proc_query)
        Proc_dict[proc_query].print()
    else:
        print("Not found")


def specific_result(data_key):
    for x in Proc_dict.keys():
        print(x)
        for y in Proc_dict[x].run_results.keys():
            for z in Proc_dict[x].run_results[y].results.keys():
                if z == data_key:
                    dir_name = Proc_dict[x].run_results[y].get_name(y)
                    if dir_name == "None":
                        print(y)
                    else:
                        print(dir_name)
                    print(data_key + ": " + Proc_dict[x].run_results[y].
                          results[z])


def gen_report():
    opt = input("Enter 1 for full report. Enter 2 for a report summary: ")
    file_name = input("Name for output file: ")
    data_key = input("What data should be retrieved?: ")
    report_type = input("Enter 1 for orginal ordering. Enter 2 for high to" +
                        " low ordering.(Only for summarized reports.)\n:")
    orig_stdout = sys.stdout
    out_file = open(file_name, "w")
    sys.stdout = out_file
    if opt == "1":
        specific_result(data_key)
    if opt == "2":
        sum_report(data_key, report_type)
    sys.stdout = orig_stdout
    out_file.close()


def sum_report(data_key, report_type):
    if report_type == "1":
        for x in Proc_dict.keys():
            print(x)
            print(str(Proc_dict[x].sum_proc(data_key)) + "kb\n")
    else:
        process_results = {}
        for x in Proc_dict.keys():
            process_results[x] = Proc_dict[x].sum_proc(data_key)
        while len(process_results) != 0:
            max_entry = max(process_results, key=process_results.get)
            name = Proc_dict[max_entry].name
            if len(name) == 0:
                name = "N/a"
            print(max_entry)
            print("Name: " + name)
            print("Total Size: " + str(process_results[max_entry]) + "kb\n")
            del process_results[max_entry]


def critical_report(critical_results):
    critical_file = open("critical_results.txt", "w")
    for x in critical_results.keys():
        critical_file.write(x + "\n")
        critical_file.write(Proc_dict[x].name + "\n")
        critical_file.write("Before: " + critical_results[x][0] + "\n")
        critical_file.write("After: " + critical_results[x][1] + "\n")
        critical_file.write("Diff: " + critical_results[x][2] + "\n")
        critical_file.write("Pct_change: " + critical_results[x][3] + "\n\n")
    critical_file.close()


def process_comp_report(process_res, process_res2):
    critical_results = {}
    while len(process_res) != 0:
        max_entry = max(process_res, key=process_res.get)
        name = Proc_dict[max_entry].name
        rep1 = process_res[max_entry]
        rep1_s = str(rep1) + "kB"
        rep2 = process_res2[max_entry]
        if str(rep2).isdigit():
            rep2_s = str(rep2) + "kB"
            if rep1 == rep2:
                res = "0kb"
            if rep1 > rep2:
                res = "-" + str(rep1-rep2) + "kB"
            else:
                change = rep2-rep1
                if rep1 != 0:
                    pct_increase = (change/rep1) * 100
                elif rep1 == 0 and rep2 == 0:
                    pct_increase = 0
                else:
                    pct_increase = 100
                res = "+" + str(change) + "kB"
                if pct_increase > 1:
                    critical_results[max_entry] = [rep1_s, rep2_s,
                                                   res, "%" +
                                                   str(pct_increase)[:4]]
        else:
            res = "N/A"
            rep2_s = rep2
        if len(name) == 0:
            name = "N/A"
        print(max_entry)
        print("Name: " + name)
        print("Before total Size: " + rep1_s)
        print("After Total Size: " + rep2_s)
        print("Diff: " + res + "\n")
        del process_res[max_entry]
    critical_report(critical_results)
    if len(Mem_dict) != 0:
        gen_mem_makeup()


def sum_comp_report(data_key):
    process_results = {}
    process_results2 = {}
    for x in Proc_dict.keys():
        if x in Proc_dict2.keys():
            rep1 = Proc_dict[x].sum_proc(data_key)
            rep2 = Proc_dict2[x].sum_proc(data_key)
            process_results[x] = rep1
            process_results2[x] = rep2
        else:
            rep1 = Proc_dict[x].sum_proc(data_key)
            rep2 = "N/A"
            process_results[x] = rep1
            process_results2[x] = rep2
    process_comp_report(process_results, process_results2)


def gen_comp_report():
    comp_file = input("Enter file to be compared: ")
    file_name = input("Enter the name of the output file: ")
    data_key = input("Enter the data to be retrieved: ")
    Discovery(comp_file, Proc_dict2)
    orig_stdout = sys.stdout
    out_file = open(file_name, "w")
    sys.stdout = out_file
    sum_comp_report(data_key)
    sys.stdout = orig_stdout
    out_file.close()


def menu():
    options = ["0: Recieve full proc result from proc specified.",
               "1: Get individual result(s) from proc.",
               "2: Generate report.", "3: Compare and generate reports."]
    for x in options:
        print(x)
    usr_option = input("Enter option: ")
    if usr_option == "0":
        full_proc_query()
        return
    if usr_option == "1":
        data_key = input("What data should be retrieved?: ")
        specific_result(data_key)
        return
    if usr_option == "2":
        gen_report()
        return
    if usr_option == "3":
        gen_comp_report()
        return
    else:
        print("Option not valid pick a valid one please.")
        menu()


def get_pid(line):
    proc_dir = line.split("/")
    if len(proc_dir) == 4:
        return proc_dir[2]
    else:
        return "None"


def pidLink(pidData):
    pid_file = open(pidData, "r")
    pid = ""
    lfName = False
    for x in pid_file.readlines():
        line = x.strip("\n")
        if not lfName and "/proc" in line:
            pid = get_pid(line)
            lfName = True
        if lfName and "Name" in line:
            if pid in Pid_dict.keys():
                proc = Pid_dict[pid]
                pid_name = line[6:]
                Proc_dict[proc].set_name(pid_name)
                lfName = False
            else:
                # print("Not in other data sheet: " + pid)
                # Wasn't present in other data sheet, could do other here...
                lfName = False
    pid_file.close()


def print_mem_makeup(out_file, res_dict):
    total = 0
    while len(res_dict) != 0:
        max_entry = max(res_dict, key=res_dict.get)
        total = total + res_dict[max_entry]
        out_file.write(max_entry + ": %" + str(res_dict[max_entry]) + "\n")
        del res_dict[max_entry]
    out_file.write("TOTAL IS: " + str(total))


def gen_mem_makeup():
    mem_file = open("mem_sum.txt", "w")
    mem_total_s = Mem_dict["MemTotal"]
    mem_total = int(mem_total_s[:-2])
    mem_file.write("Total_size: " + mem_total_s + "\n")
    res_dict = {}
    total = 0
    for x in Proc_dict.keys():
        size = Proc_dict[x].sum_proc("Size")
        total = total + size
        size_pct = round((size/mem_total) * 100, 2)
        proc_name = Proc_dict[x].name
        if proc_name in res_dict.keys():
            old_pct = res_dict[proc_name]
            res_dict[proc_name] = round(old_pct + (size_pct/100), 2)
        else:
            res_dict[proc_name] = size_pct
    mem_file.write("Total is: " + str(total))
    print_mem_makeup(mem_file, res_dict)
    mem_file.close()


def get_mem_info(mem_file):
    mem_file = open(mem_file, "r")
    for x in mem_file:
        line = x.strip("\n")
        mem_data = line.split(":")
        if len(mem_data) < 2:
            print("Error in parsing mem_file data!")
        else:
            Mem_dict[mem_data[0]] = mem_data[1]
    mem_file.close()


def get_mem_option():
    gen_mem_sum = input("Would you like to generate memory summary" +
                        "from a memory file?(y/n): ")
    if "y" in gen_mem_sum or "Y" in gen_mem_sum:
        mem_file = input("Enter mem_file name: ")
        get_mem_info(mem_file)
        return
    if "n" in gen_mem_sum or "N" in gen_mem_sum:
        return
    else:
        get_mem_option()


if __name__ == "__main__":
    argc = len(sys.argv)
    if argc == 1:
        Usage(sys.argv[0])
    if argc == 2 and sys.argv[1] == "-h":
        menu_usage(sys.argv[0])
    if argc > 5 or sys.argv[1] != "-d":
        Usage(sys.argv[0])
    else:
        Discovery(sys.argv[2], Proc_dict)
        if argc == 5 and sys.argv[3] == "-p":
            pidLink(sys.argv[4])
    get_mem_option()
    menu()

# Will print all entries in dictionary.
# Basically prints back original data file.
#    for x in Proc_dict.keys():
#        print(x)
#        for y in Proc_dict[x].run_results.keys():
#            print(y)
#            for z in Proc_dict[x].run_results[y].results.keys():
#                print(z + Proc_dict[x].run_results[y].results[z])
