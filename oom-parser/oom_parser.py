import os, sys

Proc_dict = {}

class proc_obj():
    def __init__(self, proc_line):
        self.dir = self.getProc_dir(proc_line)
        self.proc_line = proc_line
        self.proc_num = self.getProc_num(proc_line)
        self.run_results= {}

    def getProc_dir(self, proc_line):
        return proc_line

    def getProc_num(self, proc_line):
        return proc_line

    def add_result(self, result_obj):
        self.run_results[result_obj.id] = result_obj

    def print(self):
        print(self.dir)
        for x in self.run_results.keys():
            self.run_results[x].print()


class result_obj():
    def __init__(self, result_line):
        self.id = result_line
        self.results = {}

    def add_result(self, mem_type, size):
        self.results[mem_type] = size

    def print(self):
        print(self.id)
        for x in self.results.keys():
            print(x + ": " + self.results[x])



def Usage(script_name):
    sys.stdout.write("Usage: python3 "+ script_name + " -d " + " <data_file>\n")
    sys.stdout.write("Enter " + script_name + " -h" + "for help with option menu\n")
    sys.exit()


def menu_usage(script_name):
    sys.stdout.write("Menu options:\n 0: Recieve full proc result from proc specified.\n 1: Get individual result(s) from proc.\n 2: Generate report.\n")
    sys.stdout.write(" 0: Enter desired proc i.e '==> /proc/1/smaps <==' to retrieve full information of mem leaks runs for the specified.\n")
    sys.stdout.write(" 1: Enter individual data point to draw from given report, returns metric for each proc and entry.\n")
    sys.stdout.write(" 2: Enter individual data point to be put into outpt file specified. i.e 'Rss', 'Pss', 'Shared_Clean.\n'")
    sys.exit()


def Discovery(data_file):
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
                    print(y)
                    print(data_key + ": " + Proc_dict[x].run_results[y].results[z])

def gen_report():
    file_name = input("Name for output file: ")
    data_key = input("What data should be retrieved?: ")
    orig_stdout = sys.stdout
    out_file = open(file_name, "w")
    sys.stdout = out_file
    specific_result(data_key)
    sys.stdout = orig_stdout
    out_file.close()


def menu():
    options= ["0: Recieve full proc result from proc specified.", "1: Get individual result(s) from proc.", "2: Generate report."]
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
    else:
        print("Option not valid pick a valid one please.")
        menu()


if __name__ == "__main__":
    argc = len(sys.argv)
    if argc == 2 and sys.argv[1] == "-h":
        menu_usage(sys.argv[0])
    if argc != 3 or sys.argv[1] != "-d":
        Usage(sys.argv[0])
    else:
        Discovery(sys.argv[2])
    menu()



## Will print all entries in dictionary, basically prints back original data file.
##    for x in Proc_dict.keys():
##        print(x)
##        for y in Proc_dict[x].run_results.keys():
##            print(y)
##            for z in Proc_dict[x].run_results[y].results.keys():
##                print(z + Proc_dict[x].run_results[y].results[z])
